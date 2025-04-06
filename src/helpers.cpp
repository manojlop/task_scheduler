#include "types_defines.h"

#include <iostream>
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