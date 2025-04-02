#ifndef SCHEDULER_TYPES_DEFINES_H_
#define SCHEDULER_TYPES_DEFINES_H_

#include <cstddef>

using TaskID = std::size_t;

enum t_TaskState { PENDING, READY, RUNNING, COMPLETED, FAILED, CANCELLED };

#endif