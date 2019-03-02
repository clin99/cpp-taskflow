// A simple example to capture the following task dependencies.
//
// TaskA---->TaskB---->TaskD
// TaskA---->TaskC---->TaskD

#include <taskflow/taskflow.hpp>  // the only include you need

int main(){

  tf::Taskflow tf;

  tf::Framework f1;
  f1.name("F1")
    .emplace(
    [&](){ std::puts("TaskA"); },
    [&](){ std::puts("TaskB"); },
    [&](){ std::puts("TaskC"); }
  );

  tf::Framework f2;
  f2.name("F2")
    .emplace(
    [&](){ std::puts("  TaskD"); },
    [&](auto &subflow){ 
      std::puts("  TaskE"); 
      auto [E1, E2] = subflow.emplace(
        [](){ std::puts("    Task E1"); },
        [](){ std::puts("    Task E2"); }
      );
    }
  );


  tf::WorkGroup wg;
  auto F1 = wg.emplace(f1);
  auto F2 = wg.emplace(f2);
  F1.precede(F2);
  wg.emplace([](){ std::cout << "Glue task\n"; });

  tf.run_until(wg, [iter = 4] () mutable { std::puts("\n"); return iter-- == 0; }, [](){}).get();

  return 0;
}



