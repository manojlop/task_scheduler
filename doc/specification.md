# Project Specification: A C++11 Implementation of a Concurrent, Dependency-Aware Task Scheduler

## 1. Abstract (v0.0.0)

This document specifies the design and implementation of a concurrent task scheduling system utilizing C++11 standard library features. The system manages a collection of tasks, potentially with interdependencies forming a Directed Acyclic Graph (DAG), and executes them using a pool of worker threads. Key objectives include achieving correct concurrent execution respecting all specified dependencies, demonstrating the application of C++11 concurrency primitives (`std::thread`, `std::mutex`, `std::condition_variable`), modern C++ idioms (`std::function`, lambdas, smart pointers), and providing a foundation for future extensions. The design emphasizes clarity, correctness, and adherence to C++11 standards. Evaluation focuses on correctness through defined test scenarios.

## 2. Introduction

*   **2.1. Motivation:** The proliferation of multi-core processors necessitates efficient parallel execution models. Many computational problems can be decomposed into smaller tasks, some of which may depend on the results of others. Task scheduling systems provide a framework for managing such task sets and orchestrating their execution on available processing resources, maximizing throughput and resource utilization while respecting execution constraints.
*   **2.2. Problem Statement:** Design and implement a task scheduler capable of:
    *   Accepting computational tasks represented as C++ callable entities.
    *   Managing explicit dependencies between tasks, ensuring a task only executes after its prerequisite tasks have completed successfully.
    *   Executing independent tasks concurrently using a configurable pool of worker threads.
    *   Providing mechanisms for adding tasks, initiating execution, and ensuring orderly shutdown.
    *   Guaranteeing thread safety and avoiding common concurrency pitfalls like race conditions and deadlocks within the scheduler's internal state.
*   **2.3. Scope and Objectives:**
    *   Implement the scheduler purely using C++11 standard library features.
    *   Support a fixed-size thread pool model.
    *   Handle task dependencies forming a DAG. Cycle detection is outside the initial scope but dependencies implying cycles will lead to tasks never becoming ready.
    *   Focus on correctness of execution order and basic concurrency. Performance optimization (e.g., minimizing lock contention, work-stealing) is a secondary goal or future work.
    *   Implement basic exception handling for task execution failures.
*   **2.4. Contributions:** The primary contribution is a well-documented, C++11-idiomatic implementation of a dependency-aware concurrent scheduler, serving as both a practical utility and an educational example of applying modern C++ concurrency features.

## 3. System Design

*   **3.1. Architecture Overview:** The system comprises three main components:
    1.  **Task Representation:** Data structures holding task information, including the work function, state, and dependency relationships.
    2.  **Scheduler Core:** Manages the task collection, the ready queue, worker threads, and synchronization primitives. It orchestrates the task lifecycle.
    3.  **Worker Threads:** Execute tasks dequeued from the ready queue.

    ```
    +---------------------+      Add Task      +-----------------+      Notify      +----------------+
    |      User Code      |------------------->| Scheduler Core  |<-----------------| Worker Threads |
    +---------------------+                    | - Task Map      |                  | (Pool of N)    |
            | Start/Stop                       | - Ready Queue   |----- Dequeue --->| - Execute Task |
            |                                  | - Thread Pool   | Task             | - Update State |
            |                                  | - Sync Prims    |                  +----------------+
            +--------------------------------->|                 |
                                               +-----------------+
                                                  |            ^
                                                  | Update     | Notify
                                                  | Task State | Dependents
                                                  V            |
                                               +-----------------+
                                               | Task Objects    |
                                               | - State         |
                                               | - Dependencies  |
                                               | - Unmet Count   |
                                               +-----------------+
    ```
    *Figure 1: High-Level System Architecture*

