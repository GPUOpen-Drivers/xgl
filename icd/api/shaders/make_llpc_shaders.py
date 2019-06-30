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

import binascii
import os
import subprocess
import sys

def Is64Bit():
    # https://docs.python.org/2/library/sys.html#sys.maxsize
    # https://docs.python.org/2/library/platform.html#cross-platform
    return sys.maxsize > 2**32

vulkanSDK = os.getenv("VULKAN_SDK")

if vulkanSDK is None:
    sys.exit("Specify path to Vulkan SDK by setting VULKAN_SDK enviroment variable.")

glslangValidator = os.path.join(vulkanSDK, "bin" if Is64Bit() else "Bin32", "glslangValidator")

def GenerateSpvHeaderFile(inFile, outFile, marcoDef):
    # Generate spv file
    print(">>>  (glslangValidator) " + inFile + " ==> " + outFile + ".spv")
    cmdLine = "glslangValidator -V " + inFile + " "+ marcoDef + " -o " + outFile + ".spv";
    subprocess.call(cmdLine, shell = True)

    # Convert .spv file to a hex file
    spvFile = outFile + ".spv"
    hFile = outFile +"_spv.h"
    print(">>>  (bin2hex) " + spvFile + "  ==>  " + hFile)
    fBin = open(spvFile, "rb")
    binData = fBin.read()
    fBin.close()

    hexData = binascii.hexlify(binData).decode()
    fHex = open(hFile, "w")
    hexText = "// do not edit by hand; created from source file " + inFile + " by executing script make_llpc_shaders.py\n"
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

    return #GenerateSpvHeaderFile

GenerateSpvHeaderFile("copy_timestamp_query_pool.comp", "copy_timestamp_query_pool", " ")
GenerateSpvHeaderFile("copy_timestamp_query_pool.comp", "copy_timestamp_query_pool_strided", "-DSTRIDED_COPY")
