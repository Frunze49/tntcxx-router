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
CMAKE_SOURCE_DIR = /home/adil/tntcxx-router/examples

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/adil/tntcxx-router/examples

# Include any dependencies generated for this target.
include CMakeFiles/unix_server.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/unix_server.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/unix_server.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/unix_server.dir/flags.make

CMakeFiles/unix_server.dir/unix_server.cpp.o: CMakeFiles/unix_server.dir/flags.make
CMakeFiles/unix_server.dir/unix_server.cpp.o: unix_server.cpp
CMakeFiles/unix_server.dir/unix_server.cpp.o: CMakeFiles/unix_server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/adil/tntcxx-router/examples/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/unix_server.dir/unix_server.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/unix_server.dir/unix_server.cpp.o -MF CMakeFiles/unix_server.dir/unix_server.cpp.o.d -o CMakeFiles/unix_server.dir/unix_server.cpp.o -c /home/adil/tntcxx-router/examples/unix_server.cpp

CMakeFiles/unix_server.dir/unix_server.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/unix_server.dir/unix_server.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/adil/tntcxx-router/examples/unix_server.cpp > CMakeFiles/unix_server.dir/unix_server.cpp.i

CMakeFiles/unix_server.dir/unix_server.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/unix_server.dir/unix_server.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/adil/tntcxx-router/examples/unix_server.cpp -o CMakeFiles/unix_server.dir/unix_server.cpp.s

# Object files for target unix_server
unix_server_OBJECTS = \
"CMakeFiles/unix_server.dir/unix_server.cpp.o"

# External object files for target unix_server
unix_server_EXTERNAL_OBJECTS =

unix_server: CMakeFiles/unix_server.dir/unix_server.cpp.o
unix_server: CMakeFiles/unix_server.dir/build.make
unix_server: libev.a
unix_server: CMakeFiles/unix_server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/adil/tntcxx-router/examples/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable unix_server"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/unix_server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/unix_server.dir/build: unix_server
.PHONY : CMakeFiles/unix_server.dir/build

CMakeFiles/unix_server.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/unix_server.dir/cmake_clean.cmake
.PHONY : CMakeFiles/unix_server.dir/clean

CMakeFiles/unix_server.dir/depend:
	cd /home/adil/tntcxx-router/examples && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/adil/tntcxx-router/examples /home/adil/tntcxx-router/examples /home/adil/tntcxx-router/examples /home/adil/tntcxx-router/examples /home/adil/tntcxx-router/examples/CMakeFiles/unix_server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/unix_server.dir/depend

