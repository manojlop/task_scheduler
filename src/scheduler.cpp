#include "scheduler.h"

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  std::lock_guard<std::mutex> lck(mtx_);
  // Todo : validate dependencies

  // Return current value and increment -> that is the one that is assigned
  // atomic variable cannot be in make_pair
  TaskID currentId = id_.fetch_add(1);
  // Todo : std::make_shared() in C++14
  std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
  // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
  tasks_.emplace(currentId, std::move(ptr));
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(currentId);
  }
  // Todo : Check if said task doesnt't depend on anyone -> put to ready immediately if doesn't
  if(dependencies.empty()){
    // ptr->state_ = t_TaskState::READY; // We moved from ptr -> Segmentation fault!
    tasks_[currentId]->state_ = t_TaskState::READY;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
     if (tasks_[currentId]->state_  == t_TaskState::READY) { // Check Task's default state logic
        tasks_[currentId]->state_ = t_TaskState::PENDING;
     }
     // TODO: Add check if all dependencies already exist and are COMPLETED?
     // (More advanced: handle adding task where deps are already done)
  }
  return currentId;
}

// Todo : revise if we want this -> potential problems with ids if we take outside ids. -> with moving const variables and creating new id (outside dependencies??)
TaskID Scheduler::addTask(Task &&task){
  std::lock_guard<std::mutex> lck(mtx_);
  // Todo : validate dependencies

  // Return current value and increment -> that is the one that is assigned
  // atomic variable cannot be in make_pair
  TaskID currentId = id_.fetch_add(1);
  std::shared_ptr<Task> ptr(new Task(std::move(task)));
  // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
  tasks_.emplace(currentId, std::move(ptr));
  for(auto dep : task.dependencies_){
    // We map to the assigned ID, not the incremented one
    downwardDependencies_[dep].push_back(currentId);
  }
  // Todo : Check if said task doesnt't depend on anyone -> put to ready immediately if doesn't
  if(task.dependencies_.empty()){
    // ptr->state_ = t_TaskState::READY; // We moved from ptr -> segmentation fault!
    tasks_[currentId]->state_ = t_TaskState::READY;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
     if (tasks_[currentId]->state_  == t_TaskState::READY) { // Check Task's default state logic
        tasks_[currentId]->state_ = t_TaskState::PENDING;
     }
     // TODO: Add check if all dependencies already exist and are COMPLETED?
     // (More advanced: handle adding task where deps are already done)
  }
  return currentId;
}


void Scheduler::notifyDependents(TaskID taskId){
  // Needs locking, as it accesses shared resources and alters them
  std::lock_guard<std::mutex> lck(mtx_);

  // Handle if wrong task id passed
  auto completed_task_it = tasks_.find(taskId);
  if (completed_task_it == tasks_.end()) { 
    /* Log error */ 
    return; 
  }
  auto completed_task_ptr = tasks_[taskId];

  if(completed_task_ptr->state_ == t_TaskState::FAILED){
    // Propagate failure
    // don't decrement unmet count, iterate and mark failed
    // First need to check if there is entry to map
    if(downwardDependencies_.count(taskId)){
      for(auto dependent_id : downwardDependencies_.at(taskId)){
        auto dep_it = tasks_.find(dependent_id);
        if(dep_it != tasks_.end()){
          dep_it->second->state_ = t_TaskState::FAILED;
          // Todo : may induce stack overflow -> use outside queue/stack and no recursion
          notifyDependents(dependent_id);
        }
      }
    }
    return;
  }

  if (completed_task_ptr->getState() != t_TaskState::COMPLETED) {
     // Task wasn't completed? Log warning? Or maybe called too early?
     return;
  }

  // Process only if COMPLETED
  // Need to check if in map
  if(downwardDependencies_.count(taskId)){
    for(auto dependent_id : downwardDependencies_.at(taskId)){
      auto dep_it = tasks_.find(dependent_id);
      if(dep_it == tasks_.end()){
        // Log error
        continue;
      }
      auto dep_ptr = tasks_[dependent_id];

      // Proceed only if dependent is still pending
      if(dep_ptr->state_ == t_TaskState::PENDING){
        // Returns true if zero
        if(dep_ptr->decrement_unmet_dependencies()){
          dep_ptr->state_ = t_TaskState::READY;
          readyTasks_.push_back(dependent_id);
          workAvailable_.notify_one();
        }
      }
      // TBD : Is it error if its not pending?
    }
  }
}


void Scheduler::start(){
  worker_threads_.reserve(workers_.size()); // Reserve space
  for(size_t i = 0; i < workers_.size(); ++i){
    // Launch thread and store it
    // Pass pointer to the Worker object managed by unique_ptr
    worker_threads_.emplace_back(&Scheduler::Worker::run, workers_[i].get());
  }
}

void Scheduler::stop(){
  this->stop_requested_.store(true);
  std::lock_guard<std::mutex> lck(mtx_);
  for(auto taskId : readyTasks_) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        // Log critical error: Task ID from queue not found!
        continue;
    }
    // TBD : set to cancelled?
    tasks_[taskId]->state_ = t_TaskState::FAILED;
  }
  this->readyTasks_.clear();
  this->workAvailable_.notify_all();
  // We must join all started threads
  for(auto& thread : worker_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}


std::shared_ptr<Task> Scheduler::createTask(std::function<void()> func, const std::vector<TaskID> &dependencies){
  return std::shared_ptr<Task>();
}


void Scheduler::Worker::run(){
  while(true){
    int currTaskID = -1;
    std::unique_lock<std::mutex> lck(scheduler_.mtx_);
    // We need a predicate here on which we wait -> if not, what if we would have notify but empty array as someone took the thread
    scheduler_.workAvailable_.wait(lck, [this] {
      return !scheduler_.readyTasks_.empty() || scheduler_.stop_requested_.load();
    });

    if(scheduler_.stop_requested_.load()){
      // Todo : implement end of life thread -> One thread to run to "cleanup" everything
      lck.unlock();
      break;
    }
    

    // Once the lock is aquired, we get the task id and pop it from ready tasks
    // TaskID taskID = scheduler_.readyTasks_.front();
    currTaskID = scheduler_.readyTasks_.front();
    scheduler_.readyTasks_.pop_front();
    std::shared_ptr<Task> task_ptr = nullptr;

    auto it = scheduler_.tasks_.find(currTaskID);
    if (it == scheduler_.tasks_.end()) {
        // Log critical error: Task ID from queue not found!
        lck.unlock(); // Release lock before continuing
        continue;
    }
    task_ptr = it->second; // Get the shared_ptr

    

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
    }

    lck.unlock();

    // Todo : do logging via logger
    
    bool succesfullTask = true;
    // Executing the task
    if(task_ptr){
      try{
        std::cout << "Worker: " << this->id_ << " is processing task: " << currTaskID << "\n";
        task_ptr->run();
      } catch (...){
        // Todo handle error and log
        succesfullTask = false;
      }
    }

    // Check if finished correctly (TBD : how to do this?)
    lck.lock();
    if(succesfullTask) {
      task_ptr->setState(t_TaskState::COMPLETED);
    } else { 
      // TBD : How to handle failed tasks in scheduler regarding the dependencies
      task_ptr->setState(t_TaskState::FAILED);
    }

    scheduler_.notifyDependents(currTaskID);

    lck.unlock();
    task_ptr = nullptr;
  } 
}
