#include <gtest/gtest.h>

#include "types_defines.h"

int main(int argc, char **argv){
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}