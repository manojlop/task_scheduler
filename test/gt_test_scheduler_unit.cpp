#include <gtest/gtest.h>
#include <types_defines.h>
#include <scheduler.h>

// Fixture to create a Scheduler instance for tests
class SchedulerTest : public ::testing::Test{
protected:
  // We use consistent number of threads for setup, but they won't run
  Scheduler scheduler_{2};

  // Helper to access internal tasks via friend declaration
  std::shared_ptr<Task> getTask(TaskID id){
    auto it = scheduler_.tasks_.find(id);
    return (it != scheduler_.tasks_.end()) ? it->second : nullptr;
  }

  // We use reference to avoid unnecessary copies of deque
  // As readyTasks_ always exists, we can return reference
  const std::deque<TaskID>& getReadyQueue() const {
    return scheduler_.readyTasks_;
  }

  // We use pointer to avoid unnecessary copies of vector
  // We must return pointer, not reference in case there is no instance to return
  // We don't need smart pointer here, as we are just granting read access
  const std::vector<TaskID>* getDependents(TaskID id) {
    auto it = scheduler_.downwardDependencies_.find(id);
    return (it != scheduler_.downwardDependencies_.end()) ? &(it->second) : nullptr;
  }

  // Return weather task map has entries
  bool tasksPresent(){
    return scheduler_.tasks_.empty();
  }

  void waitLockMutex(){
    std::lock_guard<std::mutex> lock(scheduler_.mtx_);
  }
};

class SchedulerTest_AddTask_Test : public SchedulerTest {};

// Test suite for addTask
TEST_F(SchedulerTest_AddTask_Test, AddTaskNoDeps){
  bool task_run = false;
  TaskID id = scheduler_.addTask([&](){ task_run = true; });

  ASSERT_GT(id, 0); // Should get valid positive ID
  auto task_ptr = getTask(id);
  ASSERT_NE(task_ptr, nullptr); // task should be in the map
  EXPECT_EQ(task_ptr->getState(), t_TaskState::READY); // No dependencies -> READY
  EXPECT_EQ(task_ptr->getUnmetCount(), 0);

  const auto& ready_queue = getReadyQueue();
  ASSERT_EQ(ready_queue.size(), 1);
  EXPECT_EQ(ready_queue.front(), id);

  EXPECT_EQ(getDependents(id), nullptr);
}

TEST_F(SchedulerTest_AddTask_Test, AddTaskWithValidDeps) {
  TaskID id1 = scheduler_.addTask([](){});
  ASSERT_GT(id1, 0);

  TaskID id2 = scheduler_.addTask([](){}, {id1});
  ASSERT_GT(id2, 0);

  auto task1_ptr = getTask(id1);
  auto task2_ptr = getTask(id2);
  ASSERT_NE(task1_ptr, nullptr);
  ASSERT_NE(task2_ptr, nullptr);

  EXPECT_EQ(task1_ptr->getState(), t_TaskState::READY);
  EXPECT_EQ(task2_ptr->getState(), t_TaskState::PENDING); // has dependency
  EXPECT_EQ(task2_ptr->getUnmetCount(), 1);

  const auto& ready_queue = getReadyQueue();
  ASSERT_EQ(ready_queue.size(), 1);
  EXPECT_EQ(ready_queue.front(), id1);

  //Check dependency map
  const auto* dependendts_of_1 = getDependents(id1);
  ASSERT_NE(dependendts_of_1, nullptr);
  ASSERT_EQ(dependendts_of_1->size(), 1);
  EXPECT_EQ(dependendts_of_1->at(0), id2); // Task 2 depends on Task 1
}

TEST_F(SchedulerTest_AddTask_Test, AddTaskWithInvalidDeps){
  TaskID invalid_dep_id = 999;
  TaskID id = scheduler_.addTask([](){}, {invalid_dep_id});

  EXPECT_EQ(id, -2); // Expect error code for invalid dependency
  ASSERT_TRUE(tasksPresent());
}

