### Create XGL Library #################################################################################################
add_library(xgl SHARED "")
set(ICD_TARGET amdvlk${TARGET_ARCHITECTURE_BITS})
set_target_properties(xgl PROPERTIES OUTPUT_NAME ${ICD_TARGET})
set_target_properties(xgl PROPERTIES PREFIX "")
install(TARGETS xgl DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: What should be global options?

        target_compile_options(xgl PRIVATE
            /GF     # Eliminate duplicate strings (one buffer multiple pointers)
            /FC     # Full path of source code file in diagnostics
        )

        # CMAKE-TODO: These are /W4 (level 4) warnings
        # Review what is acceptable and what is not.
        target_compile_options(xgl PRIVATE
            /wd4063 # case 'N' is not a valid value for switch of enum 'X'
            /wd4100 # unreferenced formal parameter
            /wd4127 # conditional expression is constant
            /wd4189 # local variable is initialized but not referenced
            /wd4201 # nonstandard extension used : nameless struct/union
            /wd4244 # 'Z' : conversion from 'X' to 'Y', possible loss of data
            /wd4310 # cast truncates constant value
            /wd4505 # unreferenced local function has been removed
            /wd4512 # assignment operator could not be generated
	    /wd4458 # declaration of 'priority' hides class member
        )

        # CMAKE-TODO: warning C6011: Dereferencing NULL pointer 'X'
        target_compile_options(xgl PRIVATE /wd6011)

        # CMAKE-TODO: vk_device.cpp(1041/1061) : warning C6255: _alloca indicates failure by raising
        # a stack overflow exception.  Consider using _malloca instead.
        target_compile_options(xgl PRIVATE /wd6255)

        if(CMAKE_BUILD_TYPE_DEBUG)
            # Disable frame pointer omission
            target_compile_options(xgl PRIVATE /Oy-)

            # Generate intrinsic functions
            target_compile_options(xgl PRIVATE /Oi)
        else()
            # Generate intermediate language for whole program optimization instead of code
            # for LTCG (link time-code generation optimization).
            target_compile_options(xgl PRIVATE /GL)
        endif()

        # Turn on "warnings as errors" if enabled.
        if(ICD_ANALYSIS_WARNINGS_AS_ERRORS)
            target_compile_options(xgl PRIVATE /analyze)
        else()
            target_compile_options(xgl PRIVATE /analyze:WX-)
        endif()

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(xgl PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fPIC -fno-rtti -fno-math-errno>)
        target_compile_options(xgl PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers -std=c++0x >)

        # bil
        target_compile_options(xgl PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-switch>)
        #set(GCC_SYMBOL_TYPE -ggdb)

        target_compile_options(xgl PRIVATE
            -Werror
            -pthread
            -fno-exceptions
        )

        # GCC only
        target_compile_options(xgl PRIVATE
            -mpreferred-stack-boundary=6
            -fno-threadsafe-statics
            -fmerge-all-constants
            -fms-extensions
        )

        # Disable warnings and errors that GCC will throw about comment formatting.
        target_compile_options(xgl PRIVATE -Wno-error=comment -Wno-comment)

        # Disable warnings and errors that GCC will throw about pointer arithmetic.
        target_compile_options(xgl PRIVATE -Wno-error=pointer-arith -Wno-pointer-arith)

        if(TARGET_ARCHITECTURE_BITS EQUAL 32)
            target_compile_options(xgl PRIVATE -msse -msse2)
        endif()

        if(CMAKE_BUILD_TYPE_RELEASE)
            target_compile_options(xgl PRIVATE -fno-strict-aliasing -fno-delete-null-pointer-checks -O3)
            execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
            if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                add_definitions("-flto  -fuse-linker-plugin -Wno-odr")
                message(WARNING "LTO enabled for XGL")
            endif()
        endif()

        set(ICD_USE_GCC TRUE)
        #message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
        #message(STATUS "Configured compiler options for Intel ICC.")
        message(WARNING "Intel ICC is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Build Definitions ##################################################################################################
target_compile_definitions(xgl PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)

target_compile_definitions(xgl PRIVATE ICD_BUILD_APPPROFILE)
if(ICD_OPEN_SOURCE)
    target_compile_definitions(xgl PRIVATE VK_OPEN_SOURCE)
endif()

# Internal developer build
if(VK_INTERNAL_DEVELOPER)
    target_compile_definitions(xgl PRIVATE VK_INTERNAL_DEVELOPER)
endif()

if(ICD_BUILD_LLPC)
    target_compile_definitions(xgl PRIVATE ICD_BUILD_LLPC)
endif()

if(ICD_BUILD_SCPC)
    target_compile_definitions(xgl PRIVATE ICD_BUILD_SCPC)
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

# Configure Vulkan SDK version definitions
if(USE_NEXT_SDK)
    set(VULKAN_SDK_VERSION sdk-1.1)
    target_compile_definitions(xgl PRIVATE SDK_1_1)
else()
    set(VULKAN_SDK_VERSION sdk-1.0)
    target_compile_definitions(xgl PRIVATE SDK_1_0)
endif()

# Enable relevant GPUOpen preprocessor definitions
if(ICD_GPUOPEN_DEVMODE_BUILD)
    target_compile_definitions(xgl PRIVATE ICD_GPUOPEN_DEVMODE_BUILD)
endif()

target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
target_compile_definitions(xgl PRIVATE PAL_CLIENT_INTERFACE_MAJOR_VERSION_SUPPORTS_SHADER_CACHE_EXPECTED_ENTRIES=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})

# Also can get FQDN Fully qualified domain name.
cmake_host_system_information(RESULT ICD_HOST_NAME QUERY HOSTNAME)
string(TIMESTAMP ICD_RESOURCE_TIME)

if(UNIX)
    target_compile_definitions(xgl PRIVATE ICD_LNX_BUILD)

    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XCB_KHR)
    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_XLIB_KHR)

    # CMAKE-TODO: Figure out how to setup the install targets.
    # I haven't done a top level build so don't know how these are used.
    # set(INST_TARGETS ${BUILD_DIR}/${ICD_TARGET:=.${CMAKE_SHARED_LIBRARY_SUFFIX}})

    # if(NOT DEFINED DEBUG_NO_NMSYM)
    #     INST_TARGETS += ${BUILD_DIR}/${ICD_TARGET:=.nms}
    # endif()

    # INST_DIR Defines for each OS version
    # Brahma requires vulkan install to lib
    #set(INST_DIR lib)
    #INST_DEST=${foreach dir,$(INST_DIR}, ${subst $(INST_DIR_SPACE},${SPACE},${subst,/,"$(INST_PATH})/${INST_SUB_PATH}/${dir}"))
