# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build

# Include any dependencies generated for this target.
include src/CMakeFiles/gsad.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/gsad.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/gsad.dir/flags.make

src/CMakeFiles/gsad.dir/gsad.c.o: src/CMakeFiles/gsad.dir/flags.make
src/CMakeFiles/gsad.dir/gsad.c.o: ../src/gsad.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object src/CMakeFiles/gsad.dir/gsad.c.o"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/gsad.dir/gsad.c.o   -c /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/gsad.c

src/CMakeFiles/gsad.dir/gsad.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/gsad.dir/gsad.c.i"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -E /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/gsad.c > CMakeFiles/gsad.dir/gsad.c.i

src/CMakeFiles/gsad.dir/gsad.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/gsad.dir/gsad.c.s"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -S /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/gsad.c -o CMakeFiles/gsad.dir/gsad.c.s

src/CMakeFiles/gsad.dir/gsad.c.o.requires:
.PHONY : src/CMakeFiles/gsad.dir/gsad.c.o.requires

src/CMakeFiles/gsad.dir/gsad.c.o.provides: src/CMakeFiles/gsad.dir/gsad.c.o.requires
	$(MAKE) -f src/CMakeFiles/gsad.dir/build.make src/CMakeFiles/gsad.dir/gsad.c.o.provides.build
.PHONY : src/CMakeFiles/gsad.dir/gsad.c.o.provides

src/CMakeFiles/gsad.dir/gsad.c.o.provides.build: src/CMakeFiles/gsad.dir/gsad.c.o

src/CMakeFiles/gsad.dir/validator.c.o: src/CMakeFiles/gsad.dir/flags.make
src/CMakeFiles/gsad.dir/validator.c.o: ../src/validator.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object src/CMakeFiles/gsad.dir/validator.c.o"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/gsad.dir/validator.c.o   -c /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/validator.c

src/CMakeFiles/gsad.dir/validator.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/gsad.dir/validator.c.i"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -E /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/validator.c > CMakeFiles/gsad.dir/validator.c.i

src/CMakeFiles/gsad.dir/validator.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/gsad.dir/validator.c.s"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && /usr/bin/gcc  -Wl,-rpath,/usr/local/openvas/lib64 -Wl,-rpath,/usr/local/openvas/lib $(C_DEFINES) $(C_FLAGS) -S /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src/validator.c -o CMakeFiles/gsad.dir/validator.c.s

src/CMakeFiles/gsad.dir/validator.c.o.requires:
.PHONY : src/CMakeFiles/gsad.dir/validator.c.o.requires

src/CMakeFiles/gsad.dir/validator.c.o.provides: src/CMakeFiles/gsad.dir/validator.c.o.requires
	$(MAKE) -f src/CMakeFiles/gsad.dir/build.make src/CMakeFiles/gsad.dir/validator.c.o.provides.build
.PHONY : src/CMakeFiles/gsad.dir/validator.c.o.provides

src/CMakeFiles/gsad.dir/validator.c.o.provides.build: src/CMakeFiles/gsad.dir/validator.c.o

# Object files for target gsad
gsad_OBJECTS = \
"CMakeFiles/gsad.dir/gsad.c.o" \
"CMakeFiles/gsad.dir/validator.c.o"

# External object files for target gsad
gsad_EXTERNAL_OBJECTS =

src/gsad: src/CMakeFiles/gsad.dir/gsad.c.o
src/gsad: src/CMakeFiles/gsad.dir/validator.c.o
src/gsad: src/CMakeFiles/gsad.dir/build.make
src/gsad: src/libgsad_omp.a
src/gsad: src/libgsad_base.a
src/gsad: src/CMakeFiles/gsad.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable gsad"
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/gsad.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/gsad.dir/build: src/gsad
.PHONY : src/CMakeFiles/gsad.dir/build

src/CMakeFiles/gsad.dir/requires: src/CMakeFiles/gsad.dir/gsad.c.o.requires
src/CMakeFiles/gsad.dir/requires: src/CMakeFiles/gsad.dir/validator.c.o.requires
.PHONY : src/CMakeFiles/gsad.dir/requires

src/CMakeFiles/gsad.dir/clean:
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src && $(CMAKE_COMMAND) -P CMakeFiles/gsad.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/gsad.dir/clean

src/CMakeFiles/gsad.dir/depend:
	cd /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7 /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/src /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src /home/peterwang/OpenVAS_Source/greenbone-security-assistant-5.0.7/build/src/CMakeFiles/gsad.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/gsad.dir/depend
