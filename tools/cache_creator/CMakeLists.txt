##
 #######################################################################################################################
 #
 #  Copyright (c) 2021 Google LLC. All Rights Reserved.
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

# Cache creator is a tool for building pipeline binary cache files out of shader/pipeline elf files.
# These input elf files can be compiled offline and packaged as cache file on a host machine without a GPU.
# The "XGL_BUILD_CACHE_CREATOR" CMake option enables the cache creator targets.

# This interface library is intended be used by cache-creator, cache-info, and by the related unit tests.
add_library(cache_creator_lib INTERFACE)

list(APPEND CMAKE_MODULE_PATH "${XGL_LLVM_BUILD_PATH}/${CMAKE_CFG_INTDIR}/lib/cmake/llvm")
include(LLVMConfig)

# Find all the LLVM libraries necessary to use the LLVM components that we use.
# See https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project.
llvm_map_components_to_libnames(llvm_libs Object Support)
target_link_libraries(cache_creator_lib INTERFACE pal xgl_cache_support ${llvm_libs})

target_include_directories(cache_creator_lib INTERFACE
    ${XGL_LLVM_SRC_PATH}/include
    ${LLVM_INCLUDE_DIRS}  # This is necessary to discover the auto-generated llvm-config.h header.
    ${XGL_VKGC_PATH}/include
)

get_target_property(XGL_COMPILE_OPTIONS xgl COMPILE_OPTIONS)
get_target_property(XGL_COMPILE_DEFINITIONS xgl COMPILE_DEFINITIONS)

# Compile with the same flags and definitions as the ICD and LLVM.
target_compile_definitions(cache_creator_lib INTERFACE
    ${LLVM_DEFINITIONS}
    ${XGL_COMPILE_DEFINITIONS}
)
target_compile_options(cache_creator_lib INTERFACE ${XGL_COMPILE_OPTIONS})
if(NOT LLVM_ENABLE_EH)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cache_creator_lib INTERFACE  $<$<COMPILE_LANGUAGE:CXX>: -fno-exceptions>)
    endif()
endif()
if(NOT LLVM_ENABLE_RTTI)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cache_creator_lib INTERFACE  $<$<COMPILE_LANGUAGE:CXX>: -fno-rtti>)
    endif()
endif()

target_sources(cache_creator_lib INTERFACE cache_creator.cpp cache_info.cpp)

# This executable takes AMDGPU elf files as input and outputs a Vulkan cache file blob
# that can be later loaded by the ICD.
# The `cache-creator` binary follows the standard llvm-based tool naming convention.
# Note that running the cache-creator tool doesn't require a Vulkan device to be installed.
add_executable(cache-creator)
target_sources(cache-creator PRIVATE cache_creator_main.cpp)
llvm_map_components_to_libnames(llvm_libs
    BinaryFormat
)
target_link_libraries(cache-creator PRIVATE cache_creator_lib ${llvm_libs})

# This executable analyzes pipeline binary cache files produced either by the ICD or by the cache-creator tool.
# We use the output in automated regression tests but it may also be useful in manual analysis/debugging.
# The `cache-info` binary follows the standard llvm-based tool naming convention.
# Note that running the cache-info tool doesn't require a Vulkan device to be installed.
add_executable(cache-info)
target_sources(cache-info PRIVATE cache_info_main.cpp)
target_link_libraries(cache-info PRIVATE cache_creator_lib ${llvm_libs})

# Build cache creator tools whenever we build XGL.
add_dependencies(xgl cache-creator cache-info)

if(XGL_BUILD_TESTS)
    message(STATUS "Building cache creator tests")
    set(CACHE_CREATOR_TOOLS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    add_subdirectory(test "${CMAKE_CURRENT_BINARY_DIR}/test/cache-creator/lit")
    add_subdirectory(unittests "${CMAKE_CURRENT_BINARY_DIR}/test/cache-creator/unittests")
endif()
