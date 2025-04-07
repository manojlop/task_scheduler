#include "scheduler.h"
const char* Scheduler::taskStateName[] =  { "PENDING", "READY", "RUNNING", "COMPLETED", "FAILED", "CANCELLED" };
      
// TODO: Implement full cycle detection here if graph modification rules change
// (e.g., batch adds, adding dependencies to existing tasks).
// Current rule (new task only depends on existing tasks) prevents cycles
// involving the new task at the time of addition.
// Chosen detection method (e.g., DFS on simulated graph copy, or targeted reachability)
// will need to be implemented here before committing changes to tasks_ and downwardDependencies_.

// Todo : This implementation can maybe be sped up with dynamic programming? (if 5 can't reach 7, neither can 3 throught 5...) May use more resources, but may be benefitiary when large number of threads, to reduce workload
bool Scheduler::check_cycles(TaskID dependand, TaskID depends_on, std::vector<TaskID>& cycle){
  // std::stack<TaskID> stack;
  if((downwardDependencies_.count(dependand) == 0) || downwardDependencies_[dependand].empty()) {
    cycle.pop_back();
    return false;
  }
  for(int i = 0; i < int(downwardDependencies_[dependand].size()); i++){
    cycle.push_back(downwardDependencies_[dependand][i]);
    if(downwardDependencies_[dependand][i] == cycle[0]) {
      return true;
    } else if(check_cycles(downwardDependencies_[dependand][i], dependand, cycle))
      return true;
  }
  return false;
}


void Scheduler::notifyDependents(TaskID taskId){
  safe_print(("Task: " + std::to_string(taskId) + " is: " + taskStateName[tasks_[taskId]->getState()] + " and it's notifying dependents"), "Scheduler", INFO);

  // Handle if wrong task id passed
  auto completed_task_it = tasks_.find(taskId);
  if (completed_task_it == tasks_.end()) { 
    safe_print(("Task with id: " + std::to_string(taskId) + " couldn't be found in tasks map"), "Scheduler", ERROR);
    return; 
  }
  auto completed_task_ptr = tasks_[taskId];

  if(completed_task_ptr->getState() == t_TaskState::FAILED){
    // Propagate failure
    // Don't decrement unmet count, iterate and mark failed
    // First need to check if there is entry to map
    if(downwardDependencies_.count(taskId)){
      std::stack<TaskID> stack;
      stack.push(taskId);
      while(!stack.empty()){
        TaskID currFailed = stack.top();
        stack.pop();
        if(!tasks_[currFailed]->setStateFailed())
          return;
        failedTasks_.fetch_add(1);
        if(downwardDependencies_.count(currFailed)){
          for(auto dependent_id : downwardDependencies_.at(currFailed)){
            auto dep_it = tasks_.find(dependent_id);
            if(dep_it != tasks_.end()){
              stack.push(dep_it->first);
            } else {
              safe_print(("Task with id: " + std::to_string(dependent_id) + " couldn't be found in tasks map"), "Scheduler", ERROR);
            }
          }
        }
      }
    }
    return;
  }

  if (completed_task_ptr->getState() != t_TaskState::COMPLETED) {
    // Task wasn't completed? Log warning? Or maybe called too early?
    safe_print(("Task with id: " + std::to_string(taskId) + " wasn't COMPLETED nor FINISHED, but notify dependents was called"), "Scheduler", ERROR);
    return;
  }

  // Process only if COMPLETED
  // Need to check if in map
  if(downwardDependencies_.count(taskId)){
    for(auto dependent_id : downwardDependencies_.at(taskId)){
      auto dep_it = tasks_.find(dependent_id);
      if(dep_it == tasks_.end()){
        safe_print(("Task with id: " + std::to_string(dependent_id) + " couldn't be found in tasks map"), "Scheduler", ERROR);
        continue;
      }
      auto dep_ptr = tasks_[dependent_id];

      // Proceed only if dependent is still pending
      if(dep_ptr->getState() == t_TaskState::PENDING){
        // Returns true if zero
        if(dep_ptr->decrement_unmet_dependencies()){
          if(!dep_ptr->setStateReady())
            return;
          readyTasks_.push_back(dependent_id);
          workAvailable_.notify_one();
        }
      } else if (dep_ptr->getState() == t_TaskState::RUNNING || dep_ptr->getState() == t_TaskState::READY){
        safe_print(("Task with id: " + std::to_string(dep_ptr->id_) + " is already in state: " + taskStateName[dep_ptr->getState()] + " but it depends on task: " + std::to_string(taskId) + " which has just COMPLETED"), "Scheduler", ERROR);
      }
    }
  }
}


