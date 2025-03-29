#!/usr/bin/env python3

import argparse
import subprocess
import os

def run_command(command):
    """Helper function to run a shell command."""
    print(f"Running: {' '.join(command)}")
    result = subprocess.run(command)

def main():
    parser = argparse.ArgumentParser(description="Compile and build project using CMake.")
    
    # Changed from default=None to action="append" to collect multiple definitions
    parser.add_argument("-d", "--define", help="Set compile definitions for Project (can be used multiple times)", 
                        action="append", dest="definitions")
    parser.add_argument("-q", "--quick", help="Enable quick compilation mode", action="store_true")
    parser.add_argument("-t", "--target", help="Target filename for quick compilation (used with -q)", default=None)
    parser.add_argument("-dbg", "--debug", help="Enable debug mode with debug flags", action="store_true")
    
    args = parser.parse_args()
    build_dir = "build"
    os.makedirs(build_dir, exist_ok=True)
    
    cmake_command = ["cmake", "-B", build_dir, "-S", "."]
    
    # Handle multiple definitions by adding them to CMAKE_CXX_FLAGS
    if args.definitions:
        # Join all definitions with proper formatting
        definitions_str = " ".join([f"-D{definition}" for definition in args.definitions])
        cmake_command.append(f"-DCMAKE_CXX_FLAGS={definitions_str}")
    
    if args.quick:
        if not args.target:
            print("Error: Quick mode (-q) requires a target filename (-t).")
            exit(1)
        cmake_command.append("-DQ=ON")
        cmake_command.append(f"-DQUICK={args.target}")
    
    if args.debug:
        cmake_command.append("-DCMAKE_BUILD_TYPE=Debug")
    
    # Run CMake configuration
    run_command(cmake_command)
    
    # Build the project
    build_command = ["cmake", "--build", build_dir]
    run_command(build_command)
    
    print("Build complete!")

if __name__ == "__main__":
    main()
