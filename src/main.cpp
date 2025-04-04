#include "tests.h"


#include <iomanip>

std::mutex globalMutex;

t_Verbosity verbosityPrinted = INFO;

void safe_print(std::string msg, std::string name, t_Verbosity verbosity){
    std::lock_guard<std::mutex> lock(globalMutex);
    if(verbosity <= verbosityPrinted) {
      std::cout << std::left 
      << std::setw(7) 
      <<
      ( (verbosity == DEBUG)    ? "DEBUG" :
        (verbosity == INFO)     ? "INFO" : 
        (verbosity == WARNING)  ? "WARNING" :
        (verbosity == ERROR)    ? "ERROR" : 
                                  "NONE" 
      ) 
      << " | " 
      << std::setw(15) 
      << name  
      << " | " 
      << msg 
      << std::endl;
    }
}

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
  #else
  ret = test_sanity();
  #endif

  return ret;
}