#ifdef __TEST_DEPENDENCY_CHAIN__

#include "tests.h"

int test_dependency_chain(){
  safe_print("--- Test dependency chain ---");

  const int num_worker_threads = 3;
  Scheduler scheduler(num_worker_threads);
  safe_print(("Scheduler created with " + std::to_string(num_worker_threads) + " workers."));

  scheduler.start();
  safe_print("Scheduler started.");

  TaskID t1, t2, t3, t4, t5, t6;

  auto make_work = [&](const std::string& name, TaskID& id_ref, int sleep_ms) {
      return [&, name, sleep_ms]() {
          TaskID current_id = id_ref; // Capture assigned ID
          std::string prefix = name + "[ID:" + std::to_string(current_id) + "]";
          safe_print(" starting work.", prefix);
          std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
          safe_print(" finished work.", prefix);
      };
  };

  safe_print("\nAdding tasks...");

  // Level 1 (Independent)
  t1 = scheduler.addTask(make_work("Task 1", t1, 80));
  t2 = scheduler.addTask(make_work("Task 2", t2, 60));
  safe_print(("Added T1(" + std::to_string(t1) + "), T2(" + std::to_string(t2) + ")"));

  // Level 2
  t3 = scheduler.addTask(make_work("Task 3", t3, 50), {t1});
  t4 = scheduler.addTask(make_work("Task 4", t4, 70), {t2});
  safe_print(("Added T3(" + std::to_string(t3) + "){dep T1}, T4(" + std::to_string(t4) + "){dep T2}"));

  // Level 3
  t5 = scheduler.addTask(make_work("Task 5", t5, 40), {t3, t4});
  safe_print(("Added T5(" + std::to_string(t5) + "){dep T3, T4}"));

  // Level 4
  t6 = scheduler.addTask(make_work("Task 6", t6, 30), {t5});
  safe_print(("Added T6(" + std::to_string(t6) + "){dep T5}"));


  // Estimate wait time: Longest path T2->T4->T5->T6 = 60+70+40+30 = 200ms. Add buffer.
  long long wait_ms = 500;
  safe_print(("\nWaiting approx " + std::to_string(wait_ms) + " ms for tasks..."));
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

  safe_print("\n--- Test dependency chain: End ---");
  return 0;
}

#endif