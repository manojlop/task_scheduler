#include <gtest/gtest.h>
#include <task.h>
#include <types_defines.h>

// Test fixture for Task tests
class TaskTest : public ::testing::Test {};

// Defines an individual test named ConstructionNoDeps, that uses the test fixture class TaskTest. Test suite is named TaskTest
TEST_F(TaskTest, ConstructionNoDeps) {
  TaskID id = 10;
  Task task(id, [](){}, {}); // no dependencies, no function
  ASSERT_EQ(task.getID(), id);
  ASSERT_EQ(task.getState(), t_TaskState::READY);
  ASSERT_EQ(task.getUnmetCount(), 0);
}

TEST_F(TaskTest, ConstructionWithDeps){
  TaskID id = 11;
  std::vector<TaskID> deps = {1, 2};
  Task task(id, [](){}, deps);
  ASSERT_EQ(task.getID(), id);
  ASSERT_EQ(task.getState(), t_TaskState::PENDING);
  ASSERT_EQ(task.getUnmetCount(), deps.size());
}

TEST_F(TaskTest, DecrementDependencies) {
  TaskID id = 12;
  std::vector<TaskID> deps = {1, 2};
  Task task(id, [](){}, deps); // Starts with unmetCount = 2

  ASSERT_EQ(task.getUnmetCount(), 2);
  ASSERT_FALSE(task.decrement_unmet_dependencies()); // Count becomes 1
  ASSERT_EQ(task.getUnmetCount(), 1);
  ASSERT_TRUE(task.decrement_unmet_dependencies());
  ASSERT_EQ(task.getUnmetCount(), 0);

  // Check subsequent calls don't go below zero -> But they never should be called (how to check that?)
  // Todo : Decide what should the behavior be
}

TEST_F(TaskTest, SetStateValidation){
  TaskID id = 13;
  Task task(id, [](){}, {1}); // PENDING
  ASSERT_EQ(task.getState(), t_TaskState::PENDING);

  // Valid transitions
  EXPECT_FALSE(task.setStateReady());
  ASSERT_EQ(task.getState(), t_TaskState::PENDING);
  EXPECT_TRUE(task.decrement_unmet_dependencies());
  ASSERT_EQ(task.getUnmetCount(), 0);
  EXPECT_TRUE(task.setStateReady());
  ASSERT_EQ(task.getState(), t_TaskState::READY);
  EXPECT_TRUE(task.setStateRunning());
  ASSERT_EQ(task.getState(), t_TaskState::RUNNING);
  EXPECT_TRUE(task.setStateCompleted());
  ASSERT_EQ(task.getState(), t_TaskState::COMPLETED);

  // Invalid from COMPLETED
  EXPECT_FALSE(task.setStateRunning());
  ASSERT_EQ(task.getState(), t_TaskState::COMPLETED);

  Task task2(14, [](){}, {}); //READY

  EXPECT_FALSE(task2.setStatePending());
  ASSERT_EQ(task2.getState(), t_TaskState::READY);

}