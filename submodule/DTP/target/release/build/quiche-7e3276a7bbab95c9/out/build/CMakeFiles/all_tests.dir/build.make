# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.14

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.14.4/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.14.4/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/deps/boringssl

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/target/release/build/quiche-7e3276a7bbab95c9/out/build

# Utility rule file for all_tests.

# Include the progress variables for this target.
include CMakeFiles/all_tests.dir/progress.make

all_tests: CMakeFiles/all_tests.dir/build.make

.PHONY : all_tests

# Rule to build all files generated by this target.
CMakeFiles/all_tests.dir/build: all_tests

.PHONY : CMakeFiles/all_tests.dir/build

CMakeFiles/all_tests.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/all_tests.dir/cmake_clean.cmake
.PHONY : CMakeFiles/all_tests.dir/clean

CMakeFiles/all_tests.dir/depend:
	cd /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/target/release/build/quiche-7e3276a7bbab95c9/out/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/deps/boringssl /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/deps/boringssl /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/target/release/build/quiche-7e3276a7bbab95c9/out/build /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/target/release/build/quiche-7e3276a7bbab95c9/out/build /Users/lihongsheng/Desktop/About_DTP/submodule/DTP/target/release/build/quiche-7e3276a7bbab95c9/out/build/CMakeFiles/all_tests.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/all_tests.dir/depend

