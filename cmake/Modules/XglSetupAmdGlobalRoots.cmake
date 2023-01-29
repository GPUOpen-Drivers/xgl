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

# find_dk_root must be available
if(NOT DEFINED GLOBAL_ROOT_DK_DIR)
    execute_process(
        COMMAND find_dk_root
        OUTPUT_VARIABLE GLOBAL_ROOT_DK_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT ("${GLOBAL_ROOT_DK_DIR}" STREQUAL ""))
        set(GLOBAL_ROOT_DK_DIR ${GLOBAL_ROOT_DK_DIR} CACHE PATH "Global root dk directory..")
    endif()
endif()

if(NOT DEFINED GLOBAL_ROOT_SRC_DIR)
    if(EXISTS ${PROJECT_SOURCE_DIR}/../../drivers)
        get_filename_component(GLOBAL_ROOT_SRC_DIR ${PROJECT_SOURCE_DIR}/../.. ABSOLUTE)
    else()
        get_filename_component(GLOBAL_ROOT_SRC_DIR ${PROJECT_SOURCE_DIR}/.. ABSOLUTE)
    endif()
    set(GLOBAL_ROOT_SRC_DIR ${GLOBAL_ROOT_SRC_DIR} CACHE STRING "Global root source directory.")
endif()