TaskID Scheduler::addTask(std::function<void()> func, const std::vector<TaskID>& dependencies){
  safe_print(("Adding new task"), "Scheduler", DEBUG);
  // Waiting for access to shared resources
  std::lock_guard<std::mutex> lck(mtx_);
  safe_print(("Adding new task, obtained mutex"), "Scheduler", DEBUG);
  // We load current value of id, to check if adding would induce dependencies (it won't in backward dependencies, but to be consistent)
  TaskID currentId = id_.load();
  // Check weather there is depndency in the arguments that is not already in tasks. We cannot depend on task that wasnt created
  // We need to do checking of cyclical dependencies here, because we don't want to insert them into the task map
  // Tbd : optimize, it probably isn't best to copy, but we need new entries in order to check cycles, rigt?
  for(auto depends_on : dependencies){
    if(tasks_.count(depends_on) == 0){
      safe_print(("Tried to add dependency for id: " + std::to_string(currentId) + " to " + std::to_string(depends_on) + " which is not in already created tasks"), "Scheduler", ERROR);
      return -2;
    }
    // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
    // Checks for cycles. If  there are cycles, returns true, if there are none, returns false
    // Updates cycle if there is cycle, so we can print it
    // V1.0 -> Currently, there can not be a cycle when depending on already 
    // TBD : Should this be provided as public method also, so that 'user' can check for dependencies before trying to add new task?
    std::vector<TaskID> cycle;
    cycle.push_back(depends_on);
    cycle.push_back(currentId);
    if(this->check_cycles(currentId, depends_on, cycle) == true) {
      std::string cyc = "";
      for(auto id : cycle)
        cyc = cyc + ", " + std::to_string(id);
      safe_print(("Cycle detected when inserting new dependency: " + std::to_string(depends_on) + " cycle: " + cyc), "Scheduler", ERROR);
      return -2;
    }
  }
  // If we come to here, there are no dependencies
  id_++;
  safe_print(("New task id: " + std::to_string(currentId)), "Scheduler", INFO);
  try{
    // C++14 : std::make_shared() in C++14
    std::shared_ptr<Task> ptr(new Task(currentId, func, dependencies));
    // atomic variable cannot be in make_pair (for emplace) -> that is why we needed to store it in currentId
    tasks_.emplace(currentId, std::move(ptr));
    addedTasks_.fetch_add(1);
  } catch (...) {
    safe_print("Unable to create unique pointer for thread", "Scheduler", ERROR);
    return -1;
  }
  // Insert current id into list of dependencies
  for(auto dep : dependencies){
    downwardDependencies_[dep].push_back(currentId);
    // Decrement count if dependency is already completed
    if(tasks_[dep]->getState() == t_TaskState::COMPLETED) {
      safe_print(("Task: " + std::to_string(currentId) + "  depends on task: " + std::to_string(dep) + " which is already COMPLETED, so counter is decremented"), "Scheduler", INFO);
      tasks_[currentId]->decrement_unmet_dependencies();
    }
    // FAILURE is propagated
    else if (tasks_[dep]->getState() == t_TaskState::FAILED) {
      safe_print(("Task: " + std::to_string(currentId) + "  depends on task: " + std::to_string(dep) + " which has FAILED, so its also FAILS"), "Scheduler", INFO);
      if(!tasks_[currentId]->setStateFailed())
        return -10;
    }
  }
  if(tasks_[currentId]->unmetCount_.load() == 0){
    // ptr->getState() = t_TaskState::READY; // We moved from ptr -> Segmentation fault!
    safe_print(("Task with id: " + std::to_string(currentId) + " doesn't have any more unmet dependencies, so it is READY"), "Scheduler", INFO);
    if(!tasks_[currentId]->setStateReady())
      return -10;
    readyTasks_.push_back(currentId);
    workAvailable_.notify_one();
  } else {
     // Ensure initial state is PENDING if it wasn't set by default
      if (tasks_[currentId]->getState()  != t_TaskState::PENDING) {
        if(!tasks_[currentId]->setStatePending())
          return -10;
      }
      safe_print(("Task with id: " + std::to_string(currentId) + " is PENDING"), "Scheduler", INFO);
  }
  return currentId;
}

// Todo : revise if we want this -> potential problems with ids if we take outside ids. -> with moving const variables and creating new id (outside dependencies??)
// TaskID Scheduler::addTask(Task &&task){
//   std::lock_guard<std::mutex> lck(mtx_);
//   // Todo : validate dependencies

