#ifndef SCHEDULER_TESTS__
#define SCHEDULER_TESTS__


#include "scheduler.h" // Your Scheduler header
#include "task.h"      // Your Task header
#include "types_defines.h" // Your types header

#include <iostream>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <stdexcept>    // For std::runtime_error
#include <string>


// Declare the mutex as extern to be able to use it globally
extern std::mutex globalMutex;

void safe_print(const std::string& msg);

int sanity_test();
int failure_test();

#endif
