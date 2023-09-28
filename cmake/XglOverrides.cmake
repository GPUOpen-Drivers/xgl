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

macro(xgl_get_path)
    # icd path
    set(XGL_ICD_PATH ${PROJECT_SOURCE_DIR}/icd CACHE PATH "The path of xgl, it is read-only.")

    # XGL cache creator tool
    set(XGL_CACHE_CREATOR_PATH ${PROJECT_SOURCE_DIR}/tools/cache_creator CACHE PATH "Path to the cache creator tool")

    # PAL path
    if(EXISTS ${PROJECT_SOURCE_DIR}/icd/imported/pal)
        set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/icd/imported/pal CACHE PATH "Specify the path to the PAL project.")
    elseif(EXISTS ${PROJECT_SOURCE_DIR}/../pal)
        set(XGL_PAL_PATH ${PROJECT_SOURCE_DIR}/../pal CACHE PATH "Specify the path to the PAL project.")
    endif()

#if VKI_RAY_TRACING
    if(VKI_RAY_TRACING)
        if(EXISTS ${PROJECT_SOURCE_DIR}/icd/imported/gpurt)
            set(XGL_GPURT_PATH ${PROJECT_SOURCE_DIR}/icd/imported/gpurt CACHE PATH "Specify the path to the GPURT project.")
        elseif(EXISTS ${PROJECT_SOURCE_DIR}/../gpurt)
            set(XGL_GPURT_PATH ${PROJECT_SOURCE_DIR}/../gpurt CACHE PATH "Specify the path to the GPURT project.")
        endif()

        set(GPURT_DEVELOPER_MODE ON CACHE BOOL "GPURT_DEVELOPER_MODE override." FORCE)
        set(GPURT_CLIENT_API "VULKAN" CACHE STRING "GPURT_CLIENT_API_VULKAN override." FORCE)
        set(GPURT_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_GPURT_CLIENT_MAJOR_VERSION} CACHE STRING "GPURT_CLIENT_INTERFACE_MAJOR_VERSION override." FORCE)
    endif()
#endif

#if VKI_GPU_DECOMPRESS
    if(VKI_GPU_DECOMPRESS)
        set(XGL_GPUTEXDECODER_PATH ${PROJECT_SOURCE_DIR}/icd/imported/gputexdecoder CACHE PATH "Specify the path to the gpu texture decoe project.")
    endif()
#endif

    # VKGC path
    if (EXISTS ${XGL_ICD_PATH}/api/compiler)
        set(XGL_VKGC_PATH ${XGL_ICD_PATH}/api/compiler CACHE PATH "Specify the path to the compiler.")
    elseif(EXISTS ${PROJECT_SOURCE_DIR}/../llpc)
        # On github, the default repo name is llpc instead of compiler
        set(XGL_VKGC_PATH ${PROJECT_SOURCE_DIR}/../llpc CACHE PATH "Specify the path to the llpc repository.")
    endif()

    # external Vulkan headers path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../Vulkan-Headers)
        set(VULKAN_HEADERS_PATH ${PROJECT_SOURCE_DIR}/../Vulkan-Headers CACHE PATH "The path of Vulkan headers.")
    endif()

    # Third_party path
    set(THIRD_PARTY_PATH ${PROJECT_SOURCE_DIR}/../third_party CACHE PATH "The path of third_party.")
    # Metrohash path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../MetroHash)
        set(XGL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../MetroHash CACHE PATH "The path of metrohash.")
    elseif(EXISTS ${THIRD_PARTY_PATH}/metrohash)
        set(XGL_METROHASH_PATH ${THIRD_PARTY_PATH}/metrohash CACHE PATH "The path of metrohash.")
    endif()

    # cwpack path
    if(EXISTS ${PROJECT_SOURCE_DIR}/../CWPack)
        set(XGL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../CWPack CACHE PATH "The path of cwpack.")
    elseif(EXISTS ${THIRD_PARTY_PATH}/cwpack)
        set(XGL_CWPACK_PATH ${THIRD_PARTY_PATH}/cwpack CACHE PATH "The path of cwpack.")
    endif()
endmacro()

