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

macro(xgl_get_version)

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

# This will become the value of GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION.  It describes the version of the GPUOPEN interface
# that the ICD supports.
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

endmacro()

macro(xgl_get_path)
    # icd path
    set(XGL_ICD_PATH ${PROJECT_SOURCE_DIR}/icd CACHE PATH "The path of xgl, it is read-only.")

    # XGL cache creator tool
    set(XGL_CACHE_CREATOR_PATH ${PROJECT_SOURCE_DIR}/tools/cache_creator CACHE PATH "Path to the cache creator tool")

    # PAL path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../pal)
        set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/../pal CACHE PATH "Specify the path to the PAL project.")
    endif()

    # VKGC path
    if (EXISTS ${XGL_ICD_PATH}/api/compiler/CMakeLists.txt)
        set(XGL_VKGC_PATH ${XGL_ICD_PATH}/api/compiler CACHE PATH "Specify the path to the compiler." FORCE)
    else()
        # On github, the default repo name is llpc instead of compiler
        set(XGL_VKGC_PATH ${PROJECT_SOURCE_DIR}/../llpc CACHE PATH "Specify the path to the llpc repository." FORCE)
    endif()

    # external Vulkan headers path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../Vulkan-Headers)
        set(VULKAN_HEADERS_PATH ${PROJECT_SOURCE_DIR}/../Vulkan-Headers CACHE PATH "The path of Vulkan headers.")
    endif()

    # Metrohash path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../MetroHash)
        set(XGL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../MetroHash CACHE PATH "The path of metrohash.")
    else()
        set(XGL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../third_party/metrohash CACHE PATH "The path of metrohash.")
    endif()

    # cwpack path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../CWPack)
        set(XGL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../CWPack CACHE PATH "The path of cwpack.")
    else()
        set(XGL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../third_party/cwpack CACHE PATH "The path of cwpack.")
    endif()
endmacro()

macro(xgl_overrides_pal)
### For PAL ###########################################################################################################
    set(PAL_BUILD_JEMALLOC OFF CACHE BOOL "Force jemalloc off" FORCE)

    set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_CLIENT_INTERFACE_MINOR_VERSION ${ICD_PAL_CLIENT_MINOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_ENABLE_LTO ${XGL_ENABLE_LTO} CACHE BOOL "XGL override to build PAL with LTO support" FORCE)

    set(PAL_MEMTRACK ${ICD_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GPUOPEN ${ICD_GPUOPEN_DEVMODE_BUILD} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_RAVEN2 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_RENOIR ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GFX10 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI14 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GFX103 ${XGL_BUILD_GFX103} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI21 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI22 ${XGL_BUILD_NAVI22} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    # Wayland
    set(PAL_BUILD_WAYLAND ${BUILD_WAYLAND_SUPPORT} CACHE BOOL "Build PAL with Wayland support" FORCE)

    # Dri3
    set(PAL_BUILD_DRI3 ${BUILD_DRI3_SUPPORT} CACHE BOOL "PAL build with Dri3 enabled" FORCE)

#if VKI_3RD_PARTY_IP_PROPERTY_ID
    set(PAL_3RD_PARTY_IP_PROPERTY_ID ${VKI_3RD_PARTY_IP_PROPERTY_ID})
#endif

    if(EXISTS ${XGL_METROHASH_PATH})
        set(PAL_METROHASH_PATH ${XGL_METROHASH_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
    endif()

    if(EXISTS ${XGL_CWPACK_PATH})
        set(PAL_CWPACK_PATH ${XGL_CWPACK_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
    endif()

endmacro()

macro(xgl_overrides_vkgc)
### For LLPC ##########################################################################################################
    set(LLPC_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_LLPC_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    if(ICD_BUILD_LLPC)

        set(LLPC_BUILD_LIT ${XGL_BUILD_LIT} CACHE BOOL "${PROJECT_NAME} override." FORCE)

        set(LLPC_BUILD_NAVI22 ${XGL_BUILD_NAVI22} CACHE BOOL "${PROJECT_NAME} override." FORCE)

        set(LLPC_BUILD_RAVEN2 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

        set(LLPC_BUILD_VEGA20 ${XGL_BUILD_VEGA20} CACHE BOOL "${PROJECT_NAME} override." FORCE)

        set(LLPC_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    endif()

endmacro()

macro(xgl_overrides)

    xgl_get_version()

    xgl_get_path()

    if(ICD_BUILD_LLPCONLY)
        set(ICD_BUILD_LLPC ON CACHE BOOL "ICD_BUILD_LLPCONLY override." FORCE)
    endif()

    if(NOT ICD_BUILD_LLPC)
        set(XGL_LLVM_UPSTREAM OFF CACHE BOOL "XGL_LLVM_UPSTREAM is overrided to false." FORCE)
    endif()

    set(XGL_USE_SANITIZER "" CACHE STRING "Build with sanitizers, e.g. Address;Undefined")

    if(XGL_USE_SANITIZER)
        set(LLVM_USE_SANITIZER "${XGL_USE_SANITIZER}" CACHE BOOL "LLVM_USE_SANITIZER is overridden." FORCE)
    endif()

    if(XGL_ENABLE_ASSERTIONS)
        set(LLVM_ENABLE_ASSERTIONS "${XGL_ENABLE_ASSERTIONS}" CACHE BOOL "LLVM_ENABLE_ASSERTIONS is overridden." FORCE)
    endif()

    set(VAM_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(ADDR_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(METROHASH_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    xgl_overrides_pal()

    xgl_overrides_vkgc()

### XCB required ######################################################################################################
    set(XCB_REQUIRED ON)

endmacro()