elseif(WIN32)
    target_compile_definitions(xgl PRIVATE ICD_WIN_BUILD)

    if(ICD_WDDM2_BUILD)
        target_compile_definitions(xgl PRIVATE ICD_WDDM2_BUILD ${WNXTDEFS})
        target_compile_definitions(xgl PRIVATE ${WNXTDEFS})
    endif()

    target_compile_definitions(xgl PRIVATE VK_USE_PLATFORM_WIN32_KHR)

    if(DXC_DEVDRIVER_BUILD)
        target_compile_definitions(xgl PRIVATE DXC_DEVDRIVER_BUILD)
    endif()

    target_compile_definitions(xgl PRIVATE
        WINNT
        WINLH
        BUILD_WIN32_DLL
    )

    if(CMAKE_BUILD_TYPE_DEBUG)
        target_compile_definitions(xgl PRIVATE DBG FPO=0)
    else()
        target_compile_definitions(xgl PRIVATE FPO NDEBUG)
    endif()

    # options that are different between 64 and 32 bit targets go here
    if(TARGET_ARCHITECTURE_BITS EQUAL 64)
        # CMAKE-TODO: This should be unnecessary and not used.
        # target_compile_definitions(xgl PRIVATE _WIN64 _M_AMD64)

        # CMAKE-TODO: Where is this used?
        # set(DDK_LIB_DIR ${DK_C}/lib/wlh/amd64)
    else()
        target_compile_definitions(xgl PRIVATE WIN32_LEAN_AND_MEAN)
        # CMAKE-TODO: This should be unnecessary and not used.
        #target_compile_definitions(xgl PRIVATE IS_32 i386)
    endif()

    # CMAKE-TODO: Figure out how to setup the install targets.
    # I haven't done a top level build so don't know how these are used.
    # set(INST_TARGETS ${BUILD_DIR}/${ICD_TARGET:=.dll})
    #            ${BUILD_DIR}/${ICD_TARGET:=.pdb}

    # if(NOT DEFINED DEBUG_NO_NMSYM)
    #     INST_TARGETS += ${BUILD_DIR}/${ICD_TARGET:=.nms}
    # endif()
endif()

