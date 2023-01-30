##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

include_guard()

# Include Frequently Used Modules ##################################################################
include(CMakeDependentOption)

# Build Type Helper ################################################################################
if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE_DEBUG $<CONFIG:Debug>)
    set(CMAKE_BUILD_TYPE_RELEASE $<CONFIG:Release>)
    set(CMAKE_BUILD_TYPE_RELWITHDEBINFO $<CONFIG:RelWithDebInfo>)
else()
    string(TOUPPER "${CMAKE_BUILD_TYPE}" capital_CMAKE_BUILD_TYPE)

    if (CMAKE_BUILD_TYPE AND
        NOT capital_CMAKE_BUILD_TYPE MATCHES "^(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
        message(FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    endif()

    if(capital_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_BUILD_TYPE_DEBUG ON)
        set(CMAKE_BUILD_TYPE_RELEASE OFF)
    else()
        set(CMAKE_BUILD_TYPE_DEBUG OFF)
        set(CMAKE_BUILD_TYPE_RELEASE ON)
    endif()
endif()

# System Architecture Helpers ######################################################################
include(TestBigEndian)

function(get_system_architecture_endianess endianess)
    test_big_endian(architectureIsBigEndian)
    if (architectureIsBigEndian)
        set(${endianess} "BIG" PARENT_SCOPE)
    else()
        set(${endianess} "LITTLE" PARENT_SCOPE)
    endif()
endfunction()

function(get_system_architecture_bits bits)
    math(EXPR ${bits} "8*${CMAKE_SIZEOF_VOID_P}")
    set(${bits} ${${bits}} PARENT_SCOPE)
endfunction()

# Architecture Endianness ##########################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_ENDIANESS)
    get_system_architecture_endianess(TARGET_ARCHITECTURE_ENDIANESS)
    # INTERNAL since this value should never be changed by the user
    set(TARGET_ARCHITECTURE_ENDIANESS ${TARGET_ARCHITECTURE_ENDIANESS} CACHE INTERNAL "")
endif()

# Architecture Bits ################################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
    get_system_architecture_bits(TARGET_ARCHITECTURE_BITS)
    # INTERNAL since this value should never be changed by the user
    set(TARGET_ARCHITECTURE_BITS ${TARGET_ARCHITECTURE_BITS} CACHE INTERNAL "")
endif()
