# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/adil/tntcxx-router

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/adil/tntcxx-router

# Include any dependencies generated for this target.
include CMakeFiles/MempoolUnit.test.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/MempoolUnit.test.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/MempoolUnit.test.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/MempoolUnit.test.dir/flags.make

CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o: CMakeFiles/MempoolUnit.test.dir/flags.make
CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o: test/MempoolUnitTest.cpp
CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o: CMakeFiles/MempoolUnit.test.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/adil/tntcxx-router/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o -MF CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o.d -o CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o -c /home/adil/tntcxx-router/test/MempoolUnitTest.cpp

CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/adil/tntcxx-router/test/MempoolUnitTest.cpp > CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.i

CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/adil/tntcxx-router/test/MempoolUnitTest.cpp -o CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.s

# Object files for target MempoolUnit.test
MempoolUnit_test_OBJECTS = \
"CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o"

# External object files for target MempoolUnit.test
MempoolUnit_test_EXTERNAL_OBJECTS =

MempoolUnit.test: CMakeFiles/MempoolUnit.test.dir/test/MempoolUnitTest.cpp.o
MempoolUnit.test: CMakeFiles/MempoolUnit.test.dir/build.make
MempoolUnit.test: libev.a
MempoolUnit.test: CMakeFiles/MempoolUnit.test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/adil/tntcxx-router/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable MempoolUnit.test"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/MempoolUnit.test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/MempoolUnit.test.dir/build: MempoolUnit.test
.PHONY : CMakeFiles/MempoolUnit.test.dir/build

CMakeFiles/MempoolUnit.test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/MempoolUnit.test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/MempoolUnit.test.dir/clean

CMakeFiles/MempoolUnit.test.dir/depend:
	cd /home/adil/tntcxx-router && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/adil/tntcxx-router /home/adil/tntcxx-router /home/adil/tntcxx-router /home/adil/tntcxx-router /home/adil/tntcxx-router/CMakeFiles/MempoolUnit.test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/MempoolUnit.test.dir/depend

