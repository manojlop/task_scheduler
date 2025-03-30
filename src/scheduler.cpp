#include "scheduler.h"

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  // Todo : check id availability

  std::lock_guard<std::mutex> lck(mtx_);
  // Return current value and increment
  // atomic variable cannot be in make_pair
  TaskID currentId = id_.fetch_add(1);
  std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
  // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
  tasks_.emplace(std::make_pair(currentId, ptr));
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(id_);
  }
  // Todo : Check if said task is completed -> put to ready immediately if is
  return currentId;
}

TaskID Scheduler::addTask(Task &&task){
  // Todo : check id availability

  std::lock_guard<std::mutex> lck(mtx_);
  // Return current value and increment
  // atomic variable cannot be in make_pair
  TaskID currentId = id_.fetch_add(1);
  std::shared_ptr<Task> ptr = std::make_shared<Task>(std::move(task));
  // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
  // Return current value and increment
  tasks_.emplace(std::make_pair(currentId, ptr));
  for(auto dep : task.dependencies_){
    downwardDependencies_[dep].push_back(id_);
  }
  // Todo : Check if said task is completed -> put to ready immediately if is
  return currentId;
}

void Scheduler::start(){

}

void Scheduler::stop(){
}


std::shared_ptr<Task> Scheduler::createTask(std::function<void()> func, const std::vector<TaskID> &dependencies){
  return std::shared_ptr<Task>();
}

void Scheduler::Worker::fetch_and_execute(){
  while(true){
    std::unique_lock<std::mutex> lck(scheduler_.mtx_);
    scheduler_.workAvailable_.wait(lck);

    // Once the lock is aquired, we get the task id and pop it from ready tasks
    // TaskID taskID = scheduler_.readyTasks_.front();
    this->currTaskID = scheduler_.readyTasks_.front();
    this->has_task = true;
    scheduler_.readyTasks_.pop_front();
    std::shared_ptr<Task> task_ptr = nullptr;
    try {
      task_ptr = scheduler_.tasks_.at(currTaskID);
    } catch (const std::out_of_range& oor){
      // Todo : log error, handle
      continue;
    }

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
    }

    lck.unlock();

    // Todo : do logging via logger
    std::cout << "Worker: " << this->id_ << " is processing task: " << currTaskID << "\n";
    
    bool succesfullTask = true;
    // Executing the task
    if(task_ptr){
      try{
        task_ptr->run();
      } catch (...){
        succesfullTask = false;
      }
    }

    // Check if finished correctly (TBD : how to do this?)
    lck.lock();
    if(succesfullTask)
      task_ptr->setState(t_TaskState::COMPLETED);
    else 
      task_ptr->setState(t_TaskState::FAILED);
    
    this->has_task = false;
    this->currTaskID = -1;
    task_ptr = nullptr;
    lck.unlock();
  } 
}

void Scheduler::Worker::wait_stop(){
}

void Scheduler::Worker::run()
{
  std::thread work(&Scheduler::Worker::fetch_and_execute, this);
  std::thread stop(&Scheduler::Worker::wait_stop, this);

  stop.join();

  work.join();
}

