#include "tests.h"

int sanity_test() {
    std::cout << "--- Scheduler Test Scenario (No Failures) Start ---" << std::endl;

    // 1. Create Scheduler instance (e.g., with 3 worker threads)
    const int num_worker_threads = 3;
    Scheduler scheduler(num_worker_threads);
    std::cout << "Scheduler created with " << num_worker_threads << " worker threads." << std::endl;

    // 2. Start the scheduler threads
    scheduler.start();
    std::cout << "Scheduler started." << std::endl;

    // 3. Define Tasks and Dependencies
    TaskID t1_id, t2_id, t3_id, t4_id, t5_id;

    // --- Task Definitions (using Lambdas) - All Succeed ---

    // Task 1: Independent, takes 100ms
    auto work1 = [&]() {
        // Using TaskID directly in lambda requires it to be captured AFTER it's assigned.
        // We'll print a generic message or update it later. For now, use "Task 1" text.
        TaskID current_id = t1_id; // Capture by value *after* assignment
        std::cout << "Task 1 [ID:" << current_id << "] starting work." << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "Task 1 [ID:" << current_id << "] finished work successfully." << std::endl;
    };

    // Task 2: Independent, takes 140ms
    auto work2 = [&]() {
        TaskID current_id = t2_id;
        std::cout << "Task 2 [ID:" << current_id << "] starting work." << std::endl;
        for (int i = 0; i < 7; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "Task 2 [ID:" << current_id << "] finished work successfully." << std::endl;
    };

    // Task 3: Depends on T1, takes 80ms
    auto work3 = [&]() {
        TaskID current_id = t3_id;
        std::cout << "Task 3 [ID:" << current_id << "] starting work (depends on T1)." << std::endl;
        for (int i = 0; i < 4; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "Task 3 [ID:" << current_id << "] finished work successfully." << std::endl;
    };

    // Task 4: Depends on T1 and T2, takes 120ms
    auto work4 = [&]() {
        TaskID current_id = t4_id;
        std::cout << "Task 4 [ID:" << current_id << "] starting work (depends on T1, T2)." << std::endl;
         for (int i = 0; i < 6; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "Task 4 [ID:" << current_id << "] finished work successfully." << std::endl;
    };

    // Task 5: Depends on T4, takes 60ms
    auto work5 = [&]() {
        TaskID current_id = t5_id;
        std::cout << "Task 5 [ID:" << current_id << "] starting work (depends on T4)." << std::endl;
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "Task 5 [ID:" << current_id << "] finished work successfully." << std::endl;
    };


    // --- Add Tasks to Scheduler ---
    std::cout << "\nAdding tasks..." << std::endl;

    // Add Task 1 (no dependencies)
    // Assign ID FIRST, then the lambda captures it by value.
    t1_id = scheduler.addTask(work1);
    std::cout << "Added Task 1 with ID: " << t1_id << std::endl;

    // Add Task 2 (no dependencies)
    t2_id = scheduler.addTask(work2);
    std::cout << "Added Task 2 with ID: " << t2_id << std::endl;

    // Add Task 3 (depends on T1)
    const std::vector<TaskID> t3_deps = {t1_id};
    t3_id = scheduler.addTask(work3, t3_deps);
    std::cout << "Added Task 3 with ID: " << t3_id << " (Depends on " << t1_id << ")" << std::endl;

    // Add Task 4 (depends on T1, T2)
    const std::vector<TaskID> t4_deps = {t1_id, t2_id};
    t4_id = scheduler.addTask(work4, t4_deps);
    std::cout << "Added Task 4 with ID: " << t4_id << " (Depends on " << t1_id << ", " << t2_id << ")" << std::endl;

    // Add Task 5 (depends on T4)
    const std::vector<TaskID> t5_deps = {t4_id};
    t5_id = scheduler.addTask(work5, t5_deps);
    std::cout << "Added Task 5 with ID: " << t5_id << " (Depends on " << t4_id << ")" << std::endl;


    // 4. Wait for tasks to likely complete (NOT ROBUST - use wait() when available)
    // Calculate a reasonable wait time:
    // Longest path: T1/T2 -> T4 -> T5
    // Rough estimate: max(T1, T2) + T4 + T5 = max(100, 140) + 120 + 60 = 140 + 120 + 60 = 320ms
    // Add some buffer for scheduling overhead, context switching etc. Let's wait 1 second.
    long long estimated_completion_ms = 1000; // 1 second
    std::cout << "\nWaiting for tasks to likely complete (approx " << estimated_completion_ms << " ms)..." << std::endl;
    std::cout << "(This is not a robust wait mechanism without scheduler.wait())" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(estimated_completion_ms));


    // 5. Stop the scheduler
    std::cout << "\nRequesting scheduler stop..." << std::endl;
    scheduler.stop(); // This will wait for running tasks to finish and join threads


    std::cout << "\n--- Scheduler Test Scenario (No Failures) End ---" << std::endl;
    return 0;
}