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

cmake_minimum_required(VERSION 3.13.4)

# This part set before "project(XGL VERSION 1 LANGUAGES C CXX)".
# In a system has both gcc and clang compiler.
option(XGL_USE_CLANG "Build with clang?" OFF)
if(UNIX AND XGL_USE_CLANG)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    include(XglCompilerOptions)
    xgl_use_clang_compiler()
endif()

### CMake Includes ####################################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

include(XglVersions)
include(XglCompilerOptions)

### Project ###########################################################################################################
project(XGL VERSION 1 LANGUAGES C CXX)

include(AmdCmakeHelper)
include(CMakeDependentOption)
include(XglSetupAmdGlobalRoots)
include(XglOptions)
include(XglOverrides)
include(XglCompileDefinitions)
include(XglHelper)
if(UNIX)
  include(XglPackaging)
endif()

### Project Options ###################################################################################################
xgl_options()

### Option Overrides ##################################################################################################
xgl_overrides()

### Set BUILD TYPE related variables ##################################################################################
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

# Set Default Build Type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)
endif()

# Set TARGET_ARCHITECTURE_BITS
if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(TARGET_ARCHITECTURE_BITS 64)
    else( CMAKE_SIZEOF_VOID_P EQUAL 4 )
        set(TARGET_ARCHITECTURE_BITS 32)
    endif()
endif()

# Generate a CompilationDatabase (compile_commands.json file) for our build
# for intellisense used by VScode, YouCompleteMe, etc
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Strip the symbols for release build
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s")
    endif()
endif()

### Set Compiler ######################################################################################################
xgl_set_compiler()

### Sanitizers ########################################################################################################
# Temporary variables
set(ICD_SANITIZER_LINK_FLAGS "")
set(ICD_SANITIZER_COMPILE_FLAGS "")

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
        elseif (XGL_USE_SANITIZER MATCHES "Memory(WithOrigins)?")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=memory")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=memory")
            if(XGL_USE_SANITIZER STREQUAL "MemoryWithOrigins")
                string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize-memory-track-origins")
                string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize-memory-track-origins")
            endif()
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
        elseif(XGL_USE_SANITIZER STREQUAL "Leaks")
            xgl_append_common_sanitizer_flags()
            string(APPEND ICD_SANITIZER_COMPILE_FLAGS " -fsanitize=leak")
            string(APPEND ICD_SANITIZER_LINK_FLAGS " -fsanitize=leak")
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

# LLVM libc++
if(XGL_ENABLE_LIBCXX)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -stdlib=libc++")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -stdlib=libc++")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS " -stdlib=libc++")
        string(APPEND CMAKE_CXX_FLAGS " -stdlib=libc++")
    else()
        message(WARNING "Not sure how to specify libc++ for this compiler")
    endif()
endif()

### GCOV source code coverage #########################################################################################
# Temporary variables
set(ICD_GCOV_LINK_FLAGS "")
set(ICD_GCOV_COMPILE_FLAGS "")

if(XGL_ENABLE_GCOV)
    if(UNIX)
        message(STATUS "This gcov is supported on the UNIX environment: ${XGL_ENABLE_GCOV}")
        xgl_append_gcov_coverage_flags()
    elseif(MSVC)
        message(FATAL_ERROR "This gcov not yet supported in the MSVC environment: ${XGL_ENABLE_GCOV}")
    else()
        message(FATAL_ERROR "XGL_ENABLE_GCOV is not supported on this platform.")
    endif()
endif()

string(APPEND CMAKE_EXE_LINKER_FLAGS "${ICD_GCOV_LINK_FLAGS}")
string(APPEND CMAKE_SHARED_LINKER_FLAGS "${ICD_GCOV_LINK_FLAGS}")
string(APPEND CMAKE_C_FLAGS "${ICD_GCOV_COMPILE_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS "${ICD_GCOV_COMPILE_FLAGS}")

### Generator Dependencies ############################################################################################
# Python3
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

# Perl
find_package(Perl 5)
if(NOT PERL_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Perl 5 is needed to generate some source files.")
    endif()
endif()
set(PERL_CMD ${PERL_EXECUTABLE})

# Wayland required
if (BUILD_WAYLAND_SUPPORT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client)
endif()

# Other Dependencies
if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSSL REQUIRED libssl)
    pkg_check_modules(XCB_DRI2 REQUIRED xcb-dri2)
    pkg_check_modules(XCB_DRI3 REQUIRED xcb-dri3)
    pkg_check_modules(XCB_PRESENT REQUIRED xcb-present)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    pkg_check_modules(XSHMFENCE REQUIRED xshmfence)
endif()

### Khronos Interface #################################################################################################
add_library(khronos_vulkan_interface INTERFACE)

if(EXISTS ${VULKAN_HEADERS_PATH})
    target_include_directories(khronos_vulkan_interface
        INTERFACE
            ${VULKAN_HEADERS_PATH}/include
            icd/api/include/khronos
    )
    target_compile_definitions(khronos_vulkan_interface
        INTERFACE
            EXTERNAL_VULKAN_HEADERS=1
    )
else()
    target_include_directories(khronos_vulkan_interface
        INTERFACE
            icd/api/include/khronos
    )
endif()

### xgl Interface #################################################################################################
add_library(xgl_interface INTERFACE)
target_include_directories(xgl_interface
    INTERFACE
        # Generated string headers
        ${CMAKE_CURRENT_BINARY_DIR}/icd/api
        icd
        icd/api
        icd/api/include
)

### Identify Packge Type ##########################################################################################
if(UNIX)
  identifyPackageType()
endif()

### Add Subdirectories ################################################################################################
# Metrohash
if(EXISTS ${XGL_METROHASH_PATH})
    add_subdirectory(${XGL_METROHASH_PATH} ${PROJECT_BINARY_DIR}/metrohash)
else()
    message(FATAL_ERROR "${XGL_METROHASH_PATH} cannot be found!")
endif()

# Cwpack
if(EXISTS ${XGL_CWPACK_PATH})
    add_subdirectory(${XGL_CWPACK_PATH} ${PROJECT_BINARY_DIR}/cwpack)
else()
    message(FATAL_ERROR "${XGL_CWPACK_PATH} cannot be found!")
endif()

# icd
if(NOT ICD_BUILD_LLPCONLY)
add_subdirectory(icd)
endif()

# VKGC compiler
add_subdirectory(${XGL_VKGC_PATH} ${CMAKE_BINARY_DIR}/compiler)

# Cache-creator tool
if(XGL_BUILD_CACHE_CREATOR)
    if(NOT ICD_BUILD_LLPC)
        message(FATAL_ERROR "Cannot build cache-creator tools without LLPC")
    endif()
    add_subdirectory(${XGL_CACHE_CREATOR_PATH} ${CMAKE_BINARY_DIR}/tools)
endif()

### Generate Packages #################################################################################################
if(UNIX)
  generateInstallTargets()
  generatePackageTarget()
endif()

