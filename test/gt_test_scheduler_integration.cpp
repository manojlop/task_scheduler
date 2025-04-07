#include "gtest/gtest.h"
#include "scheduler.h"
#include "types_defines.h"

#include <future> // For std::promise, std::future
#include <chrono>
#include <vector>
#include <string>
#include <atomic>

// Fixture for integration tests
class SchedulerTest_Integration_Test : public ::testing::Test{
protected:
  // Allow creating scheduler with different thread counts if needed
  std::unique_ptr<Scheduler> scheduler_;

  void createScheduler(int num_threads){
    scheduler_ = std::unique_ptr<Scheduler>(new Scheduler(num_threads));
  }

  void TearDown() override {
    // Ensure scheduler is stopped even if test fails
    if(scheduler_) {
      scheduler_->stop();
    }
  }

  void waitTasksToFinish() {
    scheduler_->waitTasksToEnd();
  }
};


TEST_F(SchedulerTest_Integration_Test, SimpleDependencyOrder) {
  createScheduler(2);

  // We are setting up synchronization mechanism between two tasks
  std::promise<void> task1_finished_promise; // Will signal when task1 is finished (void -> it doesn't deliver a value, just notification)
  std::promise<void> task2_started_promise;
  // std::future<void> task1_finished_future = task1_finished_promise.get_future(); // Future object which will become ready when task1_finished_promise is fulfilled
  // std::future<void> task2_started_future = task2_started_promise.get_future();

  // Create a shared future for task1 so that it can be queried multiple times (Normal can be consumed only once)
  std::shared_future<void> task1_shared_future = task1_finished_promise.get_future().share();
  std::shared_future<void> task2_started_future = task2_started_promise.get_future().share();

  
  // Flag to track ordering violation
  std::mutex mutex;
  bool ordering_violation = false;

  // Task1
  auto work1 = [&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    task1_finished_promise.set_value(); // Signal completion
  };

  auto work2 = [&]() {
    // Check if task1 has finished by testing future status
    // If ready() returns false, task1 hasn't signaled completion yet
    if (task1_shared_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      std::lock_guard<std::mutex> lock(mutex);
      ordering_violation = true;
    }

    task2_started_promise.set_value(); // Signal start
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  };

  scheduler_->start(); // Start workers before addign tasks

  TaskID id1 = scheduler_->addTask(work1);
  TaskID id2 = scheduler_->addTask(work2, {id1});

  // Wait for Task1 to finish (with a timeout)
  auto status1 = task1_shared_future.wait_for(std::chrono::seconds(1));
  ASSERT_EQ(status1, std::future_status::ready) << "Task 1 did not finish in time.";

  // Then wait for Task2 to start (with a timeout)
  auto status2 = task2_started_future.wait_for(std::chrono::seconds(1));
  ASSERT_EQ(status2, std::future_status::ready) << "Task 2 did not start after task1 finished.";

  // Allow task2 to finish 
  scheduler_->waitTasksToEnd();

  ASSERT_FALSE(ordering_violation) << "Task2 started before Task 1 finished - dependency violation!";

  // Stop is called by TearDown
}

TEST_F(SchedulerTest_Integration_Test, AllTasksCompleteLoadTest){
  createScheduler(8);
  const int num_tasks = 5000;
  std::atomic<int> completed_count{0};

  std::vector<TaskID> ids(num_tasks);

  scheduler_->start();

  // Add tasks (some with simple dependencies)
  for(int i = 0; i < num_tasks; i++){
    std::vector<TaskID> deps;
    if(i > 1) {
      srand(time(0));
      int num_of_dep = rand() % 10;
      for(int j = 0; j < num_of_dep; j++) {
        deps.push_back(ids[rand() % i]);
      }
    }
    auto work = [&completed_count]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(1 + (rand() % 10)));
      completed_count.fetch_add(1, std::memory_order_relaxed);
    };
    ids[i] = scheduler_->addTask(work, deps);
  }

  scheduler_->waitTasksToEnd();

  scheduler_->stop();

  EXPECT_EQ(completed_count.load(), num_tasks);
}