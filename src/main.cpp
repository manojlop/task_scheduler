#include "tests.h"
#include "types_defines.h"


int main() {
  int ret = 0;

  #ifdef __TEST_FAILURE_PROPAGATED__
  ret = test_failure_propagated();
  #elif __TEST_CONCURRENT_TASKS__
  ret = test_concurrent_tasks();
  #elif __TEST_DEPENDENCY_CHAIN__
  ret = test_dependency_chain();
  #elif __TEST_STRESS__
  ret = test_stress();
  #elif __TEST_SANITY__
  ret = test_sanity();
  #else 
    std::cout << "No test specified\n";
  #endif

  return ret;
}