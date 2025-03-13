##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
        set(GPURT_CLIENT_INTERFACE_MAJOR_VERSION ${VKI_GPURT_CLIENT_MAJOR_VERSION} CACHE STRING "GPURT_CLIENT_INTERFACE_MAJOR_VERSION override." FORCE)
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

    set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${VKI_PAL_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_CLIENT "VULKAN" CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(PAL_ENABLE_PRINTS_ASSERTS ${VKI_ENABLE_PRINTS_ASSERTS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_ENABLE_LTO ${VKI_ENABLE_LTO} CACHE BOOL "XGL override to build PAL with LTO support" FORCE)

    set(PAL_MEMTRACK ${VKI_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(PAL_BUILD_GFX11    ON CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(PAL_BUILD_PHOENIX2 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

#if VKI_BUILD_STRIX_HALO
    set(PAL_BUILD_STRIX_HALO ${VKI_BUILD_STRIX_HALO} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_GFX12
    set(PAL_BUILD_GFX12 ${VKI_BUILD_GFX12} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_NAVI48
    set(PAL_BUILD_NAVI48 ${VKI_BUILD_NAVI48} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

    # Wayland
    set(PAL_BUILD_WAYLAND ${VKI_BUILD_WAYLAND} CACHE BOOL "Build PAL with Wayland support" FORCE)

    # Dri3
    set(PAL_BUILD_DRI3 ${VKI_BUILD_DRI3} CACHE BOOL "PAL build with Dri3 enabled" FORCE)

    if(EXISTS ${XGL_METROHASH_PATH})
        set(PAL_METROHASH_PATH ${XGL_METROHASH_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
    endif()

    if(EXISTS ${XGL_CWPACK_PATH})
        set(PAL_CWPACK_PATH ${XGL_CWPACK_PATH} CACHE PATH "${PROJECT_NAME} override." FORCE)
    endif()

endmacro()

macro(xgl_overrides_llpc)
### For LLPC ##########################################################################################################
    if(VKI_ENABLE_LIBCXX)
        set(LLVM_ENABLE_LIBCXX ${VKI_ENABLE_LIBCXX} CACHE BOOL "LLVM_ENABLE_LIBCXX is overridden." FORCE)
    endif()

    if(VKI_ENABLE_ASSERTIONS)
        set(LLVM_ENABLE_ASSERTIONS "${VKI_ENABLE_ASSERTIONS}" CACHE BOOL "LLVM_ENABLE_ASSERTIONS is overridden." FORCE)
    endif()

    set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "LLVM_INCLUDE_BENCHMARKS is overriden." FORCE)

    set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "LLVM_INCLUDE_DOCS is overriden." FORCE)

    set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "LLVM_INCLUDE_EXAMPLES is overriden." FORCE)

    if(VKI_USE_SANITIZER)
        set(LLPC_USE_SANITIZER "${VKI_USE_SANITIZER}" CACHE STRING "LLPC_USE_SANITIZER is overridden." FORCE)
    endif()
    set(LLPC_ENABLE_LTO ${VKI_ENABLE_LTO} CACHE BOOL "XGL override to build LLPC with LTO support" FORCE)
    set(LLPC_MEMTRACK ${VKI_MEMTRACK} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_GFX11 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_GFX115 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)
    set(LLPC_BUILD_STRIX1 ON CACHE BOOL "${PROJECT_NAME} override." FORCE)

#if VKI_BUILD_STRIX_HALO
    set(LLPC_BUILD_STRIX_HALO ${VKI_BUILD_STRIX_HALO} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

#if VKI_BUILD_GFX12
    if(VKI_BUILD_GFX12)
        set(LLPC_BUILD_GFX12 ${VKI_BUILD_GFX12} CACHE BOOL "${PROJECT_NAME} override." FORCE)
    endif()
#endif

#if VKI_BUILD_NAVI48
    set(LLPC_BUILD_NAVI48 ${VKI_BUILD_NAVI48} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

    set(LLPC_CLIENT_INTERFACE_MAJOR_VERSION ${VKI_LLPC_CLIENT_MAJOR_VERSION} CACHE STRING "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_TOOLS ${VKI_BUILD_TOOLS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_BUILD_TESTS ${VKI_BUILD_TESTS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(LLPC_ENABLE_WERROR ${VKI_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

#if VKI_RAY_TRACING
    set(LLPC_RAY_TRACING ${VKI_RAY_TRACING} CACHE BOOL "${PROJECT_NAME} override." FORCE)
#endif

endmacro()

macro(xgl_overrides)

    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${VKI_GPUOPEN_CLIENT_MAJOR_VERSION})

#if VKI_BUILD_GFX12
    set(VKI_BUILD_GFX12 OFF)
#if VKI_BUILD_NAVI48
    if(VKI_BUILD_NAVI48)
        set(VKI_BUILD_GFX12 ON)
    endif()
#endif
#endif

    xgl_get_path()

    set(VKI_MEMTRACK ${CMAKE_BUILD_TYPE_DEBUG})

    if(VKI_BUILD_TESTS)
        set(VKI_BUILD_TOOLS ON CACHE BOOL "VKI_BUILD_TOOLS override by VKI_BUILD_TESTS." FORCE)
    endif()

    set(VKI_USE_SANITIZER "" CACHE STRING "Build with sanitizers, e.g. Address;Undefined")

    set(VAM_ENABLE_WERROR ${VKI_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(ADDR_ENABLE_WERROR ${VKI_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

    set(METROHASH_ENABLE_WERROR ${VKI_ANALYSIS_WARNINGS_AS_ERRORS} CACHE BOOL "${PROJECT_NAME} override." FORCE)

### XCB required ######################################################################################################
    set(XCB_REQUIRED OFF)
    if(UNIX AND (NOT ANDROID))
        set(XCB_REQUIRED ON)
    endif()

    xgl_overrides_pal()

    xgl_overrides_llpc()

endmacro()
