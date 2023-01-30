##
 #######################################################################################################################
 #
 #  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Output
# Boost_FOUND
# Boost_ROOT_DIR
# Boost_INCLUDE_DIRS
# Boost_LIBRARY_DIRS

# CMAKE-TODO:
# There is a built in FindBoost module: https://cmake.org/cmake/help/latest/module/FindBoost.html
# But our DK version is very inconsistent and is not structured the same way.  More testing required.

if(NOT DEFINED Boost_FOUND)
    if(NOT DEFINED AMDBoost_FIND_VERSION)
        message(FATAL_ERROR "A version to search for must be specified.")
    endif()

    if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
        message(FATAL_ERROR "TARGET_ARCHITECTURE_BITS must be defined.")
    endif()

    if(NOT DEFINED GLOBAL_ROOT_DK_DIR)
        message(FATAL_ERROR "GLOBAL_ROOT_DK_DIR must be specified.")
    endif()

    set(BOOST_VER ${AMDBoost_FIND_VERSION_MAJOR}.${AMDBoost_FIND_VERSION_MINOR}.${AMDBoost_FIND_VERSION_PATCH})

    if(MSVC)
        #MSVC++ 11.0 MSVC_VERSION == 1700 (Visual Studio 2012)
        #MSVC++ 12.0 MSVC_VERSION == 1800 (Visual Studio 2013)
        #MSVC++ 14.0 MSVC_VERSION == 1900 (Visual Studio 2015)
        if(MSVC_VERSION EQUAL 1700)
            set(Boost_ROOT_DIR ${GLOBAL_ROOT_DK_DIR}/boost/${BOOST_VER}/vc11 CACHE PATH "Boost root directory.")
        elseif(MSVC_VERSION GREATER_EQUAL 1800) # CMAKE-TODO: Set to GREATER_EQUAL until VS projects are supported correctly.
            set(Boost_ROOT_DIR ${GLOBAL_ROOT_DK_DIR}/boost/${BOOST_VER}/vc12 CACHE PATH "Boost root directory.")
        else()
            message(FATAL_ERROR "The MSVC Version: ${MSVC_VERSION} is currently unsopported for: ${CMAKE_PARENT_LIST_FILE}")
        endif()
        message(STATUS "Boost Version: ${BOOST_VER} for MSVC Version: ${MSVC_VERSION}")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        set(Boost_ROOT_DIR ${GLOBAL_ROOT_DK_DIR}/boost/${BOOST_VER}/gcc-${CMAKE_CXX_COMPILER_VERSION} CACHE PATH "Boost root directory.")
        message(STATUS "Boost Version: ${BOOST_VER} for GCC Version: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    mark_as_advanced(Boost_ROOT_DIR)

    message(STATUS "Boost: ${Boost_ROOT_DIR}")

if (Boost_ROOT_DIR)
    set(Boost_INCLUDE_DIRS
            ${Boost_ROOT_DIR}/include
            CACHE PATH "Boost include directories."
        )
        mark_as_advanced(Boost_INCLUDE_DIRS)

        if(WIN32)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(Boost_LIBRARY_DIRS
                    ${Boost_ROOT_DIR}/lib/x64
                    CACHE PATH "Boost library directories."
                )
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(Boost_LIBRARY_DIRS
                    ${Boost_ROOT_DIR}/lib/x86-fastcall
                    CACHE PATH "Boost library directories."
                )
            endif()
        elseif(UNIX)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(Boost_LIBRARY_DIRS
                    ${Boost_ROOT_DIR}/lib/x64-fPIC
                    CACHE PATH "Boost library directories."
                )
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(Boost_LIBRARY_DIRS
                    ${Boost_ROOT_DIR}/lib/x86-fPIC
                    CACHE PATH "Boost library directories."
                )
            endif()
        endif()
        mark_as_advanced(Boost_LIBRARY_DIRS)

        set(Boost_FOUND 1)
    else()
      set(Boost_FOUND 0)
    endif()

    set(Boost_FOUND ${Boost_FOUND} CACHE STRING "Was Boost found?")
    mark_as_advanced(Boost_FOUND)
endif()
