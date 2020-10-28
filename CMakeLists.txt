##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
cmake_minimum_required(VERSION 3.13.4)

# This part set before "project(XGL VERSION 1 LANGUAGES C CXX)".
# In a system has both gcc and clang compiler. The cmake output log will be more reasonable.
### Clang build ########################################################################################################
option(XGL_USE_CLANG "Build with clang?" OFF)
if(UNIX)
    if(XGL_USE_CLANG)
        set(CMAKE_CXX_COMPILER_ID "Clang")

        foreach(major RANGE 8 20)
            find_program(CLANG_VER clang-${major})
            if(CLANG_VER)
                set(CLANG_VER ${major})
                break()
            endif()
        endforeach()

        find_program(CLANG_C_COMPILER clang)
        if (CLANG_C_COMPILER)
             set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_C_COMPILER clang-${CLANG_VER})
            if (CLANG_C_COMPILER)
                set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "clang cannot be found!")
            endif()
        endif()

        find_program(CLANG_CXX_COMPILER clang++)
        if (CLANG_CXX_COMPILER)
             set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_CXX_COMPILER clang++-${CLANG_VER})
            if (CLANG_CXX_COMPILER)
                 set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "clang++ cannot be found!")
            endif()
        endif()

        find_program(CLANG_AR llvm-ar)
        if (CLANG_AR)
             set(CMAKE_AR ${CLANG_AR} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_AR llvm-ar-${CLANG_VER})
            if (CLANG_AR)
                 set(CMAKE_AR ${CLANG_AR} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-ar cannot be found!")
            endif()
        endif()

        find_program(CLANG_LINKER llvm-link)
        if (CLANG_LINKER)
             set(CMAKE_LINKER ${CLANG_LINKER} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_LINKER llvm-link-${CLANG_VER})
            if (CLANG_LINKER)
                 set(CMAKE_LINKER ${CLANG_LINKER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-link cannot be found!")
            endif()
        endif()

        find_program(CLANG_NM llvm-nm)
        if (CLANG_NM)
             set(CMAKE_NM ${CLANG_NM} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_NM llvm-nm-${CLANG_VER})
            if (CLANG_NM)
                 set(CMAKE_NM ${CLANG_NM} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-nm cannot be found!")
            endif()
        endif()

        find_program(CLANG_OBJDUMP llvm-objdump)
        if (CLANG_OBJDUMP)
             set(CMAKE_OBJDUMP ${CLANG_OBJDUMP} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_OBJDUMP llvm-objdump-${CLANG_VER})
            if (CLANG_OBJDUMP)
                 set(CMAKE_OBJDUMP ${CLANG_OBJDUMP} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-objdump cannot be found!")
            endif()
        endif()

        find_program(CLANG_RANLIB llvm-ranlib)
        if (CLANG_RANLIB)
             set(CMAKE_RANLIB ${CLANG_RANLIB} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_RANLIB llvm-ranlib-${CLANG_VER})
            if (CLANG_RANLIB)
                 set(CMAKE_RANLIB ${CLANG_RANLIB} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-ranlib cannot be found!")
            endif()
        endif()

    endif()
endif()

project(XGL VERSION 1 LANGUAGES C CXX)

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

### CMake Includes #####################################################################################################
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
if(EXISTS ${PROJECT_SOURCE_DIR}/../cmake)
    list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../cmake)
endif()

### Set BUILD TYPE related variables ###################################################################################
if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE_DEBUG $<CONFIG:Debug>)
    set(CMAKE_BUILD_TYPE_RELEASE $<CONFIG:Release>)
    set(CMAKE_BUILD_TYPE_RELWITHDEBINFO $<CONFIG:RelWithDebInfo>)
else()
    string(TOUPPER "${CMAKE_BUILD_TYPE}" capital_CMAKE_BUILD_TYPE)

    if (CMAKE_BUILD_TYPE AND
        NOT capital_CMAKE_BUILD_TYPE MATCHES "^(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
        message(FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    endif()

    if(capital_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_BUILD_TYPE_DEBUG ON)
        set(CMAKE_BUILD_TYPE_RELEASE OFF)
    else()
        set(CMAKE_BUILD_TYPE_DEBUG OFF)
        set(CMAKE_BUILD_TYPE_RELEASE ON)
    endif()
endif()

### Set TARGET_ARCHITECTURE_BITS #######################################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(TARGET_ARCHITECTURE_BITS 64)
    else( CMAKE_SIZEOF_VOID_P EQUAL 4 )
        set(TARGET_ARCHITECTURE_BITS 32)
    endif()
endif()

# Before GCC7, when LTO is enabled, undefined reference error was observed when linking static libraries.
# Use the gcc-ar wrapper instead of ar, this invokes ar with the right plugin arguments
# --plugin /usr/lib/gcc/.../liblto_plugin.so
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        find_program(GCC_AR gcc-ar)
        if (GCC_AR)
             set(CMAKE_AR ${GCC_AR})
        else()
            message(FATAL_ERROR "gcc-ar cannot be found!")
        endif()
        find_program(GCC_RANLIB gcc-ranlib)
        if (GCC_RANLIB)
            set(CMAKE_RANLIB ${GCC_RANLIB})
        else()
            message(FATAL_ERROR "gcc-ranlib cannot be found!")
        endif()
    endif()
endif()

### Set Default Build Type #############################################################################################
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)
endif()

# Generate a CompilationDatabase (compile_commands.json file) for our build
# for intellisense used by VScode, YouCompleteMe, etc
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(CMakeDependentOption)
include(XglSetupAmdGlobalRoots)

### Strip the symbols for release build ################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s")
    endif()
endif()

### Cached Project Options #############################################################################################

option(XGL_ENABLE_LTO "Build with LTO enabled?" ON)

option(ICD_BUILD_LLPC "Build LLPC?" ON)
cmake_dependent_option(ICD_COMPILER_DEFAULT "Default value of compiler" ON "ICD_BUILD_LLPC" OFF)
option(XGL_BUILD_VEGA20 "Build open source vulkan for Vega20?" ON)
option(XGL_BUILD_RAVEN2 "Build open source vulkan for Raven2?" ON)
option(XGL_BUILD_RENOIR "Build open source vulkan for Renoir?" ON)

option(XGL_BUILD_LIT "Build with Lit test?" OFF)
option(XGL_BUILD_GFX10 "Build open source vulkan for GFX10" ON)

cmake_dependent_option(XGL_BUILD_NAVI14 "Build open source vulkan for Navi14" ON "XGL_BUILD_GFX10" OFF)

#if VKI_KHR_SHADER_SUBGROUP_EXTENDED_TYPES
option(VKI_KHR_SHADER_SUBGROUP_EXTENDED_TYPES "Build vulkan with KHR_SHADER_SUBGROUP_EXTENDED_TYPES" OFF)
#endif
#if VKI_EXT_EXTENDED_DYNAMIC_STATE
option(VKI_EXT_EXTENDED_DYNAMIC_STATE "Build vulkan with EXTENDED_DYNAMIC_STATE extention" OFF)
#endif
option(ICD_BUILD_LLPCONLY "Build LLPC Only?" OFF)

if(ICD_BUILD_LLPCONLY)
    set(ICD_BUILD_LLPC ON CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
endif()

set(XGL_USE_SANITIZER "" CACHE STRING "Build with sanitizers, e.g. Address;Undefined")

if(XGL_USE_SANITIZER)
    set(LLVM_USE_SANITIZER "${XGL_USE_SANITIZER}" CACHE BOOL "LLVM_USE_SANITIZER is overridden." FORCE)
endif()

option(XGL_LLVM_UPSTREAM "Build with upstreamed LLVM?" OFF)

option(XGL_ENABLE_ASSERTIONS "Enable assertions in release builds" OFF)

if(XGL_ENABLE_ASSERTIONS)
    set(LLVM_ENABLE_ASSERTIONS "${XGL_ENABLE_ASSERTIONS}" CACHE BOOL "LLVM_ENABLE_ASSERTIONS is overridden." FORCE)
endif()

if(NOT ICD_BUILD_LLPC)
    set(XGL_LLVM_UPSTREAM OFF CACHE BOOL "XGL_LLVM_UPSTREAM is overrided to false." FORCE)
endif()

option(ICD_GPUOPEN_DEVMODE_BUILD "Build ${PROJECT_NAME} with GPU Open Developer Mode driver support?" ON)

option(ICD_MEMTRACK "Turn on memory tracking?" ${CMAKE_BUILD_TYPE_DEBUG})

if (NOT WIN32)
    option(BUILD_WAYLAND_SUPPORT "Build XGL with Wayland support" ON)
    option(BUILD_XLIB_XRANDR_SUPPORT "Build Xlib with xrandr 1.6 support" OFF)
    option(BUILD_DRI3_SUPPORT "Build XGL with Dri3 support" ON)
endif()

option(ICD_ANALYSIS_WARNINGS_AS_ERRORS "Warnings as errors?" OFF)
set(VAM_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(ADDR_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(METROHASH_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

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

# Handle MINOR_VERSION in the same way
file(STRINGS icd/make/importdefs PAL_MINOR_VERSION REGEX "^ICD_PAL_CLIENT_MINOR_VERSION = [0-9]+")

if(PAL_MINOR_VERSION STREQUAL "")
    message(STATUS "Failed to find ICD_PAL_CLIENT_MINOR_VERSION")
else()
    string(REGEX REPLACE "ICD_PAL_CLIENT_MINOR_VERSION = " "" PAL_MINOR_VERSION ${PAL_MINOR_VERSION})
    message(STATUS "Detected ICD_PAL_CLIENT_MINOR_VERSION is " ${PAL_MINOR_VERSION})
endif()

set(ICD_PAL_CLIENT_MINOR_VERSION ${PAL_MINOR_VERSION})

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
if(EXISTS ${PROJECT_SOURCE_DIR}/../pal)
    set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/../pal CACHE PATH "Specify the path to the PAL project.")
endif()

set(PAL_BUILD_JEMALLOC OFF CACHE BOOL "Force jemalloc off" FORCE)
set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)
set(PAL_CLIENT_INTERFACE_MINOR_VERSION ${ICD_PAL_CLIENT_MINOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)
set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)
set(PAL_ENABLE_LTO ${XGL_ENABLE_LTO} CACHE BOOL "XGL override to build PAL with LTO support" FORCE)

set(PAL_MEMTRACK ${ICD_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_GPUOPEN ${ICD_GPUOPEN_DEVMODE_BUILD} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_RAVEN2 ${XGL_BUILD_RAVEN2} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_RENOIR ${XGL_BUILD_RENOIR} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_GFX10 ${XGL_BUILD_GFX10} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_NAVI14 ${XGL_BUILD_NAVI14} CACHE BOOL "${PROJECT_NAME} override." FORCE)

# Wayland
set(PAL_BUILD_WAYLAND ${BUILD_WAYLAND_SUPPORT} CACHE BOOL "Build PAL with Wayland support" FORCE)
if (BUILD_WAYLAND_SUPPORT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client)
endif()

# Dri3
set(PAL_BUILD_DRI3 ${BUILD_DRI3_SUPPORT} CACHE BOOL "PAL build with Dri3 enabled" FORCE)

# Sanitizers
# Temporary variables
set(ICD_SANITIZER_LINK_FLAGS "")
set(ICD_SANITIZER_COMPILE_FLAGS "")

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

if(XGL_USE_SANITIZER)
    # -pthread is needed sometimes to fix a cmake bug: https://gitlab.kitware.com/cmake/cmake/issues/16609
    string(APPEND ICD_SANITIZER_LINK_FLAGS " -pthread")
    if(UNIX)
        if(XGL_USE_SANITIZER MATCHES "Address")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -shared-libasan")
        endif()

        if(XGL_USE_SANITIZER STREQUAL "Address")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=address")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=address")
        elseif(XGL_USE_SANITIZER STREQUAL "Undefined")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=undefined -fno-sanitize=vptr,function \
                                                        -fno-sanitize-recover=all")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=undefined")
        elseif(XGL_USE_SANITIZER STREQUAL "Thread")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=thread")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=thread")
        elseif(XGL_USE_SANITIZER STREQUAL "Address;Undefined" OR
               XGL_USE_SANITIZER STREQUAL "Undefined;Address")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=address,undefined -fno-sanitize=vptr,function \
                                                        -fno-sanitize-recover=all")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=address,undefined")
        else()
            message(FATAL_ERROR "Unsupported value of XGL_USE_SANITIZER: ${XGL_USE_SANITIZER}")
        endif()
    elseif(MSVC)
        if(XGL_USE_SANITIZER STREQUAL "Address")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=address")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=address")
        else()
            message(FATAL_ERROR "This sanitizer not yet supported in the MSVC environment: ${XGL_USE_SANITIZER}")
        endif()
    else()
        message(FATAL_ERROR "XGL_USE_SANITIZER is not supported on this platform.")
    endif()

    if(XGL_USE_SANITIZER MATCHES "(Undefined;)?Address(;Undefined)?")
        string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize-address-use-after-scope")
    endif()
endif()

string(APPEND CMAKE_EXE_LINKER_FLAGS "${ICD_SANITIZER_LINK_FLAGS}")
string(APPEND CMAKE_SHARED_LINKER_FLAGS "${ICD_SANITIZER_LINK_FLAGS}")
string(APPEND CMAKE_C_FLAGS "${ICD_SANITIZER_COMPILE_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS "${ICD_SANITIZER_COMPILE_FLAGS}")

# Assertions
if(XGL_ENABLE_ASSERTIONS)
    # MSVC doesn't like _DEBUG on release builds.
    if(NOT MSVC)
        add_definitions(-D_DEBUG)
    endif()
    # On non-Debug builds CMake automatically defines NDEBUG, so we explicitly undefine it:
    if(NOT CMAKE_BUILD_TYPE_DEBUG)
        add_definitions(-UNDEBUG)

        # Also remove /D NDEBUG to avoid MSVC warnings about conflicting defines.
        foreach(flags_var_to_scrub
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_MINSIZEREL)
            string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " "
                "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
        endforeach()
    endif()
endif()

### Generator Dependencies #############################################################################################
find_package(PythonInterp 3)
if(NOT PYTHONINTERP_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Python 3 is needed to generate some source files.")
    endif()
else()
    if(UNIX)
        if(${PYTHON_VERSION_MINOR} LESS "6")
            message(FATAL_ERROR "Python 3.6(CPython) or higher is needed to generate some source files.")
        endif()
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

### Metrohash ##########################################################
if(EXISTS ${PROJECT_SOURCE_DIR}/../MetroHash)
    set(XGL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../MetroHash CACHE PATH "The path of metrohash.")
else()
    set(XGL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../third_party/metrohash CACHE PATH "The path of metrohash.")
endif()

if(EXISTS ${XGL_METROHASH_PATH})
    add_subdirectory(${XGL_METROHASH_PATH} ${PROJECT_BINARY_DIR}/metrohash)
    set(PAL_METROHASH_PATH ${XGL_METROHASH_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
else()
    message(FATAL_ERROR "${XGL_METROHASH_PATH} cannot be found!")
endif()

### cwpack #############################################################
if(EXISTS ${PROJECT_SOURCE_DIR}/../CWPack)
    set(XGL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../CWPack CACHE PATH "The path of cwpack.")
else()
    set(XGL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../third_party/cwpack CACHE PATH "The path of cwpack.")
endif()

if(EXISTS ${XGL_CWPACK_PATH})
    add_subdirectory(${XGL_CWPACK_PATH} ${PROJECT_BINARY_DIR}/cwpack)
    set(PAL_CWPACK_PATH ${XGL_CWPACK_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
else()
    message(FATAL_ERROR "${XGL_CWPACK_PATH} cannot be found!")
endif()

### VKGC ###############################################################
set(XGL_ICD_PATH ${PROJECT_SOURCE_DIR}/icd CACHE PATH "The path of xgl, it is read-only.")

if (EXISTS ${XGL_ICD_PATH}/api/compiler/CMakeLists.txt)
    set(XGL_VKGC_PATH ${XGL_ICD_PATH}/api/compiler CACHE PATH "Specify the path to the compiler." FORCE)
else()
    # On github, the default repo name is llpc instead of compiler
    set(XGL_VKGC_PATH ${PROJECT_SOURCE_DIR}/../llpc CACHE PATH "Specify the path to the llpc repository." FORCE)
endif()

set(LLPC_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_LLPC_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)
set(LLPC_BUILD_LIT ${XGL_BUILD_LIT} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(LLPC_BUILD_GFX10 ${XGL_BUILD_GFX10} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(LLPC_BUILD_NAVI14 ${XGL_BUILD_NAVI14} CACHE BOOL "${PROJECT_NAME} override." FORCE)

if(ICD_BUILD_LLPC)
    set(LLPC_BUILD_RAVEN2 ${XGL_BUILD_RAVEN2} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(LLPC_BUILD_RENOIR ${XGL_BUILD_RENOIR} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(LLPC_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(LLPC_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)
endif()

add_subdirectory(${XGL_VKGC_PATH} ${CMAKE_BINARY_DIR}/compiler)

if(NOT ICD_BUILD_LLPCONLY)
    add_subdirectory(icd)
endif()
