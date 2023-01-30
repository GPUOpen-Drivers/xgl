##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
