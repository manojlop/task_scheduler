# Scheduler documentation

## 1. Introduction

### 1.1. Project Purpose

This document describes a C++ implementation of a task scheduler designed to manage and execute tasks with dependencies in a multi-threaded environment. The primary goal of this project is to provide a flexible and efficient way to run a collection of potentially interdependent computational units concurrently, maximizing resource utilization while respecting execution order constraints.\
_In this iteration of the project, executed tasks are dependent on execution of previously added tasks, but they can not utilize their results explicitly, they can just be dependent on them_

### 1.2. Key Features

*   **Dependency Management:** Allows defining tasks that depend on the successful completion of other tasks.
*   **Multi-threaded Execution:** Utilizes a pool of worker threads to execute ready tasks concurrently.
*   **Task Lifecycle Management:** Tracks the state of each task (Pending, Ready, Running, Completed, Failed, Cancelled).
*   **Failure Propagation:** Automatically marks dependent tasks as failed if a prerequisite task fails.
*   **Basic Cycle Detection:** Includes a mechanism to prevent the addition of tasks that would create circular dependencies (_currently limited to dependencies on existing tasks_. _Adding new tasks that are dependent only on previously added tasks cannot create cycles, so the method for this is placeholder for future use_).
*   **Synchronization:** Employs mutexes, condition variables, and atomic operations for thread-safe task management and execution.
*   **Configurable Verbosity:** Supports different levels of logging output for debugging and monitoring.
*   **Helper Scripts:** Provides Python scripts to simplify the build and execution workflow.

### 1.3. Intended Audience

This documentation is primarily intended for:

*   **Software Developers:** Who want to use this scheduler library in their own C++ applications.
*   **Contributors:** Who aim to understand the internal workings of the scheduler for maintenance, debugging, or extending its functionality.

A good understanding of C++ (including C++11/14 features like lambdas, smart pointers, standard library containers, threading primitives) and general concurrency concepts is assumed. Familiarity with CMake is beneficial for building the project.

### 1.4. Document Scope

This document covers:

*   Core concepts and design principles of the scheduler.
*   Instructions for building and running the project and its associated tests.
*   A detailed reference for the public API of the `Scheduler` and `Task` classes.
*   An overview of the internal implementation details, including concurrency mechanisms and data structures.
*   The testing strategy employed, covering both manual and automated tests.
*   Details about the build system (CMake) and utility scripts.
*   Known assumptions, limitations, and potential areas for future development.

This document does *not* cover:

*   Basic C++ programming concepts.
*   Detailed explanations of standard library components (like `std::vector`, `std::mutex`, etc.) unless specific usage context is relevant.
*   In-depth operating system threading details beyond the standard C++ library abstractions used.

## 2. Core Concepts

### 2.1. Task

A `Task` represents a unit of work to be executed by the scheduler. Key characteristics include:

