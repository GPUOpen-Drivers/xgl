##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.5)
project(XGL VERSION 1 LANGUAGES C CXX)

### Set Default Build Type #############################################################################################
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)
endif()

### CMake Includes #####################################################################################################
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/../pal/cmake/Modules)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/icd/imported/pal/cmake/Modules)
include(CMakeDependentOption)
include(AmdCmakeHelper)
include(SetupAmdGlobalRoots)

### Strip the symbols for release build ################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s")
    endif()
endif()

### Cached Project Options #############################################################################################
option(VK_INTERNAL_DEVELOPER "Build Internal Developer Build?" OFF)

option(ICD_OPEN_SOURCE "Build open source Vulkan?" ON)
option(ICD_BUILD_LLPC "Build LLPC?" ON)
option(XGL_BUILD_VEGA20 "Build open source vulkan for Vega20?" ON)
option(XGL_BUILD_RAVEN2 "Build open source vulkan for Raven2?" ON)

option(XGL_BUILD_LIT "Build with Lit test?" OFF)

option(ICD_BUILD_LLPCONLY "Build LLPC Only?" OFF)
option(ICD_BUILD_SPVONLY "Build SPV Only?" OFF)

if(ICD_BUILD_LLPCONLY)
    set(ICD_BUILD_LLPC ON CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
    set(ICD_BUILD_SPVONLY OFF CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
endif()

option(USE_NEXT_SDK "Use next SDK?" OFF)

option(ICD_BUILD_VIDEO "Build Video Support?" OFF)

option(XGL_LLVM_UPSTREAM "Build with upstreamed LLVM?" OFF)

if(NOT ICD_BUILD_LLPC)
    set(XGL_LLVM_UPSTREAM OFF CACHE BOOL "XGL_LLVM_UPSTREAM is overrided to false." FORCE)
endif()

option(ICD_GPUOPEN_DEVMODE_BUILD "Build ${PROJECT_NAME} with GPU Open Developer Mode driver support?" ON)

option(ICD_MEMTRACK "Turn on memory tracking?" ${CMAKE_BUILD_TYPE_DEBUG})

option(BUILD_WAYLAND_SUPPORT "Build XGL with Wayland support" OFF)

option(BUILD_XLIB_XRANDR_SUPPORT "Build Xlib with xrandr 1.6 support" OFF)

# Option Overrides #################################################################################
# These are options that override XGL subproject options.  As these overrides are managed and force
# set by XGL, mark_as_advanced is used to hide them from the CMake GUI.

# This will become the value of PAL_CLIENT_INTERFACE_MAJOR_VERSION.  It describes the version of the PAL interface
# that the ICD supports.  PAL uses this value to enable backwards-compatibility for older interface versions.  It must
# be updated on each PAL promotion after handling all of the interface changes described in palLib.h.
file(STRINGS icd/make/importdefs PAL_MAJOR_VERSION REGEX "^ICD_PAL_CLIENT_MAJOR_VERSION = [0-9]+")

if(PAL_MAJOR_VERSION STREQUAL "")
    message(STATUS "Failed to find ICD_PAL_CLIENT_MAJOR_VERSION")
else()
    string(REGEX REPLACE "ICD_PAL_CLIENT_MAJOR_VERSION = " "" PAL_MAJOR_VERSION ${PAL_MAJOR_VERSION})
    message(STATUS "Detected ICD_PAL_CLIENT_MAJOR_VERSION is " ${PAL_MAJOR_VERSION})
endif()

set(ICD_PAL_CLIENT_MAJOR_VERSION ${PAL_MAJOR_VERSION})

# This will become the value of LLPC_CLIENT_INTERFACE_MAJOR_VERSION.  It describes the version of the LLPC interface
# that the ICD supports.  LLPC uses this value to enable backwards-compatibility for older interface versions.  It must
# be updated on each LLPC promotion after handling all of the interface changes described in llpc.h
file(STRINGS icd/make/importdefs LLPC_MAJOR_VERSION REGEX "^ICD_LLPC_CLIENT_MAJOR_VERSION = [0-9]+")

if(LLPC_MAJOR_VERSION STREQUAL "")
    message(STATUS "Failed to find ICD_LLPC_CLIENT_MAJOR_VERSION")
else()
    string(REGEX REPLACE "ICD_LLPC_CLIENT_MAJOR_VERSION = " "" LLPC_MAJOR_VERSION ${LLPC_MAJOR_VERSION})
    message(STATUS "Detected ICD_LLPC_CLIENT_MAJOR_VERSION is " ${LLPC_MAJOR_VERSION})
endif()

set(ICD_LLPC_CLIENT_MAJOR_VERSION ${LLPC_MAJOR_VERSION})

# GPUOPEN
if(ICD_GPUOPEN_DEVMODE_BUILD)
    file(STRINGS icd/make/importdefs GPUOPEN_MAJOR_VERSION REGEX "^ICD_GPUOPEN_CLIENT_MAJOR_VERSION = [0-9]+")

    if(GPUOPEN_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find ICD_GPUOPEN_CLIENT_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "ICD_GPUOPEN_CLIENT_MAJOR_VERSION = " "" GPUOPEN_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
        message(STATUS "Detected ICD_GPUOPEN_CLIENT_MAJOR_VERSION is " ${GPUOPEN_MAJOR_VERSION})
    endif()
    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
endif()

# PAL
set(PAL_BUILD_JEMALLOC OFF CACHE BOOL "Force jemalloc off" FORCE)
set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/../pal CACHE PATH "Specify the path to the PAL project.")
set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)
set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)

