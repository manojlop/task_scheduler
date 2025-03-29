#!/usr/bin/env python3
import argparse
import subprocess
import sys
import os

def main():
    parser = argparse.ArgumentParser(description="Build and run the project.")
    
    # Changed --define to use action="append" to handle multiple definitions
    parser.add_argument("-d", "--define", 
                        help="Set compile definitions for Project (can be used multiple times)",
                        action="append", dest="definitions")
    parser.add_argument("-q", "--quick", 
                        help="Enable quick compilation mode", 
                        action="store_true")
    parser.add_argument("-t", "--target", 
                        help="Target filename for compilation and execution", 
                        default=None)
    parser.add_argument("-dbg", "--debug", 
                        help="Enable debug mode with debug flags", 
                        action="store_true")
    
    # Get the directory of the current script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Parse arguments
    args = parser.parse_args()

    clean_script = os.path.join(script_dir, "clean.py")
    clean_command = [sys.executable, clean_script]

    print(f"Running clean script: {' '.join(clean_command)}")
    build_result = subprocess.run(clean_command)
    
    # Construct command for the build script
    build_script = os.path.join(script_dir, "compile.py")
    build_command = [sys.executable, build_script]
    
    # Pass each definition as a separate -d argument to the build script
    if args.definitions:
        for definition in args.definitions:
            build_command.extend(["-d", definition])
    
    if args.quick:
        build_command.append("-q")
    if args.target:
        build_command.extend(["-t", args.target])
    if args.debug:
        build_command.append("-dbg")
    
    # Run the build script
    print(f"Running build script: {' '.join(build_command)}")
    build_result = subprocess.run(build_command)
    
    # Check if build was successful
    if build_result.returncode != 0:
        print("Build failed. Aborting run.")
        sys.exit(build_result.returncode)
    
    # Construct command for the run script
    run_script = os.path.join(script_dir, "run.py")
    run_command = [sys.executable, run_script]
    
    if args.quick:
        run_command.append("-q")
    if args.target:
        run_command.extend(["-t", args.target])
    
    # Run the run script
    print(f"Running run script: {' '.join(run_command)}")
    run_result = subprocess.run(run_command)
    
    # Return the exit code from the run script
    sys.exit(run_result.returncode)

if __name__ == "__main__":
    main()
