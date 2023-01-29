##
 #######################################################################################################################
 #
 #  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Find the kernel release
execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE KERNEL_RELEASE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Find the headers
find_path(KERNEL_HEADERS_DIR
    include/linux/user.h
    PATHS /usr/src/linux-headers-${KERNEL_RELEASE}
)

message(STATUS "Kernel release: ${KERNEL_RELEASE}")
message(STATUS "Kernel headers: ${KERNEL_HEADERS_DIR}")

if (KERNEL_HEADERS_DIR)
    set(KERNEL_HEADERS_INCLUDE_DIRS
        #${KERNEL_HEADERS_DIR}/include
        ${KERNEL_HEADERS_DIR}/include/config/drm
        ${KERNEL_HEADERS_DIR}/include/uapi/drm
        #${KERNEL_HEADERS_DIR}/arch/x86/include
        ${KERNEL_HEADERS_DIR}/arch/x86/include/generated
        CACHE PATH "Kernel headers include directories."
    )
    set(KERNEL_HEADERS_FOUND 1 CACHE STRING "Set to 1 if kernel headers were found.")
else()
  set(KERNEL_HEADERS_FOUND 0 CACHE STRING "Set to 1 if kernel headers were found.")
endif()

mark_as_advanced(KERNEL_HEADERS_FOUND)
