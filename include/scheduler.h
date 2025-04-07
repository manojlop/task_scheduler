#ifndef SCHEDULER_SCHEDULER_H_
#define SCHEDULER_SCHEDULER_H_

#include "types_defines.h"
#include "task.h"

#include <mutex>
#include <unordered_map>
#include <deque>
#include <vector>
#include <memory>
#include <condition_variable>
#include <thread>
#include <iostream>
#include <algorithm>
#include <tests.h>
#include <stack>

class Scheduler{

  // We need to make specific tests be friends of Schedulers, so they can access its internals
  friend class SchedulerTest;
  friend class SchedulerTest_AddTask_Test; // Naming convention: FixtureName_TestName_Test
  friend class SchedulerTest_Notify_Test;
  friend class SchedulerTest_CycleCheck_Test;
  friend class SchedulerTest_Integration_Test;

  enum t_SchedulerState {CREATED, STARTED, STOPPED};
  enum t_StopWay {IMMEDIATE, WAIT_ALL_TO_END};

  // Implementation of class Worker
  class Worker{
  private:
    Scheduler& scheduler_;

    const int id_;

  public:
    // Constructors
    Worker(Scheduler& sch, int workerId) : scheduler_(sch), id_(workerId) {}
    Worker(const Worker& wrk) = delete;
    Worker(Worker&& wrk) = delete;

    // Destructor
    ~Worker() = default;

    // Functions
    void run();


    // Operators
    Worker& operator=(const Worker& rhs) = delete;
    Worker& operator=(Worker&& rhs) = delete;
  };

private:
  static const char* taskStateName[];

  // State of the scheduler -> if its CREATED it can be started, or can be stopped without any work
  // If its STOPPED, it can be started without creating new threads
  t_SchedulerState state_;

  // Used to prevent locking the mutex for simple operations
  std::atomic<TaskID> id_;

  std::atomic<int> workerId_;

  std::mutex mtx_;

  const int threadNumber_;

  // We store pointers to created tasks 
  // We don't need to copy it to readyTasks, as we can just access it via key(id) in the task collection
  std::unordered_map<TaskID, std::shared_ptr<Task>> tasks_;

  std::deque<TaskID> readyTasks_;

  // Using unique ptr to handle pointers to specific workers, as they are memory safe
  std::vector<std::unique_ptr<Worker>> workers_;

  // Manages OS thread resource (holds actual std::thread objects)
  // Needed to start the threads
  // join() the threads during shutdown
  std::vector<std::thread> workerThreads_;

  /*
  Why having both workers_ and worker_threads_?
  - We separate the what(worker -> execution) from how (thread -> OS context)
  */

  // Stores IDs of downward dependent tasks fo easier handling when task finishes work
  std::unordered_map<TaskID, std::vector<TaskID>> downwardDependencies_;

  // To remove busy-wait or sleeping when readyTasks is empty
  std::condition_variable workAvailable_;

  // To inform workers that stop has been requested -> immediate stoppage of execution
  // std::condition_variable stop_requested_; // We don't need condition variable, as we don't have construct to wait -> kill
  std::atomic<bool> stopRequested_;

  // Notify all dependents and put into readyTasks if all dependencies match
  void notifyDependents(TaskID taskId);

  // Checks weather the current addition creates a cycle (Currently it never should, because we can depend only on tasks already in the scheduler)
  // Todo | TBD: Relax for already completed tasks?
  bool check_cycles(TaskID dependand, TaskID depends_on, std::vector<TaskID>& cycle);

  /*
  Counters used to track how many tasks are finished
  */
  std::atomic<int> addedTasks_;
  std::atomic<int> completedTasks_;
  std::atomic<int> failedTasks_;
  std::atomic<int> cancelledTasks_;

  std::condition_variable allTasksFinished_;

public:

  // Constructors -> Private default constructor
  Scheduler(int n = 2) :  state_(CREATED), id_(1), workerId_(0), threadNumber_(n), stopRequested_(false), addedTasks_(0), completedTasks_(0), failedTasks_(0), cancelledTasks_(0) {
    // Initialize workers after the constructor's initialization list
    workers_.reserve(n);  // Reserve space for efficiency
    for (int i = 0; i < n; i++) {
      // Todo : std::make_unique() in C++14
      workers_.push_back(std::unique_ptr<Worker>(new Worker(*this, workerId_.fetch_add(1))));
    }
  }

  // Deletion of copy constructor
  Scheduler(const Scheduler& sch) = delete;

  // Move constructor
  Scheduler(Scheduler&& sch) = delete;

  // Destructor
  ~Scheduler() { 
    safe_print(("******************************************************************************"), "Scheduler", INFO);
    safe_print(("Initiated Scheduler's destructor"), "Scheduler", INFO);
    // Stop all tasks
    stop();
    // Do i need to delete task pointers here? -> smart_ptr and unique_ptr handle this for us
    safe_print(("Scheduler stopped"), "Scheduler", INFO);
    safe_print(("******************************************************************************"), "Scheduler", INFO);
  }

  // Functions

  // Adding tasks -> via function, returns TaskID
  // Cannot create dependencies with IDs that are not present yet
  /*
  Error codes are:
  * -1 : Unable to create unique pointer
  * -2 : Value from dependencies not present in the scheduler
  * -3 : Dependency causes cycle to be made
  * -10 : State change error
  */
  TaskID addTask(std::function<void()> func, const std::vector<TaskID>& dependencies = {});

  // Moves task to scheduler
  // TaskID addTask(Task&& task);

  // Variadic template to be able to pass single task, or multiple tasks in a vector or some other structure
  // Handled with recursion
  // Bool is to be able to check cycles in a graph on addition
  // template<typename T, typename... args>
  // std::vector<TaskID> addTask(T task, args... tasks);

  // Print out tasks and their states
  void printTaskCollection(t_Verbosity verb = DEBUG);

  // Creates worker threads and starts the scheduler -> TBD : should it start if no assigned tasks?
  void start();

  // Called before Scheduler ends its life, makes sure that all threads are properly stopped. TBD : When does scheduler end?
  void stop(t_StopWay wayToStop = t_StopWay::IMMEDIATE);

  // std::shared_ptr<Task> createTask(std::function<void()> func, const std::vector<TaskID>& dependencies = {});

  /*
  Function that waits for all added tasks to end in sceduler, without stopping the sceduler itself
  */
  bool waitTasksToEnd();


  // Operators
  Scheduler& operator=(const Scheduler&) = delete;
  Scheduler& operator=(Scheduler&& rhs) = delete; 

};

#endif

// template <typename T, typename... args>
// inline std::vector<TaskID> Scheduler::addTask(T task, args... tasks){
//   return std::vector<TaskID>();
// }
