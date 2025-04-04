#ifdef __TEST_FAILURE_PROPAGATED__

#include "tests.h"

int test_failure_propagated() {
  safe_print("--- Scheduler Test Scenario Start ---");

  // 1. Create Scheduler instance (e.g., with 3 worker threads)
  const int num_worker_threads = 3;
  Scheduler scheduler(num_worker_threads);
  safe_print("Scheduler created with " + std::to_string(num_worker_threads) + " worker threads.");

  safe_print("Scheduler started");
  // 2. Start the scheduler threads
  scheduler.start();

  // 3. Define Tasks and Dependencies
  TaskID t1_id, t2_id, t3_id, t4_id, t5_id;

  // --- Task Definitions (using Lambdas) ---

  // Task 1: Independent, takes 50ms
  auto work1 = [&]() {
      safe_print("Task 1 [ID:" + std::to_string(t1_id) + "] starting work.");
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          safe_print("T1(" + std::to_string(i) + ")");
      }
      safe_print("Task 1 [ID:" + std::to_string(t1_id) + "] finished work.");
  };

  // Task 2: Independent, takes 70ms
  auto work2 = [&]() {
      safe_print("Task 2 [ID:" + std::to_string(t2_id) + "] starting work.");
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          safe_print("T2(" + std::to_string(i) + ")");
      }
      safe_print("Task 2 [ID:" + std::to_string(t2_id) + "] finished work.");
  };

  // Task 3: Depends on T1, takes 40ms, INTENTIONALLY FAILS
  auto work3 = [&]() {
      safe_print("Task 3 [ID:" + std::to_string(t3_id) + "] starting work.");
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          safe_print("T3(" + std::to_string(i) + ")");
      }
      safe_print("Task 3 [ID:" + std::to_string(t3_id) + "] FAILED intentionally.");
      throw std::runtime_error("T3 failed intentionally");
  };

  // Task 4: Depends on T1 and T2, takes 60ms
  auto work4 = [&]() {
      safe_print("Task 4 [ID:" + std::to_string(t4_id) + "] starting work.");
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          safe_print("T4(" + std::to_string(i) + ")");
      }
      safe_print("Task 4 [ID:" + std::to_string(t4_id) + "] finished work.");
  };

  // Task 5: Depends on T3, takes 30ms (Should NOT run or should FAIL due to T3 failure)
  auto work5 = [&]() {
      // This should ideally not be printed if failure propagation works correctly
      safe_print("Task 5 [ID:" + std::to_string(t1_id) + "] starting work.");
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          safe_print("T5(" + std::to_string(i) + ")");
      }
      safe_print("Task 5 [ID:" + std::to_string(t1_id) + "] finished work.");
  };


  // --- Add Tasks to Scheduler ---
  safe_print("Adding tasks..");

  // Add Task 1 (no dependencies)
  t1_id = scheduler.addTask(work1);
  safe_print("Added Task 1 with ID: " + std::to_string(t1_id));

  // Add Task 2 (no dependencies)
  t2_id = scheduler.addTask(work2);
  safe_print("Added Task 2 with ID: " + std::to_string(t2_id));

  // Add Task 3 (depends on T1)
  const std::vector<TaskID> t3_deps = {t1_id};
  t3_id = scheduler.addTask(work3, t3_deps);
  safe_print("Added Task 3 with ID: " + std::to_string(t3_id) + " (Depends on " + std::to_string(t1_id) + ")");

  // Add Task 4 (depends on T1, T2)
  const std::vector<TaskID> t4_deps = {t1_id, t2_id};
  t4_id = scheduler.addTask(work4, t4_deps);
  safe_print("Added Task 4 with ID: " + std::to_string(t4_id) + " (Depends on " + std::to_string(t1_id)  + ", " + std::to_string(t2_id) + ")");

  // Add Task 5 (depends on T3)
  const std::vector<TaskID> t5_deps = {t3_id};
  t5_id = scheduler.addTask(work5, t5_deps);
  safe_print("Added Task 5 with ID: " + std::to_string(t5_id) + " (Depends on " + std::to_string(t3_id) + ")");

  // 4. Wait for tasks to likely complete
  // For a simple test, we just wait for a fixed duration.
  // A more robust solution would involve a scheduler.wait() method or
  // checking task statuses individually.
  safe_print("Waiting for tasks to process(aprox 3 seconds)");
  std::this_thread::sleep_for(std::chrono::seconds(3));

  safe_print("--- Scheduler Test Scenario End ---");
  return 0;
}

#endif