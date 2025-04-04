#ifndef SCHEDULER_TYPES_DEFINES_H_
#define SCHEDULER_TYPES_DEFINES_H_

#include <cstddef>
#include <mutex>
#include <string>


enum t_TaskState { PENDING, READY, RUNNING, COMPLETED, FAILED, CANCELLED };
// Todo : Add more granularity -> HIGH MEDIUM LOW
enum t_Verbosity { NONE, ERROR, WARNING, INFO, DEBUG };


// Declare the mutex as extern to be able to use it globally
extern std::mutex globalMutex;

extern t_Verbosity verbosityPrinted;

void safe_print(std::string msg, std::string name = "General", t_Verbosity verbosity = INFO);

using TaskID = std::size_t;

#endif