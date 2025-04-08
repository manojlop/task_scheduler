# C++ Task Scheduler

A multi-threaded C++ task scheduler designed for managing and executing tasks with complex dependencies efficiently.

## Overview

This project provides a robust framework for defining units of work (tasks) and their dependencies, allowing for concurrent execution using a pool of worker threads. It handles task state management, dependency resolution, failure propagation, and provides utilities for building and running.

## Features

*   **Dependency Management:** Define tasks that depend on the completion of others.
*   **Multi-threaded Execution:** Utilizes a configurable pool of worker threads for concurrency.
*   **Task Lifecycle:** Tracks tasks through states: `PENDING`, `READY`, `RUNNING`, `COMPLETED`, `FAILED`, `CANCELLED`.
*   **Failure Propagation:** Automatically fails tasks whose dependencies have failed.
*   **Basic Cycle Detection:** Prevents adding dependencies that create simple cycles.
*   **Synchronization:** Uses standard C++ mutexes, condition variables, and atomics for thread safety.
*   **Helper Scripts:** Python scripts included to simplify building and running.
*   **Testing:** Includes manual test scenarios and a GoogleTest suite for unit/integration testing.

## Prerequisites

*   **C++ Compiler:** Supporting C++14 or later (e.g., GCC, Clang)
*   **CMake:** Version 3.10 or later
*   **Python:** Version 3.x (for helper scripts)
*   **Git:** (Optional, for cloning)
*   **Internet Connection:** Required for the initial CMake run to download GoogleTest.

## Building

You can build the project using CMake directly or via the provided helper scripts.

**1. Using CMake Directly:**

```bash
# Clone the repository (if you haven't already)
# git clone https://github.com/manojlop/task_scheduler.git
# cd https://github.com/manojlop/task_scheduler.git

# Create a build directory
mkdir build
cd build

# Configure the project (Downloads GoogleTest on first run)
cmake .. 

# Build the targets (main executable, tests)
cmake --build . 

# Executables will be in build/bin
```

**2. Using Helper Scripts:**

The Python scripts are located in the project root (or potentially a `scripts/` subdirectory).

```bash
# Basic build (configures and builds)
./compile.py 

# Build in Debug mode
./compile.py -dbg

# Build with a specific manual test enabled (e.g., sanity)
./compile.py -d TEST_SANITY 
```

See `./compile.py --help` for more options.

## Running

**1. Main Executable:**

If the project contains a `src/main.cpp`, it builds an executable named `main`.

```bash
# Using the helper script (from project root)
./run.py 

# Directly (from build directory)
./bin/main 
```

**2. Manual Tests:**

Manual tests (`test_sanity.cpp`, etc.) are compiled into the `main` executable when enabled via build flags.

```bash
# 1. Build with the test flag
./compile.py -d TEST_SANITY # Or TEST_FAILURE_PROPAGATED, TEST_STRESS

# 2. Run the main executable
./run.py 
```

**3. Google Tests:**

The automated test suite (`MyTests` executable) can be run easily.

```bash
# Using the helper script (from project root)
./run.py -gt

# Using CTest (from build directory)
ctest -V

# Directly (from build directory)
./bin/MyTests --gtest_filter=*Integration* # Example: Run specific tests
```

**4. Combined Build and Run:**

The `all.py` script combines both steps.

```bash
# Clean, build (debug), and run Google Tests
./all.py -c -dbg -gt

# Build with sanity test defined and run it
./all.py -d TEST_SANITY
```

See `./all.py --help` for details.

## Basic Usage Example

```cpp
#include "scheduler.h" // Assuming scheduler.h is in the include path
#include "types_defines.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

int main() {
    // Create a scheduler with 4 worker threads
    Scheduler scheduler(4); 
    verbosityPrinted = t_Verbosity::INFO; // Set desired log level

    // Define some work functions (lambdas)
    auto work1 = []() { 
        safe_print("Task 1 running..."); 
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        safe_print("Task 1 finished.");
    };

    auto work2 = []() { 
        safe_print("Task 2 running..."); 
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
        safe_print("Task 2 finished.");
    };

    TaskID id3; // Forward declare ID needed inside lambda
    auto work3 = [&id3]() { 
        safe_print("Task 3 (depends on 1) running..."); 
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        safe_print("Task 3 finished.");
    };


    // Start the scheduler threads
    scheduler.start();
    safe_print("Scheduler started.");

    // Add tasks
    TaskID id1 = scheduler.addTask(work1);
    TaskID id2 = scheduler.addTask(work2);
    id3 = scheduler.addTask(work3, {id1}); // Task 3 depends on Task 1

    safe_print("Tasks added.");
    safe_print(("Task 1 ID: " + std::to_string(id1)));
    safe_print(("Task 2 ID: " + std::to_string(id2)));
    safe_print(("Task 3 ID: " + std::to_string(id3) + " depends on " + std::to_string(id1)));


    // Wait for all added tasks to complete
    safe_print("Waiting for tasks to complete...");
    scheduler.waitTasksToEnd();

    safe_print("All tasks finished. Stopping scheduler.");
    // Scheduler destructor will call stop() automatically, or you can call scheduler.stop();

    safe_print("Program finished.");
    return 0;
}
```

## Documentation

For a comprehensive guide including API reference, implementation details, design decisions, and advanced usage, please see [DOCUMENTATION.md](doc/DOCUMENTATION.md).

## Testing

The project includes:

1.  **Manual Tests:** Scenarios enabled via compile flags (`TEST_SANITY`, `TEST_FAILURE_PROPAGATED`, `TEST_STRESS`) compiled into the `main` executable.
2.  **Automated Tests:** Unit and integration tests using GoogleTest, built into the `MyTests` executable. Run via `./run.py -gt` or `ctest`.

See the [DOCUMENTATION.md](doc/DOCUMENTATION.md) for more details on the testing strategy.

## Continuous Integration (CI)

[![Build Status](https://github.com/manojlop/task_scheduler/actions/workflows/ci.yml/badge.svg)](https://github.com/manojlop/task_scheduler/actions/workflows/ci.yml) 

This project uses **GitHub Actions** for Continuous Integration. A workflow (`.github/workflows/ci.yml`) is configured to automatically build and test the project on every push or pull request targeting the `main` branch.

**The CI pipeline:**

1.  Checks out the source code.
2.  Sets up an Ubuntu environment with necessary build tools (CMake, GCC, Ninja).
3.  Configures the project using CMake (Debug build).
4.  Builds the project executables, including the test suite.
5.  Runs the automated tests using CTest.

This ensures that code changes integrate correctly and pass all tests in a clean environment, helping maintain code quality and stability. For more details on the workflow steps, see the [CI Workflow section](DOCUMENTATION.md#64-continuous-integration-ci-workflow-githubworkflowsciyml) in the main documentation.

## Contributing

Contributions are welcome! Please feel free to open an issue to discuss potential changes or submit a pull request. 

## License

This project is licensed under the GNU GENERAL PUBLIC LICENSE License - see the [LICENSE](LICENSE) file for details. 