# CMAKE-TODO: What is log target used for? Where and how is it used?
# Common Makefile for Mantle ICD DLL build
# if(TARGET_ARCHITECTURE_BITS EQUAL 64)
#     if(${ICD_OS_TYPE} EQUAL w8)
#         set(LOG_TARGET icd_w864a)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNow)
#         set(LOG_TARGET icd_wNow64a)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNxt)
#         set(LOG_TARGET icd_wNxt64a)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL lnx)
#         set(LOG_TARGET icd_lnx64a)
#     endif()
# else()
#     if(${ICD_OS_TYPE} EQUAL w8)
#         set(LOG_TARGET icd_w8)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNow)
#         set(LOG_TARGET icd_wNow)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNxt)
#         set(LOG_TARGET icd_wNxt)
#     endif()
#     if(${ICD_OS_TYPE} EQUAL lnx)
#         set(LOG_TARGET icd_lnx)
#     endif()
# endif()

### Include Directories ################################################################################################

# Boost include path
if(ICD_BUILD_SCPC)
    if(DEFINED GLOBAL_ROOT_DK_DIR)
        find_package(AMDBoost 1.64.0)
    else()
        find_package(Boost 1.64)
        message(WARNING "Finding the boost package without GLOBAL_ROOT_DK_DIR is not tested.")
    endif()

    target_include_directories(xgl PRIVATE ${Boost_INCLUDE_DIRS})
endif()

if(UNIX)
    # CMAKE-TODO: What is this used for?  Should the toolchain be including this?
    # if(GLOBAL_ROOT_DK_DIR)
    #     if(TARGET_ARCHITECTURE_BITS EQUAL 32)
    #         target_include_directories(xgl PRIVATE
    #             ${GLOBAL_ROOT_DK_DIR}/${DK_INC}/include/c++/4.2.0
    #             ${GLOBAL_ROOT_DK_DIR}/${DK_INC}/include
    #         )
    #     elseif(TARGET_ARCHITECTURE_BITS EQUAL 64)
    #         target_include_directories(xgl PRIVATE
    #             ${GLOBAL_ROOT_DK_DIR}/${DK_INC}/include/c++/4.2.0
    #             ${GLOBAL_ROOT_DK_DIR}/${DK_INC}/sys-root/usr/include
    #         )
    #     endif()
    # endif()
elseif(WIN32)
    target_include_directories(xgl PRIVATE ${GLOBAL_ROOT_SRC_DIR}/drivers/pxproxy/inc)
endif()

# CMAKE-TODO: Figure out how to setup the install targets.
# I haven't done a top level build so don't know how these are used.
# if(TARGET_ARCHITECTURE_BITS EQUAL 64)
#     if(&(ICD_OS_TYPE) EQUAL w8)
#         set(INST_DIR ${INST_DIR_R6_W864A} ${INST_DIR_FGL_W864A})
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNow)
#         set(INST_DIR ${INST_DIR_R6_WNOW64A} ${INST_DIR_FGL_WNOW64A})
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNxt)
#         set(INST_DIR ${INST_DIR_R6_WNXT64A} ${INST_DIR_FGL_WNXT64A})
#     endif()
# else()
#     if(${ICD_OS_TYPE} EQUAL w8)
#         set(INST_DIR ${INST_DIR_R6_W8} ${INST_DIR_FGL_W8} ${INST_DIR_R6_W864A})
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNow)
#         set(INST_DIR ${INST_DIR_R6_WNOW} ${INST_DIR_FGL_WNOW} ${INST_DIR_R6_WNOW64A})
#     endif()
#     if(${ICD_OS_TYPE} EQUAL wNxt)
#         set(INST_DIR ${INST_DIR_R6_WNXT} ${INST_DIR_FGL_WNXT} ${INST_DIR_R6_WNXT64A})
#     endif()
# endif()

