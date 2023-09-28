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

macro(xgl_options)

### Cached Project Options #############################################################################################

    option(XGL_ENABLE_PRINTS_ASSERTS "Build with debug print enabled?" OFF)

    option(XGL_ENABLE_LTO "Build with LTO enabled?" ON)

    option(XGL_ENABLE_GCOV "Build with gcov source code coverage?" OFF)

    option(XGL_BUILD_GFX103 "Build vulkan for GFX103" ON)

    option(XGL_BUILD_NAVI12 "Build vulkan for Navi12" ON)

    option(XGL_BUILD_REMBRANDT "Build vulkan for REMBRANDT" ON)

    option(XGL_BUILD_RAPHAEL "Build vulkan for RAPHAEL" ON)

    option(XGL_BUILD_MENDOCINO "Build vulkan for MENDOCINO" ON)

#if VKI_BUILD_GFX11
    option(XGL_BUILD_GFX11 "Build vulkan for GFX11" ON)
#endif

#if VKI_BUILD_NAVI31
    option(XGL_BUILD_NAVI31 "Build vulkan for Navi31" ON)
#endif

#if VKI_BUILD_NAVI32
    option(XGL_BUILD_NAVI32 "Build vulkan for Navi32" ON)
#endif

#if VKI_BUILD_NAVI33
    option(XGL_BUILD_NAVI33 "Build vulkan for Navi33" ON)
#endif

    option(XGL_BUILD_PHOENIX1 "Build vulkan for PHOENIX1" ON)

    option(XGL_BUILD_TESTS "Build tests?" OFF)

    option(XGL_BUILD_TOOLS "Build tools?" OFF)

    option(XGL_BUILD_CACHE_CREATOR "Build cache-creator tools?" OFF)

#if VKI_RAY_TRACING
    option(VKI_RAY_TRACING "Build vulkan with RAY_TRACING" ON)
#endif

#if VKI_GPU_DECOMPRESS
    option(VKI_GPU_DECOMPRESS "Build vulkan with GPU_DECOMPRESS" ON)
#endif

        option(ICD_BUILD_LLPC "Build LLPC?" ON)
    option(ICD_BUILD_LLPCONLY "Build LLPC Only?" OFF)

    option(XGL_LLVM_UPSTREAM "Build with upstreamed LLVM?" OFF)

    option(XGL_ENABLE_ASSERTIONS "Enable assertions in release builds" OFF)

    option(XGL_ENABLE_LIBCXX "Use libc++. This is intended for MemorySanitizer support only." OFF)

    option(ICD_GPUOPEN_DEVMODE_BUILD "Build ${PROJECT_NAME} with GPU Open Developer Mode driver support?" ON)

    option(ICD_MEMTRACK "Turn on memory tracking?" ${CMAKE_BUILD_TYPE_DEBUG})

    if(UNIX AND (NOT ANDROID))
        option(BUILD_WAYLAND_SUPPORT "Build XGL with Wayland support" ON)

        option(BUILD_DRI3_SUPPORT "Build XGL with Dri3 support" ON)
    endif()

    option(ICD_ANALYSIS_WARNINGS_AS_ERRORS "Warnings as errors?" OFF)

endmacro()
