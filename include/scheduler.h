#ifndef __SCHEDULER__
#define __SCHEDULER__

#include "types_defines.h"

#include <mutex>

using namespace std;

// Sceduler is to be singleton class, because we only need on instance of it
class Scheduler{
private:
  static Scheduler* scheduler;

  static mutex mtx;

  // Constructors -> Private default constructor
  Scheduler() {}
public:

  // Deletion of copy constructor
  Scheduler(const Scheduler& sch) = delete;

  static Scheduler* getScheduler(){
    if(scheduler == nullptr){
      // RAII -> Binds life cycle of a resource that must be aquired before use to lifetime of object
      // On creation, attempts to take ownership of given mutex
      // When control leaves the scope, lock_guard is desctructed and mutex is released
      lock_guard<mutex> lock(mtx);
      if(scheduler == nullptr){
        scheduler = new Scheduler();
      }
    }
    return scheduler;
  }

  ~Scheduler(){
    delete scheduler;
  }
};



#endif