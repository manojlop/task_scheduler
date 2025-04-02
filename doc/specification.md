# Project Specification: A C++11 Implementation of a Concurrent, Dependency-Aware Task Scheduler

## 1. Abstract

This document specifies the design and implementation of a concurrent task scheduling system developed using C++11 standard library features. The system manages a collection of tasks, represented as Directed Acyclic Graphs (DAGs) based on user-defined dependencies, and orchestrates their execution across a configurable pool of worker threads. Key objectives include achieving correct concurrent execution while respecting task dependencies, demonstrating idiomatic application of C++11 concurrency primitives (`std::thread`, `std::mutex`, `std::condition_variable`), memory management features (`std::unique_ptr`, `std::shared_ptr`), and functional programming utilities (`std::function`). The design emphasizes correctness, modularity (via `Scheduler`, `Task`, and nested `Worker` classes), and provides a foundation for future functional and performance enhancements. This document reflects the current implementation state, including external type definitions (`types_defines.h`), the task representation (`task.h`), and the core scheduler logic (`scheduler.h`).

## 2. Introduction

*   **2.1. Motivation:** With widespread multi-core processors now common, software architectures must be designed to take advantage of parallelism. Many complex problems can be decomposed into smaller, manageable tasks. Often, inherent dependencies exist between these tasks, requiring a structured execution flow. Task scheduling systems address this need by managing task sets, enforcing execution order based on dependencies, and distributing work across available processing resources to enhance throughput and efficiency.
*   **2.2. Problem Statement:** Design and implement a task scheduler in C++11 capable of:
    *   Accepting computational tasks defined via `std::function<void()>`.
    *   Managing explicit dependencies between tasks, ensuring tasks execute only after prerequisites complete.
    *   Executing independent (ready) tasks concurrently using a fixed-size pool of worker threads.
    *   Providing a clear API for task submission (`addTask`) and control (`start`, `stop`).
    *   Guaranteeing thread safety for internal scheduler state using standard C++ synchronization primitives.
    *   Handling task execution failures and propagating failure states to dependents.
    *   Ensuring orderly scheduler shutdown, including joining worker threads.
*   **2.3. Scope and Objectives (Current Implementation):**
    *   Utilize C++11 standard library features exclusively.
    *   Implement a fixed-size worker thread pool model.
    *   Support task dependencies forming a DAG; cycle detection is planned but not yet fully implemented (current rules inherently prevent cycles on single task addition).
    *   Provide basic exception handling for task execution failures within worker threads.
    *   Handle failure propagation recursively through the dependency graph.
    *   Focus on functional correctness and clear C++11 idioms. Performance optimization is deferred.
*   **2.4. Contributions:** A documented, thread-safe, dependency-aware task scheduler implemented in C++11. Serves as a practical demonstration of modern C++ concurrency, memory management, and design patterns applicable to parallel computing problems.

**3. System Design**

