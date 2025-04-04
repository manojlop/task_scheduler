# C++11 Concurrent Task Scheduler

A dependency-aware task scheduler implemented in C++11, designed to manage and execute tasks concurrently across a pool of worker threads while respecting specified dependencies.

## Overview

In modern multi-core environments, effectively managing parallel execution is crucial. This project provides a framework for defining computational tasks, specifying dependencies between them (forming a Directed Acyclic Graph - DAG), and executing tasks whose prerequisites have been met using a fixed-size pool of C++11 `std::thread` workers.

The primary goal is to demonstrate the practical application of modern C++11 features for concurrency control (`std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`), memory management (`std::unique_ptr`, `std::shared_ptr`), and functional programming (`std::function`, lambdas) in a non-trivial context.

## Features (Current Implementation)

*   **Task Definition:** Tasks defined via `std::function<void()>` and assigned unique `TaskID`s.
*   **Dependency Management:** Supports defining dependencies between tasks. Tasks only run after their dependencies enter the `COMPLETED` state.
*   **Concurrent Execution:** Utilizes a fixed-size thread pool (`std::vector<std::thread>`) to execute ready tasks concurrently.
*   **State Tracking:** Tasks transition through states: `PENDING`, `READY`, `RUNNING`, `COMPLETED`, `FAILED`, `CANCELLED`.
*   **Failure Propagation:** Task execution failures (`FAILED` state) are propagated recursively to dependent tasks. *(Note: Current implementation uses recursion, iterative approach planned).*
*   **Thread Safety:** Uses `std::mutex` and `std::condition_variable` to protect shared scheduler state (task queue, task map, dependency map).
*   **Clean Shutdown:** Implements graceful shutdown, ensuring worker threads are properly joined.
*   **RAII:** Resource Acquisition Is Initialization used for thread and worker object lifetime management.

## Technical Details

### Architecture

The system comprises three main components:

1.  **`Task` Class:** Represents a unit of work, holding its ID, state, dependencies (`std::vector<TaskID>`), work function (`std::function<void()>`), and unmet dependency count (`std::atomic<TaskID>`). Non-copyable, movable.
2.  **`Scheduler` Class:** The central orchestrator. Manages:
    *   `tasks_`: `std::unordered_map<TaskID, std::shared_ptr<Task>>` storing all tasks.
    *   `readyTasks_`: `std::deque<TaskID>` holding IDs of tasks ready for execution.
    *   `downwardDependencies_`: `std::unordered_map<TaskID, std::vector<TaskID>>` for reverse dependency lookup.
    *   `workers_`: `std::vector<std::unique_ptr<Worker>>` owning worker logic objects.
    *   `workerThreads_`: `std::vector<std::thread>` holding OS thread handles.
    *   Synchronization Primitives: `std::mutex`, `std::condition_variable`, `std::atomic<bool> stopRequested_`.
3.  **`Scheduler::Worker` Class (Nested):** Contains the `run()` loop executed by each worker thread. Interacts with the `Scheduler`'s shared state via a reference (`Scheduler&`) to fetch and execute tasks.

### Core Concepts

*   **Dependency Tracking:** Tasks maintain an atomic `unmetCount_`. When a dependency completes successfully, the count of its dependents is decremented (`notifyDependents`).
*   **Readiness:** A task transitions to `READY` when its `unmetCount_` reaches zero. Its ID is then pushed onto the `readyTasks_` queue.
*   **Producer-Consumer:** The `Scheduler` (specifically `addTask` and `notifyDependents` logic) acts as the producer, adding `TaskID`s to `readyTasks_`. The `Worker` threads act as consumers, waiting on `workAvailable_` and dequeuing `TaskID`s when available.
*   **Notification:** `workAvailable_.notify_one()` is called when a task becomes `READY`. `workAvailable_.notify_all()` is called during `stop()`.
*   **Shutdown (Policy 2):** When `stop()` is called, `stopRequested_` is set, remaining tasks in `readyTasks_` are marked `CANCELLED`, and workers exit their loop immediately upon observing `stopRequested_`. `stop()` then joins all worker threads.

### C++11 Features Used

