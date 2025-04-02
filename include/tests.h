#ifndef SCHEDULER_TESTS__
#define SCHEDULER_TESTS__


#include "types_defines.h"
#include "scheduler.h"
#include "task.h"

#include <iostream>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <stdexcept>    // For std::runtime_error
#include <string>



int sanity_test();
int failure_test();

#endif