set(PAL_DEVELOPER_BUILD ${VK_INTERNAL_DEVELOPER} CACHE BOOL "${PROJECT_NAME} override." FORCE)
if(ICD_BUILD_SPVONLY)
    set(PAL_ENABLE_PRINTS_ASSERTS ON CACHE BOOL "${PROJECT_NAME} override." FORCE)
endif()

set(PAL_MEMTRACK ${ICD_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_GPUOPEN ${ICD_GPUOPEN_DEVMODE_BUILD} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_VIDEO ${ICD_BUILD_VIDEO} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_RAVEN2 ${XGL_BUILD_RAVEN2} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)

# LLVM
set(XGL_LLVM_SRC_PATH ${PROJECT_SOURCE_DIR}/../llvm CACHE PATH "Specify the path to the LLVM.")

# Wayland
if (BUILD_WAYLAND_SUPPORT)
    set(PAL_BUILD_WAYLAND ON CACHE BOOL "Build PAL with Wayland support" FORCE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client)
endif()

### Generator Dependencies #############################################################################################
find_package(PythonInterp 3)
if(NOT PYTHONINTERP_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Python 3 is needed to generate some source files.")
    endif()
endif()
set(PYTHON_CMD ${PYTHON_EXECUTABLE})

find_package(Perl 5)
if(NOT PERL_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Perl 5 is needed to generate some source files.")
    endif()
endif()
set(PERL_CMD ${PERL_EXECUTABLE})

### Add Subdirectories #################################################################################################

### LLPC ###############################################################
set(XGL_ICD_PATH ${PROJECT_SOURCE_DIR}/icd CACHE PATH "The path of xgl, it is read-only.")
set(XGL_LLPC_PATH ${PROJECT_SOURCE_DIR}/../llpc CACHE PATH "Specify the path to the LLPC.")
set(LLPC_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_LLPC_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)
set(LLPC_BUILD_LIT ${XGL_BUILD_LIT} CACHE BOOL "${PROJECT_NAME} override." FORCE)
if(ICD_BUILD_LLPC)
    set(LLVM_ENABLE_ASSERTIONS ${CMAKE_BUILD_TYPE_DEBUG} CACHE BOOL Force)
    set(LLVM_TARGETS_TO_BUILD AMDGPU CACHE STRING Force)
    set(LLVM_BUILD_TESTS OFF CACHE BOOL Force)
    set(LLVM_BUILD_TOOLS ON CACHE BOOL Force)
    set(LLVM_BUILD_UTILS OFF CACHE BOOL Force)
    set(LLVM_INCLUDE_DOCS OFF CACHE BOOL Force)
    set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL Force)
    set(LLVM_INCLUDE_GO_TESTS OFF CACHE BOOL Force)
    set(LLVM_INCLUDE_TESTS OFF CACHE BOOL Force)
    set(LLVM_INCLUDE_TOOLS ON CACHE BOOL Force)
    set(LLVM_INCLUDE_UTILS ON CACHE BOOL Force)
    set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL Force)
    set(LLVM_ENABLE_ZLIB OFF CACHE BOOL Force)
    set(LLPC_BUILD_RAVEN2 ${XGL_BUILD_RAVEN2} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(LLPC_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    # confict with PAL
    #set(LLVM_OPTIMIZED_TABLEGEN ON CACHE BOOL Force)
    add_subdirectory(${XGL_LLVM_SRC_PATH} ${PROJECT_BINARY_DIR}/llvm)
    set(XGL_LLVM_BUILD_PATH ${CMAKE_BINARY_DIR}/llvm)

    add_subdirectory(${XGL_LLPC_PATH} ${CMAKE_BINARY_DIR}/llpc)
endif()

if(ICD_BUILD_SPVONLY)
    # add_subdirectory(${XGL_BILTOOL_PATH} ${PROJECT_BINARY_DIR}/api/bil/tool)
    # target_link_libraries(xgl biltool)
endif()

if(NOT ICD_BUILD_SPVONLY)
    if(NOT ICD_BUILD_LLPCONLY)
        add_subdirectory(icd)
    endif()
endif()

#build wsa
option(BUILD_WSA "Build wsa" OFF)

if (BUILD_WSA)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../wsa ${PROJECT_BINARY_DIR}/wsa)
endif()

