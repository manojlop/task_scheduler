#include "tests.h"


int failure_test() {
  std::cout << "--- Scheduler Test Scenario Start ---" << std::endl;

  // 1. Create Scheduler instance (e.g., with 3 worker threads)
  const int num_worker_threads = 3;
  Scheduler scheduler(num_worker_threads);
  std::cout << "Scheduler created with " << num_worker_threads << " worker threads." << std::endl;

  // 2. Start the scheduler threads
  scheduler.start();
  std::cout << "Scheduler started." << std::endl;

  // 3. Define Tasks and Dependencies
  TaskID t1_id, t2_id, t3_id, t4_id, t5_id;

  // --- Task Definitions (using Lambdas) ---

  // Task 1: Independent, takes 50ms
  auto work1 = [&]() {
      std::cout << "Task 1 [ID:" << t1_id << "] starting work." << std::endl;
      for (int i = 0; i < 5; ++i) {
          // Simulate work
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          std::cout << " T1(" << i << ")"; // Optional inner loop print
      }
      std::cout << "Task 1 [ID:" << t1_id << "] finished work." << std::endl;
  };

  // Task 2: Independent, takes 70ms
  auto work2 = [&]() {
      std::cout << "Task 2 [ID:" << t2_id << "] starting work." << std::endl;
      for (int i = 0; i < 7; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          std::cout << " T2(" << i << ")";
      }
      std::cout << "Task 2 [ID:" << t2_id << "] finished work." << std::endl;
  };

  // Task 3: Depends on T1, takes 40ms, INTENTIONALLY FAILS
  auto work3 = [&]() {
      std::cout << "Task 3 [ID:" << t3_id << "] starting work (depends on T1)." << std::endl;
      for (int i = 0; i < 4; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          std::cout << " T3(" << i << ")";
      }
      std::cout << "Task 3 [ID:" << t3_id << "] FAILED intentionally." << std::endl;
      // throw std::runtime_error("T3 failed intentionally");
  };

  // Task 4: Depends on T1 and T2, takes 60ms
  auto work4 = [&]() {
      std::cout << "Task 4 [ID:" << t4_id << "] starting work (depends on T1, T2)." << std::endl;
        for (int i = 0; i < 6; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          std::cout << " T4(" << i << ")";
      }
      std::cout << "Task 4 [ID:" << t4_id << "] finished work." << std::endl;
  };

  // Task 5: Depends on T3, takes 30ms (Should NOT run or should FAIL due to T3 failure)
  auto work5 = [&]() {
      // This should ideally not be printed if failure propagation works correctly
      std::cout << "Task 5 [ID:" << t5_id << "] starting work (depends on T3)." << std::endl;
      for (int i = 0; i < 3; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          std::cout << " T5(" << i << ")";
      }
      std::cout << "Task 5 [ID:" << t5_id << "] finished work." << std::endl;
  };


  // --- Add Tasks to Scheduler ---
  std::cout << "\nAdding tasks..." << std::endl;

  // Add Task 1 (no dependencies)
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

  // Add Task 5 (depends on T3)
  const std::vector<TaskID> t5_deps = {t3_id};
  t5_id = scheduler.addTask(work5, t5_deps);
  std::cout << "Added Task 5 with ID: " << t5_id << " (Depends on " << t3_id << ")" << std::endl;


  // 4. Wait for tasks to likely complete
  // For a simple test, we just wait for a fixed duration.
  // A more robust solution would involve a scheduler.wait() method or
  // checking task statuses individually.
  std::cout << "\nWaiting for tasks to process (approx 3 seconds)..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));


  // 5. Stop the scheduler
  std::cout << "\nRequesting scheduler stop..." << std::endl;
  scheduler.stop(); // This will wait for running tasks to finish and join threads


  // 6. (Optional) Check final task states if Scheduler provides a getter
  // Example: scheduler.getTaskState(t5_id) -> should be FAILED or CANCELLED


  std::cout << "\n--- Scheduler Test Scenario End ---" << std::endl;
  return 0;
}