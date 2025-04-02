#include "scheduler.h"
const char* Scheduler::taskStateName[] =  { "PENDING", "READY", "RUNNING", "COMPLETED", "FAILED", "CANCELLED" };

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  safe_print("Adding new task");
  // Waiting for access to shared resources
  std::lock_guard<std::mutex> lck(mtx_);
  safe_print("Adding new task, obtained mutex");
  // Check weather there is depndency in the arguments that is not already in tasks. We cannot depend on task that wasnt created
  // We need to do checking of cyclical dependencies here, because we don't want to insert them into the task map
  // Tbd : optimize, it probably isn't best to copy, but we need new entries in order to check cycles, rigt?
  std::unordered_map<TaskID, std::vector<TaskID>> downwardDependenciesCopy = downwardDependencies_;
  for(auto dep : dependencies){
    if(tasks_.count(dep) == 0){
      safe_print("ERROR in addTask. Tried to add dependency id: " + std::to_string(dep) + " which is not in already created tasks");
      return -1;
    }
    // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
    // Checks for cycles. If  there are cycles, returns true, if there are none, returns false
    // Updates downwardDependenciesCopy
    // TBD : Should this be provided as public method also, so that 'user' can check for dependencies before trying to add new task -> then we cannot update map
    // if(this->check_cycles(downwardDependenciesCopy, dep) == true)
    //   return -2;
  }
  // Store current task id and increment by one for following task
  TaskID currentId = id_.fetch_add(1);
  safe_print("New task id: " + std::to_string(currentId));
  // C++14 : std::make_shared() in C++14
  std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
  // atomic variable cannot be in make_pair (for emplace) -> that is why we needed to store it in currentId
  tasks_.emplace(currentId, std::move(ptr));
  // Insert current id into list of dependencies
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(currentId);
    // Decrement count if dependency is already completed
    if(tasks_[dep]->state_ == t_TaskState::COMPLETED) {
      safe_print("Task: " + std::to_string(currentId) + "  depends on task: " + std::to_string(dep) + " which is already COMPLETED, so counter is decremented");
      tasks_[currentId]->decrement_unmet_dependencies();
    }
    // FAILURE is propagated
    else if (tasks_[dep]->state_ == t_TaskState::FAILED) {
      safe_print("Task: " + std::to_string(currentId) + "  depends on task: " + std::to_string(dep) + " which has FAILED, so its also FAILS");
      tasks_[currentId]->state_ = t_TaskState::FAILED;
    }
  }
  if(tasks_[currentId]->unmetCount_.load() == 0){
    // ptr->state_ = t_TaskState::READY; // We moved from ptr -> Segmentation fault!
    safe_print("Task with id: " + std::to_string(currentId) + " doesn't have any more unmet dependencies, so it is READY");
    tasks_[currentId]->state_ = t_TaskState::READY;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
      if (tasks_[currentId]->state_  != t_TaskState::PENDING) {
        tasks_[currentId]->state_ = t_TaskState::PENDING;
      }
      safe_print("Task with id: " + std::to_string(currentId) + " is PENDING");
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
  // std::lock_guard<std::mutex> lck(mtx_);
  safe_print("Task: " + std::to_string(taskId) + " is: " + taskStateName[tasks_[taskId]->state_] + " and it's notifying dependents");

  // Handle if wrong task id passed
  auto completed_task_it = tasks_.find(taskId);
  if (completed_task_it == tasks_.end()) { 
    safe_print("ERROR | Task with id: " + std::to_string(taskId) + " couldn't be found in tasks map");
    return; 
  }
  auto completed_task_ptr = tasks_[taskId];

  if(completed_task_ptr->state_ == t_TaskState::FAILED){
    // Propagate failure
    // Don't decrement unmet count, iterate and mark failed
    // First need to check if there is entry to map
    if(downwardDependencies_.count(taskId)){
      for(auto dependent_id : downwardDependencies_.at(taskId)){
        auto dep_it = tasks_.find(dependent_id);
        if(dep_it != tasks_.end()){
          dep_it->second->state_ = t_TaskState::FAILED;
          // Todo : may induce stack overflow -> use outside queue/stack and no recursion for this
          notifyDependents(dependent_id);
        } else {
          safe_print("ERROR | Task with id: " + std::to_string(dependent_id) + " couldn't be found in tasks map");
        }
      }
    }
    return;
  }

  if (completed_task_ptr->getState() != t_TaskState::COMPLETED) {
    // Task wasn't completed? Log warning? Or maybe called too early?
    safe_print("Task with id: " + std::to_string(taskId) + " wasn't COMPLETED nor FINISHED, but norify dependents was called");
    return;
  }

  // Process only if COMPLETED
  // Need to check if in map
  if(downwardDependencies_.count(taskId)){
    for(auto dependent_id : downwardDependencies_.at(taskId)){
      auto dep_it = tasks_.find(dependent_id);
      if(dep_it == tasks_.end()){
        safe_print("ERROR | Task with id: " + std::to_string(dependent_id) + " couldn't be found in tasks map");
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
      } else if (dep_ptr->state_ == t_TaskState::RUNNING || dep_ptr->state_ == t_TaskState::READY){
        safe_print("Task with id: " + std::to_string(dep_ptr->id_) + " is already in state: " + taskStateName[dep_ptr->state_] + " but it depends on task: " + std::to_string(taskId) + " which has just COMPLETED");
      }
    }
  }
}


