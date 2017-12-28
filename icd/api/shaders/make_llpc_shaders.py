##
 #######################################################################################################################
 #
 #  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

import binascii
import os
import subprocess
import sys

inFile = "copy_timestamp_query_pool"

# Generate spv file
print(">>>  (glslangValidator) " + inFile + ".comp ==> " + inFile + ".spv")
cmd = "../../../test/bil/install/win64/glslangValidator.exe -V " + inFile + ".comp -o " + inFile + ".spv"
subprocess.call(cmd)

# Convert .spv file to a hex file
spvFile = inFile + ".spv"
hFile = inFile +"_spv.h"
print(">>>  (bin2hex) " + spvFile + "  ==>  " + hFile)
fBin = open(spvFile, "rb")
binData = fBin.read()
fBin.close()

hexData = binascii.hexlify(binData).decode()
fHex = open(hFile, "w")
hexText = "// do not edit by hand; created from source file \"copy_timestamp_query_pool.comp\" by executing script make_llpc_shaders.py\n"
i = 0
while i < len(hexData):
    hexText += "0x"
    hexText += hexData[i]
    hexText += hexData[i + 1]
    i += 2
    if (i != len(hexData)):
        hexText += ", "
    if (i % 32 == 0):
        hexText += "\n"
fHex.write(hexText)
fHex.close()
