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

# Set up the LIT testing environment. See https://llvm.org/docs/CommandGuide/lit.html.

# These constants are used to fill in lit.site.cfg.py.in and propagate the project configuration to LIT.
set(CACHE_CREATOR_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CACHE_CREATOR_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CACHE_CREATOR_AMDLLPC_DIR ${LLPC_BINARY_DIR})
set(CACHE_CREATOR_DEFAULT_GFXIP "9")

# Check if the SPVGEN binary dir is defined and not empty.
if(XGL_SPVGEN_BUILD_PATH STREQUAL "")
    message(FATAL_ERROR "SPVGEN is required for XGL Cache Creator LIT test but cannot be found.")
endif()
set(CACHE_CREATOR_SPVGEN_DIR ${XGL_SPVGEN_BUILD_PATH})

# Required by configure_lit_site_cfg; this allows it to find the llvm-lit executable.
set(LLVM_LIT_OUTPUT_DIR ${LLVM_TOOLS_BINARY_DIR})
configure_lit_site_cfg(
    ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
    MAIN_CONFIG
    ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

# You can execute test by building the `check-cache-creator` target, e.g., `ninja check-cache-creator`.
add_lit_testsuite(check-cache-creator "Running the XGL cache creator regression tests"
    ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS cache-creator cache-info amdllpc FileCheck count not split-file llvm-objdump llvm-readelf
)
