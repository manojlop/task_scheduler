#include <iostream>

int main() {
    std::cout << "Hello, CMake! from spike" << std::endl;
    #ifdef __FLAG
      std::cout << "FLAG SET" << std::endl;
    #endif
    return 0;
}
