##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################
include_guard()

### Helper Macros #####################################################################################################
macro(target_find_headers _target)
    get_target_property(${_target}_INCLUDES_DIRS ${_target} INCLUDE_DIRECTORIES)

    if(${_target}_INCLUDES_DIRS)
        foreach(_include_dir IN ITEMS ${${_target}_INCLUDES_DIRS})
            file(GLOB_RECURSE _include_files
                LIST_DIRECTORIES false
                "${_include_dir}/*.h"
                "${_include_dir}/*.hpp"
            )

            list(APPEND ${_target}_INCLUDES ${_include_files})
        endforeach()

        target_sources(${_target} PRIVATE ${${_target}_INCLUDES})
    endif()
endmacro()

# Source Groups Helper #############################################################################
# This helper creates source groups for generators that support them. This is primarily MSVC and
# XCode, but there are other generators that support IDE project files.
#
# Note: this only adds files that have been added to the target's SOURCES property. To add headers
# to this list, be sure that you call target_find_headers before you call target_source_groups.
macro(target_source_groups _target)
    get_target_property(${_target}_SOURCES ${_target} SOURCES)
    foreach(_source IN ITEMS ${${_target}_SOURCES})
        set(_source ${_source})
        get_filename_component(_source_path "${_source}" ABSOLUTE)
        file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
        get_filename_component(_source_path_rel "${_source_path_rel}" DIRECTORY)
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endmacro()

macro(xgl_append_common_sanitizer_flags)
    if(NOT MSVC)
        # Append -fno-omit-frame-pointer and turn on debug info to get better stack traces.
        string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fno-omit-frame-pointer")
        if (NOT CMAKE_BUILD_TYPE_DEBUG)
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -gline-tables-only")
        else()
            # Use -O1 even in debug mode, otherwise sanitizers slowdown is too large.
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -O1")
        endif()
    elseif(CLANG_CL)
        # Keep frame pointers around.
        string(APPEND ICD_SANITIZER_COMPILE_FLAGS " /Oy-")
        # Always ask the linker to produce symbols with asan.
        string(APPEND ICD_SANITIZER_COMPILE_FLAGS " /Z7")
        string(APPEND ICD_SANITIZER_LINK_FLAGS " -debug")
    endif()
endmacro()

macro(xgl_append_gcov_coverage_flags)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # This option is used to compile and link code instrumented for coverage analysis.
        # The option --coverage is a synonym for -fprofile-arcs -ftest-coverage (when compiling) and -lgcov (when linking)
        # Ref link: https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#Instrumentation-Options
        string(APPEND ICD_GCOV_COMPILE_FLAGS " --coverage")
        string(APPEND ICD_GCOV_LINK_FLAGS    " --coverage")

        if (NOT CMAKE_BUILD_TYPE_DEBUG)
            # Use -O0 even in not debug mode, otherwise code coverage is not accurate.
            string(APPEND ICD_GCOV_COMPILE_FLAGS " -O0")
        endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            string(APPEND ICD_GCOV_COMPILE_FLAGS " -Xclang -coverage-cfg-checksum")
            string(APPEND ICD_GCOV_COMPILE_FLAGS " -Xclang -coverage-no-function-names-in-data")
            string(APPEND ICD_GCOV_COMPILE_FLAGS " -Xclang -coverage-version='408*'")
        endif()
    else()
        message(FATAL_ERROR "Unknown compiler ID: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endmacro()
