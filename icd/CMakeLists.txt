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
add_library(xgl SHARED)

set(ICD_TARGET amdvlk${TARGET_ARCHITECTURE_BITS})
set_target_properties(xgl PROPERTIES OUTPUT_NAME ${ICD_TARGET})
set_target_properties(xgl PROPERTIES PREFIX "")

install(TARGETS xgl DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

### Compiler Options ###################################################################################################
xgl_compiler_options(xgl)

# Add global compile option to enable position independent code (we get link errors for clang/lld otherwise)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fPIC)
endif()

### Build Definitions #################################################################################################
xgl_set_compile_definitions()

### Include Directories ################################################################################################
if (BUILD_WAYLAND_SUPPORT)
    target_include_directories(xgl PUBLIC ${WAYLAND_INCLUDE_DIRS})
endif()

target_include_directories(xgl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    api
    api/include
)

### XGL Subprojects ####################################################################################################
# PAL
add_subdirectory(${XGL_PAL_PATH} ${PROJECT_BINARY_DIR}/pal)

# XGL cache creator tool
if(XGL_BUILD_CACHE_CREATOR)
    if(NOT ICD_BUILD_LLPC)
        message(FATAL_ERROR "Cannot build cache-creator tools without LLPC")
    endif()
    add_subdirectory(${XGL_CACHE_CREATOR_PATH} ${CMAKE_BINARY_DIR}/tools)
endif()

### XGL Sources ########################################################################################################

### ICD api ###################################################################
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
    api/cache_adapter.cpp
    api/shader_cache.cpp
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
    api/vk_pipeline.cpp
    api/vk_pipeline_layout.cpp
    api/vk_pipeline_cache.cpp
    api/vk_private_data_slot.cpp
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

if(ICD_BUILD_LLPC)
    target_sources(xgl PRIVATE
        api/compiler_solution_llpc.cpp
    )
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

### ICD Auto-generated Shader Profiles Files ##################################
# ICD_GENDIR Path to the code generation tools
set(ICD_GENDIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate)

# ICD shader profile code generation main script
set(ICD_GEN_SHADER_PROFILE_SCRIPTS ${ICD_GENDIR}/genShaderProfile.py ${ICD_GENDIR}/shaderProfileTemplate.py)

set(ICD_SHADER_PROFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/api/appopt)

set(ICD_SHADER_PROFILE_SOURCE_FILES_DIR
    ${ICD_SHADER_PROFILE_DIR}/shader_profiles
    )

add_custom_command(
    OUTPUT ${ICD_SHADER_PROFILE_DIR}/g_shader_profile.cpp ${ICD_SHADER_PROFILE_DIR}/g_shader_profile.h
    COMMAND ${PYTHON_CMD} ${ICD_GENDIR}/genShaderProfile.py ${ICD_SHADER_PROFILE_DIR}/shader_profiles
    DEPENDS ${ICD_GEN_SHADER_PROFILE_SCRIPTS} ${ICD_SHADER_PROFILE_SOURCE_FILES_DIR}
    COMMENT "Generating shader profiles code from all profile.json files"
)

add_custom_target(
    GenerateShaderProfiles
    DEPENDS ${ICD_GEN_SHADER_PROFILE_SCRIPTS} ${ICD_SHADER_PROFILE_SOURCE_FILES_DIR}
    COMMENT "Checking if re-generation is required for shader profiles"
)

add_dependencies(xgl GenerateShaderProfiles)

### ICD Auto-generated String Files ###########################################
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

### ICD Auto-generated Settings Files #########################################
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

### ICD api/sqtt ##############################################################
target_sources(xgl PRIVATE
    api/sqtt/sqtt_layer.cpp
    api/sqtt/sqtt_mgr.cpp
    api/sqtt/sqtt_object_mgr.cpp
)

### ICD api/devmode ###########################################################
if(ICD_GPUOPEN_DEVMODE_BUILD)
    target_sources(xgl PRIVATE api/devmode/devmode_mgr.cpp)
endif()

### ICD layer ##################################################################
target_sources(xgl PRIVATE
    layers/query_dlist.cpp
    layers/vk_layer_all_null_devices.cpp
    layers/vk_layer_switchable_graphics.cpp
)

### ICD Android interface ######################################################

### Link Libraries #####################################################################################################
if (UNIX)
    target_link_libraries(xgl PRIVATE dl c stdc++ ${CMAKE_DL_LIBS} pthread)

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

target_link_libraries(xgl PRIVATE vkgc)
target_link_libraries(xgl PRIVATE pal)
target_link_libraries(xgl PRIVATE khronos_vulkan_interface)

### XGL cache_support ####
add_library(xgl_cache_support INTERFACE)

target_sources(xgl_cache_support INTERFACE
    api/binary_cache_serialization.cpp
)
target_include_directories(xgl_cache_support INTERFACE
    api
    api/include/khronos
)
target_link_libraries(xgl_cache_support INTERFACE pal)

target_link_libraries(xgl PRIVATE xgl_cache_support)

### Visual Studio Filters ##############################################################################################
target_find_headers(xgl)
if(MSVC)
    target_source_groups(xgl)
endif()

### ICD loader configuration ###########################################################################################
if(UNIX)
    include(GNUInstallDirs)
    set(ISABITS ${TARGET_ARCHITECTURE_BITS})
    set(AMDVLK_INSTALL_PATH ${CMAKE_INSTALL_FULL_LIBDIR})
    configure_file(Loader/LunarG/Lnx/amd-icd.json amd_icd${ISABITS}.json @ONLY)
endif()

