#ifndef SCHEDULER_TYPES_DEFINES_H_
#define SCHEDULER_TYPES_DEFINES_H_

#include <cstddef>
#include <mutex>
#include <string>

// Declare the mutex as extern to be able to use it globally
extern std::mutex globalMutex;

void safe_print(std::string&& msg);

using TaskID = std::size_t;

enum t_TaskState { PENDING, READY, RUNNING, COMPLETED, FAILED, CANCELLED };

#endif