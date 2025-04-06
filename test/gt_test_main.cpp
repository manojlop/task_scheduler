#include <gtest/gtest.h>

#include "types_defines.h"

int main(int argc, char **argv){
  ::testing::InitGoogleTest(&argc, argv);

  // Todo : make it so that global variable that is passed to safe_print can bechanged from here -> to not output errors by itself

  return RUN_ALL_TESTS();
}