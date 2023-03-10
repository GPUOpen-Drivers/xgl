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

# XGL Cache Creator Tools unit tests.
# To execute all cache-creator tools unit tests, run:
#   cmake --build . --target check-cache-creator-units

# Required to use LIT on Windows.
find_package(Python3 ${LLVM_MINIMUM_PYTHON_VERSION} REQUIRED
  COMPONENTS Interpreter)

# Find the TestingSupport library name that we will link with.
llvm_map_components_to_libnames(llvm_testing_support_lib TestingSupport)

# Use the gtest support provided by llvm
set(LLVM_GTEST_LIBS llvm_gtest llvm_gtest_main ${llvm_testing_support_lib})
if(LLVM_PTHREAD_LIB)
  list(APPEND LLVM_GTEST_LIBS pthread)
endif()

add_executable(CacheCreatorUnitTests)
target_sources(CacheCreatorUnitTests PRIVATE
  cache_creator_tests.cpp
  cache_info_tests.cpp
)

llvm_map_components_to_libnames(llvm_libs
    BinaryFormat
)
target_link_libraries(CacheCreatorUnitTests PRIVATE
  ${LLVM_GTEST_LIBS}
  ${llvm_libs}
  cache_creator_lib
)
target_include_directories(CacheCreatorUnitTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add a LIT target to execute all unit tests.
# Required by lit.site.cfg.py.in.
set(XGL_CACHE_CREATOR_UNIT_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(XGL_CACHE_CREATOR_UNIT_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
# Required by configure_lit_site_cfg.
set(LLVM_LIT_OUTPUT_DIR ${LLVM_TOOLS_BINARY_DIR})

# Main config for unit tests.
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG
    ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

add_lit_testsuite(check-cache-creator-units "Running the XGL cache-creator tools unit tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS
    CacheCreatorUnitTests
)
