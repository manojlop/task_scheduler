#ifndef SCHEDULER_TASK_H_
#define SCHEDULER_TASK_H_

#include "types_defines.h"

#include <vector>
#include <functional>
#include <atomic>

class Scheduler;

class Task{
  friend class Scheduler;

private:
  /*
  Removed for two reasons:
    1. Task itselft shouldn't be tasked with naming its own id
    2. Using static, non-atomic variable calculations is not thread-safe
  */
  // static TaskID ids; 
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

  // Only the Scheduler should be able to run the task
  void run() const{
    // Todo : check the state of function
    if(work)
      work();
    // else 
      // Todo : handle error when no work function
  }

public:

  // Constructors
  // Default constructor -> We don't need it as it doesn't specify task
  Task() = delete;

  Task(int identifier, std::function<void()> func, const std::vector<TaskID> dep = std::vector<TaskID>()) : id(identifier), work(func), dependencies(dep), unmetCount(dep.size()) {
    state = dep.size() == 0 ? READY : PENDING;
  }

  // Copy constructor -> Tasks shouldn't be copyiable
  Task(const Task&) = delete;
  // Task(const Task& task) : Task(task.id, task.work, task.dependencies) {} // -> Creates new task with same id -> WRONG

  // Move constructor -> Needed, for efficiently transferring ownership of resources.
  // std::function and std::vector are movable.
  // Moving a task (when emplacing it into Scheduler's map) should be more efficient than copying(if it were allowed)
  Task(Task&& task) = default; // Works here because members std::function, std::vector, std::atomic are all MOVABLE themselves, and const members don't prevent moves

  // Desctructor -> TBD: No pointers -> not needed
  // ~Task() {}

  // Functions
  TaskID getID() const {
    return id;
  }

  /*
  Accessing state might need external synchronization (locking in the Scheduler) depending on when/where it's called, as it's mutable and potentially accessed by multiple threads
  */
  t_TaskState getState() const {
    return state;
  }

  /*
  Returning the value of an atomic requires specifying a memory order. std::memory_order_relaxed is often sufficient for just reading the value if you don't need synchronization guarantees with other variables based on this read
  */
  TaskID getUnmetCount() const {
    return unmetCount.load(std::memory_order_relaxed);
  }

  // Operators
  Task& operator=(const Task&) = delete;
  Task& operator=(Task&& rhs) noexcept = default; // Works here because members std::function, std::vector, std::atomic are all MOVABLE themselves, and const members don't prevent moves

  // Move assignment operator
};

#endif