#ifdef __TEST_STRESS__

#include "tests.h"

std::atomic<int> completed_task_counter {0};

int test_stress(){
  safe_print("--- Stress test ---");

  const int num_worker_threads = 8;
  const int num_initial_tasks = 50;
  const int num_dependent_tasks = 500;
  Scheduler scheduler(num_worker_threads);
  safe_print(("Scheduler created with " + std::to_string(num_worker_threads) + " workers."));

  scheduler.start();
  safe_print("Scheduler started.");

  std::vector<TaskID> initial_ids(num_initial_tasks);
  std::vector<TaskID> dependent_ids(num_dependent_tasks);

  safe_print("\nAdding initial independent tasks...");
  for(int i = 0; i < num_initial_tasks; ++i) {
      auto work = [i, &initial_ids]() {
          TaskID current_id = initial_ids[i];
          // std::string prefix = "InitTask " + std::to_string(i) + "[ID:" + std::to_string(current_id) + "]";
          // safe_print(" starting.", prefix);
          std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 3)*5)); // Short work
          // safe_print(" finished.", prefix);
          completed_task_counter++;
      };
      initial_ids[i] = scheduler.addTask(work);
  }
    safe_print(("Added " + std::to_string(num_initial_tasks) + " initial tasks."));

  safe_print("\nAdding dependent tasks...");
    for(int i = 0; i < num_dependent_tasks; ++i) {
      // Each dependent task depends on one of the initial tasks
      TaskID dependency = initial_ids[i % num_initial_tasks];
      auto work = [i, dependency, &dependent_ids]() {
            TaskID current_id = dependent_ids[i];
          // std::string prefix = "DepTask " + std::to_string(i) + "[ID:" + std::to_string(current_id) + "]";
          // safe_print((" starting (needs " + std::to_string(dependency) + ")."), prefix);
          std::this_thread::sleep_for(std::chrono::milliseconds(5 + (i % 2)*5)); // Very short work
          // safe_print(" finished.", prefix);
          completed_task_counter++;
      };
      dependent_ids[i] = scheduler.addTask(work, {dependency});
  }
  safe_print(("Added " + std::to_string(num_dependent_tasks) + " dependent tasks."));


  // Wait based on rough estimate
  long long wait_ms = 2000;
  safe_print(("\nWaiting approx " + std::to_string(wait_ms) + " ms for tasks..."));
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

  safe_print("\nRequesting scheduler stop...");
  scheduler.stop();

  int final_count = completed_task_counter.load();
  safe_print(("\nApproximate completed tasks: " + std::to_string(final_count) + " / " + std::to_string(num_initial_tasks + num_dependent_tasks)));
  if (final_count != (num_initial_tasks + num_dependent_tasks)) {
        safe_print("WARNING: Not all tasks may have completed (or counter is wrong)!", "General", WARNING);
  }

  scheduler.printTaskCollection(INFO);

  safe_print("--- Stress test End ---");
  return 0;
}

#endif