*   Concurrency: `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`
*   Memory Management: `std::unique_ptr`, `std::shared_ptr`, RAII
*   Functional: `std::function`, Lambda Expressions
*   Containers: `std::unordered_map`, `std::deque`, `std::vector`
*   Utilities: `<chrono>`, `<algorithm>`, move semantics

## Building

The project requires a C++11 compliant compiler (e.g., GCC 4.8+, Clang 3.3+, MSVC 2015+).

**Using CMake (Recommended):**

```cmake
# Create and navigate to a build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the project
cmake --build .
```

**Using python scripts provided (Example):**

There are 3 (essentially 3 python scripts provided):
- compile.py
- run.py
- all.py

*Their explanation will be provided later, when they are defined as completed. For now, to run main, you can use all.py*

> To run a specified test case, script ./all.py can be used, with flag test set. Like this: `./all.py -test stress`

## Usage

1.  Include `scheduler.h`.
2.  Create a `Scheduler` instance, optionally specifying the number of worker threads:
    ```c++
    Scheduler scheduler(4); // Use 4 worker threads
    ```
3.  Start the worker threads:
    ```c++
    scheduler.start();
    ```
4.  Add tasks using `addTask`, providing a work function (lambda recommended) and a vector of dependency TaskIDs:
    ```c++
    TaskID t1 = scheduler.addTask([](){ /* work */ });
    TaskID t2 = scheduler.addTask([](){ /* work */ }, {t1}); // Depends on t1
    ```
5.  Allow tasks to run. For robust waiting, implement and use `scheduler.wait()`. For simple tests, `std::this_thread::sleep_for` can be used (less reliable).
6.  Stop the scheduler gracefully (_or we can wait for the destructor to be called, as it calls the stop function_):
    ```c++
    scheduler.stop();
    ```

Refer to `main.cpp` (*failure_test* and *saniy_test*) for a concrete example scenario.

## Current Status & Limitations

*   **Functional:** Core scheduling, dependency handling, basic failure propagation, and shutdown work for DAGs.
*   **Failure Propagation:** Implemented recursively via `notifyDependents`. **Risk of stack overflow** on deep dependency chains. Iterative (queue/stack) approach is planned.
*   **Cycle Detection:** Basic check placeholder exists (`check_cycles`), but full implementation is **TODO**. Current rules prevent cycles on single adds, but detection is needed for robustness and future features.
*   **`scheduler.wait()`:** Not yet implemented. Synchronization relies on `sleep_for` in examples.
*   **Logging:** Uses basic `std::cout` and a `safe_print` utility relying on a global mutex. A proper thread-safe logging library/framework is needed.
*   **Testing:** Lacks automated unit or integration tests. Correctness verified via manual inspection of `main` execution output.
*   **`addTask(Task&&)`:** Overload exists but has potential ID management complexities and is recommended for review/removal.

## Future Work

*   **Core Functionality:**
    *   Implement `Scheduler::wait()` method.
    *   Implement robust, iterative failure propagation.
    *   Implement robust cycle detection within `addTask`.
    *   Implement cooperative task cancellation (requires Task modification and checks within work functions).
    *   Implement task priorities (e.g., using `std::priority_queue`).
    *   Implement task return values (e.g., using `std::packaged_task`, `std::future`).
*   **API & Usability:**
    *   Implement variadic template `addTask` for batch submission.
    *   Add `getTaskState(TaskID)` method.
*   **Performance & Scalability:**
    *   Profile and potentially optimize locking (finer-grained locks).
    *   Consider work-stealing queues.
*   **Robustness & Refinement:**
    *   Integrate a proper thread-safe logging library.
    *   Remove global state (`globalMutex`, etc.).
    *   Add comprehensive unit and integration tests (e.g., Google Test).
    *   Remove `threadBusy_`.
    *   Refine or remove `addTask(Task&&)`.
    *   Add `CANCELLED` state handling more explicitly.
    *   Use C++14/17/20 features where beneficial (`make_unique`, etc.).

## License

```
GNU GENERAL PUBLIC LICENSEV

Copyright (c) [2025] [Manojlo Pekovic]

Permission is hereby granted, free of charge, to any person obtaining a copy
```