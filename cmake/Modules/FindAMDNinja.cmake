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

# Output
# Ninja_FOUND
# Ninja_DIR
# Ninja_EXECUTABLE

if(NOT DEFINED Ninja_FOUND)
    if(NOT DEFINED AMDNinja_FIND_VERSION)
        message(FATAL_ERROR "A version to search for must be specified.")
    endif()
    if(NOT DEFINED GLOBAL_ROOT_DK_DIR)
        message(FATAL_ERROR "GLOBAL_ROOT_DK_DIR must be specified.")
    endif()

    set(Ninja_DIR ${GLOBAL_ROOT_DK_DIR}/ninja/${AMDNinja_FIND_VERSION} CACHE FILEPATH "Ninja Direction")
    mark_as_advanced(Ninja_DIR)
    set(Ninja_EXECUTABLE ${Ninja_DIR}/ninja.exe CACHE FILEPATH "Ninja Executable")
    mark_as_advanced(Ninja_EXECUTABLE)

    message(STATUS "Ninja: ${Ninja_EXECUTABLE}")

    set(Ninja_FOUND ${Ninja_FOUND} CACHE STRING "Was Ninja found?")
    mark_as_advanced(Ninja_FOUND)
endif()
