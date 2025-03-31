#include "tests.h"

std::mutex globalMutex;

void safe_print(const std::string& msg){
    std::lock_guard<std::mutex> lock(globalMutex); 
    std::cout << msg << std::endl;
}

int main() {
  int ret = sanity_test();

  return ret;
}