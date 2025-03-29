#include "scheduler.h"

Scheduler* Scheduler::scheduler = nullptr;
std::mutex Scheduler::mtx;