*   **3.1. Architecture Overview:** The system is modular, comprising:
    1.  **`Task` Class (`task.h`):** Represents a single unit of work. Encapsulates a unique ID (`TaskID`), the work itself (`std::function`), dependencies (`std::vector<TaskID>`), current execution state (`t_TaskState`), and dependency tracking (`std::atomic<TaskID> unmetCount_`). Designed to be non-copyable and movable.
    2.  **`Scheduler` Class (`scheduler.h`):** The central orchestrator. Manages the collection of all tasks, the queue of ready tasks, the worker thread pool, dependency information, and synchronization primitives. Provides the public API for interaction.
    3.  **`Scheduler::Worker` Class (Nested):** Represents the execution logic for a single worker thread. Each `Worker` instance is associated with a `std::thread` and interacts with the `Scheduler`'s shared resources to fetch and execute tasks.
    4.  **Type Definitions (`types_defines.h`):** Defines shared types (`TaskID`, `t_TaskState`, `t_Verbosity`) and includes a global mutex (`globalMutex`) and verbosity level (`verbosityPrinted`) for a basic thread-safe printing utility (`safe_print`). *Note: Use of global variables is generally discouraged in larger systems.*

    ```
    +---------------------+      addTask()     +-----------------------------+      notify()      +----------------------+
    |      User Code      |------------------->| Scheduler                   |<-------------------| Scheduler::Worker    |
    | (e.g., main.cpp)    |                    | - tasks_ (map<ID, shr_ptr>) |                    | (N instances, each   |
    +---------------------+                    | - readyTasks_ (deque<ID>)   |---- Dequeue ID --->|  owning no state)    |
            | Start/Stop                       | - downwardDependencies_     |                    | - Executes Task::run |
            |                                  | - workers_ (vec<unq_ptr>)   |                    | - Calls notifyDep()  |
            |                                  | - workerThreads_ (vec<thr>) |                    +----------------------+
            +--------------------------------->| - mtx_, workAvailable_      |                       ^ | run() executed by
                                               | - stopRequested_            |                       | V std::thread
                                               +-----------------------------+------------------+--------------------+
                                                            |           ^                       | std::thread        |
                                                            | Update    | Fetch Task*           | (N instances)      |
                                                            V           |                       +--------------------+
                                                        +-----------------+
                                                        | Task Objects    |
                                                        | - state_        |
                                                        | - unmetCount_   |
                                                        | - dependencies_ |
                                                        | - work_         |
                                                        +-----------------+
    ```
    *Figure 1: High-Level System Architecture*

*   **3.2. Data Structures:**
    *   **`Scheduler::tasks_`**: `std::unordered_map<TaskID, std::shared_ptr<Task>>`. Central repository for all submitted tasks, enabling O(1) average lookup. `std::shared_ptr` manages `Task` object lifetime.
    *   **`Scheduler::readyTasks_`**: `std::deque<TaskID>`. FIFO queue holding IDs of tasks whose state is `READY`.
    *   **`Scheduler::downwardDependencies_`**: `std::unordered_map<TaskID, std::vector<TaskID>>`. Stores the reverse dependency graph (TaskID -> list of tasks that depend on it) for efficient notification upon task completion/failure.
    *   **`Scheduler::workers_`**: `std::vector<std::unique_ptr<Worker>>`. Owns the `Worker` logic objects. `std::unique_ptr` ensures automatic cleanup.
    *   **`Scheduler::workerThreads_`**: `std::vector<std::thread>`. Holds the OS thread handles executing the `Worker::run` logic. Separates execution context from worker logic.
    *   **`Task::dependencies_`**: `const std::vector<TaskID>`. Stores the IDs of prerequisite tasks. Immutable after `Task` creation.
    *   **`Task::unmetCount_`**: `std::atomic<TaskID>`. Atomically tracks the number of outstanding dependencies for efficient readiness checks.
    *   **`Task::state_`**: `t_TaskState` (enum). Current lifecycle state of the task.

*   **3.3. Synchronization Primitives:**
    *   **`Scheduler::mtx_`**: `std::mutex`. Primary mutex protecting access to shared scheduler resources: `tasks_`, `readyTasks_`, `downwardDependencies_`, and coordinating state updates.
    *   **`Scheduler::workAvailable_`**: `std::condition_variable`. Used by worker threads to wait efficiently (when `readyTasks_` is empty) for new tasks or the stop signal. Used with `mtx_`.
    *   **`Scheduler::stopRequested_`**: `std::atomic<bool>`. Flag indicating a shutdown request, checked by workers.
    *   **`Task::unmetCount_`**: `std::atomic<TaskID>`. Allows thread-safe decrements when dependencies complete.
    *   **`types_defines.h::globalMutex`**: `std::mutex`. External mutex used by the `safe_print` utility.

