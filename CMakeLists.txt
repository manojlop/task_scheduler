# Minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(MyProject VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set output directories for better organization
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find all .cpp files in the src directory
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

# Check if lib directory exists and has source files
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    file(GLOB_RECURSE LIB_SOURCE_FILES "lib/*.cpp")
    if(LIB_SOURCE_FILES)
        # Create a library from the lib sources
        add_library(MyLibrary STATIC ${LIB_SOURCE_FILES})
        target_include_directories(MyLibrary PUBLIC include)
        
        # Apply the same compile options to the library
        target_compile_options(MyLibrary PRIVATE 
            -Wall
            -Wextra
            -pedantic
        )
        
        # Define preprocessor macros for the library
        target_compile_definitions(MyLibrary PRIVATE 
            __FLAG
        )
    endif()
endif()

# Add the executable with all source files
add_executable(main ${SOURCE_FILES})

# Add include directories
target_include_directories(main PUBLIC include)

# Link with library if it exists
if(TARGET MyLibrary)
    target_link_libraries(main PRIVATE MyLibrary)
endif()

# Add compile options to a specific target
target_compile_options(main PRIVATE 
    -Wall               # Enable all warnings
    -Wextra             # Enable extra warnings
    -pedantic           # Ensure compliance with language standards
    -O0
    -fno-elide-constructors
)

# Define preprocessor macros
target_compile_definitions(main PRIVATE 
    EXAMPLE_DEFINE
)

# Apply the -save-temps flag globally as in your original file
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -save-temps -masm=intel")

# Add optimization level options
# # Optimization Levels
# # No optimization - easiest to understand
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
# # Moderate optimization
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
# # Aggressive optimization - hardest to understand
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Add test support if test directory exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    file(GLOB_RECURSE TEST_SOURCE_FILES "test/*.cpp")
    if(TEST_SOURCE_FILES)
        enable_testing()
        add_executable(MyTests ${TEST_SOURCE_FILES})
        target_include_directories(MyTests PRIVATE include)
        
        # Apply the same compile options to tests
        target_compile_options(MyTests PRIVATE 
            -Wall
            -Wextra
            -pedantic
        )
        
        # Define the same preprocessor macros for tests
        target_compile_definitions(MyTests PRIVATE 
            EXAMPLE_DEFINE
            TESTING
        )
        
        # Link with library if it exists
        if(TARGET MyLibrary)
            target_link_libraries(MyTests PRIVATE MyLibrary)
        endif()
        
        # Add each test as a CTest test
        add_test(NAME AllTests COMMAND MyTests)
    endif()
endif()

# Option to enable quick compilation
option(Q "Enable quick compilation for a specific file" OFF)
set(QUICK "" CACHE STRING "Filename for quick compilation")

# Ensure quick mode is enabled and a filename is provided
if(Q AND NOT "${QUICK}" STREQUAL "")
    message(STATUS "Quick mode enabled for ${QUICK}")

    # Extract filename without extension
    get_filename_component(SPIKE_NAME "${QUICK}" NAME_WE)

    # Construct full path
    set(SPIKE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/spike/${QUICK}")

    if(EXISTS "${SPIKE_SOURCE}")
        add_executable(spike ${SPIKE_SOURCE} ${SOURCE_FILES})
        target_include_directories(spike PRIVATE include)

        # Apply compile options to spike executable
        target_compile_options(spike_${SPIKE_NAME} PRIVATE 
            -Wall
            -Wextra
            -pedantic
        )

        # Define preprocessor macros
        target_compile_definitions(spike_${SPIKE_NAME} PRIVATE 
            EXAMPLE_DEFINE
            EXPERIMENTAL
        )

        # Link with MyLibrary if it exists
        if(TARGET MyLibrary)
            target_link_libraries(spike_${SPIKE_NAME} PRIVATE MyLibrary)
        endif()

    else()
        message(FATAL_ERROR "Specified file ${SPIKE_SOURCE} does not exist.")
    endif()
else()
    message(STATUS "Quick mode not enabled or no filename provided.")
endif()


# Add an option to easily enable/disable features from command line with cmake -DENABLE_FEATURE
option(ENABLE_FEATURE "Enable a specific feature" OFF)
if(ENABLE_FEATURE)
    target_compile_definitions(main PRIVATE FEATURE_ENABLED)
    if(TARGET MyLibrary)
        target_compile_definitions(MyLibrary PRIVATE FEATURE_ENABLED)
    endif()
endif()