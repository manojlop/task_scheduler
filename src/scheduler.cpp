#include "scheduler.h"

Scheduler* Scheduler::scheduler = nullptr;
mutex Scheduler::mtx;