macro(xgl_overrides_pal)
### For PAL ###########################################################################################################
    set(PAL_BUILD_JEMALLOC OFF CACHE BOOL "Force jemalloc off" FORCE)

    set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_ENABLE_PRINTS_ASSERTS ${XGL_ENABLE_PRINTS_ASSERTS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_ENABLE_LTO ${XGL_ENABLE_LTO} CACHE BOOL "XGL override to build PAL with LTO support" FORCE)

    set(PAL_MEMTRACK ${ICD_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GPUOPEN ${ICD_GPUOPEN_DEVMODE_BUILD} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI12 ${XGL_BUILD_NAVI12} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI14 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GFX103 ${XGL_BUILD_GFX103} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI21 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI22 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI23 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_NAVI24 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_REMBRANDT ${XGL_BUILD_REMBRANDT} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_RAPHAEL ${XGL_BUILD_RAPHAEL} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_MENDOCINO ${XGL_BUILD_MENDOCINO} CACHE BOOL "${PROJECT_NAME} override." FORCE)

#if VKI_BUILD_NAVI31
    set(PAL_BUILD_NAVI31 ${XGL_BUILD_NAVI31} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI32
    set(PAL_BUILD_NAVI32 ${XGL_BUILD_NAVI32} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI33
    set(PAL_BUILD_NAVI33 ${XGL_BUILD_NAVI33} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

    set(PAL_BUILD_PHOENIX1 ${XGL_BUILD_PHOENIX1} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    # Wayland
    set(PAL_BUILD_WAYLAND ${BUILD_WAYLAND_SUPPORT} CACHE BOOL "Build PAL with Wayland support" FORCE)

    # Dri3
    set(PAL_BUILD_DRI3 ${BUILD_DRI3_SUPPORT} CACHE BOOL "PAL build with Dri3 enabled" FORCE)

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

    set(LLPC_BUILD_TOOLS ${XGL_BUILD_TOOLS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_TESTS ${XGL_BUILD_TESTS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_NAVI12 ${XGL_BUILD_NAVI12} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_REMBRANDT ${XGL_BUILD_REMBRANDT} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_RAPHAEL ${XGL_BUILD_RAPHAEL} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_MENDOCINO ${XGL_BUILD_MENDOCINO} CACHE BOOL "${PROJECT_NAME} override." FORCE)

#if VKI_BUILD_GFX11
    set(LLPC_BUILD_GFX11 ${XGL_BUILD_GFX11} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI31
    set(LLPC_BUILD_NAVI31 ${XGL_BUILD_NAVI31} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI32
    set(LLPC_BUILD_NAVI32 ${XGL_BUILD_NAVI32} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI33
    set(LLPC_BUILD_NAVI33 ${XGL_BUILD_NAVI33} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

    set(LLPC_BUILD_PHOENIX1 ${XGL_BUILD_PHOENIX1} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

endmacro()

macro(xgl_overrides)

    if(ICD_GPUOPEN_DEVMODE_BUILD)
        set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${ICD_GPUOPEN_CLIENT_MAJOR_VERSION})
    endif()

    xgl_get_path()

    if(XGL_BUILD_TESTS)
        set(XGL_BUILD_TOOLS ON CACHE BOOL "XGL_BUILD_TOOLS override by XGL_BUILD_TESTS." FORCE)
    endif()

    if(ICD_BUILD_LLPCONLY)
        set(ICD_BUILD_LLPC ON CACHE BOOL "ICD_BUILD_LLPC override." FORCE)
        set(XGL_BUILD_TOOLS ON CACHE BOOL "XGL_BUILD_TOOLS override by ICD_BUILD_LLPCONLY." FORCE)
    endif()

    if(NOT ICD_BUILD_LLPC)
        set(XGL_LLVM_UPSTREAM OFF CACHE BOOL "XGL_LLVM_UPSTREAM is overrided to false." FORCE)
    endif()

    set(XGL_USE_SANITIZER "" CACHE STRING "Build with sanitizers, e.g. Address;Undefined")

    if(XGL_USE_SANITIZER)
        set(LLVM_USE_SANITIZER "${XGL_USE_SANITIZER}" CACHE STRING "LLVM_USE_SANITIZER is overridden." FORCE)
    endif()

    if(XGL_ENABLE_LIBCXX)
        set(LLVM_ENABLE_LIBCXX "${XGL_ENABLE_LIBCXX}" CACHE BOOL "LLVM_ENABLE_LIBCXX is overridden." FORCE)
    endif()

    if(XGL_ENABLE_ASSERTIONS)
        set(LLVM_ENABLE_ASSERTIONS "${XGL_ENABLE_ASSERTIONS}" CACHE BOOL "LLVM_ENABLE_ASSERTIONS is overridden." FORCE)
    endif()

    set(VAM_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(ADDR_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(METROHASH_ENABLE_WERROR ${ICD_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

### XCB required ######################################################################################################
    set(XCB_REQUIRED OFF)
    if(UNIX AND (NOT ANDROID))
        set(XCB_REQUIRED ON)
    endif()

    xgl_overrides_pal()

    xgl_overrides_vkgc()

endmacro()
