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

class Scheduler{
  // Implementation of class Worker
  class Worker{
  private:
    Scheduler& scheduler_;

    const int id_;


    bool has_task = false;
    int currTaskID = -1;

    std::mutex mtx;

  public:
    // Constructors
    Worker(Scheduler& sch, int workerId) : scheduler_(sch), id_(workerId) {}
    Worker(const Worker& wrk) = delete;
    Worker(Worker&& wrk) = delete;

    // Destructor
    ~Worker() = default;

    // Functions
    void fetch_and_execute();
    void wait_stop();

    void run();


    // Operators
    Worker& operator=(const Worker& rhs) = delete;
    Worker& operator=(Worker&& rhs) = delete;
  };
private:

  // Used to prevent locking the mutex for simple operations
  std::atomic<TaskID> id_;

  std::atomic<int> workerId;

  std::mutex mtx_;

  const int threadNumber_;

  // We store pointers to created tasks 
  // We don't need to copy it to readyTasks, as we can just access it via key(id) in the task collection
  std::unordered_map<TaskID, std::shared_ptr<Task>> tasks_;

  std::deque<TaskID> readyTasks_;

  // Using unique ptr to handle pointers to specific workers, as they are memory safe
  std::vector<std::unique_ptr<Worker>> workers_;

  // Stores IDs of downward dependent tasks fo easier handling when task finishes work
  std::unordered_map<TaskID, std::vector<TaskID>> downwardDependencies_;

  // To remove busy-wait or sleeping when readyTasks is empty
  std::condition_variable workAvailable_;

  // To inform workers that stop has been requested -> immediate stoppage of execution
  std::condition_variable stop_requested_;

public:


  // Constructors -> Private default constructor
  Scheduler(int n = 2) : id_(1), threadNumber_(n){
    // Initialize workers after the constructor's initialization list
    workers_.reserve(n);  // Reserve space for efficiency
    for (int i = 0; i < n; i++) {
        workers_.push_back(std::unique_ptr<Worker>(new Worker(*this, workerId++)));
    }
  }

  // Deletion of copy constructor
  Scheduler(const Scheduler& sch) = delete;

  // Move constructor
  Scheduler(Scheduler&& sch) = delete;

  // Destructor
  ~Scheduler() { 
    // Stop all tasks
    stop();
  }

  // Functions

  // Adding tasks -> via function, returns TaskID
  // Cannot create dependencies with IDs that are not present yet
  TaskID addTask(std::function<void()> func, const std::vector<TaskID>& dependencies = {});

  // Moves task to scheduler
  TaskID addTask(Task&& task);

  // Variadic template to be able to pass single task, or multiple tasks in a vector or some other structure
  // Handled with recursion
  // Bool is to be able to check cycles in a graph on addition
  template<typename T, typename... args>
  std::vector<TaskID> addTask(T task, args... tasks);

  // Creates worker threads and starts the scheduler -> TBD : should it start if no assigned tasks?
  void start();

  // Called before Scheduler ends its life, makes sure that all threads are properly stopped. TBD : When does scheduler end?
  void stop();

  std::shared_ptr<Task> createTask(std::function<void()> func, const std::vector<TaskID>& dependencies = {});

  // Operators
  Scheduler& operator=(const Scheduler&) = delete;
  Scheduler& operator=(Scheduler&& rhs) = delete; 

};

#endif

template <typename T, typename... args>
inline std::vector<TaskID> Scheduler::addTask(T task, args... tasks){
  return std::vector<TaskID>();
}
