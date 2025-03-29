#!/usr/bin/env python3

import argparse
import subprocess
import os

def run_command(command):
    """Helper function to run a shell command."""
    print(f"Running: {' '.join(command)}")
    result = subprocess.run(command)

def main():
    parser = argparse.ArgumentParser(description="Run the compiled project or a quick spike.")
    parser.add_argument("-t", "--target", help="Specify the target executable to run", default="main")
    parser.add_argument("-q", "--quick", help="Run quick mode (spike)", action="store_true")
    
    args = parser.parse_args()
    build_dir = "build/bin"
    
    if args.quick:
        executable = os.path.join(build_dir, "spike")
    else:
        executable = os.path.join(build_dir, args.target if args.target else "main")
    
    if not os.path.exists(executable):
        print(f"Error: Executable {executable} not found. Did you compile it?")
        exit(1)
    
    # Run the executable
    run_command([executable])
    
if __name__ == "__main__":
    main()
