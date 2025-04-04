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



int test_sanity();
int test_failure_propagated();
int test_concurrent_tasks();
int test_dependency_chain();
// Use an atomic counter to track completed tasks (approximate check)
extern std::atomic<int> completed_task_counter;
int test_stress();

#endif
