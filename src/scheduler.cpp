#include "scheduler.h"

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  std::cout << "Adding new task\n";
  std::lock_guard<std::mutex> lck(mtx_);
  std::cout << "Adding new task, obtained mutex\n";
  // Todo : validate dependencies

  // Return current value and increment -> that is the one that is assigned
  // atomic variable cannot be in make_pair
  TaskID currentId = id_.fetch_add(1);
  std::cout << "New task id: " << currentId << "\n";
  // Todo : std::make_shared() in C++14
  std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
  // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
  tasks_.emplace(currentId, std::move(ptr));
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(currentId);
    if(tasks_[dep]->state_ == t_TaskState::COMPLETED) {
      std::cout << "Task: " << currentId << "  depends on task: " << dep << " which is already COMPLETED, so counter is decremented\n";
      tasks_[currentId]->decrement_unmet_dependencies();
    }
    else if (tasks_[dep]->state_ == t_TaskState::FAILED) {
      std::cout << "Task: " << currentId << "  depends on task: " << dep << " which has FAILED, so its also FAILS\n";
      tasks_[currentId]->state_ = t_TaskState::FAILED;
    }
  }
  // Todo : Check if said task doesnt't depend on anyone -> put to ready immediately if doesn't
  if(tasks_[currentId]->unmetCount_.load() == 0){
    // ptr->state_ = t_TaskState::READY; // We moved from ptr -> Segmentation fault!
    std::cout << "Task with id: " << currentId << " doesn't have any more unment dependecies, so it is READY\n";
    tasks_[currentId]->state_ = t_TaskState::READY;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
      if (tasks_[currentId]->state_  == t_TaskState::READY) { // Check Task's default state logic
        tasks_[currentId]->state_ = t_TaskState::PENDING;
      }
      std::cout << "Task with id: " << currentId << " is PENDING\n";
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
    if(tasks_[dep]->state_ == t_TaskState::COMPLETED)
      tasks_[currentId]->decrement_unmet_dependencies();
    else if (tasks_[dep]->state_ == t_TaskState::FAILED)
      tasks_[currentId]->state_ = t_TaskState::FAILED;
  }
  // Todo : Check if said task doesnt't depend on anyone -> put to ready immediately if doesn't
  if(tasks_[currentId]->unmetCount_.load() == 0){
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
  std::cout << "Task: " << taskId << " is: " << tasks_[taskId]->state_ << " and it's notifying dependents, lock aquired\n";

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
  workerThreads_.reserve(workers_.size()); // Reserve space
  for(size_t i = 0; i < workers_.size(); ++i){
    // Launch thread and store it
    // Pass pointer to the Worker object managed by unique_ptr
    workerThreads_.emplace_back(&Scheduler::Worker::run, workers_[i].get());
  }
}

void Scheduler::stop(){
  // std::lock_guard<std::mutex> lck(mtx_); // We cannot use lock guard, because we need to unlock it in order for threads to finish
  std::unique_lock<std::mutex> lck(mtx_);
  std::cout << "Lock aquired to stop the scheduler\n";
  this->stopRequested_.store(true);
  for(auto taskId : readyTasks_) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        // Log critical error: Task ID from queue not found!
        continue;
    }
    // TBD : set to cancelled?
    std::cout << "Scheduler is being stopped, so task: " << taskId << " which was READY is being CANCELLED\n";
    tasks_[taskId]->state_ = t_TaskState::FAILED;
  }
  this->readyTasks_.clear();
  this->workAvailable_.notify_all();
  lck.unlock();
  std::cout << "Lock released to stop the scheduler\n";
  // We must join all started threads
  for(int i = 0; i < workerThreads_.size(); i++){
    auto& thread = workerThreads_[i];
    if (thread.joinable()) {
      if(threadBusy_[i].load()) {
        std::cout << "Worker: " << i << " is still busy, so we wait for him\n";
        thread.join();
        std::cout << "Worker: " << i << " has finished work\n";
      }
      std::cout << "Worker: " << i << " has no more work\n";
    }
  }
  std::cout << "End of stop in scheduler\n";
}


std::shared_ptr<Task> Scheduler::createTask(std::function<void()> func, const std::vector<TaskID> &dependencies){
  return std::shared_ptr<Task>();
}


void Scheduler::Worker::run(){
  std::cout << "\nWorker: " << this->id_ << " started work\n";
  while(true){
    int currTaskID = -1;
    bool stopReq = false;
    std::unique_lock<std::mutex> lck(scheduler_.mtx_);
    // We need a predicate here on which we wait -> if not, what if we would have notify but empty array as someone took the thread
    scheduler_.workAvailable_.wait(lck, [this, &stopReq] {
      stopReq = scheduler_.stopRequested_.load();
      return !scheduler_.readyTasks_.empty() || stopReq;
    });

    if(stopReq){
      // Todo : implement end of life thread -> One thread to run to "cleanup" everything
      std::cout << "Stop was requested from scheduler\n";
      for(auto& thr : scheduler_.threadBusy_)
        thr.store(false);
      lck.unlock();
      break;
    }
    
    std::cout << "Lock was aquired by worker: " << this->id_ << ", and its trying to find thread to work on\n";

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
    std::cout << "Worker: " << this->id_ << " is responsible for task: " << task_ptr->id_ << "\n";

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
      scheduler_.threadBusy_[id_].store(true);
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
        std::cout << "Worker: " << this->id_ << " has failed to process task: " << currTaskID << "\n";
        succesfullTask = false;
      }
    }

    // Check if finished correctly (TBD : how to do this?)
    lck.lock();
    std::cout << "Worker: " << this->id_ << " aquired end of tak lock\n";
    if(succesfullTask) {
      std::cout << "Worker: " << this->id_ << " has processed task: " << currTaskID << " and it is COMPLETED\n";
      task_ptr->setState(t_TaskState::COMPLETED);
    } else { 
      // TBD : How to handle failed tasks in scheduler regarding the dependencies
      std::cout << "Worker: " << this->id_ << " has processed task: " << currTaskID << " and it is FAILED\n";
      task_ptr->setState(t_TaskState::FAILED);
    }
    
    lck.unlock();
    std::cout << "Worker: " << this->id_ << " released end of tak lock\n";

    scheduler_.notifyDependents(currTaskID);

    // lck.lock();

    std::cout << "Worker: " << this->id_ << " has no more work to do\n";
    // scheduler_.threadBusy_[id_].store(false);
    task_ptr = nullptr;
    // lck.unlock();
  } 
}