*   **3.4. Algorithms:**
    *   **Task Addition (`Scheduler::addTask(func, deps)`):**
        1.  Acquire `lock_guard` on `mtx_`.
        2.  Validate existence of all `dependencies` in `tasks_`. Return error code (-2) if invalid.
        3.  *Placeholder:* Perform cycle detection (currently a stub `check_cycles` returning false). Return error code (-3) if cycle detected.
        4.  Generate unique `currentId` using atomic `id_.fetch_add(1)`.
        5.  Create `Task` object via `new` and wrap in `std::shared_ptr`. Handle potential allocation failure (return -1).
        6.  `emplace` the new task into `tasks_`.
        7.  Iterate `dependencies`: Update `downwardDependencies_` map (`map[dep].push_back(currentId)`). Check state of `dep` in `tasks_`: if `COMPLETED`, decrement `unmetCount_` for `currentId`; if `FAILED`, set `currentId` state directly to `FAILED`.
        8.  Check final `unmetCount_` for `currentId`. If 0, set state to `READY`, push `currentId` to `readyTasks_`, and `notify_one` on `workAvailable_`. Otherwise, ensure state is `PENDING`.
        9.  Release lock (automatic). Return `currentId`.
    *   **Worker Execution Loop (`Worker::run`):**
        1.  Enter infinite `while(true)` loop.
        2.  Acquire `std::unique_lock` on `scheduler_.mtx_`.
        3.  Wait on `scheduler_.workAvailable_` using a predicate lambda checking `!scheduler_.readyTasks_.empty() || scheduler_.stopRequested_.load()`. Capture `stopReq` flag state within predicate.
        4.  Check `stopReq` flag *after* wait returns. If true, unlock and `break` loop (Policy 2 shutdown).
        5.  Dequeue `currTaskID` from `scheduler_.readyTasks_`.
        6.  Lookup `task_ptr` (`shared_ptr<Task>`) in `scheduler_.tasks_` using `find()`. Handle "not found" error (unlock, log, `continue`).
        7.  Set `task_ptr->setState(RUNNING)`.
        9.  Unlock `lck`.
        10. Execute `task_ptr->run()` within a `try/catch(...)` block, tracking success/failure.
        11. Re-acquire lock (`lck.lock()`).
        12. Set final task state (`COMPLETED` or `FAILED`) via `task_ptr->setState()`.
        13. Call `scheduler_.notifyDependents(currTaskID)` (requires lock).
        14. Unlock `lck`.
    *   **Dependency Notification (`Scheduler::notifyDependents` - Private):**
        1.  *Requires caller (Worker::run) to hold the lock on `mtx_`.*
        2.  Find originating `taskId` in `tasks_`. Handle "not found".
        3.  Check originating task's state (`FAILED` or `COMPLETED`).
        4.  If `FAILED`: Iterate through `downwardDependencies_` for `taskId`. For each dependent, set state to `FAILED` and recursively call `notifyDependents`. *Note: Recursive call risks stack overflow; iterative queue/stack approach is safer.*
        5.  If `COMPLETED`: Iterate through `downwardDependencies_`. For each dependent: Find it in `tasks_`. If `PENDING`, call `task->decrement_unmet_dependencies()`. If count reaches zero, set state to `READY`, push ID to `readyTasks_`, and `notify_one` on `workAvailable_`. Handle/log cases where dependent is not `PENDING`.
    *   **Scheduler Start (`Scheduler::start`):**
        1.  Reserve space in `workerThreads_`.
        2.  Loop `0` to `threadNumber_ - 1`.
        3.  `emplace_back` a new `std::thread` targeting `&Scheduler::Worker::run` on the corresponding `workers_[i].get()` pointer. Handle potential thread creation exceptions.
    *   **Scheduler Shutdown (`Scheduler::stop`):**
        1.  Acquire `std::unique_lock` on `mtx_`.
        2.  Set `stopRequested_ = true`.
        3.  Iterate `readyTasks_`, marking corresponding tasks in `tasks_` as `CANCELLED` (or `FAILED`).
        4.  `readyTasks_.clear()`.
        5.  `workAvailable_.notify_all()`.
        6.  Unlock `lck`.
        7.  Iterate `workerThreads_`. For each joinable thread, call `thread.join()`. *Remove `threadBusy_` check.*

**4. Implementation Details**

