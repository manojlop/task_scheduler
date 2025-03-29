#ifndef __TASK__
#define __TASK__

#include "types_defines.h"

#include <vector>
#include <functional>
#include <atomic>

class Task{
private:
  static TaskID ids;
  const TaskID id;
  t_TaskState state;
  /* Explanation:
  Class template from functional - General purpose, polymorphic function wrapper
  Can store, copy and invoke any callable target -> anything invokable using () operator (regular function, lambda expression, function objects, pointers to member functions, function pointers)
  Has its own type, regardless of the specific type of callable target it currently holds
  */
  // ---------------------
  // Returns: 
  // - void
  // ---------------------
  // Parameters: 
  // - none
  const std::function<void()> work;

  const std::vector<TaskID> dependencies;
  // vector<TaskID> dependents; // Moved to scheduler, as the task doesn't need to know who depends on it

  // Design tradeoffs unmet_count:
  /*
  1. unmetCount approach:
    - Fast checking if a task is ready to run, efficient update
    - Slow checking which dependecies remain
  2. remainingDependencies approach:
    - Efficient check for which dependencies remain -> direct iteration of decreasing array. Check if tasks is ready with .empty()
    - When task A finishes, we need to notify dependent -> finding and removing A's ID (set/vector)
    - Modifying list requires synchronization
    - Potentially more dynamic memory allocation/deallocation
  Decision:
    For now, we are sticking with unment count approach.
  Question:
    Does the task need to know who else it depends on often?
  */
  // Atomic:
  std::atomic<TaskID> unmetCount;

public:

  // Constructors
  // Default constructor
  Task() : id(ids++), state(READY), unmetCount(0) {}

  Task(std::function<void()> func, const std::vector<TaskID> dep = std::vector<TaskID>()) : id(ids++), work(func), dependencies(dep), unmetCount(dep.size()) {
    state = dep.size() == 0 ? READY : PENDING;
  }

  // Copy constructor 
  Task(const Task& task) : Task(task.work, task.dependencies) {}

  // Move constructor -> TBD: No pointers -> not needed

  // Desctructor -> TBD: No pointers -> not needed
  // ~Task() {}

  // Functions
  TaskID getID() {
    return id;
  }

  t_TaskState getState(){
    return state;
  }

  int getUnmetCount(){
    return unmetCount;
  }

  void run(){
    work();
  }
  
  // Operators
};

#endif