*   **Task ID (`TaskID`):** A unique identifier (`std::size_t`) assigned by the `Scheduler` upon creation.
*   **Work Function (`std::function<void()>`):** The actual code to be executed. This is typically a lambda expression or function pointer.
*   **Dependencies (`std::vector<TaskID>`):** A list of other `TaskID`s that must be completed before this task can run.
*   **State (`t_TaskState`):** The current stage of the task in its lifecycle (see [2.4. Task States](#24-task-states-t_taskstate)).
*   **Unmet Dependency Count (`std::atomic<TaskID>`):** An internal counter tracking how many direct dependencies are not yet completed. When this count reaches zero, the task transitions to the `READY` state (if not already failed or cancelled).

Tasks are managed by the `Scheduler` and are not intended to be created or manipulated directly by the user outside the `Scheduler::addTask` method.

### 2.2. Scheduler

The `Scheduler` is the central component responsible for managing the entire task execution process. Its roles include:

*   **Task Registry:** Stores all added tasks, typically using their `TaskID` as a key.
*   **Dependency Tracking:** Maintains the relationship between tasks, understanding which tasks depend on others.
*   **Worker Thread Pool:** Creates and manages a pool of worker threads (`Scheduler::Worker`) that execute tasks.
*   **Ready Queue:** Holds tasks that have met their dependencies and are ready to be run by a worker thread.
*   **State Management:** Transitions tasks through their lifecycle states based on dependency completion, execution start/finish, and failures.
*   **Synchronization:** Coordinates access to shared resources (task map, ready queue) among worker threads and the main thread adding tasks.

### 2.3. Dependencies

Dependencies define the execution order constraints between tasks. A task cannot enter the `READY` state until all the tasks listed in its dependency list have reached the `COMPLETED` state.

*   **Definition:** Dependencies are specified as a vector of `TaskID`s when adding a new task via `Scheduler::addTask`.
*   **Constraint:** A task can only depend on tasks that have already been added to the scheduler. This simplifies initial cycle detection.
*   **Tracking:** The scheduler uses an internal `downwardDependencies_` map to quickly find tasks that depend *on* a given task (needed for notification upon completion) and the `unmetCount_` within each `Task` object to track *its own* remaining dependencies.
*   **Failure Propagation:** If a task fails (`FAILED` state), the scheduler automatically propagates this failure to all tasks that directly or indirectly depend on it.

### 2.4. Task States (`t_TaskState`)

Each task progresses through a defined set of states during its lifetime. The `t_TaskState` enum defines these states:

```cpp
enum class t_TaskState {
    PENDING,    // Task added but dependencies are not yet met.
    READY,      // Dependencies met, waiting for a worker thread to execute.
    RUNNING,    // A worker thread is currently executing the task's work function.
    COMPLETED,  // The work function executed successfully without exceptions.
    FAILED,     // The work function threw an exception, or a dependency failed.
    CANCELLED   // The task was explicitly cancelled (e.g., during scheduler shutdown) before completion.
};
```

State transitions are managed internally by the `Scheduler` and the `Task`'s `setState` methods, with validation checks to ensure logical progression (e.g., a `COMPLETED` task cannot become `PENDING`).

### 2.5. Concurrency Model

The scheduler employs multiple threads to execute tasks concurrently:

*   **Worker Threads:** A fixed number of `Scheduler::Worker` instances run on separate `std::thread`s. Each worker attempts to fetch and execute tasks from the `Scheduler`'s ready queue.
*   **Main Thread:** The thread that creates the `Scheduler` instance and calls its methods (e.g., `addTask`, `start`, `stop`).
*   **Synchronization Primitives:**
    *   `std::mutex (Scheduler::mtx_)`: A primary mutex protecting shared scheduler resources like `tasks_`, `readyTasks_`, and `downwardDependencies_`. Most operations modifying the scheduler's state require acquiring this lock.
    *   `std::condition_variable (Scheduler::workAvailable_)`: Used by worker threads to wait efficiently when the `readyTasks_` queue is empty. It's notified when a new task becomes ready or when the scheduler is stopping.
    *   `std::condition_variable (Scheduler::allTasksFinished_)`: Used by `waitTasksToEnd` to wait until all added tasks have reached a terminal state (COMPLETED, FAILED, CANCELLED).
    *   `std::atomic`: Used for thread-safe counters and flags where full mutex protection is not required or desired (e.g., `Task::unmetCount_`, `Scheduler::id_`, `Scheduler::stopRequested_`, state counters like `completedTasks_`).
    *   `std::mutex (globalMutex)`: A global mutex used by the `safe_print` function for thread-safe console output. *([See Future Considerations](#10-future-considerations--todos) for potential removal)*.

### 2.6. Verbosity Levels (`t_Verbosity`)

The `safe_print` logging utility uses verbosity levels to control the amount of output generated.

```cpp
enum class t_Verbosity {
    NONE,    // No output.
    ERROR,   // Only critical errors.
    WARNING, // Errors and warnings.
    INFO,    // Errors, warnings, and informational messages (default).
    DEBUG    // Maximum output, including detailed step-by-step information.
};
```

The global variable `verbosityPrinted` determines the maximum level of messages that will be printed.

## 3. Getting Started

### 3.1. Prerequisites

*   **C++ Compiler:** A compiler supporting C++14 or later (required by GoogleTest and used project features). GCC or Clang are recommended.
*   **CMake:** Version 3.10 or later.
*   **Python:** Version 3.x (for the helper scripts).
*   **Git:** (Optional, if cloning the repository).
*   **Internet Connection:** Required during the first CMake configuration step to download GoogleTest via `FetchContent`.

### 3.2. Building the Project

The project uses CMake for building. You can build using CMake directly or use the provided Python helper scripts.

#### 3.2.1. Standard Build (Using CMake directly)

```bash
# 1. Create a build directory (recommended)
mkdir build
cd build

# 2. Configure the project (downloads GoogleTest on first run)
#    Replace <path_to_source_directory> with the actual path
cmake <path_to_source_directory>

# 3. Build the project targets (main executable, tests)
cmake --build .

# Binaries will be located in the 'build/bin' directory.
# Libraries (if any) will be in 'build/lib'.
```

#### 3.2.2. Using Compile Helper Script (`compile.py`)

The `compile.py` script automates the CMake configuration and build steps. It resides in the project's root directory (or a `scripts` subdirectory if moved).

```bash
# Basic build (equivalent to standard CMake build)
./compile.py  # Or: python3 compile.py

# Build with specific definitions (e.g., for manual tests)
./compile.py -d TEST_SANITY # Enables the sanity test
./compile.py -d TEST_STRESS -d ANOTHER_DEFINE # Multiple defines

# Build in Debug mode (sets CMAKE_BUILD_TYPE=Debug)
./compile.py -dbg

# See ./compile.py --help for all options.
```

This script creates the `build` directory if it doesn't exist and runs the necessary CMake commands.

#### 3.2.3. Build Options (CMake options)

CMake options can be passed during the configuration step:

*   **`-DCMAKE_BUILD_TYPE=Debug/Release`:** Standard CMake option to control optimization levels and debug symbols (`compile.py -dbg` uses `Debug`).
*   **`-DQ=ON -DQUICK=<filename.cpp>`:** (Used by `compile.py -q -t <filename>`) Enables quick compilation mode for a specific file located in the `spike/` directory (if it exists). This builds a separate `spike` executable linking only the specified file and necessary scheduler sources. Useful for rapid prototyping or testing isolated pieces of code.
*   **`-DEXTRA_FLAGS="-DSOME_DEFINE -DANOTHER_DEFINE"`:** (Used by `compile.py -d`) Passes preprocessor definitions to the compiler. The `compile.py` script simplifies adding definitions like `TEST_SANITY`, `TEST_STRESS`, etc.
*   **`-DENABLE_FEATURE=ON`:** Example of a custom CMake option to conditionally compile feature code (if implemented using `#ifdef FEATURE_ENABLED`).

### 3.3. Running

#### 3.3.1. Running the Main Executable

If the project includes a `main.cpp` defining a primary executable (target name `main`), you can run it after building:

```bash
# Assuming you are in the build directory
./bin/main

# Or using the run.py script from the project root
./run.py # Runs 'main' by default
./run.py -t main # Explicitly run 'main'
```

#### 3.3.2. Running Manual Tests

The manual tests (`test_sanity.cpp`, `test_failure_propagated.cpp`, `test_stress.cpp`) are compiled into the `main` executable when their corresponding preprocessor flag is defined during the build.

1.  **Build with the Test Flag:** Use `compile.py` with the `-d` option:
    ```bash
    # Example: Build for the sanity test
    ./compile.py -d TEST_SANITY

    # Example: Build for the stress test
    ./compile.py -d TEST_STRESS
    ```

2.  **Run the Main Executable:** The `main` executable will now run the enabled test code.
    ```bash
    ./run.py
    ```

Only one manual test can be enabled at a time via these defines.

#### 3.3.3. Running Unit/Integration Tests (GoogleTest)

The GoogleTest suite (`MyTests` executable) contains automated unit and integration tests.

1.  **Build:** Ensure the tests were built (they are built by default with the standard build or `compile.py`).
2.  **Run:**
    *   **Using `run.py`:**
        ```bash
        ./run.py -gt # Or ./run.py --googleTest
        ```
    *   **Using CTest (from the `build` directory):**
        ```bash
        ctest
        # Or for more verbose output:
        ctest -V
        ```
    *   **Directly executing:**
        ```bash
        # Assuming you are in the build directory
        ./bin/MyTests
        # Pass GoogleTest arguments if needed, e.g., to run specific tests:
        ./bin/MyTests --gtest_filter=SchedulerTest_AddTask*.*
        ```

#### 3.3.4. Using the Combined Script (`all.py`)

The `all.py` script streamlines the process of compiling and then immediately running a target. It accepts arguments for both `compile.py` and `run.py`.

```bash
# Clean, build (debug), and run Google Tests
./all.py -c -dbg -gt

# Build with sanity test defined and run it
./all.py -d TEST_SANITY

# Build and run the 'main' target (default)
./all.py

# See ./all.py --help for details.
```

## 4. API Reference

This section details the public interface of the scheduler components.

### 4.1. Global Types and Defines (`types_defines.h`)

This header provides common types, enums, and utility functions used throughout the scheduler project.

#### 4.1.1. Enums

*   **`enum class t_TaskState`**
    *   Defines the possible states a `Task` can be in.
    *   Values: `PENDING`, `READY`, `RUNNING`, `COMPLETED`, `FAILED`, `CANCELLED`
    *   See Section 2.4 for detailed descriptions.

*   **`enum class t_Verbosity`**
    *   Defines logging verbosity levels used by `safe_print`.
    *   Values: `NONE`, `ERROR`, `WARNING`, `INFO`, `DEBUG`
    *   See Section 2.6 for detailed descriptions.

#### 4.1.2. Typedefs

*   **`using TaskID = std::size_t;`**
    *   Defines the type used for unique task identifiers.

#### 4.1.3. Global Variables

*   **`extern std::mutex globalMutex;`**
    *   A global mutex intended for synchronizing access, currently used by `safe_print`.
    *   *Note: Use of global variables, especially mutexes, can be problematic. See [Future Considerations](#10-future-considerations--todos).*

*   **`extern t_Verbosity verbosityPrinted;`**
    *   Controls the maximum level of messages printed by `safe_print`. Defaults typically to `INFO`.
    *   *Note: Global state variable. See [Future Considerations](#10-future-considerations--todos).*

#### 4.1.4. Helper Functions

*   **`void safe_print(std::string msg, std::string name = "General", t_Verbosity verbosity = t_Verbosity::INFO);`**
    *   Prints a formatted message to standard output in a thread-safe manner using `globalMutex`.
    *   Messages are only printed if their `verbosity` level is less than or equal to the current `verbosityPrinted` level.
    *   Includes timestamp, verbosity level, source name, and the message content.
    *   **Parameters:**
        *   `msg`: The message string to print.
        *   `name`: Identifier for the source of the message (e.g., "Scheduler", "Worker: 1"). Defaults to "General".
        *   `verbosity`: The severity level of the message. Defaults to `INFO`.
    *   **Implementation:** Found in `src/helpers.cpp`.

### 4.2. `Task` Class (`task.h`)

Represents a single unit of work with dependencies and state. Tasks are primarily managed by the `Scheduler`.

#### 4.2.1. Overview

Tasks encapsulate a work function and its dependencies. They maintain their execution state and an internal count of unmet dependencies. `Task` objects are non-copyable but movable.

#### 4.2.2. Member Variables (Private)

*   `const TaskID id_`: Unique identifier assigned by the Scheduler.
*   `t_TaskState state_`: The current state of the task.
*   `const std::function<void()> work_`: The callable object representing the task's work.
*   `const std::vector<TaskID> dependencies_`: List of IDs of tasks this task depends on.
*   `std::atomic<TaskID> unmetCount_`: Number of dependencies not yet completed.
*   `std::string description_`: Optional description (currently initialized but not heavily used).
*   `static const char* taskStateName[]`: Array to map `t_TaskState` enum values to strings (used internally for logging).

#### 4.2.3. Constructors & Destructor

*   **`Task(int identifier = 0, std::function<void()> func = {}, const std::vector<TaskID>& dep = std::vector<TaskID>(), std::string descr = "");`**
    *   The primary constructor used internally by `Scheduler::addTask`.
    *   Initializes the task with its ID, work function, dependencies, and an optional description.
    *   Sets the initial state to `READY` if `dep` is empty, otherwise `PENDING`. Initializes `unmetCount_` based on the size of `dep`.
*   **`Task() = delete;`**: Default constructor is deleted.
*   **`Task(const Task&) = delete;`**: Copy constructor is deleted. Tasks should not be copied.
*   **`Task(Task&& task) noexcept;`**: Move constructor. Allows efficient transfer of task ownership (e.g., when emplacing into the `Scheduler`'s map). Marked `noexcept`.
*   **`~Task()`**: Default destructor. Smart pointers handle resource management.

#### 4.2.4. Public Member Functions

*   **`TaskID getID() const;`**
    *   Returns the unique ID of the task.
*   **`t_TaskState getState() const;`**
    *   Returns the current state (`t_TaskState`) of the task.
    *   *Note: Accessing state might require external synchronization (locking the `Scheduler::mtx_`) if called concurrently with state modifications.*
*   **`TaskID getUnmetCount() const;`**
    *   Atomically reads and returns the current number of unmet dependencies. Uses `std::memory_order_relaxed` internally.
*   **`bool decrement_unmet_dependencies();`**
    *   Atomically decrements the `unmetCount_`. Asserts that the count is greater than 0 before decrementing.
    *   **Returns:** `true` if the count becomes 0 after decrementing, `false` otherwise.
*   **`inline bool setStateRunning();`**
*   **`inline bool setStatePending();`**
*   **`inline bool setStateFailed();`**
*   **`inline bool setStateCompleted();`**
*   **`inline bool setStateCancelled();`**
*   **`inline bool setStateReady();`**
    *   Inline wrappers around the private `setState` method for specific target states.
    *   **Returns:** `true` if the state transition was valid and performed, `false` otherwise.
    *   **IMPORTANT:** These methods are *not* thread-safe by themselves. They must only be called while holding the `Scheduler::mtx_` lock.

#### 4.2.5. Private Member Functions

*   **`void run() const;`**
    *   Executes the stored `work_` function.
    *   Checks if `work_` is valid before calling it. Logs an error if no function is provided.
    *   Called exclusively by the `Scheduler::Worker`.
*   **`bool setState(t_TaskState st);`**
    *   Internal method to change the task's state.
    *   Contains validation logic to prevent invalid state transitions (e.g., PENDING -> RUNNING if dependencies are unmet, RUNNING -> PENDING, changing a final state). Logs errors on invalid attempts.
    *   **Returns:** `true` on successful state change, `false` otherwise.
    *   **IMPORTANT:** This method is *not* thread-safe and must only be called while holding the `Scheduler::mtx_` lock.

#### 4.2.6. Operators

*   **`Task& operator=(const Task&) = delete;`**: Copy assignment is deleted.
*   **`Task& operator=(Task&& rhs) noexcept = default;`**: Move assignment operator. Allows moving task state. Default implementation works because members are movable.

#### 4.2.7. Friend Declarations

*   `friend class Scheduler;`: Allows `Scheduler` to access private members (like `run`, `setState`).
*   `friend class TaskTest;`: Allows the GoogleTest fixture to access private members for testing.

### 4.3. `Scheduler` Class (`scheduler.h`)

The main class for managing and executing tasks.

#### 4.3.1. Overview

The `Scheduler` orchestrates task execution using a worker pool. It handles task addition, dependency resolution, state transitions, and thread synchronization. It is non-copyable and non-movable.

#### 4.3.2. Nested `Worker` Class

*   **`class Worker`**
    *   **Purpose:** Represents a worker thread entity. Each `Worker` instance runs in its own `std::thread`.
    *   **Members (Private):**
        *   `Scheduler& scheduler_`: Reference to the owning `Scheduler`.
        *   `const int id_`: Unique ID for the worker thread.
    *   **Constructor:** `Worker(Scheduler& sch, int workerId);` (Takes owning scheduler and ID).
    *   **Deleted:** Copy/Move constructors and assignment operators.
    *   **Public Functions:**
        *   `void run();`: The main loop executed by the worker thread. Fetches tasks from the `Scheduler`'s ready queue and executes them. Handles waiting on `workAvailable_` and checking `stopRequested_`.

#### 4.3.3. Member Variables (Private)

*   `t_SchedulerState state_`: Current state of the scheduler (`CREATED`, `STARTED`, `STOPPED`).
*   `std::atomic<TaskID> id_`: Atomic counter for generating unique `TaskID`s. Starts at 1.
*   `std::atomic<int> workerId_`: Atomic counter for assigning IDs to `Worker` instances.
*   `std::mutex mtx_`: The primary mutex protecting shared scheduler data.
*   `const int threadNumber_`: Number of worker threads to create.
*   `std::unordered_map<TaskID, std::shared_ptr<Task>> tasks_`: Stores all tasks, keyed by `TaskID`. Uses `shared_ptr` for managing task lifetimes.
*   `std::deque<TaskID> readyTasks_`: Queue of `TaskID`s ready for execution. Accessed by workers.
*   `std::vector<std::unique_ptr<Worker>> workers_`: Owns the `Worker` objects. `unique_ptr` ensures proper cleanup.
*   `std::vector<std::thread> workerThreads_`: Holds the actual `std::thread` objects associated with the `Worker` instances. Needed for starting and joining.
*   `std::unordered_map<TaskID, std::vector<TaskID>> downwardDependencies_`: Maps a `TaskID` to a list of tasks that depend *on* it. Used for efficient notification.
*   `std::condition_variable workAvailable_`: Signals workers when tasks become ready or when stopping.
*   `std::atomic<bool> stopRequested_`: Flag indicating if `stop()` has been called. Workers check this flag to exit their loops.
*   `std::atomic<int> addedTasks_`, `completedTasks_`, `failedTasks_`, `cancelledTasks_`: Counters for tracking task statistics.
*   `std::condition_variable allTasksFinished_`: Signals when all added tasks have reached a terminal state.
*   `static const char* taskStateName[]`: Array to map `t_TaskState` enum values to strings (used internally for logging).

#### 4.3.4. Enums (Private)

*   **`enum t_SchedulerState {CREATED, STARTED, STOPPED};`**: Defines the operational states of the scheduler itself.
*   **`enum t_StopWay {IMMEDIATE, WAIT_ALL_TO_END};`**: Defines options for the `stop()` method behavior.

#### 4.3.5. Constructors & Destructor

*   **`Scheduler(int n = 2);`**
    *   Constructor. Initializes the scheduler with `n` worker threads (defaulting to 2).
    *   Sets the initial state to `CREATED`. Initializes atomic counters, reserves space for workers, and creates the `Worker` objects using `std::unique_ptr`. Does *not* start the threads yet.
*   **`Scheduler(const Scheduler& sch) = delete;`**: Copy constructor deleted.
*   **`Scheduler(Scheduler&& sch) = delete;`**: Move constructor deleted.
*   **`~Scheduler();`**
    *   Destructor. Ensures the scheduler is stopped cleanly by calling `stop()` (which joins all worker threads). Logs the destruction process.

#### 4.3.6. Public Member Functions

*   **`TaskID addTask(std::function<void()> func, const std::vector<TaskID>& dependencies = {});`**
    *   Adds a new task to the scheduler.
    *   **Parameters:**
        *   `func`: The work function for the task.
        *   `dependencies`: A vector of `TaskID`s this task depends on. Defaults to empty.
    *   **Returns:**
        *   The unique `TaskID` assigned to the new task on success.
        *   `-1`: If unable to create the internal `Task` object (e.g., memory allocation failure).
        *   `-2`: If any `TaskID` in `dependencies` does not correspond to an existing task in the scheduler.
        *   `-3`: If adding this dependency would create a cycle (currently only checks simple backward reachability).
        *   `-10`: Internal state change error during task initialization.
    *   **Behavior:** Acquires the mutex, generates a new ID, validates dependencies, checks for cycles (basic check), creates the `Task` object (`std::shared_ptr`), inserts it into `tasks_`, updates `downwardDependencies_`, decrements unmet count for already completed/failed dependencies, and potentially adds the task to `readyTasks_` if it has no unmet dependencies. Notifies one worker if a task becomes ready.
*   **`void start();`**
    *   Starts the worker threads, allowing the scheduler to begin processing tasks.
    *   Transitions the scheduler state from `CREATED` or `STOPPED` to `STARTED`.
    *   If restarting from `STOPPED`, clears previous task data.
    *   Idempotent if already `STARTED` (logs a warning). Reserves space and launches `std::thread`s for each `Worker`.
*   **`void stop(t_StopWay wayToStop = t_StopWay::IMMEDIATE);`**
    *   Stops the scheduler and its worker threads.
    *   **Parameters:**
        *   `wayToStop`:
            *   `IMMEDIATE` (default): Sets `stopRequested_`, notifies all workers, cancels tasks currently in the `readyTasks_` queue, and joins all worker threads. Waits for currently *running* tasks to finish naturally.
            *   `WAIT_ALL_TO_END`: First calls `waitTasksToEnd()` before proceeding with the immediate stop logic.
    *   **Behavior:** Sets `stopRequested_` flag, clears `readyTasks_` (cancelling those tasks), notifies waiting workers via `workAvailable_`, unlocks the mutex, and then joins all `workerThreads_`. Transitions scheduler state to `STOPPED`. Idempotent if already `STOPPED`.
*   **`bool waitTasksToEnd();`**
    *   Blocks the calling thread until all tasks *currently added* to the scheduler have reached a terminal state (`COMPLETED`, `FAILED`, or `CANCELLED`), or until `stop()` is called concurrently.
    *   Uses the `allTasksFinished_` condition variable, waiting on a predicate that checks if `completed + failed + cancelled == added`.
    *   **Returns:** `true` if all tasks finished normally, `false` if the wait was interrupted by `stopRequested_` being set.
    *   Only functional when the scheduler is in the `STARTED` state.
*   **`void printTaskCollection(t_Verbosity verb = t_Verbosity::DEBUG);`**
    *   Prints the status of all tasks currently held by the scheduler to standard output using `safe_print`.
    *   Includes Task ID, current state, unmet dependency count, and the list of dependencies.
    *   Requires the specified `verb` level to be less than or equal to `verbosityPrinted`.

#### 4.3.7. Private Member Functions

*   **`void notifyDependents(TaskID taskId);`**
    *   Called by a `Worker` after a task (`taskId`) finishes execution (either `COMPLETED` or `FAILED`).
    *   **IMPORTANT:** Assumes the caller (Worker) holds the `Scheduler::mtx_` lock.
    *   **Behavior:**
        *   If the task `FAILED`: Propagates the failure recursively to all dependent tasks by setting their state to `FAILED`.
        *   If the task `COMPLETED`: Iterates through the list of tasks that depend on `taskId` (using `downwardDependencies_`). For each pending dependent, it calls `decrement_unmet_dependencies()`. If a dependent's count reaches zero, its state is set to `READY`, it's added to `readyTasks_`, and `workAvailable_` is notified.
        *   Logs errors if `taskId` or dependent IDs are not found.
*   **`bool check_cycles(TaskID dependant, TaskID depends_on, std::vector<TaskID>& cycle);`**
    *   Performs a basic check for cycles when adding a dependency from `dependant` to `depends_on`.
    *   **IMPORTANT:** Assumes the caller holds the `Scheduler::mtx_` lock.
    *   **Current Logic:** Uses Depth First Search (DFS) starting from `dependant`, traversing the *existing* `downwardDependencies_` graph. If `depends_on` is reachable from `dependant` *before* the new edge is added, it indicates a cycle would be formed. *(Note: This check is only sufficient because tasks can only depend on *already existing* tasks. It wouldn't catch cycles within a batch addition).*
    *   **Parameters:**
        *   `dependant`: The ID of the task that will depend on `depends_on`.
        *   `depends_on`: The ID of the task being depended upon.
        *   `cycle`: Output parameter; populated with the detected cycle path if one is found.
    *   **Returns:** `true` if a cycle is detected, `false` otherwise.

#### 4.3.8. Friend Declarations

*   Various `friend class SchedulerTest*`: Allows different GoogleTest fixtures access to private/protected members for testing purposes (e.g., `tasks_`, `readyTasks_`, `notifyDependents`, `mtx_`).

## 5. Implementation Details

### 5.1. Architecture Overview

The scheduler follows a classic worker pool pattern:

1.  **Task Creation:** Users add tasks via `Scheduler::addTask`. The `Scheduler` validates inputs, assigns an ID, creates a `Task` object (managed by `std::shared_ptr`), stores it in `tasks_`, and updates dependency information (`downwardDependencies_`, `unmetCount_`).
2.  **Readiness:** If a task has no dependencies or its dependencies are already met upon addition, it's immediately marked `READY` and placed in the `readyTasks_` deque.
3.  **Worker Loop (`Worker::run`):** Each worker thread runs a loop:
    *   It waits on `workAvailable_` using `Scheduler::mtx_` until `readyTasks_` is not empty or `stopRequested_` is true.
    *   If stopped, the worker exits.
    *   If work is available, it dequeues a `TaskID` from `readyTasks_`, finds the corresponding `Task` pointer in `tasks_`, sets its state to `RUNNING`, and releases the lock (`mtx_`).
    *   It executes the task's `work_` function via `task_ptr->run()`.
    *   It catches exceptions during `run()`.
    *   It re-acquires the lock (`mtx_`).
    *   It sets the task state to `COMPLETED` (on success) or `FAILED` (on exception).
    *   It calls `notifyDependents(completedTaskID)` to update dependencies.
    *   It checks if all tasks are now finished and notifies `allTasksFinished_` if necessary.
    *   It releases the lock and loops back to wait for more work.
4.  **Notification (`notifyDependents`):** When a task completes successfully, this function (called by the worker holding the lock) finds all tasks depending on the completed one, decrements their `unmetCount_`. If a dependent's count hits zero, it's transitioned to `READY` and added to `readyTasks_`, and `workAvailable_` is notified. If a task fails, failure is propagated.
5.  **Shutdown (`stop`):** Sets `stopRequested_`, notifies workers, cancels pending ready tasks, and joins threads.

### 5.2. Task Lifecycle Management

*   **Creation:** Tasks start as `PENDING` or `READY` in `Scheduler::addTask`.
*   **PENDING -> READY:** Transition occurs within `notifyDependents` when the last dependency completes successfully. Requires `mtx_`.
*   **READY -> RUNNING:** Transition occurs in `Worker::run` just before releasing the lock to execute the task. Requires `mtx_`.
*   **RUNNING -> COMPLETED:** Transition occurs in `Worker::run` after successful execution and re-acquiring the lock. Requires `mtx_`.
*   **RUNNING -> FAILED:** Transition occurs in `Worker::run` after catching an exception during execution and re-acquiring the lock. Requires `mtx_`.
*   **PENDING -> FAILED:** Transition occurs during failure propagation in `notifyDependents`. Requires `mtx_`.
*   **READY -> CANCELLED:** Transition occurs during `Scheduler::stop(IMMEDIATE)` for tasks waiting in the ready queue. Requires `mtx_`.
*   **Validation:** The `Task::setState` method enforces valid transitions, preventing illogical changes (e.g., COMPLETED -> PENDING).

### 5.3. Concurrency and Synchronization

*   **`Scheduler::mtx_`:** This is the coarse-grained lock protecting the core scheduler state (`tasks_`, `readyTasks_`, `downwardDependencies_`, task state changes). It's held during `addTask`, `stop`, `notifyDependents`, and parts of the `Worker::run` loop (fetching work, updating state post-execution). This simplicity prevents many race conditions but can become a bottleneck under high contention (See Future Considerations).
*   **`workAvailable_`:** Prevents busy-waiting by workers. Used with `Scheduler::mtx_` in a standard condition variable pattern: wait on a predicate (`!readyTasks_.empty() || stopRequested_`). Notification occurs in `notifyDependents` (when a task becomes `READY`) and `stop`.
*   **`allTasksFinished_`:** Coordinates `waitTasksToEnd` with worker completion. Waits on a predicate checking task counters. Notification occurs in `Worker::run` after `notifyDependents` if the total finished count matches the added count.
*   **`Task::unmetCount_` (`std::atomic<TaskID>`):** Atomically tracks remaining dependencies. Accessed via `load()`, `fetch_sub()`. `memory_order_relaxed` is often sufficient as synchronization is primarily handled by `Scheduler::mtx_` around the logic that uses this count (e.g., in `notifyDependents`). `fetch_sub` provides the necessary atomicity for the decrement operation itself.
*   **`Scheduler::id_`, `Scheduler::workerId_` (`std::atomic`):** Ensure unique ID generation without requiring the main lock (`mtx_`) for simple increments (`fetch_add`).
*   **`Scheduler::stopRequested_` (`std::atomic<bool>`):** Allows thread-safe checking and setting of the stop flag without needing the main lock constantly. Workers check it within their wait predicate and before potentially long operations.
*   **Task Counters (`std::atomic<int>`):** Safely track task statistics across worker threads using `fetch_add`.
*   **`globalMutex` / `safe_print`:** Serializes console output from multiple threads.

### 5.4. Dependency Tracking

*   **`Task::dependencies_` (`const std::vector<TaskID>`):** Stored within each task, lists what it needs *before* it can run. Used during `addTask` to initialize `unmetCount_`.
*   **`Task::unmetCount_` (`std::atomic<TaskID>`):** The primary mechanism for determining readiness. Decremented atomically when a dependency completes.
*   **`Scheduler::downwardDependencies_` (`std::unordered_map<TaskID, std::vector<TaskID>>`):** The "reverse" mapping. Stores which tasks depend *on* a given `TaskID`. Essential for `notifyDependents` to efficiently find which tasks to update when one completes or fails. This map is updated in `addTask`.

### 5.5. Error Handling and Propagation

*   **`addTask` Return Codes:** Uses negative integers (`-1`, `-2`, `-3`, `-10`) to signal specific errors during task addition (allocation failure, invalid dependency, cycle detected, state error).
*   **Task Execution Failure:** If `task->run()` throws an exception within `Worker::run`, the task is marked `FAILED`.
*   **Failure Propagation:** `notifyDependents` handles propagation. If the notified task (`taskId`) is `FAILED`, it recursively marks all its dependents (and their dependents, etc.) as `FAILED` using a stack-based traversal. This prevents dependent tasks from running if a prerequisite failed.
*   **Logging:** `safe_print` is used extensively to log errors, warnings, and informational messages related to state transitions, invalid operations, task execution status, etc.

### 5.6. Cycle Detection (`check_cycles`)

*   **Current Implementation:** Uses a recursive DFS approach (`check_cycles` function).
*   **Trigger:** Called within `addTask` *before* adding the new task and its dependency edge to the internal structures.
*   **Logic:** For a new task `C` depending on existing task `P`, it checks if `P` is reachable from `C` by traversing the *existing* dependency graph (`downwardDependencies_`) starting from `C`. Since `C` isn't in the graph yet, this effectively checks if adding the `C -> P` edge would create a path from `P` back to itself through `C`.
*   **Limitations:** This approach only works because new tasks can only depend on tasks *already present* in the scheduler. It would not detect cycles formed entirely *within* a batch of tasks added simultaneously if such a feature were implemented. A more robust cycle detection algorithm (e.g., full graph DFS/Tarjan's algorithm on the state including the proposed additions) would be needed for batch additions or allowing modifications to existing dependencies.

### 5.7. Memory Management

*   **Tasks (`std::shared_ptr<Task>`):** `tasks_` stores tasks using `std::shared_ptr`. This allows multiple references (e.g., the map itself, potentially temporary references in workers) while ensuring the `Task` object persists as long as needed (e.g., until all dependents have processed its completion/failure). When the last `shared_ptr` goes out of scope (e.g., scheduler is destroyed or task is explicitly removed - although removal isn't implemented), the `Task` object is automatically deleted.
*   **Workers (`std::unique_ptr<Worker>`):** `workers_` stores `Worker` objects using `std::unique_ptr`. This signifies unique ownership by the `Scheduler`. When the `Scheduler` is destructed, the `unique_ptr`s automatically delete the `Worker` objects.
*   **Threads (`std::vector<std::thread>`):** `workerThreads_` manages the OS thread handles. Threads must be explicitly `join()`ed (done in `Scheduler::stop`/`~Scheduler`) before the `std::thread` object is destroyed to avoid resource leaks or program termination.

## 6. Testing Strategy

### 6.1. Overview

The project employs a combination of manual (compile-time enabled) tests and automated unit/integration tests using the GoogleTest framework to ensure correctness and robustness.

### 6.2. Manual Tests (`test_*.cpp`)

These tests reside in the `src/` directory (e.g., `test_sanity.cpp`) and are compiled into the main executable (`main`) only when a specific preprocessor macro is defined during compilation. They typically perform higher-level scenario testing.

#### 6.2.1. `test_sanity.cpp` (`__TEST_SANITY__` flag)

*   **Purpose:** Executes a basic scenario with a small number of tasks and dependencies, where all tasks are expected to complete successfully. Verifies basic dependency ordering and execution flow.
*   **Usage:** Compile with `-d __TEST_SANITY__` (e.g., `./compile.py -d __TEST_SANITY__`) and run the resulting `main` executable (`./run.py`).

#### 6.2.2. `test_failure_propagated.cpp` (`__TEST_FAILURE_PROPAGATED__` flag)

*   **Purpose:** Tests the failure propagation mechanism. Includes a task designed to fail (throw an exception). Verifies that tasks depending on the failing task are correctly marked as `FAILED` and do not execute.
*   **Usage:** Compile with `-d __TEST_FAILURE_PROPAGATED__` and run `main`.

#### 6.2.3. `test_stress.cpp` (`__TEST_STRESS__` flag)

*   **Purpose:** Creates a large number of tasks (initial independent tasks followed by dependent tasks) to stress the scheduler under load with multiple worker threads. Checks if the expected number of tasks complete. Useful for identifying potential race conditions, deadlocks, or performance bottlenecks under concurrency.
*   **Usage:** Compile with `-d __TEST_STRESS__` and run `main`.

#### 6.2.4. How to Enable/Run

As described above, use the `compile.py` script with the appropriate `-d <FLAG_NAME>` argument to build the `main` executable containing the desired test, then execute `main` using `./run.py`. Only one flag should be active per build.

### 6.3. Unit & Integration Tests (GoogleTest - `test/`)

Automated tests using the GoogleTest framework are located in the `test/` directory and compiled into a separate executable (`MyTests`). These provide more granular and repeatable verification.

#### 6.3.1. Setup (`main_test.cpp`, CMake integration)

*   `main_test.cpp`: Contains the `main` function for the test executable, initializing GoogleTest (`::testing::InitGoogleTest(&argc, argv);`) and running all discovered tests (`RUN_ALL_TESTS();`).
*   `CMakeLists.txt`: Uses `FetchContent` to download GoogleTest, defines the `MyTests` executable, links it against `gtest_main`, includes necessary project source files (`scheduler.cpp`, `task.cpp`, `helpers.cpp`) and header directories, and uses `gtest_discover_tests` to integrate with CTest.

#### 6.3.2. `Task` Tests (`task_test.cpp`)

*   **Fixture:** `TaskTest` (simple fixture, inherits from `::testing::Test`).
*   **Covered Aspects:**
    *   Task construction with and without dependencies (checking initial state, unmet count).
    *   `decrement_unmet_dependencies` logic.
    *   State transition validation (`setState` logic via public wrappers).

#### 6.3.3. `Scheduler` Unit Tests (`scheduler_test.cpp`)

*   **Fixture:** `SchedulerTest` (inherits from `::testing::Test`). Creates a `Scheduler` instance (typically with 2 threads, but they aren't started for most unit tests). Provides helper methods (`getTask`, `getReadyQueue`, `getDependents`, `tasksPresent`, `waitLockMutex`) using `friend` access to inspect internal scheduler state without starting threads.
*   **Sub-Fixtures:** `SchedulerTest_AddTask_Test`, `SchedulerTest_Notify_Test` inherit from `SchedulerTest` to group related tests.
*   **Covered Aspects:**
    *   `addTask`: Adding tasks with no dependencies, valid dependencies, invalid dependencies, dependencies on pre-completed tasks. Checks resulting task state, unmet count, ready queue content, and `downwardDependencies_` map.
    *   `notifyDependents`: Simulates task completion/failure (by manually setting state and calling the private `notifyDependents` via helper, while holding the lock) and verifies correct state propagation (READY or FAILED) and ready queue updates for dependent tasks.

#### 6.3.4. `Scheduler` Integration Tests (`scheduler_integration_test.cpp`)

*   **Fixture:** `SchedulerTest_Integration_Test` (inherits from `::testing::Test`). Creates a `Scheduler` using `std::unique_ptr` (allowing different thread counts per test). Provides `createScheduler`, `waitTasksToFinish`, and `TearDown` (to ensure `scheduler_->stop()` is called). These tests *start* the scheduler threads.
*   **Covered Aspects:**
    *   **SimpleDependencyOrder:** Uses `std::promise`/`std::shared_future` to verify that a dependent task only starts executing *after* its dependency has finished, testing the end-to-end flow through the worker threads and notification mechanism.
    *   **AllTasksCompleteLoadTest:** Adds a large number of tasks with randomized simple dependencies, starts the scheduler with multiple threads, waits for completion using `waitTasksToEnd`, and verifies that the total number of completed tasks matches the number added. Tests overall throughput and completion signaling under load.

#### 6.3.5. Running Tests

Use the `run.py` script or CTest:

```bash
# Run all Google Tests using the helper script
./run.py -gt

# Run all Google Tests using CTest (from build directory)
ctest

# Run specific Google Tests directly (from build directory)
./bin/MyTests --gtest_filter=TaskTest.* # Run all Task tests
./bin/MyTests --gtest_filter=SchedulerTest_Integration_Test.SimpleDependencyOrder # Run specific test case
```

## 7. Build System (`CMakeLists.txt`)

### 7.1. Overview

The project uses CMake for managing the build process, dependencies, and test integration.

*   **Minimum Version:** 3.10
*   **Project Name:** `MyProject` (Version 1.0)
*   **C++ Standard:** C++14 (`CMAKE_CXX_STANDARD 14`, `CMAKE_CXX_STANDARD_REQUIRED True`).

### 7.2. Dependencies

*   **GoogleTest:** Managed automatically using `FetchContent`. CMake downloads and builds GoogleTest (version specified in `CMakeLists.txt`, currently v1.14.0) during the configuration phase if it's not already present in the build directory. `FetchContent_MakeAvailable(googletest)` makes its targets available.
    *   `set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)` is included for Windows compatibility.

### 7.3. Targets

CMake defines the following primary build targets:

*   **`main` (Executable):** Built from sources in `src/` (found via `file(GLOB_RECURSE SOURCE_FILES ...)`). This is the main application or the executable used for manual tests. Links against `MyLibrary` if it exists.
*   **`MyLibrary` (Static Library, Optional):** If source files exist in the `lib/` directory, they are compiled into a static library named `MyLibrary`. `main` and `MyTests` link against this if it's created.
*   **`MyTests` (Executable):** Built from specific sources (`src/scheduler.cpp`, `src/task.cpp`, `src/helpers.cpp`) and all test sources (`test/*.cpp`). This target contains the GoogleTest suite. Links against `gtest_main`, `gmock_main`, and `MyLibrary` (if it exists).
*   **`spike` (Executable, Conditional):** Built only if the `Q` option is `ON` and the `QUICK` variable specifies a valid filename in the `spike/` directory. Links the specified spike file with necessary scheduler sources (`src/*.cpp`) and `MyLibrary` (if it exists).

Output directories are configured for better organization (`CMAKE_RUNTIME_OUTPUT_DIRECTORY`, etc.) placing binaries in `build/bin` and libraries in `build/lib`.

### 7.4. Include Directories

*   `target_include_directories(main PUBLIC include)`: Adds the `include` directory to the include path for the `main` target (and anything linking `main`).
*   `target_include_directories(MyLibrary PUBLIC include)`: Adds `include` for the library target.
*   `target_include_directories(MyTests PUBLIC include)`: Adds `include` for the test target.
*   `target_include_directories(MyTests PRIVATE ${gtest_SOURCE_DIR}/...)`: Adds GoogleTest include directories for compiling the tests.

### 7.5. Compile Options & Definitions

Common compile options are applied to ensure code quality and enable necessary features:

*   **Applied to `main`, `MyLibrary`, `MyTests`, `spike`:**
    *   `-Wall`: Enable common warnings.
    *   `-Wextra`: Enable additional warnings.
    *   `-pedantic`: Enforce strict standard compliance.
    *   `-pthread`: Link with the pthreads library (essential for `std::thread`, `std::mutex`, etc. on POSIX systems).
*   **Applied to `main` only (potentially for debugging):**
    *   `-O0`: Disable optimizations.
    *   `-fno-elide-constructors`: Prevent copy/move elision (can sometimes help in debugging object lifetimes, but generally not recommended for release builds).
*   **Global Flags:**
    *   `-save-temps`: Instructs the compiler (like GCC/Clang) to save intermediate files (preprocessing output `.i`, assembly `.s`). Useful for debugging compilation issues.
    *   `-masm=intel`: Use Intel assembly syntax (instead of AT&T) in generated `.s` files.
*   **Preprocessor Definitions:**
    *   `target_compile_definitions(... PRIVATE EXAMPLE_DEFINE)`: Example of adding a project-wide define.
    *   `target_compile_definitions(MyTests PRIVATE TESTING)`: Defines `TESTING` specifically for the test build.
    *   `target_compile_definitions(... PRIVATE FEATURE_ENABLED)`: Conditionally added if the `ENABLE_FEATURE` CMake option is ON.
    *   Defines like `TEST_SANITY`, `TEST_STRESS` are passed via `EXTRA_FLAGS` using the `compile.py` script.

### 7.6. Test Configuration

*   `enable_testing()`: Enables CTest support for the project.
*   `include(GoogleTest)`: Includes CMake module for GoogleTest integration.
*   `gtest_discover_tests(MyTests)`: Automatically discovers tests within the `MyTests` executable and registers them with CTest, allowing `ctest` to run them.

### 7.7. Build Options

Custom CMake options allow configuration from the command line (`cmake -D<OPTION>=<VALUE>`) or via `compile.py`:

*   **`option(Q "Enable quick compilation..." OFF)`:** Boolean flag to enable quick mode.
*   **`set(QUICK "" CACHE STRING "Filename for quick compilation")`:** String variable to hold the target filename for quick mode. CMake logic checks if `Q` is `ON` and `QUICK` is non-empty and exists before defining the `spike` target.
*   **`option(ENABLE_FEATURE "Enable a specific feature" OFF)`:** Example boolean option to conditionally compile code using `#ifdef FEATURE_ENABLED`.

## 8. Utility Scripts

Several Python scripts are provided in the project root (or a `scripts/` directory) to simplify common development tasks. They require Python 3.

### 8.1. `clean.py`

*   **Purpose:** Removes the `build` directory, effectively cleaning all CMake cache files and build artifacts.
*   **Usage:** `./clean.py` or `python3 clean.py`

### 8.2. `compile.py`

*   **Purpose:** Wraps the CMake configuration and build steps into a single command. Handles passing common options like definitions and build types.
*   **Usage:** `./compile.py [options]`
*   **Key Arguments:**
    *   `-d DEFINITION` or `--define DEFINITION`: Adds a preprocessor definition (e.g., `-d TEST_SANITY`). Can be used multiple times. Passed via `EXTRA_FLAGS`.
    *   `-q` or `--quick`: Enables quick compilation mode (sets CMake `Q=ON`). Requires `-t`.
    *   `-t FILENAME` or `--target FILENAME`: Specifies the target `.cpp` file (relative to `spike/` directory) for quick compilation (sets CMake `QUICK`). Used with `-q`.
    *   `-test TESTNAME`: Adds the appropriate `-DTEST_<TESTNAME>` definition for running a manual test (e.g., `-test sanity` adds `-DTEST_SANITY`).
    *   `-dbg` or `--debug`: Configures CMake with `-DCMAKE_BUILD_TYPE=Debug`.
    *   See `./compile.py --help` for full details.

### 8.3. `run.py`

*   **Purpose:** Executes compiled targets located in the `build/bin` directory.
*   **Usage:** `./run.py [options]`
*   **Key Arguments:**
    *   `-t TARGETNAME` or `--target TARGETNAME`: Specifies the executable to run (default: `main`).
    *   `-q` or `--quick`: Runs the `spike` executable (built via `compile.py -q`).
    *   `-gt` or `--googleTest`: Runs the `MyTests` executable.
    *   See `./run.py --help` for full details.

### 8.4. `all.py`

*   **Purpose:** Combines the build and run steps. Calls `compile.py` and then `run.py`, passing relevant arguments to each.
*   **Usage:** `./all.py [options]`
*   **Key Arguments:**
    *   Accepts most arguments from `compile.py` (like `-d`, `-q`, `-t <compile_target>`, `-test`, `-dbg`) and `run.py` (like `-t <run_target>`, `-q`, `-gt`).
    *   `-c` or `--clean`: Runs `clean.py` before starting the build process.
    *   See `./all.py --help` for full details.

## 9. Assumptions and Limitations

*   **Non-Preemptive Tasks:** Tasks run to completion or until they throw an exception. The scheduler does not preempt running tasks. Long-running tasks can block worker threads.
*   **No Task Priorities:** All ready tasks are treated equally (FIFO order from the `readyTasks_` deque). There is no mechanism to prioritize certain tasks over others.
*   **No Return Values:** Tasks are defined as `std::function<void()>`. They cannot directly return values back to the scheduler or other tasks. Communication would require external mechanisms (e.g., shared memory, promises/futures managed outside the core task function).
*   **Basic Cycle Detection:** The current cycle detection (`check_cycles`) only prevents cycles formed by adding a dependency on an existing task that can already reach the new task. It does not support detecting cycles within a batch of tasks added simultaneously or cycles formed by modifying dependencies of existing tasks (which isn't supported).
*   **Coarse-Grained Locking:** The main `Scheduler::mtx_` protects multiple critical sections. While simplifying correctness, this can become a performance bottleneck under high contention with many threads frequently adding tasks or completing short tasks.
*   `addTask` Blocking: The `addTask` function acquires the main lock and performs validation, potentially blocking other scheduler operations (including workers finishing tasks) while it runs.
*   **No Dynamic Worker Adjustment:** The number of worker threads is fixed at scheduler creation.
*   **Limited `stop()` Granularity:** `stop(IMMEDIATE)` cancels tasks *waiting* in the ready queue but allows *currently running* tasks to complete. There's no immediate "kill" mechanism for running tasks. `stop(WAIT_ALL_TO_END)` ensures all *added* tasks finish before shutdown.
*   **Global State:** Relies on global `globalMutex` and `verbosityPrinted` for logging, which is generally discouraged in library design.
*   **Task Modification:** Once a task is added, its work function and dependencies cannot be modified.
*   **Task Removal:** There is no public API to remove a task from the scheduler once added.

## 10. Future Considerations / TODOs

This section lists potential improvements and areas for future development, consolidating TODOs found in the code and previous discussions.

*   **Core Functionality:**
    *   Implement cooperative task cancellation (e.g., add an atomic `cancel_requested_` flag to `Task`, checkable within the `work_` function; `Scheduler::cancelTask(TaskID)` method).
    *   Implement task priorities. This requires changing `readyTasks_` from `std::deque` to a priority queue (e.g., `std::priority_queue<TaskID, std::vector<TaskID>, CompareTaskPriority>`) and potentially adjusting worker fetching logic. `Task` would need a priority member.
    *   Allow tasks to return results. Could potentially leverage `std::packaged_task` internally and provide a way to retrieve `std::future` associated with a `TaskID`. `Scheduler::addTask` signature would change.
*   **API & Usability:**
    *   Implement batch `addTask` (e.g., taking `std::vector<TaskDefinition>` or using variadic templates). Requires robust cycle detection capable of handling cycles within the batch *before* committing any tasks from the batch. Transactional addition might be needed (all succeed or none are added).
    *   Add methods to query task status: `std::optional<t_TaskState> getTaskState(TaskID) const;`. Requires careful locking.
    *   Add methods to query task results (if return values are implemented): `std::future<ResultType> getResult(TaskID);`.
    *   Improve error reporting: Consider using `std::optional` or a custom result class instead of magic numbers (`-1`, `-2`...) for `addTask`. Potentially use exceptions for critical errors.
*   **Performance & Scalability:**
    *   Investigate lock contention on `Scheduler::mtx_`. Explore finer-grained locking strategies, such as separate locks for `tasks_`, `readyTasks_`, and `downwardDependencies_`, or using lock-free structures (much more complex).
    *   Implement work-stealing queues for worker threads. Each worker could have its own local ready queue, and idle workers could "steal" tasks from busy workers' queues, potentially improving load balancing, especially with tasks of varying durations.
    *   Optimize data structures if performance analysis indicates bottlenecks (e.g., choice of map/deque).
    *   Explore creating preemptive tasks
*   **Robustness & Refinement:**
    *   Implement a proper thread-safe logging framework (e.g., spdlog, glog, or a custom solution) to replace the basic `safe_print` and direct `std::cout` usage.
    *   Remove global variables (`globalMutex`, `verbosityPrinted`) by passing logger instances or configuration objects to the `Scheduler` upon construction. `safe_print` could become a method of a logger class.
    *   Refine or remove the commented-out `addTask(Task&&)` overload. If kept, clarify its semantics regarding ID assignment and dependency handling compared to the primary `addTask`. Ensure it integrates correctly with cycle detection and state management.
    *   Consider using C++14/17/20 features where appropriate (e.g., `std::make_unique`, `std::make_shared` for safer allocation, `std::optional` for return values, potentially `std::jthread` in C++20 for simpler thread management and cancellation requests if applicable).
    *   Strengthen cycle detection if task modification or batch additions are implemented.
    *   Add more comprehensive testing, especially for edge cases in concurrency and failure propagation.