*   **Language Standard:** C++11.
*   **Compiler:** Conforming C++11 compiler required.
*   **Dependencies:** Standard C++ Library. External headers: `types_defines.h`, `task.h`, `scheduler.h`.
*   **Key C++11 Features:** `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`, `std::function`, `std::unique_ptr`, `std::shared_ptr`, `std::unordered_map`, `std::deque`, lambdas, `nullptr`, range-based for loops, `noexcept`, `<chrono>`.
*   **RAII:** `Scheduler` destructor calls `stop()` ensuring thread joining. `unique_ptr` and `shared_ptr` manage memory. `lock_guard`/`unique_lock` manage mutexes.
*   **Exception Safety:** Task execution failures are caught in `Worker::run`, task state set to `FAILED`. Thread creation errors handled in `start`. Basic robustness provided.
*   **Logging:** Basic thread-unsafe `std::cout` used. A thread-safe logger (`safe_print` using `globalMutex`) is provided but needs consistent use.

**5. Evaluation and Testing**

*   **Current State:** Basic testing via `main` scenarios verifies core functionality (concurrency, simple dependencies, failure propagation).
*   **Needs:** Formal unit tests (e.g., Google Test) for `Task` state transitions, `Scheduler::addTask` logic (especially dependency/failure pre-resolution), `Scheduler::notifyDependents`, and `Worker` logic segments. Integration tests for complex dependency graphs and concurrency scenarios. Stress tests for stability.
*   **Correctness Criteria:** Dependency enforcement, all tasks reach terminal state (in non-cyclic graphs), absence of data races (requires analysis/tools), graceful shutdown.

**6. Assumptions and Limitations**

*   Task dependencies must form a DAG *(cycle detection planned)*.
*   Tasks are non-preemptive.
*   Fixed-size thread pool.
*   Dynamic memory allocation (`new`, smart pointers, containers) is acceptable.
*   Failure propagation is currently recursive, risking stack overflow on deep graphs.
*   `Scheduler::addTask(Task&&)` overload has potential ID management issues.
*   Reliance on external `safe_print` utility with global state (`globalMutex`, `verbosityPrinted`).

**7. Future Work**

*   **Core Functionality:**
    *   Implement `Scheduler::wait()` method using counters and dedicated conditional variable.
    *   Implement robust cycle detection (e.g., DFS-based) within `addTask`.
    *   Implement iterative (queue/stack) failure propagation in `notifyDependents`.
    *   Implement cooperative task cancellation (e.g., `Task::cancel_requested_` flag checked within task work).
    *   Implement task priorities (requires changes to `readyTasks_` - e.g., `std::priority_queue` - and potentially worker fetching logic).
    *   Allow tasks to return results (e.g., using `std::packaged_task` and `std::future`).
*   **API & Usability:**
    *   Implement variadic template `addTask` (or vector addition) for batch submission (requires careful handling of inter-dependencies within the batch and cycle detection).
    *   Add methods to query task status (`getTaskState(TaskID)`).
    *   Add methods to query task results (if implemented).
    *   Reports states of all tasks at completion
    *   Improve error reporting (return codes, exceptions).
*   **Performance & Scalability:**
    *   Investigate lock contention on `mtx_`. Explore finer-grained locking strategies (more complex).
    *   Implement work-stealing queues for potentially better load balancing among workers (advanced).
    *   Optimize data structures if needed.
*   **Robustness & Refinement:**
    *   Implement a proper thread-safe logging framework (replace `safe_print` and `std::cout`).
    *   Remove global variables (`globalMutex`, `verbosityPrinted`) by passing logger instances.
    *   Validate `Task` state transitions more formally (`Task::setState`).
    *   Refine or remove `addTask(Task&&)` overload. (Or find a way to incorporate it)
    *   Consider using C++14/17/20 features where appropriate (`make_unique`, `make_shared`, `jthread` for cancellation).

**8. Conclusion**

This document outlines a C++11 concurrent task scheduler featuring dependency management (including failure propagation) and execution via a fixed-size thread pool. The design utilizes standard C++11 features for concurrency, memory management, and task representation. The current implementation provides core functionality suitable for basic testing, with clearly identified areas for future enhancement in terms of robustness, features, and performance.
