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