target_include_directories(xgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(xgl PRIVATE
    api
    api/include
    api/include/khronos
)

if(ICD_BUILD_LLPC)
    target_include_directories(xgl PRIVATE api/llpc/include)
    target_link_libraries(xgl PRIVATE llpc)
endif()

target_include_directories(xgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imported)

target_include_directories(xgl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(xgl PRIVATE
    USERNAME="$ENV{USERNAME}"
    HOST_BUILD="${ICD_HOST_NAME}"
    ICD_BINARY_NAME="${ICD_TARGET}.${CMAKE_SHARED_LIBRARY_SUFFIX}"
)

if(WIN32)
    target_sources(xgl PRIVATE res/amdicddll.rc)
endif()

### XGL Subprojects ####################################################################################################
### PAL ########################################################################
if(ICD_BUILD_SCPC)
    target_include_directories(xgl PRIVATE ${XGL_SC_PATH})
endif()
add_subdirectory(${XGL_PAL_PATH} ${PROJECT_BINARY_DIR}/pal)
target_link_libraries(xgl PRIVATE pal)

### SC  ########################################################################
if(ICD_BUILD_SCPC)
    add_subdirectory(${XGL_SC_PATH} ${PROJECT_BINARY_DIR}/sc)
    target_link_libraries(xgl PRIVATE sc)
endif()

# if(UNIX)
#     add_subdirectory(Loader)
#     target_link_libraries(xgl loader)
# endif()

### XGL Sources ########################################################################################################

### ICD api ####################################################################
# # Add UVD IP interface file.
# LCXXINCS += -I$(DEPTH)/drivers/uvdfwlib/uvdfw_inc

target_sources(xgl PRIVATE
    api/app_profile.cpp
    api/app_shader_optimizer.cpp
    api/gpu_event_mgr.cpp
    api/internal_mem_mgr.cpp
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
    api/vk_descriptor_set.cpp
    api/vk_descriptor_set_layout.cpp
    api/vk_descriptor_pool.cpp
    api/vk_device.cpp
    api/vk_dispatch.cpp
    api/vk_display.cpp
    api/vk_display_manager.cpp
    api/vk_event.cpp
    api/vk_fence.cpp
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
    api/vk_semaphore.cpp
    api/vk_shader.cpp
    api/vk_swapchain.cpp
    api/vk_surface.cpp
    api/vk_gpa_session.cpp
    api/vk_descriptor_update_template.cpp
    api/render_state_cache.cpp
    api/peer_resource.cpp
    api/renderpass/renderpass_builder.cpp
    api/renderpass/renderpass_logger.cpp
    api/utils/temp_mem_arena.cpp
    api/utils/json_reader.cpp
)

# warning C4996: 'getenv': This function or variable may be unsafe.
# Consider using _dupenv_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS.
target_compile_definitions(xgl PRIVATE _CRT_SECURE_NO_WARNINGS)

if(ICD_OPEN_SOURCE)
    target_sources(xgl PRIVATE api/open_strings/strings.cpp)
endif()

# Or for a separate lib: icdapi
# add_subdirectory(${XGL_API_PATH} ${PROJECT_BINARY_DIR}/api)
# target_link_libraries(xgl api)

### ICD api/sqtt ###############################################################
target_sources(xgl PRIVATE
    api/sqtt/sqtt_layer.cpp
    api/sqtt/sqtt_mgr.cpp
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

### XGL Auto-generated Files ###########################################################################################
# ICD_GENDIR Path to the code generation tools
set(ICD_GENDIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate)

# ICD settings code generation main script
set(ICD_GEN_SETTINGS ${ICD_GENDIR}/genSettingsCode.py)

set(ICD_GEN_SETTINGS_FILES ${ICD_GEN_SETTINGS} ${ICD_GENDIR}/settingsCodeTemplates.py)

set(ICD_SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/settings)

set(PAL_GENDIR ${XGL_PAL_PATH}/tools/generate)

add_custom_command(
    OUTPUT ${ICD_SETTINGS_DIR}/g_settings.cpp ${ICD_SETTINGS_DIR}/g_settings.h
    COMMAND ${PYTHON_CMD} ${ICD_GEN_SETTINGS} ${ICD_SETTINGS_DIR}/settings.cfg ${ICD_SETTINGS_DIR} g_settings ${PAL_GENDIR}
    DEPENDS ${ICD_GEN_SETTINGS_FILES} ${ICD_SETTINGS_DIR}/settings.cfg
    COMMENT "Generating Vulkan settings code from settings.cfg"
)

add_custom_target(
    RunVKSettingsGenerator
    DEPENDS ${ICD_GEN_SETTINGS_FILES} ${ICD_SETTINGS_DIR}/settings.cfg
    COMMENT "Checking if re-generation is required for settings"
)

add_dependencies(xgl RunVKSettingsGenerator)

target_sources(xgl PRIVATE
    settings/g_settings.cpp
    settings/settings.cpp
)

### XGL LLVM ####
if(ICD_BUILD_LLPC)
    target_link_libraries(xgl PRIVATE llpc dl stdc++)
    llvm_map_components_to_libnames(llvm_libs amdgpucodegen amdgpuinfo amdgpuasmparser amdgpudisassembler LTO ipo analysis bitreader bitwriter codegen irreader linker mc passes support target transformutils coroutines)
    target_link_libraries(xgl PRIVATE ${llvm_libs})
endif()

### Link Libraries #####################################################################################################
if (UNIX)

    target_link_libraries(xgl PRIVATE c stdc++ ${CMAKE_DL_LIBS} pthread)

    if(NOT ICD_USE_GCC)
        message(WARNING "Intel ICC untested in CMake.")
        target_link_libraries(xgl PRIVATE -fabi-version=0 -static-intel)
    endif()

    if(CMAKE_BUILD_TYPE_RELEASE)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
            if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                target_link_libraries(xgl PRIVATE -flto=4  -fuse-linker-plugin -Wno-odr)
                message(WARNING "LTO enabled for Linking")
            endif()
        endif()
    endif()

    target_link_libraries(xgl PRIVATE
        -Wl,-Bdynamic
        -Wl,-z,noexecstack
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/make/amdicd.so.${VULKAN_SDK_VERSION}.def
        -Wl,-Map=${ICD_TARGET}.map
        -Wl,-soname=${ICD_TARGET}.so.1
    )
    #set(DLL_TARGET ${ICD_TARGET})
    #set(SO_VERSION_NUMBER 1.1.0)

    # CMAKE-TODO: What is whole-archive used for?
    #target_link_libraries(xgl -Wl,--whole-archive ${ICD_LIBS} -Wl,--no-whole-archive)
    if(CMAKE_BUILD_TYPE_RELEASE)
        execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
            target_link_libraries(xgl PRIVATE -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/src/libpal.a -Wl,--no-whole-archive)
            target_link_libraries(xgl PUBLIC -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/metrohash/libmetrohash.a -Wl,--no-whole-archive)
            target_link_libraries(xgl PUBLIC -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/gpuopen/libgpuopen.a -Wl,--no-whole-archive)
            target_link_libraries(xgl PUBLIC -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/vam/libvam.a -Wl,--no-whole-archive)
            target_link_libraries(xgl PUBLIC -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/addrlib/libaddrlib.a -Wl,--no-whole-archive)
            target_link_libraries(xgl PUBLIC -Wl,--whole-archive ${PROJECT_BINARY_DIR}/pal/jemalloc/libjemalloc.a -Wl,--no-whole-archive)
        endif()
    endif()

    #${ICD_TARGET}.so${SO_VERSION_NUMBER} : ${filter-out -Wl%,$(LLLIBS})

elseif(WIN32)
    # BufferOverflowU.lib should already be added by the toolchain.
    target_link_libraries(xgl PRIVATE SetupAPI.lib AdvAPI32.lib)

    target_link_libraries(xgl PRIVATE
        -pdbstripped:${ICD_TARGET}.pdb.pub
        #-merge:_PAGE=PAGE   # CMAKE-TODO: Why is this necessary?
        #-merge:_TEXT=.text  # CMAKE-TODO: Why is this necessary?
        #-section:INIT,d     # CMAKE-TODO: Makes the INIT section Discardable. Why is this necessary?
        #-subsystem:windows
        -def:${CMAKE_CURRENT_SOURCE_DIR}/make/amdicd.dll.${VULKAN_SDK_VERSION}.def
        -map:amdvlk64.map
    )

    if(TARGET_ARCHITECTURE_BITS EQUAL 64)
        # This is required to parse callstack correctly in WinDbg. Without "pdata",
        # only 1-2 calls can be parsed under 64bit OS
        #target_link_libraries(xgl PRIVATE -debugtype:cv,fixup,pdata)
    elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
        # Turn off warnings about -incremental with -map
        # and sections with different attributes.
        target_link_libraries(xgl PRIVATE -ignore:4075,4078,4092)

        # MS recommends setting the alignment value to 4K for Longhorn
        # 32bit and 64bit builds to avoid loader problems on 64bit OSes.
        # When not overridden, the alignment defaults to 4K
        target_link_libraries(xgl PRIVATE -entry:_DllMainCRTStartup@12)
    endif()

    # Perform link time code generation &
    # Optimize away unnecessary long-branch table entries for rel build
    # Disable identical COMDAT folding for debug builds to preserve callstacks.
    if(CMAKE_BUILD_TYPE_DEBUG)
        # We want the linker to ignore libcd when it resolves external references.
        target_link_libraries(xgl PRIVATE -nodefaultlib:libcd)

        target_link_libraries(xgl PRIVATE -OPT:NOICF -OPT:NOREF)
    else()
        target_link_libraries(xgl PRIVATE -LTCG -OPT:LBR -OPT:ICF -OPT:REF)
    endif()
endif()

### Visual Studio Filters ##############################################################################################
target_vs_filters(xgl)
