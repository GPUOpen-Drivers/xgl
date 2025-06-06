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

### Create XGL Library #################################################################################################
add_library(xgl SHARED)

set(VKI_TARGET amdvlk${TARGET_ARCHITECTURE_BITS})
set_target_properties(xgl PROPERTIES OUTPUT_NAME ${VKI_TARGET})
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
if (VKI_BUILD_WAYLAND)
    target_include_directories(xgl PUBLIC ${WAYLAND_INCLUDE_DIRS})
endif()

#if VKI_RAY_TRACING
if (VKI_RAY_TRACING)
    target_include_directories(xgl PRIVATE ${XGL_GPURT_PATH})
endif()
#endif

#if VKI_GPU_DECOMPRESS
if (VKI_GPU_DECOMPRESS)
    target_include_directories(xgl PRIVATE ${XGL_GPUTEXDECODER_PATH})
endif()
#endif

if (UNIX)
    # 'PAL/src/core/os/amdgpu/include/drm' may be removed later. PAL drm moved to 'shared' dir, old path left for compatibility.
    target_include_directories(xgl PRIVATE ${XGL_PAL_PATH}/src/core/os/amdgpu/include/drm)
    target_include_directories(xgl PRIVATE ${XGL_PAL_PATH}/shared/amdgpu/include/drm)
endif()

### XGL Subprojects ####################################################################################################
# PAL
add_subdirectory(${XGL_PAL_PATH} ${PROJECT_BINARY_DIR}/pal)

# Add LLPC version before gpurt
include("${XGL_VKGC_PATH}/cmake/llpc_version.cmake")
add_llpc_version_projects()

#if VKI_RAY_TRACING
# GPURT
if (VKI_RAY_TRACING)
    add_subdirectory(${XGL_GPURT_PATH} ${PROJECT_BINARY_DIR}/gpurt)
endif()
#endif

#if VKI_GPU_DECOMPRESS
# GPU Texture Decode
if (VKI_GPU_DECOMPRESS)
    add_subdirectory(${XGL_GPUTEXDECODER_PATH} ${PROJECT_BINARY_DIR}/gputexdecoder)
endif()
#endif

### XGL Sources ########################################################################################################

### ICD api ###################################################################
target_sources(xgl PRIVATE
    api/app_profile.cpp
    api/app_resource_optimizer.cpp
    api/app_shader_optimizer.cpp
    api/barrier_policy.cpp
    api/cmd_buffer_ring.cpp
    api/color_space_helper.cpp
    api/compiler_solution.cpp
    api/debug_printf.cpp
    api/entry.cpp
    api/gpumemory_event_handler.cpp
    api/internal_mem_mgr.cpp
    api/pipeline_compiler.cpp
    api/pipeline_binary_cache.cpp
    api/graphics_pipeline_common.cpp
    api/cache_adapter.cpp
    api/virtual_stack_mgr.cpp
    api/vk_alloccb.cpp
    api/vk_buffer.cpp
    api/vk_buffer_view.cpp
    api/vk_cmdbuffer.cpp
    api/vk_cmdbuffer_transfer.cpp
    api/vk_cmd_pool.cpp
    api/vk_compute_pipeline.cpp
    api/vk_conv.cpp
    api/vk_debug_report.cpp
    api/vk_debug_utils.cpp
    api/vk_descriptor_buffer.cpp
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
    api/vk_graphics_pipeline_library.cpp
    api/vk_image.cpp
    api/vk_image_view.cpp
    api/vk_instance.cpp
    api/vk_memory.cpp
    api/vk_pipeline.cpp
    api/vk_pipeline_binary.cpp
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
    api/vk_utils.cpp
    api/vk_indirect_commands_layout.cpp
    api/appopt/barrier_filter_layer.cpp
    api/appopt/strange_brigade_layer.cpp
    api/appopt/baldurs_gate3_layer.cpp
    api/appopt/shadow_of_the_tomb_raider_layer.cpp
    api/appopt/g_shader_profile.cpp
    api/render_state_cache.cpp
    api/renderpass/renderpass_builder.cpp
    api/utils/temp_mem_arena.cpp
    api/utils/json_reader.cpp
    api/utils/json_writer.cpp
    api/icd_main.cpp
)

