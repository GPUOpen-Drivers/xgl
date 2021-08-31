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

macro(xgl_options)

### Cached Project Options #############################################################################################

    option(XGL_ENABLE_LTO "Build with LTO enabled?" ON)

    option(XGL_ENABLE_GCOV "Build with gcov source code coverage?" OFF)

    option(XGL_BUILD_VEGA20 "Build open source vulkan for Vega20?" ON)

    option(XGL_BUILD_GFX103 "Build open source vulkan for GFX103" ON)

    option(XGL_BUILD_NAVI12 "Build open source vulkan for Navi12" ON)

    option(XGL_BUILD_NAVI22 "Build open source vulkan for Navi22" ON)

    option(XGL_BUILD_NAVI23 "Build open source vulkan for Navi23" ON)

    option(XGL_BUILD_TESTS "Build all tests?" OFF)

    # Deprecated, use XGL_BUILD_TESTS instead.
    option(XGL_BUILD_LIT "Build with Lit test?" OFF)

    option(XGL_BUILD_CACHE_CREATOR "Build cache-creator tools?" OFF)

#if VKI_EXT_EXTENDED_DYNAMIC_STATE2
    option(VKI_EXT_EXTENDED_DYNAMIC_STATE2 "Build vulkan with EXT_EXTENDED_DYNAMIC_STATE2" OFF)
#endif

#if VKI_KHR_SHADER_SUBGROUP_EXTENDED_TYPES
    option(VKI_KHR_SHADER_SUBGROUP_EXTENDED_TYPES "Build vulkan with KHR_SHADER_SUBGROUP_EXTENDED_TYPES" OFF)
#endif

#if VKI_GPU_DECOMPRESS
    option(VKI_GPU_DECOMPRESS "Build vulkan with GPU_DECOMPRESS" OFF)
#endif

#if VKI_EXT_EXTENDED_DYNAMIC_STATE
    option(VKI_EXT_EXTENDED_DYNAMIC_STATE "Build vulkan with EXTENDED_DYNAMIC_STATE extention" OFF)
#endif

    option(ICD_BUILD_LLPC "Build LLPC?" ON)

    option(ICD_BUILD_LLPCONLY "Build LLPC Only?" OFF)

    option(XGL_LLVM_UPSTREAM "Build with upstreamed LLVM?" OFF)

    option(XGL_ENABLE_ASSERTIONS "Enable assertions in release builds" OFF)

    option(XGL_ENABLE_LIBCXX "Use libc++. This is intended for MemorySanitizer support only." OFF)

    option(ICD_GPUOPEN_DEVMODE_BUILD "Build ${PROJECT_NAME} with GPU Open Developer Mode driver support?" ON)

    option(ICD_MEMTRACK "Turn on memory tracking?" ${CMAKE_BUILD_TYPE_DEBUG})

    if (NOT WIN32)
        option(BUILD_WAYLAND_SUPPORT "Build XGL with Wayland support" ON)

        option(BUILD_XLIB_XRANDR_SUPPORT "Build Xlib with xrandr 1.6 support" OFF)

        option(BUILD_DRI3_SUPPORT "Build XGL with Dri3 support" ON)
    endif()

    option(ICD_ANALYSIS_WARNINGS_AS_ERRORS "Warnings as errors?" OFF)

endmacro()
