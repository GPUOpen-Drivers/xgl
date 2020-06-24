##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
add_library(xgl SHARED "")
set(ICD_TARGET amdvlk${TARGET_ARCHITECTURE_BITS})
set_target_properties(xgl PROPERTIES OUTPUT_NAME ${ICD_TARGET})
set_target_properties(xgl PROPERTIES PREFIX "")
install(TARGETS xgl DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

### Compiler Options ###################################################################################################
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(ICD_ANALYSIS_WARNINGS_AS_ERRORS)
        target_compile_options(xgl PRIVATE
            -Werror
            -Wno-error=comment
            -Wno-error=pointer-arith
        )
    endif()

    target_compile_options(xgl PRIVATE
        -fno-exceptions
        -fno-strict-aliasing
        -fPIC
        -pthread
    )

    target_compile_options(xgl PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
        -fno-math-errno
        -fno-rtti
        -std=c++14
        -Wno-comment
        -Wno-ignored-qualifiers
        -Wno-missing-field-initializers
        -Wno-pointer-arith
        -Wno-switch
        -Wno-unused
        -Wno-unused-parameter
    >)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(xgl PRIVATE
            # Output with color if in terminal: https://github.com/ninja-build/ninja/wiki/FAQ
            -fdiagnostics-color=always
            -mpreferred-stack-boundary=6
            -fno-threadsafe-statics
            -fmerge-all-constants
            -fms-extensions
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(xgl PRIVATE
            # Output with color if in terminal: https://github.com/ninja-build/ninja/wiki/FAQ
            -fcolor-diagnostics
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
            OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7))
        target_compile_options(xgl PRIVATE
            -fno-delete-null-pointer-checks
        )
    endif()

    if(TARGET_ARCHITECTURE_BITS EQUAL 32)
        target_compile_options(xgl PRIVATE -msse -msse2)
    endif()

    if(CMAKE_BUILD_TYPE_RELEASE)
        target_compile_options(xgl PRIVATE -O3)
        if(XGL_ENABLE_LTO)
            if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
                execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
                if(GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                    # add global definition to enable LTO here since some components have no option
                    # to enable it.
                    add_definitions("-flto -fuse-linker-plugin -Wno-odr")
                    message(WARNING "LTO enabled for XGL")
                endif()
            elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
                # add global definition to enable LTO here since some components have no option
                # to enable it.
                add_definitions("-flto=thin")
                message(WARNING "LTO enabled for XGL")
            endif()
        endif()
    endif()
else()
    message(FATAL_ERROR "Using unknown compiler")
endif()

### Build Definitions ##################################################################################################
target_compile_definitions(xgl PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)
target_compile_definitions(xgl PRIVATE PAL_BUILD_GFX9=1)

# Internal developer build

if(ICD_BUILD_LLPC)
    target_compile_definitions(xgl PRIVATE ICD_BUILD_LLPC)
endif()

if(XGL_BUILD_RAVEN2)
    target_compile_definitions(xgl PRIVATE VKI_BUILD_RAVEN2=1)
    target_compile_definitions(xgl PRIVATE PAL_BUILD_RAVEN2)
endif()
if(XGL_BUILD_RENOIR)
    target_compile_definitions(xgl PRIVATE VKI_BUILD_RENOIR=1)
    target_compile_definitions(xgl PRIVATE PAL_BUILD_RENOIR)
endif()
if(XGL_BUILD_VEGA20)
    target_compile_definitions(xgl PRIVATE VKI_BUILD_VEGA20=1)
    target_compile_definitions(xgl PRIVATE PAL_BUILD_VEGA20)
endif()
if(XGL_BUILD_GFX10)
    target_compile_definitions(xgl PRIVATE VKI_BUILD_GFX10=1)
    target_compile_definitions(xgl PRIVATE PAL_BUILD_GFX10)
    target_compile_definitions(xgl PRIVATE LLPC_BUILD_GFX10)
endif()
if(XGL_BUILD_NAVI14)
    target_compile_definitions(xgl PRIVATE VKI_BUILD_NAVI14=1)
endif()

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
    # The name of the variable below must match the one in the GpuOpen CMakeLists.txt
endif()

target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION_SUPPORTS_SHADER_CACHE_EXPECTED_ENTRIES=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MINOR_VERSION=${PAL_CLIENT_INTERFACE_MINOR_VERSION})

target_compile_definitions(xgl PRIVATE LLPC_CLIENT_INTERFACE_MAJOR_VERSION=${LLPC_CLIENT_INTERFACE_MAJOR_VERSION})

if (BUILD_WAYLAND_SUPPORT)
    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
    target_include_directories(xgl PUBLIC ${WAYLAND_INCLUDE_DIRS})
endif()

set(XCB_REQUIRED ON)
if(XCB_REQUIRED)
    find_package(XCB)
endif()

if (${XCB_RANDR_LEASE})
    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XLIB_XRANDR_EXT=1)
