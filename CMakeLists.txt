cmake_minimum_required(VERSION 3.5)
project(XGL VERSION 1 LANGUAGES C CXX)

### CMake Includes #####################################################################################################
include(CMakeDependentOption)
include(AmdCmakeHelper)
include(SetupAmdGlobalRoots)

### Cached Project Options #############################################################################################
option(VK_INTERNAL_DEVELOPER "Build Internal Developer Build?" OFF)

option(ICD_OPEN_SOURCE "Build open source Vulkan?" ON)
option(ICD_BUILD_LLPC "Build LLPC?" ON)
option(ICD_BUILD_SCPC "Build SCPC?" OFF)

option(ICD_BUILD_LLPCONLY "Build LLPC Only?" OFF)
option(ICD_BUILD_SPVONLY "Build SPV Only?" OFF)

if(ICD_BUILD_LLPCONLY)
    set(ICD_BUILD_LLPC ON CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
    set(ICD_BUILD_SPVONLY OFF CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
endif()

option(USE_NEXT_SDK "Use next SDK?" OFF)

option(ICD_BUILD_GFX9 "GFX9 Support?" ON)

option(ICD_BUILD_VIDEO "Build Video Support?" OFF)

# get multithreaded libc  (/MTd)
#set(MULTI_THREAD_LIBC 1)

# if(ICD_OS_BUILD)
#     if(${ICD_OS_BUILD} EQUAL w8)
#         # win_commondefs still generates softice symbols for 32-bit builds by default
#         set(DEBUG_NO_NMSYM 1)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL w864a)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL wNow)
#         # win_commondefs still generates softice symbols for 32-bit builds by default
#         set(DEBUG_NO_NMSYM 1)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL wNow64a)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL wNxt)
#         # win_commondefs still generates softice symbols for 32-bit builds by default
#         set(DEBUG_NO_NMSYM 1)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL wNxt64a)
#         set(DEBUG_NO_MAPSYM 1)
#         set(ICD_COMPILE_TYPE 64a)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL lnx)
#         set(DEBUG_NO_NMSYM 1)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     if(${ICD_OS_BUILD} EQUAL lnx64a)
#         set(DEBUG_NO_NMSYM 1)
#         set(DEBUG_NO_MAPSYM 1)
#     endif()

#     set(ICD_BUILD_ROOT ${ICD_DEPTH}/build/${ICD_OS_BUILD})
# endif()

option(ICD_GPUOPEN_DEVMODE_BUILD "Build ${PROJECT_NAME} with GPU Open Developer Mode driver support?" ON)

option(ICD_MEMTRACK "Turn on memory tracking?" ${CMAKE_BUILD_TYPE_DEBUG})

cmake_dependent_option(ICD_ANALYSIS_WARNINGS_AS_ERRORS "Warnings as errors?" ON "WIN32" OFF)

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
set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/icd/imported/pal CACHE PATH "Specify the path to the PAL project.")
set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "XGL override." FORCE)
set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)

set(PAL_DEVELOPER_BUILD ${VK_INTERNAL_DEVELOPER} CACHE BOOL "${PROJECT_NAME} override." FORCE)
if(ICD_BUILD_SPVONLY)
    set(PAL_ENABLE_PRINTS_ASSERTS ON CACHE BOOL "${PROJECT_NAME} override." FORCE)
endif()
set(PAL_MEMTRACK ${ICD_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_GPUOPEN ${ICD_GPUOPEN_DEVMODE_BUILD} CACHE BOOL "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_VIDEO ${ICD_BUILD_VIDEO} CACHE BOOL "${PROJECT_NAME} override." FORCE)

set(XGL_SC_PATH ${PROJECT_SOURCE_DIR}/icd/imported/sc CACHE PATH "Specify the path to the SC project.")
set(PAL_SC_PATH ${XGL_SC_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
set(PAL_BUILD_GFX9 ${ICD_BUILD_GFX9} CACHE BOOL "${PROJECT_NAME} override." FORCE)

# SC
set(SC_CLIENT_INTERFACE_MAJOR_VERSION ${SC_CLIENT_MAJOR_VERSION} CACHE STRING "XGL override." FORCE)
set(SC_DUMP_SHADERS ${ICD_BUILD_SPVONLY} CACHE BOOL "${PROJECT_NAME} override." FORCE)

# LLVM
set(XGL_LLVM_SRC_PATH ${PROJECT_SOURCE_DIR}/icd/imported/llvm CACHE PATH "Specify the path to the LLVM.")

### Generator Dependencies #############################################################################################
find_package(PythonInterp 3)
if(NOT PYTHONINTERP_FOUND)
    if(WIN32)
        set(PYTHON_EXECUTABLE ${XGL_PAL_PATH}/tools/python33/python.exe)
        message(STATUS "Using the Python found at ${PYTHON_EXECUTABLE} instead.")
    else()
        message(FATAL_ERROR "Python 3 is needed to generate some source files.")
    endif()
endif()
set(PYTHON_CMD ${PYTHON_EXECUTABLE})

find_package(Perl 5)
if(NOT PERL_FOUND)
    if(WIN32 AND DEFINED GLOBAL_ROOT_DK_DIR)
        set(PERL_EXECUTABLE ${GLOBAL_ROOT_DK_DIR}/perl/5.16.1/bin/Perl.exe)
        message(STATUS "Using the Perl found at ${PERL_EXECUTABLE} instead.")
    else()
        message(FATAL_ERROR "Perl 5 is needed to generate some source files.")
    endif()
endif()
set(PERL_CMD ${PERL_EXECUTABLE})

### Add Subdirectories #################################################################################################

### ICD api/llpc ###############################################################
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
    set(LLVM_INCLUDE_UTILS OFF CACHE BOOL Force)
    set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL Force)
    set(LLVM_ENABLE_ZLIB OFF CACHE BOOL Force)
    # confict with PAL
    #set(LLVM_OPTIMIZED_TABLEGEN ON CACHE BOOL Force)
    add_subdirectory(${XGL_LLVM_SRC_PATH} ${PROJECT_BINARY_DIR}/llvm)
    set(XGL_LLVM_BUILD_PATH ${CMAKE_BINARY_DIR}/llvm)

    add_subdirectory(icd/api/llpc ${CMAKE_BINARY_DIR}/llpc)
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

