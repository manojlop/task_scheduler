#include "scheduler.h"

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  return TaskID();
}

TaskID Scheduler::addTask(Task &&task){
  return TaskID();
}

void Scheduler::start(){

}

void Scheduler::stop(){
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
    
    // Executing the task

    // Check if finished correctly (TBD : how to do this?)
    lck.lock();
    if(1)
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