#if VKI_RAY_TRACING
if (VKI_RAY_TRACING)
    target_sources(xgl PRIVATE
        api/raytrace/vk_acceleration_structure.cpp
        api/raytrace/vk_ray_tracing_pipeline.cpp
        api/raytrace/ray_tracing_device.cpp
        api/raytrace/cps_global_memory.cpp
        api/raytrace/cps_cmdbuffer_util.cpp
        api/vk_deferred_operation.cpp
        api/appopt/bvh_batch_layer.cpp
        api/appopt/split_raytracing_layer.cpp
        api/appopt/acceleration_structure_async_layer.cpp
    )
endif()
#endif

#if VKI_GPU_DECOMPRESS
if(VKI_GPU_DECOMPRESS)
    target_sources(xgl PRIVATE
        api/appopt/gpu_decode_layer.cpp
    )
endif()
#endif

if(ICD_BUILD_LLPC)
    target_sources(xgl PRIVATE
        api/compiler_solution_llpc.cpp
    )
endif()

### ICD Auto-generated Shader Profiles Files ##################################
# VKI_GENDIR Path to the code generation tools
set(VKI_GENDIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate)

# ICD shader profile code generation main script
set(VKI_GEN_SHADER_PROFILE_SCRIPTS ${VKI_GENDIR}/genShaderProfile.py ${VKI_GENDIR}/shaderProfileTemplate.py)

set(VKI_SHADER_PROFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/api/appopt)

file(GLOB_RECURSE VKI_ALL_SHADER_PROFILE_FILES
    ${VKI_SHADER_PROFILE_DIR}/shader_profiles/profile.json
)

add_custom_command(
    OUTPUT ${VKI_SHADER_PROFILE_DIR}/g_shader_profile.cpp ${VKI_SHADER_PROFILE_DIR}/g_shader_profile.h
    COMMAND ${PYTHON_CMD} ${VKI_GENDIR}/genShaderProfile.py ${VKI_SHADER_PROFILE_DIR}
    DEPENDS ${VKI_GEN_SHADER_PROFILE_SCRIPTS} ${VKI_ALL_SHADER_PROFILE_FILES}
    COMMENT "Generating shader profiles code from all profile.json files"
)

add_custom_target(
    GenerateShaderProfiles
    DEPENDS ${VKI_GEN_SHADER_PROFILE_SCRIPTS} ${VKI_ALL_SHADER_PROFILE_FILES}
    COMMENT "Checking if re-generation is required for shader profiles"
)

add_dependencies(xgl GenerateShaderProfiles)

### ICD Auto-generated String Files ###########################################
set(VKI_STRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/api/strings)
# ICD settings code generation main script
set(VKI_GEN_STRINGS ${VKI_STRING_DIR}/generate_strings.py)

set(VKI_GEN_STRINGS_FILES ${VKI_GEN_STRINGS} ${VKI_STRING_DIR}/func_table_template.py)

set(VKI_STRING_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/api/strings)

set(VKI_GEN_STRINGS_OPT -w ${VKI_STRING_DIR} -d ${VKI_STRING_OUTPUT_DIR})

set(VKI_STRING_OUTPUT_FILES ${VKI_STRING_OUTPUT_DIR}/g_entry_points_decl.h
                            ${VKI_STRING_OUTPUT_DIR}/g_entry_points_impl.h
                            ${VKI_STRING_OUTPUT_DIR}/g_extensions_decl.h
                            ${VKI_STRING_OUTPUT_DIR}/g_extensions_impl.h
                            ${VKI_STRING_OUTPUT_DIR}/g_func_table.h
)

set(VKI_STRING_SOURCE_FILES ${VKI_STRING_DIR}/entry_points.txt
                            ${VKI_STRING_DIR}/extensions.txt
)