endif()

if(UNIX)
    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XCB_KHR)
    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XLIB_KHR)
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

### Include Directories ################################################################################################

target_include_directories(xgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(xgl PRIVATE imported)

target_include_directories(xgl PRIVATE
    api
    api/include
    api/include/khronos
    ${XGL_VKGC_PATH}/include
)

target_include_directories(xgl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

### XGL Subprojects ####################################################################################################
### PAL ########################################################################
# For Open source, MSVC build is for amdllpc only which does not depend on pal.
if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_subdirectory(${XGL_PAL_PATH} ${PROJECT_BINARY_DIR}/pal)
endif()
### XGL Sources ########################################################################################################

### ICD api ####################################################################

### ICD Auto-generated Shader Profiles Files ###########################################################################
# ICD_GENDIR Path to the code generation tools
set(ICD_GENDIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate)

# ICD shader profile code generation main script
set(ICD_GEN_SHADER_PROFILE_SCRIPTS ${ICD_GENDIR}/genShaderProfile.py ${ICD_GENDIR}/shaderProfileTemplate.py )

set(ICD_SHADER_PROFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/api/appopt)

set(ICD_SHADER_PROFILE_SOURCE_FILES_DIR ${ICD_SHADER_PROFILE_DIR}/shader_profiles)

add_custom_command(
    OUTPUT ${ICD_SHADER_PROFILE_DIR}/g_shader_profile.cpp ${ICD_SHADER_PROFILE_DIR}/g_shader_profile.h
    COMMAND ${PYTHON_CMD} ${ICD_GENDIR}/genShaderProfile.py ${ICD_SHADER_PROFILE_SOURCE_FILES_DIR}
    DEPENDS ${ICD_GEN_SHADER_PROFILE_SCRIPTS} ${ICD_SHADER_PROFILE_SOURCE_FILES_DIR}
    COMMENT "Generating shader profiles code from all profile.json files"
)

add_custom_target(
    GenerateShaderProfiles
    DEPENDS ${ICD_GEN_SHADER_PROFILE_SCRIPTS} ${ICD_SHADER_PROFILE_SOURCE_FILES_DIR}
    COMMENT "Checking if re-generation is required for shader profiles"
)

add_dependencies(xgl GenerateShaderProfiles)

target_sources(xgl PRIVATE
    api/app_profile.cpp
    api/app_resource_optimizer.cpp
    api/app_shader_optimizer.cpp
    api/barrier_policy.cpp
    api/color_space_helper.cpp
    api/compiler_solution.cpp
    api/internal_mem_mgr.cpp
    api/pipeline_compiler.cpp
    api/pipeline_binary_cache.cpp
    api/shader_cache.cpp
    api/stencil_ops_combiner.cpp
    api/vert_buf_binding_mgr.cpp
    api/virtual_stack_mgr.cpp
    api/vk_alloccb.cpp
    api/vk_buffer.cpp
    api/vk_buffer_view.cpp
    api/vk_cmdbuffer.cpp
    api/vk_cmd_pool.cpp
    api/vk_compute_pipeline.cpp
    api/vk_conv.cpp
    api/vk_debug_report.cpp
    api/vk_debug_utils.cpp
    api/vk_descriptor_set.cpp
    api/vk_descriptor_set_layout.cpp
    api/vk_descriptor_pool.cpp
    api/vk_device.cpp
    api/vk_dispatch.cpp
    api/vk_display.cpp
    api/vk_display_manager.cpp
    api/vk_event.cpp
    api/vk_fence.cpp
    api/vk_formats.cpp
    api/vk_framebuffer.cpp
    api/vk_physical_device.cpp
    api/vk_physical_device_manager.cpp
    api/vk_graphics_pipeline.cpp
    api/vk_image.cpp
    api/vk_image_view.cpp
    api/vk_instance.cpp
    api/vk_memory.cpp
    api/vk_object.cpp
    api/vk_pipeline.cpp
    api/vk_pipeline_layout.cpp
    api/vk_pipeline_cache.cpp
    api/vk_query.cpp
    api/vk_queue.cpp
    api/vk_render_pass.cpp
    api/vk_sampler.cpp
    api/vk_sampler_ycbcr_conversion.cpp
    api/vk_semaphore.cpp
    api/vk_shader.cpp
    api/vk_swapchain.cpp
    api/vk_surface.cpp
    api/vk_gpa_session.cpp
    api/vk_descriptor_update_template.cpp
    api/appopt/barrier_filter_layer.cpp
    api/appopt/strange_brigade_layer.cpp
    api/appopt/async_layer.cpp
    api/appopt/async_shader_module.cpp
    api/appopt/async_partial_pipeline.cpp
    api/appopt/g_shader_profile.cpp
    api/render_state_cache.cpp
    api/renderpass/renderpass_builder.cpp
    api/renderpass/renderpass_logger.cpp
    api/utils/temp_mem_arena.cpp
    api/utils/json_reader.cpp
    api/utils/json_writer.cpp
    api/icd_main.cpp
)

if(XGL_BUILD_GFX10)
    target_sources(xgl PRIVATE api/appopt/wolfenstein2_layer.cpp)
endif()

# vk_physical_device.cpp uses the __DATE__ and __TIME__ macros to generate a pipelineCacheUUID.
# The following rule forces vk_physical_device.cpp to be re-compiled on every build, so that
# an up-to-date time/date is always used regardless of which files were touched since the last build.
add_custom_command(
    OUTPUT "vk_physical_device.g"
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/api/vk_physical_device.cpp
)

add_custom_target(
    RebuildVkPhysicalDevice
    DEPENDS "vk_physical_device.g"
)

add_dependencies(xgl RebuildVkPhysicalDevice)

### ICD Auto-generated String Files ####################################################################################
set(ICD_STRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/api/strings)
# ICD settings code generation main script
set(ICD_GEN_STRINGS ${ICD_STRING_DIR}/generate_strings.py)

set(ICD_GEN_STRINGS_OPT -w ${ICD_STRING_DIR})

set(ICD_GEN_STRINGS_FILES ${ICD_GEN_STRINGS} ${ICD_STRING_DIR}/func_table_template.py)

set(ICD_STRING_OUTPUT_FILES ${ICD_STRING_DIR}/g_entry_points_decl.h
                            ${ICD_STRING_DIR}/g_entry_points_impl.h
                            ${ICD_STRING_DIR}/g_extensions_decl.h
                            ${ICD_STRING_DIR}/g_extensions_impl.h
                            ${ICD_STRING_DIR}/g_func_table.h
)

set(ICD_STRING_SOURCE_FILES ${ICD_STRING_DIR}/entry_points.txt
                            ${ICD_STRING_DIR}/extensions.txt
)

add_custom_command(
    OUTPUT ${ICD_STRING_OUTPUT_FILES}
    COMMAND ${PYTHON_CMD} ${ICD_GEN_STRINGS} ${ICD_GEN_STRINGS_OPT}
    DEPENDS ${ICD_GEN_STRINGS_FILES} ${ICD_STRING_SOURCE_FILES}
    COMMENT "Generating Vulkan api strings ${ICD_GEN_STRINGS_OPT}"
)

add_custom_target(
    RunVKStringsGenerator
    DEPENDS ${ICD_GEN_STRINGS_FILES} ${ICD_STRING_SOURCE_FILES} ${ICD_STRING_OUTPUT_FILES} ${ICD_STRING_DIR}/strings.cpp
    COMMENT "Checking if re-generation is required for strings"
)

add_dependencies(xgl RunVKStringsGenerator)

    target_sources(xgl PRIVATE api/strings/strings.cpp)

# Or for a separate lib: icdapi
# add_subdirectory(${XGL_API_PATH} ${PROJECT_BINARY_DIR}/api)
# target_link_libraries(xgl api)

### ICD api/sqtt ###############################################################
target_sources(xgl PRIVATE
    api/sqtt/sqtt_layer.cpp
    api/sqtt/sqtt_mgr.cpp
    api/sqtt/sqtt_object_mgr.cpp
)

# Or for a separate lib: icdapisqtt
# add_subdirectory(api/sqtt ${PROJECT_BINARY_DIR}/api/sqtt)
# target_link_libraries(xgl sqtt)

### ICD api/devmode ############################################################
if(ICD_GPUOPEN_DEVMODE_BUILD)
    target_sources(xgl PRIVATE api/devmode/devmode_mgr.cpp)

    # Or for a separate lib: icdapidevmode
    # add_subdirectory(api/devmode ${PROJECT_BINARY_DIR}/api/devmode)
    # target_link_libraries(xgl devmode)
endif()

if(ICD_BUILD_LLPC)
    target_sources(xgl PRIVATE
        api/compiler_solution_llpc.cpp
    )
endif()

### ICD layer ##################################################################
target_sources(xgl PRIVATE
    layers/query_dlist.cpp
    layers/vk_layer_all_null_devices.cpp
    layers/vk_layer_switchable_graphics.cpp
)

### ICD Android interface ######################################################

### ICD Auto-generated Files ###########################################################################################
# ICD settings code generation main script
set(ICD_GEN_SETTINGS ${ICD_GENDIR}/genSettingsCode.py)

set(ICD_GEN_SETTINGS_FILES ${ICD_GEN_SETTINGS} ${ICD_GENDIR}/vulkanSettingsCodeTemplates.py)

set(ICD_SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/settings)

set(PAL_GENDIR ${XGL_PAL_PATH}/tools/generate)

set(COMPONENT_NAME vulkan)

add_custom_command(
    OUTPUT ${ICD_SETTINGS_DIR}/g_settings.cpp ${ICD_SETTINGS_DIR}/g_settings.h
    COMMAND ${PYTHON_CMD} ${ICD_GEN_SETTINGS} ${XGL_PAL_PATH} ${COMPONENT_NAME}
    DEPENDS ${ICD_GEN_SETTINGS_FILES} ${ICD_SETTINGS_DIR}/settings_xgl.json
    COMMENT "Generating Vulkan settings code from settings_xgl.json"
)

add_custom_target(
    RunVKSettingsGenerator
    DEPENDS ${ICD_GEN_SETTINGS_FILES} ${ICD_SETTINGS_DIR}/settings_xgl.json
    COMMENT "Checking if re-generation is required for settings"
)

add_dependencies(xgl RunVKSettingsGenerator)

target_sources(xgl PRIVATE
    settings/g_settings.cpp
    settings/settings.cpp
)

### XGL VKGC ####
target_link_libraries(xgl PRIVATE vkgc dl stdc++)

### Link Libraries #####################################################################################################
if (UNIX)
    target_link_libraries(xgl PRIVATE c stdc++ ${CMAKE_DL_LIBS} pthread)

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
        message(WARNING "Intel ICC untested in CMake.")
        target_link_libraries(xgl PRIVATE -fabi-version=0 -static-intel)
    endif()

    if(CMAKE_BUILD_TYPE_RELEASE AND XGL_ENABLE_LTO)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
            if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                target_link_libraries(xgl PRIVATE -flto=4  -fuse-linker-plugin -Wno-odr)
                message(WARNING "LTO enabled for Linking")
            endif()
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            # LTO enabled for final Linking (clang)
            target_link_libraries(xgl PRIVATE -flto=thin)
        endif()
    endif()

    target_link_libraries(xgl PRIVATE
        -Wl,-Bdynamic
        -Wl,-z,noexecstack
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/make/amdicd.so.def
        -Wl,-Map=${ICD_TARGET}.map
        -Wl,-soname=${ICD_TARGET}.so.1
    )

    if(NOT XGL_USE_SANITIZER)
        # -Wl,--no-undefined is incompatible with asan
        target_link_libraries(xgl PRIVATE
            -Wl,--no-undefined
        )
    endif()

endif()

target_link_libraries(xgl PRIVATE pal)
### Visual Studio Filters ##############################################################################################
target_vs_filters(xgl)