*   **3.2. Data Structures:**
    *   **`TaskID`**: A type alias for `std::size_t`, representing unique task identifiers.
    *   **`TaskState`**: An `enum class TaskState { PENDING, READY, RUNNING, COMPLETED, FAILED };` defining the possible states of a task.
    *   **`Task`**: A `struct` or `class` encapsulating task details:
        *   `const TaskID id`: Unique identifier.
        *   `const std::function<void()> work`: The callable entity representing the task's work. Stored by value, assumes tasks are lightweight enough to copy/move `std::function`.
        *   `TaskState state`: The current lifecycle state. Initialized to `PENDING` or `READY`.
        *   `const std::vector<TaskID> dependencies`: A list of `TaskID`s this task depends upon. Immutable after creation.
        *   `std::vector<TaskID> dependents`: A list of `TaskID`s that depend on this task. Populated when tasks are added. Mutable (only additions).
        *   `std::atomic<std::size_t> unmet_dependency_count`: Initialized to `dependencies.size()`. Atomically decremented when a dependency completes. Task becomes `READY` when this reaches zero.
        *   `std::mutex task_mutex_`: (Considered, but deferred for initial scope). A mutex specific to this task instance for fine-grained locking if needed in future extensions. The initial design relies on coarser locking in the `Scheduler`.
    *   **`Scheduler::tasks_`**: `std::unordered_map<TaskID, std::shared_ptr<Task>>`. Stores all tasks managed by the scheduler, allowing O(1) average time lookup by `TaskID`. `std::shared_ptr` is used to manage the lifetime of `Task` objects, allowing them to be referenced from the map, the ready queue (implicitly), and potentially by dependent tasks without manual lifetime tracking.
    *   **`Scheduler::ready_queue_`**: `std::deque<TaskID>`. A double-ended queue holding the `TaskID`s of tasks that are in the `READY` state. `std::deque` provides efficient push_back and pop_front operations.

*   **3.3. Synchronization Primitives:**
    *   **`Scheduler::queue_mutex_`**: `std::mutex`. Protects access to the `ready_queue_` and coordinates updates to `Task::state` when transitioning tasks between `PENDING`, `READY`, and `RUNNING`. This coarse-grained lock simplifies the initial implementation but may become a contention point under high load.
    *   **`Scheduler::condition_`**: `std::condition_variable`. Used in conjunction with `queue_mutex_`. Worker threads wait on this condition variable when the `ready_queue_` is empty. It is notified when a new task becomes `READY` or when the scheduler is shutting down.
    *   **`Scheduler::stop_requested_`**: `std::atomic<bool>`. A flag indicating that the scheduler is shutting down. Checked by worker threads to determine when to exit their loop. `std::atomic` ensures visibility and atomicity without requiring explicit locking for reads/writes to this flag.
    *   **`Task::unmet_dependency_count`**: `std::atomic<std::size_t>`. Used to track remaining dependencies atomically, allowing multiple dependencies to complete concurrently without race conditions on the counter itself.

