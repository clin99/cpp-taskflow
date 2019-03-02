// A simple example to capture the following task dependencies.
//
// TaskA---->TaskB---->TaskD
// TaskA---->TaskC---->TaskD

#include <taskflow/taskflow.hpp>  // the only include you need

int main(){

  tf::Taskflow tf;
  tf::Framework f;

  constexpr size_t pipeline_length {1000000};
  std::vector<tf::Task> tasks;
  int counter {1};
  
  for(size_t i=0; i<pipeline_length; i++) {
    tasks.emplace_back(
      f.emplace( [&, i=i+1]() {
          assert(counter % pipeline_length == i % pipeline_length);
          counter ++;
        }
      )
    );
  }

  for(size_t i=0; i<pipeline_length-1; i++) {
    tasks[i].precede(tasks[i+1]);
  }

  auto fu1 = tf.pipeline_until(f, [iter = 1]() mutable { 
    return iter -- == 0; }, [&](){ assert(counter == pipeline_length+1); counter = 1; }
  );

  auto fu2 = tf.pipeline_until(f, [iter = 1]() mutable { 
    return iter -- == 0; }, [&](){ assert(counter == pipeline_length+1); counter = 1; }
  );

  //fu1.get();
  fu2.get();

  //std::cout << f.dump() << std::endl;
  //tf.wait_for_all();  // block until finished

  return 0;
}



