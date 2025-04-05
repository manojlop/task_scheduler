#ifdef __TEST_CONCURRENT_TASKS__

#include "tests.h"

int test_concurrent_tasks(){
  safe_print("--- Running concurrent test ---");

  const int num_worker_threads = 4; // Use more threads
  const int num_tasks = 8;        // Add more tasks than threads
  Scheduler scheduler(num_worker_threads);
  safe_print(("Scheduler created with " + std::to_string(num_worker_threads) + " workers."));

  scheduler.start();
  safe_print("Scheduler started.");

  std::vector<TaskID> task_ids(num_tasks);

  safe_print("Adding independent tasks...");
  for (int i = 0; i < num_tasks; ++i) {
      // Create lambda for each task
      auto work = [i, &task_ids]() {
          // Note: Capturing task_ids by reference is okay here because main waits.
          // The ID isn't known until *after* addTask returns, so we print the loop index 'i'.
            TaskID current_id = task_ids[i]; // Read the ID after it's assigned
            std::string worker_prefix = "Task " + std::to_string(i) + "[ID:" + std::to_string(current_id) + "]";
            safe_print(" starting work.", worker_prefix);
            // Simulate variable work time
            std::this_thread::sleep_for(std::chrono::milliseconds(50 + (i * 15)));
            safe_print(" finished work.", worker_prefix);
      };
      // Add task with no dependencies
      task_ids[i] = scheduler.addTask(work); // Assign the ID after adding
      safe_print(("Added Task " + std::to_string(i) + " with ID: " + std::to_string(task_ids[i])));
  }

  // Estimate wait time - longest task is ~ 50 + (7*15) = 155ms.
  // With 4 threads, they should finish faster than sequential. Wait a bit longer.
  long long wait_ms = 300;
  safe_print(("Waiting approx " + std::to_string(wait_ms) + " ms for tasks..."));
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

  safe_print("--- Concurrent test: End ---");
  return 0;
}

#endif