*   **3.4. Algorithms:**
    *   **Task Addition (`Scheduler::add_task`)**:
        1.  Acquire lock on `queue_mutex_` (to protect `tasks_` map and potentially `ready_queue_`).
        2.  Generate a new unique `TaskID` (e.g., incrementing `next_task_id_`).
        3.  Create a `Task` object, initializing `id`, `work`, `dependencies`.
        4.  Initialize `unmet_dependency_count` to `dependencies.size()`.
        5.  Initialize `state` to `PENDING`.
        6.  Add the new `Task` (wrapped in `shared_ptr`) to the `tasks_` map.
        7.  Iterate through the `dependencies` list: For each `dep_id`, retrieve the corresponding dependency `Task` object from `tasks_` and add the new task's `id` to its `dependents` list. Handle potential invalid `dep_id` (e.g., throw exception or return error).
        8.  If `unmet_dependency_count` is 0, change `state` to `READY` and push `id` onto `ready_queue_`.
        9.  Release lock on `queue_mutex_`.
        10. If a task was added to `ready_queue_`, notify one worker thread via `condition_.notify_one()`.
        11. Return the new `TaskID`.
    *   **Worker Thread Loop (`Scheduler::worker_loop`)**:
        1.  Loop indefinitely (`while (true)`).
        2.  Acquire `std::unique_lock<std::mutex> lock(queue_mutex_)`.
        3.  Wait on `condition_` while the `ready_queue_` is empty AND `stop_requested_` is false: `condition_.wait(lock, [this]{ return !ready_queue_.empty() || stop_requested_; });`.
        4.  Check if stop was requested *after* waking up: `if (stop_requested_ && ready_queue_.empty()) { return; // Exit loop }`.
        5.  Dequeue a `TaskID` from `ready_queue_`.
        6.  Retrieve the corresponding `std::shared_ptr<Task> task_ptr` from `tasks_`.
        7.  Set `task_ptr->state = TaskState::RUNNING`.
        8.  Release the lock: `lock.unlock()`. (Release *before* executing potentially long-running task work).
        9.  Execute the task's work function:
            ```cpp
            try {
                task_ptr->work();
                // Task completed successfully
                // Re-acquire lock to update state and notify dependents
                lock.lock(); // Re-acquire the same lock
                task_ptr->state = TaskState::COMPLETED;
                notify_dependents(task_ptr->id); // Pass completed ID
            } catch (const std::exception& e) {
                // Handle task execution failure
                // Log error: std::cerr << "Task " << task_ptr->id << " failed: " << e.what() << std::endl;
                lock.lock(); // Re-acquire the same lock
                task_ptr->state = TaskState::FAILED;
                // Optionally: Propagate failure state to dependents? (Outside initial scope)
            } catch (...) {
                // Handle non-standard exceptions
                // Log error: std::cerr << "Task " << task_ptr->id << " failed with unknown exception." << std::endl;
                lock.lock(); // Re-acquire the same lock
                task_ptr->state = TaskState::FAILED;
            }
            ```
        10. The lock is implicitly released when `lock` goes out of scope at the end of the loop iteration (or explicitly if needed before looping).
    *   **Dependency Notification (`Scheduler::notify_dependents`, called with `queue_mutex_` held)**:
        1.  Retrieve the completed task (`completed_task_ptr`) from `tasks_` using `completed_task_id`.
        2.  Iterate through the `dependents` list of `completed_task_ptr`.
        3.  For each `dependent_id`:
            a.  Retrieve the `dependent_task_ptr` from `tasks_`.
            b.  Atomically decrement `dependent_task_ptr->unmet_dependency_count`.
            c.  If the count becomes 0:
                i.  Assert `dependent_task_ptr->state == TaskState::PENDING`.
                ii. Set `dependent_task_ptr->state = TaskState::READY`.
                iii. Push `dependent_id` onto `ready_queue_`.
                iv. Notify one waiting worker thread: `condition_.notify_one()`.
    *   **Scheduler Shutdown (`Scheduler::stop`)**:
        1.  Set `stop_requested_ = true`.
        2.  Notify all waiting worker threads: `condition_.notify_all()`.
        3.  Join all threads in `worker_threads_`. Ensure this happens reliably (e.g., in the destructor using RAII).

## 4. Implementation Details

*   **Language Standard:** C++11.
*   **Compiler:** Conforming C++11 compiler (e.g., GCC 4.8+, Clang 3.3+, MSVC 2015+).
*   **Dependencies:** Standard C++ Library only.
*   **Key C++11 Features Utilized:** `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`, `std::function`, `std::shared_ptr`, `std::unique_lock`, `std::move`, lambdas, `enum class`, `auto`, range-based `for`, `<chrono>` for potential sleeps.
*   **RAII:** The `Scheduler` destructor must implement Resource Acquisition Is Initialization to ensure `stop()` is called and threads are joined, preventing resource leaks and program termination issues. Consider a helper class for managing the `std::vector<std::thread>` lifecycle.
*   **Exception Safety:** The `worker_loop` must catch exceptions originating from task execution to prevent thread termination. Task state should be updated to `FAILED`. Mutexes must be correctly unlocked during stack unwinding (`std::unique_lock` and `std::lock_guard` facilitate this).