add_custom_command(
    OUTPUT ${VKI_STRING_OUTPUT_FILES}
    COMMAND ${PYTHON_CMD} ${VKI_GEN_STRINGS} ${VKI_GEN_STRINGS_OPT}
    DEPENDS ${VKI_GEN_STRINGS_FILES} ${VKI_STRING_SOURCE_FILES}
    COMMENT "Generating Vulkan api strings ${VKI_GEN_STRINGS_OPT}"
)

add_custom_target(
    RunVKStringsGenerator
    DEPENDS ${VKI_STRING_OUTPUT_FILES}
    COMMENT "Checking if re-generation is required for strings"
)

add_dependencies(xgl RunVKStringsGenerator)

target_sources(xgl PRIVATE api/strings/strings.cpp)

### ICD Auto-generated Settings Files #########################################
# ICD settings code generation main script

set(XGL_DEVDRIVER_PATH ${XGL_PAL_PATH}/shared/devdriver)
set(VKI_DD_GENDIR ${XGL_DEVDRIVER_PATH}/apis/settings/codegen)

set(VKI_GEN_SETTINGS ${VKI_DD_GENDIR}/settings_codegen.py)

set(VKI_GEN_SETTINGS_FILES ${VKI_GEN_SETTINGS})

set(VKI_SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/settings)

add_custom_command(
    OUTPUT ${VKI_SETTINGS_DIR}/g_settings.cpp ${VKI_SETTINGS_DIR}/g_settings.h
    COMMAND ${PYTHON_CMD} ${VKI_GEN_SETTINGS}
        -i ${VKI_SETTINGS_DIR}/settings_xgl.json
        -o ${VKI_SETTINGS_DIR}
        -g settings
        -s settings/settings.h
        --namespaces vk
        --settings-struct-name RuntimeSettings
        --include-headers pal.h palImage.h palDevice.h
    DEPENDS ${VKI_GEN_SETTINGS_FILES} ${VKI_SETTINGS_DIR}/settings_xgl.json
    COMMENT "Generating Vulkan settings code from settings_xgl.json"
)

add_custom_target(
    RunVKSettingsGenerator
    DEPENDS ${VKI_GEN_SETTINGS_FILES} ${VKI_SETTINGS_DIR}/settings_xgl.json
    COMMENT "Checking if re-generation is required for settings"
)

add_dependencies(xgl RunVKSettingsGenerator)

target_sources(xgl PRIVATE
    settings/g_settings.cpp
    settings/settings.cpp
    settings/settings_xgl.json
)

add_custom_command(
    OUTPUT ${VKI_SETTINGS_DIR}/g_experiments.cpp ${VKI_SETTINGS_DIR}/g_experiments.h
    COMMAND ${PYTHON_CMD} ${VKI_GEN_SETTINGS}
        -i ${VKI_SETTINGS_DIR}/experiments_settings_xgl.json
        -o ${VKI_SETTINGS_DIR}
        -g experiments
        -s settings/experimentsLoader.h
        --namespaces vk
        --settings-struct-name ExpSettings
        --classname ExperimentsLoader
    DEPENDS ${VKI_GEN_SETTINGS_FILES} ${VKI_SETTINGS_DIR}/experiments_settings_xgl.json
    COMMENT "Generating Vulkan settings code from experiments_settings_xgl.json"
)

add_custom_target(
    RunVKExperimentsGenerator
    DEPENDS ${VKI_GEN_SETTINGS_FILES} ${VKI_SETTINGS_DIR}/experiments_settings_xgl.json
    COMMENT "Checking if re-generation is required for settings"
)

add_dependencies(xgl RunVKExperimentsGenerator)

target_sources(xgl PRIVATE
    settings/g_experiments.cpp
    settings/experimentsLoader.cpp
    settings/experiments_settings_xgl.json
)

### ICD api/sqtt ##############################################################
target_sources(xgl PRIVATE
    api/sqtt/sqtt_layer.cpp
    api/sqtt/sqtt_mgr.cpp
    api/sqtt/sqtt_object_mgr.cpp
)

