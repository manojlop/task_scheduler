#include "scheduler.h"

TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  std::cout << "Adding new task\n";
  // Waiting for access to shared resources
  std::lock_guard<std::mutex> lck(mtx_);
  std::cout << "Adding new task, obtained mutex\n";
  // Check weather there is depndency in the arguments that is not already in tasks. We cannot depend on task that wasnt created
  // We need to do checking of cyclical dependencies here, because we don't want to insert them into the task map
  // Tbd : optimize, it probably isn't best to copy, but we need new entries in order to check cycles, rigt?
  std::unordered_map<TaskID, std::vector<TaskID>> downwardDependenciesCopy = downwardDependencies_;
  for(auto dep : dependencies){
    if(tasks_.count(dep) == 0){
      std::cout << "Error in addTask. Tried to add depndency id: " << dep << " which is not in already created tasks\n";
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
  std::cout << "New task id: " << currentId << "\n";
  // C++14 : std::make_shared() in C++14
  std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
  // atomic variable cannot be in make_pair (for emplace) -> that is why we needed to store it in currentId
  tasks_.emplace(currentId, std::move(ptr));
  // Insert current id into list of dependencies
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(currentId);
    // Decrement count if dependency is already completed
    if(tasks_[dep]->state_ == t_TaskState::COMPLETED) {
      std::cout << "Task: " << currentId << "  depends on task: " << dep << " which is already COMPLETED, so counter is decremented\n";
      tasks_[currentId]->decrement_unmet_dependencies();
    }
    // FAILURE is propagated
    else if (tasks_[dep]->state_ == t_TaskState::FAILED) {
      std::cout << "Task: " << currentId << "  depends on task: " << dep << " which has FAILED, so its also FAILS\n";
      tasks_[currentId]->state_ = t_TaskState::FAILED;
    }
  }
  if(tasks_[currentId]->unmetCount_.load() == 0){
    // ptr->state_ = t_TaskState::READY; // We moved from ptr -> Segmentation fault!
    std::cout << "Task with id: " << currentId << " doesn't have any more unment dependecies, so it is READY\n";
    tasks_[currentId]->state_ = t_TaskState::READY;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
      if (tasks_[currentId]->state_  != t_TaskState::PENDING) {
        tasks_[currentId]->state_ = t_TaskState::PENDING;
      }
      std::cout << "Task with id: " << currentId << " is PENDING\n";
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
    std::cout << "Task with id: " << taskId << " couldn't be found in tasks map\n";
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
        }
      }
    }
    return;
  }

  if (completed_task_ptr->getState() != t_TaskState::COMPLETED) {
    // Task wasn't completed? Log warning? Or maybe called too early?
    std::cout << "Task with id: " << taskId << " wasn't COMPLETED nor FINISHED, but norify dependents was called\n";
    return;
  }

  // Process only if COMPLETED
  // Need to check if in map
  if(downwardDependencies_.count(taskId)){
    for(auto dependent_id : downwardDependencies_.at(taskId)){
      auto dep_it = tasks_.find(dependent_id);
      if(dep_it == tasks_.end()){
        std::cout << "Task with id: " << dependent_id << " couldn't be found in tasks map\n";
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
        std::cout << "Task with id: " << dep_ptr->id_ << " is already in state: " << dep_ptr->state_ << " but it depends on task: " << taskId << " which has just COMPLETED\n";
      }
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
  // std::lock_guard<std::mutex> lck(mtx_); // We cannot use lock guard, because we need to unlock it in order to wait for threads to finish
  std::unique_lock<std::mutex> lck(mtx_);
  std::cout << "Lock aquired to stop the scheduler\n";
  // Inform on stopage request
  this->stopRequested_.store(true);
  // Flush tasks that are in readyQueue -> TBD : Should we wait for them to complete
  for(auto taskId : readyTasks_) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
      std::cout << "Task with id: " << taskId << " couldn't be found in tasks map\n";
      continue;
    }
    std::cout << "Scheduler is being stopped, so task: " << taskId << " which was READY is being CANCELLED\n";
    tasks_[taskId]->state_ = t_TaskState::CANCELLED;
  }
  this->readyTasks_.clear();
  this->workAvailable_.notify_all();
  lck.unlock();
  std::cout << "Lock released to stop the scheduler\n";
  // We must join all started threads
  for(int i = 0; i < workerThreads_.size(); i++){
    auto& thread = workerThreads_[i];
    if (thread.joinable()) {
      // If thread is busy, we wait for it, but if it isn't we can just skip it -> thread has finished its work
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
      std::cout << "Stop was requested from scheduler\n";
      for(auto& thr : scheduler_.threadBusy_)
        thr.store(false);
      lck.unlock();
      break;
    }
    
    std::cout << "Lock was aquired by worker: " << this->id_ << ", and its trying to find thread to work on\n";

    // Once the lock is aquired, we get the task id and pop it from ready tasks
    currTaskID = scheduler_.readyTasks_.front();
    scheduler_.readyTasks_.pop_front();
    std::shared_ptr<Task> task_ptr = nullptr;

    auto it = scheduler_.tasks_.find(currTaskID);
    if (it == scheduler_.tasks_.end()) {
      std::cout << "Task with id: " << currTaskID << " couldn't be found in tasks map\n";
        lck.unlock(); 
        continue;
    }
    task_ptr = it->second; // Get the shared_ptr
    std::cout << "Worker: " << this->id_ << " is responsible for task: " << task_ptr->id_ << "\n";

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
      scheduler_.threadBusy_[id_].store(true);
    }

    lck.unlock();

    // Todo : do logging via custom logger
    bool succesfullTask = true;
    // Executing the task
    if(task_ptr){
      try{
        std::cout << "Worker: " << this->id_ << " is processing task: " << currTaskID << "\n";
        task_ptr->run();
      } catch (...){
        std::cout << "Worker: " << this->id_ << " has failed to process task: " << currTaskID << "\n";
        succesfullTask = false;
      }
    }

    lck.lock();
    std::cout << "Worker: " << this->id_ << " aquired end of tak lock\n";
    if(succesfullTask) {
      std::cout << "Worker: " << this->id_ << " has processed task: " << currTaskID << " and it is COMPLETED\n";
      task_ptr->setState(t_TaskState::COMPLETED);
    } else { 
      std::cout << "Worker: " << this->id_ << " has processed task: " << currTaskID << " and it is FAILED\n";
      task_ptr->setState(t_TaskState::FAILED);
    }
    
    // We need to unlokc before notifying because the notify function needs to access scheduler internals
    // TBD : Is there some way to strictly execute that task next? Can there be a race condition that someone tries to take lock?
    // Can we delete lock from the function and keep it locked here?
    lck.unlock();
    std::cout << "Worker: " << this->id_ << " released end of tak lock\n";

    scheduler_.notifyDependents(currTaskID);

    std::cout << "Worker: " << this->id_ << " has no more work to do\n";
    task_ptr = nullptr;
  } 
}