## 5. Evaluation and Testing

*   **5.1. Correctness Criteria:**
    *   All tasks added to the scheduler eventually reach a terminal state (`COMPLETED` or `FAILED`), assuming no cyclic dependencies prevent readiness.
    *   Dependency constraints are strictly enforced: A task never enters the `RUNNING` state before all its dependencies are in the `COMPLETED` state.
    *   Tasks without dependencies become `READY` immediately upon addition.
    *   The scheduler shuts down cleanly, joining all worker threads.
*   **5.2. Showcase Scenario (Verification):**
    *   Implement the scenario described in the previous specification (Tasks A, B, C, D, E with dependencies A->C, {A, B}->D, C->E).
    *   Utilize thread-safe logging (e.g., locking `std::cout` or using a dedicated logging queue) to record task start and finish times/events.
    *   Run with N=1, N=2, N=4 threads.
    *   Verify output logs manually or via script to confirm execution order respects dependencies and demonstrates concurrency (e.g., A and B running simultaneously when N >= 2).
*   **5.3. Automated Testing Strategy (using e.g., Google Test):**
    *   **Basic Tests:** `AddTask`, `StartStopEmpty`, `StartStopWithTasks`.
    *   **Dependency Tests:**
        *   `LinearDependency`: A->B->C. Verify sequential execution even with multiple threads.
        *   `ForkDependency`: A->B, A->C. Verify B and C run concurrently after A (N>=2).
        *   `JoinDependency`: A->C, B->C. Verify C runs only after both A and B complete.
        *   `ComplexDAG`: Combine fork/join patterns.
    *   **Concurrency Tests:** Add M independent tasks, run with N threads (N < M). Use atomic counters or promises/futures within tasks to verify all M complete. Measure approximate makespan (total time) and compare N=1 vs N>1.
    *   **Exception Handling Test:** Add a task designed to throw an exception. Verify its state becomes `FAILED`, other tasks are unaffected, and the scheduler remains operational.
    *   **Stress Test:** Add a large number of tasks with random dependencies (ensuring DAG structure). Run with multiple threads and verify completion and shutdown.
*   **5.4. Performance Metrics (Informal):** While not a primary goal, observe approximate makespan in concurrency tests as an indicator of parallel execution benefit. Note potential bottlenecks (e.g., `queue_mutex_`).

## 6. Assumptions and Limitations

*   Task dependencies form a DAG. Cyclic dependencies will result in tasks never becoming ready.
*   Tasks are non-preemptive once started by a worker thread.
*   The number of worker threads is fixed after calling `start()`.
*   `std::function` overhead is acceptable; task work functions are assumed movable/copyable.
*   Dynamic memory allocation (via `shared_ptr`, `unordered_map`, `deque`, `function`) is acceptable.
*   Basic exception safety is provided, but sophisticated error recovery or propagation is not implemented.

## 7. Future Work

*   Implement task priorities.
*   Implement task cancellation.
*   Allow tasks to return results (`std::future`/`std::promise`).
*   Optimize for performance (finer-grained locking, work-stealing queues).
*   Add cycle detection during task addition.
*   Explore C++17/20/23 features (`std::jthread`, parallel algorithms, coroutines, `std::format`, `std::expected`).
*   Adapt design for resource-constrained environments (static allocation, RTOS integration).

## 8. Conclusion

This specification details a robust design for a concurrent, dependency-aware task scheduler implemented using C++11 standard features. The design prioritizes correctness and clear application of modern C++ concurrency techniques. Through careful implementation and rigorous testing based on the outlined scenarios, the resulting system will serve as a functional scheduler and a valuable demonstration of C++11 capabilities in concurrent programming.