### ICD api/devmode ###########################################################
target_sources(xgl PRIVATE
    api/devmode/devmode_rgp.cpp
    api/devmode/devmode_ubertrace.cpp
)

### ICD layer ##################################################################
    target_sources(xgl PRIVATE
        layers/query_dlist.cpp
        layers/vk_layer_all_null_devices.cpp
        layers/vk_layer_switchable_graphics.cpp
    )

### Link Libraries #####################################################################################################
if (UNIX)
    target_link_libraries(xgl PRIVATE ${CMAKE_DL_LIBS})
    # Link pthread
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(xgl PRIVATE Threads::Threads)

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
        message(WARNING "Intel ICC untested in CMake.")
        target_link_libraries(xgl PRIVATE -fabi-version=0 -static-intel)
    endif()

    if(CMAKE_BUILD_TYPE_RELEASE AND VKI_ENABLE_LTO)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            target_link_options(xgl PRIVATE -Wno-stringop-overflow)
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

    target_link_options(xgl PRIVATE
        -Wl,-Bdynamic
        -Wl,-z,noexecstack
        -Wl,-Map=$<TARGET_FILE_DIR:xgl>/${VKI_TARGET}.map
        -Wl,-soname=${VKI_TARGET}.so.1
    )

    target_link_options(xgl PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/make/amdicd.so.def)

    if(NOT BUILD_SHARED_LIBS)
        # Link libstdc++ statically, so that symbols from libstdc++ do not cause conflicts
        target_link_options(xgl PRIVATE -static-libgcc -static-libstdc++)
    endif()

    if(NOT VKI_USE_SANITIZER)
        # -Wl,--no-undefined is incompatible with asan
        target_link_libraries(xgl PRIVATE
            -Wl,--no-undefined
        )
    endif()

endif()

target_link_libraries(xgl PRIVATE vkgc)
target_link_libraries(xgl PRIVATE pal)
target_link_libraries(xgl PRIVATE khronos_vulkan_interface)
target_link_libraries(xgl PRIVATE xgl_interface)

#if VKI_RAY_TRACING
if (VKI_RAY_TRACING)
    target_link_libraries(xgl PRIVATE gpurt)
endif()
#endif

#if VKI_GPU_DECOMPRESS
if(VKI_GPU_DECOMPRESS)
    target_link_libraries(xgl PRIVATE gputexdecoder)
endif()
#endif

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

# Strip the symbols for release build, generate debug file
if(UNIX)
        if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
            if (DEFINED TOOLCHAIN_TARGET)
                set(TOOLCHAIN_TARGET_PREFIX "${TOOLCHAIN_TARGET}-")
            else()
                set(TOOLCHAIN_TARGET_PREFIX "")
            endif()
            add_custom_command(
                TARGET xgl POST_BUILD
                COMMAND ${TOOLCHAIN_TARGET_PREFIX}objcopy --only-keep-debug ${CMAKE_CURRENT_BINARY_DIR}/${VKI_TARGET}.so ${CMAKE_CURRENT_BINARY_DIR}/${VKI_TARGET}.so.debug
                COMMAND ${TOOLCHAIN_TARGET_PREFIX}strip  ${CMAKE_CURRENT_BINARY_DIR}/${VKI_TARGET}.so
                COMMAND ${TOOLCHAIN_TARGET_PREFIX}objcopy --add-gnu-debuglink=${CMAKE_CURRENT_BINARY_DIR}/${VKI_TARGET}.so.debug ${CMAKE_CURRENT_BINARY_DIR}/${VKI_TARGET}.so
            )
        endif()
endif()

### ICD loader configuration ###########################################################################################
if(UNIX)
    include(GNUInstallDirs)
    set(ISABITS ${TARGET_ARCHITECTURE_BITS})
    set(AMDVLK_INSTALL_PATH ${CMAKE_INSTALL_FULL_LIBDIR})
    configure_file(Loader/LunarG/Lnx/amd-icd.json amd_icd${ISABITS}.json @ONLY)
endif()
