// A simple example to capture the following task dependencies.
//
// TaskA---->TaskB---->TaskD
// TaskA---->TaskC---->TaskD


#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <chrono>
#include <taskflow/taskflow.hpp>  // the only include you need 

// https://software.intel.com/en-us/forums/intel-moderncode-for-parallel-architectures/topic/297118
#define compiler_fence() __asm__ __volatile__ ("" : : : "memory")

// load with 'consume' (data-dependent) memory ordering 
template<typename T> 
T load_consume(T const* addr) { 
	// hardware fence is implicit on x86 
	T v = *const_cast<T const volatile*>(addr); 
  compiler_fence();
	//__memory_barrier(); // compiler fence 
	return v; 
} 

// store with 'release' memory ordering 
template<typename T> 
void store_release(T* addr, T v) { 
	// hardware fence is implicit on x86 
	//__memory_barrier(); // compiler fence 
  compiler_fence();
	*const_cast<T volatile*>(addr) = v; 
} 

// cache line size on modern x86 processors (in bytes) 
size_t const cache_line_size = 64; 
// single-producer/single-consumer queue 
template<typename T> 
class spsc_queue { 
	public: 
		spsc_queue() { 
			node* n = new node; 
			n->next_ = 0; 
			tail_ = head_ = first_= tail_copy_ = n; 
		} 

		~spsc_queue() {  
			node* n = first_; 
			do 
			{ 
				node* next = n->next_; 
				delete n; 
				n = next; 
			} while (n); 
		} 

		void enqueue(T v) { 
			node* n = alloc_node(); 
			n->next_ = 0; 
			n->value_ = v; 
			store_release(&head_->next_, n); 
			head_ = n; 
		} 

		// returns 'false' if queue is empty 
		bool dequeue(T& v) { 
			if (load_consume(&tail_->next_)) { 
				v = tail_->next_->value_; 
				store_release(&tail_, tail_->next_); 
				return true; 
			} 
			else { 
				return false; 
			} 
		} 

	private: 
		// internal node structure 
		struct node { 
			node* next_; 
			T value_; 
		}; 

		// consumer part 
		// accessed mainly by consumer, infrequently be producer 
		node* tail_; // tail of the queue 

		// delimiter between consumer part and producer part, 
		// so that they situated on different cache lines 
		char cache_line_pad_ [cache_line_size]; 

		// producer part 
		// accessed only by producer 
		node* head_; // head of the queue 
		node* first_; // last unused node (tail of node cache) 
		node* tail_copy_; // helper (points somewhere between first_ and tail_) 

		node* alloc_node() { 
			// first tries to allocate node from internal node cache, 
			// if attempt fails, allocates node via ::operator new() 

			if (first_ != tail_copy_) { 
				node* n = first_; 
				first_ = first_->next_; 
				return n; 
			} 
			tail_copy_ = load_consume(&tail_); 
			if (first_ != tail_copy_) { 
				node* n = first_; 
				first_ = first_->next_; 
				return n; 
			} 
			node* n = new node; 
			return n; 
		} 

		spsc_queue(spsc_queue const&); 
		spsc_queue& operator = (spsc_queue const&); 
}; 


class TextSlice {
    //! Pointer to one past last character in sequence
    char* logical_end;
    //! Pointer to one past last available byte in sequence.
    char* physical_end;
  public:
    //! Allocate a TextSlice object that can hold up to max_size characters.
    static TextSlice* allocate( size_t max_size ) {
      // +1 leaves room for a terminating null character.
      TextSlice* t = static_cast<TextSlice*>(std::malloc(sizeof(TextSlice)+max_size+1));
      t->logical_end = t->begin();
      t->physical_end = t->begin()+max_size;
      return t;
    }
    //! Free a TextSlice object 
    void free() {
      std::free((char*)this);
        //tbb::tbb_allocator<char>().deallocate((char*)this,sizeof(TextSlice)+(physical_end-begin())+1);
    } 
    //! Pointer to beginning of sequence
    char* begin() {return (char*)(this+1);}
    //! Pointer to one past last character in sequence
    char* end() {return logical_end;}
    void end(char c) { *logical_end = c; }
    //! Length of sequence
    size_t size() const {return logical_end-(char*)(this+1);}
    //! Maximum number of characters that can be appended to sequence
    size_t avail() const {return physical_end-logical_end;}
    //! Append sequence [first,last) to this sequence.
    void append( char* first, char* last ) {
      memcpy( logical_end, first, last-first );
      logical_end += last-first;
    }
    //! Set end() to given value.
    void set_end( char* p ) {logical_end=p;}
};


constexpr size_t MAX_CHAR_PER_INPUT_SLICE = 4000;
const std::string InputFileName = "input.txt";
const std::string OutputFileName = "output.txt";

int main(){

  auto t1 = std::chrono::high_resolution_clock::now(); 

  tf::Taskflow tf(4);
  tf::Framework f;

  spsc_queue<TextSlice*> input_queue;
  spsc_queue<TextSlice*> output_queue;

  FILE* input_file = fopen( InputFileName.c_str(), "r" );
  FILE* output_file = fopen( OutputFileName.c_str(), "w" );
  
  auto [transform, output] = f.emplace( 
    [&] () { 
      TextSlice *input;
      assert(input_queue.dequeue(input));

      input->end('\0');
      char* p = input->begin();
      TextSlice& out = *TextSlice::allocate( 2*MAX_CHAR_PER_INPUT_SLICE );
      char* q = out.begin();
      for(;;) {
        while( p<(*input).end() && !isdigit(*p) ) { 
          *q++ = *p++; 
        }
        if( p==(*input).end() ) {
          break;
        }
        long x = strtol( p, &p, 10 );
        // Note: no overflow checking is needed here, as we have twice the 
        // input string length, but the square of a non-negative integer n 
        // cannot have more than twice as many digits as n.
        long y = x*x; 
        sprintf(q,"%ld",y);
        q = strchr(q,0);
      }
      out.set_end(q);
      (*input).free();

      output_queue.enqueue(&out);
    },     
    [&] () { 
      TextSlice* out;
      assert(output_queue.dequeue(out));

      size_t n = fwrite( out->begin(), 1, out->size(), output_file );
      if( n!=out->size() ) {
          fprintf(stderr,"Can't write into file '%s'\n", OutputFileName.c_str());
          exit(1);
      }
      out->free();
    }
  ); 

  //transform.name("transform");
  //output.name("output");
  transform.name("2");
  output.name("3");

   
  // Linear   
  transform.precede(output);  

  TextSlice* next_slice = TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE );


  tf.pipeline_until(f, [&]() mutable { 
      // Read characters into space that is available in the next slice.
      size_t m = next_slice->avail();
      size_t n = fread( next_slice->end(), 1, m, input_file );
      if( !n && next_slice->size()==0 ) {
        // No more characters to process 
        std::puts("All Done");
        return true;
      } 
      else {
        // Have more characters to process.
        TextSlice& t = *next_slice;
        next_slice = TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE );
        char* p = t.end()+n;
        if( n==m ) {
          // Might have read partial number.  If so, transfer characters of partial number to next slice.
          while( p>t.begin() && isdigit(p[-1]) ) {
            --p;
          }
          next_slice->append( p, t.end()+n );
        }
        t.set_end(p);
        input_queue.enqueue(&t);
        return false;
        //return &t;
      }
    }, 
    [](){}
  ).get();

  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()/1000000.0 << std::endl;

  return 0;
}


