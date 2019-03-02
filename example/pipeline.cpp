// A simple example to capture the following task dependencies.
//
// TaskA---->TaskB---->TaskD
// TaskA---->TaskC---->TaskD

#include <taskflow/taskflow.hpp>  // the only include you need

int main(){

  tf::Taskflow tf;
  tf::Framework f;

  std::mutex mtx;
  
  auto [A, B, C] = f.emplace( 
    [&] () { 
      std::scoped_lock lock(mtx);
      std::puts("TaskA");
    },     
    [&] (auto &subflow) { 
      std::scoped_lock lock(mtx);
      std::puts("TaskB");
      subflow.emplace([&](){
       std::scoped_lock lock(mtx);
       std::puts("TaskB1");
      });
      subflow.emplace([&](){
       std::scoped_lock lock(mtx);
       std::puts("TaskB2");
      });
      subflow.detach();
    },     
    [&] () { 
      std::scoped_lock lock(mtx);
      std::puts("TaskC");
    }
  ); 

  A.name("A");
  B.name("B");
  C.name("C");
   
  // Linear   
  A.precede(B);  
  B.precede(C);  


  tf.pipeline_until(f, [iter = 3]() mutable { 
    std::cout << "iter = " << iter << std::endl;
    return iter -- == 0; }, [](){}
  ).get();

  //std::cout << f.dump() << std::endl;
  //tf.wait_for_all();  // block until finished

  return 0;
}



