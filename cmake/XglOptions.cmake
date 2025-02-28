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

macro(xgl_options)

### Cached Project Options #############################################################################################

#if VKI_ENABLE_DEBUG_BARRIERS
    option(VKI_ENABLE_DEBUG_BARRIERS "Build with debug barriers enabled?" OFF)
#endif
#if VKI_RUNTIME_APP_PROFILE
    option(VKI_RUNTIME_APP_PROFILE "Build with runtime app profile?" OFF)
#endif
#if VKI_DEVMODE_COMPILER_SETTINGS
    option(VKI_DEVMODE_COMPILER_SETTINGS "Build with devmode compiler settings?" OFF)
#endif

    option(VKI_ENABLE_PRINTS_ASSERTS "Build with debug print enabled?" OFF)

    option(VKI_ENABLE_LTO "Build with LTO enabled?" ON)

    option(VKI_ENABLE_GCOV "Build with gcov source code coverage?" OFF)
#if VKI_BUILD_STRIX_HALO
    option(VKI_BUILD_STRIX_HALO "Build vulkan for STRIX_HALO" ON)
#endif

    option(VKI_BUILD_TESTS "Build tests?" OFF)

    option(VKI_BUILD_TOOLS "Build tools?" OFF)

#if VKI_RAY_TRACING
    option(VKI_RAY_TRACING "Build vulkan with RAY_TRACING" ON)
#endif

#if VKI_GPU_DECOMPRESS
    option(VKI_GPU_DECOMPRESS "Build vulkan with GPU_DECOMPRESS" ON)
#endif

    option(ICD_BUILD_LLPC "Build LLPC?" ON)

    option(VKI_ENABLE_ASSERTIONS "Enable assertions in release builds" OFF)

    option(VKI_ENABLE_LIBCXX "Use libc++. This is intended for MemorySanitizer support only." OFF)

    if(UNIX AND (NOT ANDROID))
        option(VKI_BUILD_WAYLAND "Build XGL with Wayland support" ON)

        option(VKI_BUILD_DRI3 "Build XGL with Dri3 support" ON)
    endif()

    option(VKI_ANALYSIS_WARNINGS_AS_ERRORS "Warnings as errors?" OFF)

endmacro()
