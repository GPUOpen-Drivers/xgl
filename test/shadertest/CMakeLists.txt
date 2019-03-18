##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.3)

set(SHADER_TEST_VERSION 0.0.1)
project(shadertest
    VERSION
      ${SHADER_TEST_VERSION}
    LANGUAGES
      CXX
  )

if(DEFINED LLVM_DIR)
  set(BUILD_EXTERNAL NO)
else()
  set(BUILD_EXTERNAL YES)
endif()

if (BUILD_EXTERNAL)
    set(CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        ${CMAKE_CURRENT_SOURCE_DIR}/litScripts/cmake)
    set(LLVM_BUILD_MAIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/litScripts)
    set(LLVM_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/litScripts/bin)
    set(LLVM_TOOLS_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/litScripts/bin)
else()
    set(CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        ${LLVM_DIR})
    include(LLVMConfig)
endif()

include(AddLLVM)

# required by lit.site.cfg.py.in
set(AMDLLPC_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# required by configure_lit_site_cfg
set(LLVM_LIT_OUTPUT_DIR ${LLVM_TOOLS_BINARY_DIR})
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
    ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

add_lit_testsuite(check-amdllpc "Running the AMDLLPC regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  ARGS
    --verbose
  DEPENDS
    ${AMDLLPC_TEST_DEPS}
)

# to enable a custom test target on cmake below 3.11
# starting with 3.11 "test" is only reserved if ENABLE_TESTING(ON)
cmake_policy(PUSH)
if(POLICY CMP0037 AND ${CMAKE_VERSION} VERSION_LESS "3.11.0")
  cmake_policy(SET CMP0037 OLD)
endif(POLICY CMP0037 AND ${CMAKE_VERSION} VERSION_LESS "3.11.0")
add_custom_target(test
  DEPENDS
    check-amdllpc
)
cmake_policy(POP)

