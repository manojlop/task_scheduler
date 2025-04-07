#include "types_defines.h"

#include <iostream>
#include <iomanip>

std::mutex globalMutex;

t_Verbosity verbosityPrinted = t_Verbosity::INFO;

void safe_print(std::string msg, std::string name, t_Verbosity verbosity){
    std::lock_guard<std::mutex> lock(globalMutex);
    if(verbosity <= verbosityPrinted) {
      std::cout << std::left 
      << std::setw(7) 
      <<
      ( (verbosity == t_Verbosity::DEBUG)    ? "DEBUG" :
        (verbosity == t_Verbosity::INFO)     ? "INFO" : 
        (verbosity == t_Verbosity::WARNING)  ? "WARNING" :
        (verbosity == t_Verbosity::ERROR)    ? "ERROR" : 
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