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

macro(xgl_set_compile_definitions)
### Build Definitions #################################################################################################
    target_compile_definitions(xgl PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)

    if(TARGET_ARCHITECTURE_BITS EQUAL 32)
        target_compile_definitions(xgl PRIVATE ICD_X86_BUILD)
    elseif(TARGET_ARCHITECTURE_BITS EQUAL 64)
        target_compile_definitions(xgl PRIVATE ICD_X64_BUILD)
    endif()

    # Turn on the memory tracker if enabled.
    if(ICD_MEMTRACK)
        target_compile_definitions(xgl PRIVATE ICD_MEMTRACK)
    endif()

    # Enable relevant GPUOpen preprocessor definitions
    if(ICD_GPUOPEN_DEVMODE_BUILD)
        target_compile_definitions(xgl PRIVATE ICD_GPUOPEN_DEVMODE_BUILD)
    endif()

    if(ICD_BUILD_LLPC)
        target_compile_definitions(xgl PRIVATE ICD_BUILD_LLPC)
    endif()

    target_compile_definitions(xgl PRIVATE PAL_BUILD_GFX9=1)

    target_compile_definitions(xgl PRIVATE PAL_BUILD_GFX10)

    target_compile_definitions(xgl PRIVATE PAL_BUILD_RAVEN2)

    target_compile_definitions(xgl PRIVATE PAL_BUILD_RENOIR)

    target_compile_definitions(xgl PRIVATE PAL_BUILD_VEGA20)

    if(XGL_BUILD_NAVI12)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_NAVI12=1)
    endif()

    if(XGL_BUILD_NAVI22)
        target_compile_definitions(xgl PRIVATE VKI_BUILD_NAVI22=1)
    endif()

#if VKI_KHR_DISPLAY
    if(VKI_KHR_DISPLAY)
        target_compile_definitions(xgl PRIVATE VKI_KHR_DISPLAY)
    endif()
#endif

#if VKI_NORMALIZED_TRIG_FUNCTIONS
    if(VKI_NORMALIZED_TRIG_FUNCTIONS)
        target_compile_definitions(xgl PRIVATE VKI_NORMALIZED_TRIG_FUNCTIONS)
    endif()
#endif

    if (XGL_ENABLE_GCOV)
        target_compile_definitions(xgl PRIVATE ICD_ENABLE_GCOV)
    endif()

#if VKI_EXPOSE_EXT_CONDITIONAL_RENDERING
    if(VKI_EXPOSE_EXT_CONDITIONAL_RENDERING)
        target_compile_definitions(xgl PRIVATE VKI_EXPOSE_EXT_CONDITIONAL_RENDERING)
    endif()
#endif

    if(BUILD_WAYLAND_SUPPORT)
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