//   // Return current value and increment -> that is the one that is assigned
//   // atomic variable cannot be in make_pair
//   TaskID currentId = id_.fetch_add(1);
//   std::shared_ptr<Task> ptr(new Task(std::move(task)));
//   // Todo : check cyclical dependencies before inserting -> return -1 if cycle is created
//   tasks_.emplace(currentId, std::move(ptr));
//   for(auto dep : task.dependencies_){
//     // We map to the assigned ID, not the incremented one
//     downwardDependencies_[dep].push_back(currentId);
//     if(tasks_[dep]->getState() == t_TaskState::COMPLETED)
//       tasks_[currentId]->decrement_unmet_dependencies();
//     else if (tasks_[dep]->getState() == t_TaskState::FAILED)
//       if(!tasks_[currentId]->setStateFailed())
//         return -10;
//   }
//   if(tasks_[currentId]->unmetCount_.load() == 0){
//     // ptr->getState() = t_TaskState::READY; // We moved from ptr -> segmentation fault!
//     if(!tasks_[currentId]->setStateReady())
//       return -10;
//     readyTasks_.push_back(currentId);
//     workAvailable_.notify_one();
//   } else {
//      // Ensure initial state is PENDING if it wasn't set by default
//      if (tasks_[currentId]->getState()  == t_TaskState::READY) { // Check Task's default state logic
//         if(!tasks_[currentId]->setStatePending())
//           return -10;
//      }
//      // TODO: Add check if all dependencies already exist and are COMPLETED?
//      // (More advanced: handle adding task where deps are already done)
//   }
//   return currentId;
// }


void Scheduler::start(){
  if (state_ == t_SchedulerState::STARTED) {
    safe_print("Scheduler already started", "Scheduler", WARNING);
    return;
  }
  if(state_ == t_SchedulerState::CREATED) {
    workerThreads_.reserve(workers_.size()); // Reserve space
  }  else if (state_ == t_SchedulerState::STOPPED){
    tasks_.clear();
    downwardDependencies_.clear();
    readyTasks_.clear();
    addedTasks_.store(0);
    completedTasks_.store(0);
    failedTasks_.store(0);
    cancelledTasks_.store(0);
  }
    for(size_t i = 0; i < workers_.size(); i++){
      // Launch thread and store it
      // Pass pointer to the Worker object managed by unique_ptr
      // Todo : try ... catch for OS errors
      try {
        workerThreads_.emplace_back(&Scheduler::Worker::run, workers_[i].get());
      } catch (...) {
        safe_print(("Unable to start thread: " + std::to_string(i)), "Scheduler", ERROR);
        return;
      }
    }
  state_ = STARTED;
}

void Scheduler::stop(t_StopWay wayToStop){
  if(state_ != t_SchedulerState::STOPPED) {
    if (wayToStop == t_StopWay::WAIT_ALL_TO_END) {
      waitTasksToEnd();
    }
    // std::lock_guard<std::mutex> lck(mtx_); // We cannot use lock guard, because we need to unlock it in order to wait for threads to finish
    std::unique_lock<std::mutex> lck(mtx_);
    safe_print("Lock acquired to stop the scheduler", "Scheduler", DEBUG);
    // Inform on stopage request
    this->stopRequested_.store(true);
    // Flush tasks that are in readyQueue -> TBD : Should we wait for them to complete
    for(auto taskId : readyTasks_) {
      auto it = tasks_.find(taskId);
      if (it == tasks_.end()) {
        safe_print(("Task with id: " + std::to_string(taskId) + " couldn't be found in tasks map"), "Scheduler", ERROR);
        continue;
      }
      safe_print(("Scheduler is being stopped, so task: " + std::to_string(taskId) + " which was READY is being CANCELLED"), "Scheduler", DEBUG);
      if(!tasks_[taskId]->setStateCancelled())
        return;
    }
    this->readyTasks_.clear();
    this->workAvailable_.notify_all();
    lck.unlock();
    safe_print("Lock released to stop the scheduler", "Scheduler", DEBUG);
    // We must join all started threads
    for(int i = 0; i < int(workerThreads_.size()); i++){
      auto& thread = workerThreads_[i];
      if (thread.joinable()) {
        // We wait for thread to finish its work
        safe_print(("Worker: " + std::to_string(i) + " is still busy, so we wait for it"), "Scheduler", NONE);
        thread.join();
        safe_print(("Worker: " + std::to_string(i) + " has finished work"), "Scheduler", NONE);
      }
    }
    safe_print("End of stop in scheduler", "Scheduler", NONE);    
  }
}

bool Scheduler::waitTasksToEnd(){
  if(state_ == t_SchedulerState::STARTED){
    std::unique_lock<std::mutex> lck(mtx_);
    safe_print("Waiting for all tasks to end", "Scheduler", INFO);
    int totalTask = 0, complTask = 0, failTask = 0, cancTask = 0;
    bool stopReq = false;
    allTasksFinished_.wait(lck, [&] {
      totalTask = addedTasks_.load();
      complTask = completedTasks_.load();
      failTask = failedTasks_.load();
      cancTask = cancelledTasks_.load();
      stopReq = stopRequested_.load();
      return (((complTask + failTask + cancTask) == totalTask) || stopReq);
    });
    // If stop has been requested, stop the execution of thread
    if(stopReq){
      // Todo : implement end of life thread -> One thread to run to "cleanup" everything (or wait for all processes to finish?)
      safe_print(("Stop was requested, aborting wait for tasks to finish"), "Scheduler", NONE);
      lck.unlock();
    }

    safe_print(("All tasks currently present[" + std::to_string(totalTask) + "]finished. Completed[" + std::to_string(complTask) + "], Failed[" + std::to_string(failTask) + "], Cancelled[" + std::to_string(cancTask) + "]"), "Scheduler", INFO);
  }

  return false;
}

