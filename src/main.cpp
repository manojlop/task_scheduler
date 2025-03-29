#include <iostream>

#include "task.h"
#include "scheduler.h"

#include <iostream>

void print_f(){
  cout << "123" << endl;
}

int main() {

  Task task1(print_f);
  Task task2(task1);
  Scheduler* scheduler = Scheduler::getScheduler();

  task1.run();
  task2.run();

  return 0;
}
