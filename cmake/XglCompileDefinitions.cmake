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

macro(xgl_set_compile_definitions)
### Build Definitions #################################################################################################
    target_compile_definitions(xgl PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)

    if(TARGET_ARCHITECTURE_BITS EQUAL 32)
        target_compile_definitions(xgl PRIVATE VKI_X86_BUILD)
    elseif(TARGET_ARCHITECTURE_BITS EQUAL 64)
        target_compile_definitions(xgl PRIVATE VKI_X64_BUILD)
    endif()

    # Turn on the memory tracker if enabled.
    target_compile_definitions(xgl PRIVATE $<$<BOOL:${VKI_MEMTRACK}>:VKI_MEMTRACK>)

    if(ICD_BUILD_LLPC)
        target_compile_definitions(xgl PRIVATE ICD_BUILD_LLPC)
    endif()

#if VKI_ENABLE_DEBUG_BARRIERS
    if(VKI_ENABLE_DEBUG_BARRIERS)
        target_compile_definitions(xgl PRIVATE VKI_ENABLE_DEBUG_BARRIERS)
    endif()
#endif
#if VKI_RUNTIME_APP_PROFILE
    if(VKI_RUNTIME_APP_PROFILE)
        target_compile_definitions(xgl PRIVATE VKI_RUNTIME_APP_PROFILE)
    endif()
#endif
#if VKI_DEVMODE_COMPILER_SETTINGS
    if(VKI_DEVMODE_COMPILER_SETTINGS)
        target_compile_definitions(xgl PRIVATE VKI_DEVMODE_COMPILER_SETTINGS)
    endif()
#endif

    target_compile_definitions(xgl PRIVATE PAL_BUILD_GFX9=1)

#if VKI_BUILD_HAWK_POINT1
    if(VKI_BUILD_HAWK_POINT1)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_HAWK_POINT1=1)
    endif()
#endif

#if VKI_BUILD_HAWK_POINT2
    if(VKI_BUILD_HAWK_POINT2)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_HAWK_POINT2=1)
    endif()
#endif

#if VKI_BUILD_STRIX_HALO
    if(VKI_BUILD_STRIX_HALO)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_STRIX_HALO=1)
    endif()
#endif

#if VKI_BUILD_GFX12
    if(VKI_BUILD_GFX12)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_GFX12=1)
    endif()
#endif

#if VKI_BUILD_NAVI48
    if(VKI_BUILD_NAVI48)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_NAVI48=1)
    endif()
#endif

#if VKI_RAY_TRACING
    if (VKI_RAY_TRACING)
        target_compile_definitions(xgl PRIVATE VKI_RAY_TRACING=1)
        target_compile_definitions(xgl PRIVATE GPURT_CLIENT_CONVERT_BUILD_PARAMS=1)
        target_compile_definitions(xgl PRIVATE GPURT_CLIENT_API_VULKAN=1)
#if VKI_SUPPORT_HPLOC
        if (VKI_SUPPORT_HPLOC)
            target_compile_definitions(xgl PRIVATE VKI_SUPPORT_HPLOC=1)
        endif()
#endif
    endif()
#endif

#if VKI_RAY_TRACING
#endif

    if (VKI_ENABLE_GCOV)
        target_compile_definitions(xgl PRIVATE VKI_ENABLE_GCOV)
    endif()

#if VKI_GPU_DECOMPRESS
    if(VKI_GPU_DECOMPRESS)
        target_compile_definitions(xgl PRIVATE VKI_GPU_DECOMPRESS)
    endif()
#endif

#if VKI_RAY_TRACING
#endif

#if VKI_RAY_TRACING
#endif

    if(VKI_BUILD_WAYLAND)
        target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
    endif()

    target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
    target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MINOR_VERSION=${PAL_CLIENT_INTERFACE_MINOR_VERSION})
    target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION_SUPPORTS_SHADER_CACHE_EXPECTED_ENTRIES=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})

    target_compile_definitions(xgl PRIVATE LLPC_CLIENT_INTERFACE_MAJOR_VERSION=${LLPC_CLIENT_INTERFACE_MAJOR_VERSION})

### XCB required ######################################################################################################
    if(XCB_REQUIRED)
        find_package(XCB)
    endif()

    if(UNIX AND XCB_REQUIRED)
        target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XCB_KHR)
        target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XLIB_KHR)

        if (XCB_RANDR_LEASE)
            target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XLIB_XRANDR_EXT=1)
        endif()
    endif()

endmacro()
