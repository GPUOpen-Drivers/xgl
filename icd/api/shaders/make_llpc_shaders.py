##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#**********************************************************************************************************************
# @file  make_llpc_shaders.py
# @brief LLPC python script file: generates SPIR-V binary for internal pipelines
#**********************************************************************************************************************
"""Module to generate llpc shaders"""

from generate_shader_util import generate_spv_header_file

generate_spv_header_file("null_fragment.frag", "null_fragment", " ")
generate_spv_header_file("copy_query_pool.comp", "copy_timestamp_query_pool", " ")
generate_spv_header_file("copy_query_pool.comp", "copy_timestamp_query_pool_strided", "-DSTRIDED_COPY")
generate_spv_header_file("copy_query_pool.comp", "copy_acceleration_structure_query_pool",\
                         "-DACCELERATION_STRUCTURE_SERIALIZATION")
