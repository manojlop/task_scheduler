#ifndef __TYPES_DEFINES__
#define __TYPES_DEFINES__

#include <cstddef>

using TaskID = std::size_t;

enum t_TaskState { PENDING, READY, RUNNING, COMPLETED, FAILED };

#endif