// std::shared_ptr<Task> Scheduler::createTask(std::function<void()> func, const std::vector<TaskID> &dependencies){
//   return std::shared_ptr<Task>();
// }


void Scheduler::Worker::run(){
  safe_print(("Started work"), ("Worker: " + std::to_string(this->id_)), INFO);
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
      safe_print(("Stop was requested from scheduler"), ("Worker: " + std::to_string(this->id_)), NONE);
      lck.unlock();
      break;
    }
    
    safe_print(("Its trying to find task to work on"), ("Worker: " + std::to_string(this->id_)), DEBUG);

    // Once the lock is aquired, we get the task id and pop it from ready tasks
    currTaskID = scheduler_.readyTasks_.front();
    scheduler_.readyTasks_.pop_front();
    std::shared_ptr<Task> task_ptr = nullptr;

    auto it = scheduler_.tasks_.find(currTaskID);
    if (it == scheduler_.tasks_.end()) {
      safe_print(("Task with id: " + std::to_string(currTaskID) + " couldn't be found in tasks map"), ("Worker: " + std::to_string(this->id_)), ERROR);
      lck.unlock(); 
      continue;
    }
    task_ptr = it->second; // Get the shared_ptr
    safe_print(("Is responsible for task: " + std::to_string(currTaskID)), ("Worker: " + std::to_string(this->id_)), INFO);

    if(task_ptr){
      task_ptr->setState(t_TaskState::RUNNING);
    }

    lck.unlock();

    // Todo : do logging via custom logger
    bool succesfullTask = true;
    // Executing the task
    if(task_ptr){
      try{
        safe_print(("Is processing task: " + std::to_string(currTaskID)), ("Worker: " + std::to_string(this->id_)), INFO);
        task_ptr->run();
        safe_print(("Has COMPLETED to process task: " + std::to_string(currTaskID)), ("Worker: " + std::to_string(this->id_)), INFO);
      } catch (...){
        safe_print(("Has FAILED to process task: " + std::to_string(currTaskID)), ("Worker: " + std::to_string(this->id_)), INFO);
        succesfullTask = false;
      }
    }

    lck.lock();
    safe_print(("Acquired end of task lock: " + std::to_string(currTaskID)), ("Worker: " + std::to_string(this->id_)), DEBUG);
    if(succesfullTask) {
      safe_print(("Has processed task: " + std::to_string(currTaskID) + " and it is COMPLETED"), ("Worker: " + std::to_string(this->id_)), INFO);
      task_ptr->setState(t_TaskState::COMPLETED);
      scheduler_.completedTasks_.fetch_add(1);
    } else { 
      safe_print(("Has processed task: " + std::to_string(currTaskID) + " and it is FAILED"), ("Worker: " + std::to_string(this->id_)), INFO);
      task_ptr->setState(t_TaskState::FAILED);
    }
    
    // We need to unlokc before notifying because the notify function needs to access scheduler internals
    // TBD : Is there some way to strictly execute that task next? Can there be a race condition that someone tries to take lock?
    // Can we delete lock from the function and keep it locked here?
    scheduler_.notifyDependents(currTaskID);
    if((scheduler_.failedTasks_.load() + scheduler_.completedTasks_.load() + scheduler_.cancelledTasks_.load()) == scheduler_.addedTasks_.load()){
      scheduler_.allTasksFinished_.notify_one();
    }
    lck.unlock();
    safe_print( "Released end of task lock", ("Worker: " + std::to_string(this->id_)), DEBUG);

    safe_print( "No more work to do", ("Worker: " + std::to_string(this->id_)), DEBUG);
    task_ptr = nullptr;
  } 
}

void Scheduler::printTaskCollection(t_Verbosity verb){
  safe_print("********************************************************************************************", "Scheduler", verb);
  for(auto task : tasks_){
    std::string dependencies = "";
    for(TaskID it : task.second->dependencies_)
      dependencies += std::to_string(it) + " ";
    std::string str = "Task: " + std::to_string(task.first) + " is " + taskStateName[task.second->getState()] + " unmet count is : " + std::to_string(task.second->unmetCount_) + " and it depends on following tasks: " + dependencies;
    safe_print(str, "Scheduler", verb);
  }
  safe_print("********************************************************************************************", "Scheduler", verb);
}
