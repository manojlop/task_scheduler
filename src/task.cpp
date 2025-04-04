#include "task.h"

const char* Task::taskStateName[] =  { "PENDING", "READY", "RUNNING", "COMPLETED", "FAILED", "CANCELLED" };

bool Task::setState(t_TaskState st){
  int unmetCount = unmetCount_.load();
  if(this->state_ == PENDING && (unmetCount != 0 && (st == t_TaskState::READY || st == t_TaskState::RUNNING))){
    safe_print(("Changing state from PENDING to " + std::string(taskStateName[st]) + " , but unmet count is: " + std::to_string(unmetCount)), ("Task: " + std::to_string(id_)), t_Verbosity::ERROR);
    return false;
  }
  if(this->state_ == t_TaskState::RUNNING && st == t_TaskState::PENDING) {
    safe_print(("Changing state from RUNNING to PENDING"), ("Task: " + std::to_string(id_)), t_Verbosity::ERROR);
    return false;
  }
  if(this->state_ == t_TaskState::READY && (unmetCount == 0 && st == t_TaskState::PENDING)){
    safe_print(("Changing state from READY to PENDING, but unmet count is 0"), ("Task: " + std::to_string(id_)), t_Verbosity::ERROR);
    return false;
  }
  if(((this->state_ == t_TaskState::FAILED) || (this->state_ == t_TaskState::CANCELLED) || (this->state_ == t_TaskState::COMPLETED)) && (st != this->state_)){
    safe_print(("Trying to change final state: " + std::to_string(this->state_) + " to a different state: " + std::to_string(st)), ("Task: " + std::to_string(id_)), t_Verbosity::ERROR);
    return false;
  }
  this->state_ = st;
  return true;
}