// Todo : Add test fo cycle detecten when fully implemented

TEST_F(SchedulerTest_AddTask_Test, AddTaskWithPreCompletedDeps){
  TaskID id1 = scheduler_.addTask([](){});
  auto task1_ptr = getTask(id1);
  ASSERT_NE(task1_ptr, nullptr);

  // Manually set task 1 to completed
  {
    // This cannot be done, because GoogleTest creates class: SchedulerTest_AddTask_Test_AddTaskWithPreCompletedDeps and it isn't a friend
    // std::lock_guard<std::mutex> lock(scheduler_.mtx_); // Need lock to modify state safely
    waitLockMutex();
    task1_ptr->setStateCompleted();
  }

  TaskID id2 = scheduler_.addTask([](){}, {id1}); //add task depending on completed one
  ASSERT_GT(id2, 0);
  auto task2_ptr = getTask(id2);
  ASSERT_NE(task2_ptr, nullptr);

  EXPECT_EQ(task2_ptr->getState(), t_TaskState::READY); // should become ready immeditaly
  EXPECT_EQ(task2_ptr->getUnmetCount(), 0); // Count should be decrmented during add

  const auto& ready_queue = getReadyQueue();
  // Ready queue might cotnain id1 still, as we simulated completion but havent executed it
  bool id2_found = false;
  for(TaskID id : ready_queue) {
    if (id == id2) 
      id2_found = true;
  }
  EXPECT_TRUE(id2_found);
}

class SchedulerTest_Notify_Test : public SchedulerTest {
protected:
  void callNotifyDependents(TaskID id) {
    scheduler_.notifyDependents(id);
  }
};

TEST_F(SchedulerTest_Notify_Test, NotifyDependentsSuccess){
  TaskID id1 = scheduler_.addTask([](){});
  TaskID id2 = scheduler_.addTask([](){}, {id1});
  TaskID id3 = scheduler_.addTask([](){}, {id1});

  auto task1_ptr = getTask(id1);
  auto task2_ptr = getTask(id2);
  auto task3_ptr = getTask(id3);
  // Simulate task 1 completing
  {
    waitLockMutex();
    task1_ptr->setStateCompleted();
    // Call the notification logic (assuming it's private and doesn't lock)
    callNotifyDependents(id1);
  }

  // Check state of dependents
  EXPECT_EQ(task2_ptr->getState(), t_TaskState::READY);
  EXPECT_EQ(task3_ptr->getState(), t_TaskState::READY);
  EXPECT_EQ(task2_ptr->getUnmetCount(), 0);
  EXPECT_EQ(task3_ptr->getUnmetCount(), 0);

  // Check ready queue
  const auto& ready_queue = getReadyQueue();
  ASSERT_EQ(ready_queue.size(), 3); // id1, id2, id3 (order might vary)
}

TEST_F(SchedulerTest_Notify_Test, NotifyDependentsFailure){
  TaskID id1 = scheduler_.addTask([](){});
  TaskID id2 = scheduler_.addTask([](){}, {id1});
  TaskID id3 = scheduler_.addTask([](){}, {id2}); // Chain: 1 -> 2 -> 3

  auto task1_ptr = getTask(id1);
  auto task2_ptr = getTask(id2);
  auto task3_ptr = getTask(id3);

  // Simulate task 1 failing
  {
    waitLockMutex();
    task1_ptr->setStateFailed();
    // Call notification logic
    callNotifyDependents(id1);
  }

  // Check state of dependents - failure should propagate
  EXPECT_EQ(task2_ptr->getState(), t_TaskState::FAILED);
  EXPECT_EQ(task3_ptr->getState(), t_TaskState::FAILED); // Because 2 failed

  // Check ready queue (should be empty or just contain id1 if not cleared)
  const auto& ready_queue = getReadyQueue();
  int only_task1 = false;
  for(TaskID id : ready_queue){
    if(id == id1)
      only_task1 = true;
    else 
      only_task1 = false;
  }
  ASSERT_TRUE(only_task1);
}