void Scheduler::start(){
  workerThreads_.reserve(workers_.size()); // Reserve space
  for(size_t i = 0; i < workers_.size(); ++i){
    // Launch thread and store it
    // Pass pointer to the Worker object managed by unique_ptr
    // Todo : try ... catch for OS errors
    workerThreads_.emplace_back(&Scheduler::Worker::run, workers_[i].get());
  }
}

void Scheduler::stop(){
  // std::lock_guard<std::mutex> lck(mtx_); // We cannot use lock guard, because we need to unlock it in order to wait for threads to finish
  std::unique_lock<std::mutex> lck(mtx_);
  safe_print("Lock acquired to stop the scheduler");
  // Inform on stopage request
  this->stopRequested_.store(true);
  // Flush tasks that are in readyQueue -> TBD : Should we wait for them to complete
  for(auto taskId : readyTasks_) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
      safe_print("ERROR | Task with id: " + std::to_string(taskId) + " couldn't be found in tasks map");
      continue;
    }
    safe_print("Scheduler is being stopped, so task: " + std::to_string(taskId) + " which was READY is being CANCELLED");
    tasks_[taskId]->state_ = t_TaskState::CANCELLED;
  }
  this->readyTasks_.clear();
  this->workAvailable_.notify_all();
  lck.unlock();
  safe_print("Lock released to stop the scheduler");
  // We must join all started threads
  for(int i = 0; i < workerThreads_.size(); i++){
    auto& thread = workerThreads_[i];
    if (thread.joinable()) {
      // We wait for thread to finish its work
      safe_print("Worker: " + std::to_string(i) + " is still busy, so we wait for it");
      thread.join();
      safe_print("Worker: " + std::to_string(i) + " has finished work");
    }
  }
  safe_print("End of stop in scheduler");
}


std::shared_ptr<Task> Scheduler::createTask(std::function<void()> func, const std::vector<TaskID> &dependencies){
  return std::shared_ptr<Task>();
}


void Scheduler::Worker::run(){
  safe_print("Worker: " + std::to_string(this->id_) + " started work");
  while(true){
    int currTaskID = -1;
    bool stopReq = false;
    // We need to obtain the lock on conditional variable workAvailable_ to check if there are threads to work on, or if stop has been requested
    std::unique_lock<std::mutex> lck(scheduler_.mtx_);
    // We need a predicate here on which we wait -> if not, what if we would have notify but empty array as someone took the thread already
    scheduler_.workAvailable_.wait(lck, [this, &stopReq] {
      // Store the value to check after progressing from conditional variable -> in order for it to not be changed when we check it 
      stopReq = scheduler_.stopRequested_.load();
      return !scheduler_.readyTasks_.empty() || stopReq;
    });

    // If stop has been requested, stop the execution of thread
    if(stopReq){
      // Todo : implement end of life thread -> One thread to run to "cleanup" everything (or wait for all processes to finish?)
      safe_print("Stop was requested from scheduler");
      lck.unlock();
      break;
    }
    
    safe_print("Lock was aquired by worker: " + std::to_string(this->id_) + ", and its trying to find task to work on");

    // Once the lock is aquired, we get the task id and pop it from ready tasks
    currTaskID = scheduler_.readyTasks_.front();
    scheduler_.readyTasks_.pop_front();
    std::shared_ptr<Task> task_ptr = nullptr;

    auto it = scheduler_.tasks_.find(currTaskID);
    if (it == scheduler_.tasks_.end()) {
      safe_print("ERROR | Task with id: " + std::to_string(currTaskID) + " couldn't be found in tasks map");
      lck.unlock(); 
      continue;
    }
    task_ptr = it->second; // Get the shared_ptr
    safe_print("Worker: " + std::to_string(this->id_) + " is responsible for task: " + std::to_string(task_ptr->id_));

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
    }

    lck.unlock();

    // Todo : do logging via custom logger
    bool succesfullTask = true;
    // Executing the task
    if(task_ptr){
      try{
        safe_print("Worker: " + std::to_string(this->id_) + " is processing task: " + std::to_string(currTaskID));
        task_ptr->run();
        safe_print("Worker: " + std::to_string(this->id_) + " has COMPLETED process of task: " + std::to_string(currTaskID));
      } catch (...){
        safe_print("Worker: " + std::to_string(this->id_) + " has FAILED to process task: " + std::to_string(currTaskID));
        succesfullTask = false;
      }
    }

    lck.lock();
    safe_print("Worker: " + std::to_string(this->id_ )+ " acquired end of task lock");
    if(succesfullTask) {
      safe_print("Worker: " + std::to_string(this->id_) + " has processed task: " + std::to_string(currTaskID) + " and it is COMPLETED");
      task_ptr->setState(t_TaskState::COMPLETED);
    } else { 
      safe_print("Worker: " + std::to_string(this->id_) + " has processed task: " + std::to_string(currTaskID) + " and it is FAILED");
      task_ptr->setState(t_TaskState::FAILED);
    }
    
    // We need to unlokc before notifying because the notify function needs to access scheduler internals
    // TBD : Is there some way to strictly execute that task next? Can there be a race condition that someone tries to take lock?
    // Can we delete lock from the function and keep it locked here?
    scheduler_.notifyDependents(currTaskID);
    lck.unlock();
    safe_print("Worker: " + std::to_string(this->id_) + " released end of task lock");

    safe_print("Worker: " + std::to_string(this->id_) + " has no more work to do");
    task_ptr = nullptr;
  } 
}
