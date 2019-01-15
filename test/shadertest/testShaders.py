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
import re
import platform
import time
import shutil
import argparse
from multiprocessing import Pool

RESULT = "result"
SHADER_SRC = "shaderdb"

COMPILER = ""
SPVGEN  = ""

GFX_DIRS = [".", "gfx9"]
GFXIP = 0

fail_count = 0
total_count = 0
compile_name = "amdllpc"
gfxip_str = " -gfxip="

# Compile specified shader in sub process
def compile(cmdname, gfx, f, compiler):
    start = time.time()
    result = subprocess.call(cmdname, shell = True)
    passed = False
    if (result == 0):
        if compiler == "amdllpc":
            passed = True
        else:
            if os.path.exists(RESULT + "/" + gfx + "/" + f + ".log"):
                rf = open(RESULT + "/" + gfx + "/" + f + ".log", "r")
                while True:
                    line = rf.readline()
                    if line:
                        if re.search(compiler.upper()+ " SUCCESS", line):
                            passed = True
                            break
                    else:
                        break
                rf.close()
    end = time.time()
    escape_time = end -start;
    if (passed):
        msg = "(PASS) " + f + " (" + str(escape_time) +")"
    else :
        msg = "(FAIL) " + f + " (" + str(escape_time) +")"
    return msg

def prepareTesting():
    # parser argument
    parser = argparse.ArgumentParser(description = 'Script for shader compiler testing.')
    parser.add_argument('compiler',
            help = 'The folder of standalone shader compiler.')
    parser.add_argument('spvgen',
            help = 'The folder of spvgen (compiler depends spvgen).')
    parser.add_argument('--shaderdb',
            help = 'Folder containing shader.')
    parser.add_argument('--gfxip',
            help = 'Assign gfxip to compile the shader.')

    args = parser.parse_args()
    compiler_path = args.compiler

    global SPVGEN
    SPVGEN =  args.spvgen

    if args.shaderdb:
        global SHADER_SRC
        SHADER_SRC = args.shaderdb

    if args.gfxip:
        global GFXIP
        GFXIP = args.gfxip

    # Check compiler
    global COMPILER
    if platform.system() != "Windows":
       COMPILER = compiler_path + "/" + compile_name
    else:
       COMPILER = compiler_path + "\\" + compile_name + ".exe"

    if os.path.isfile(COMPILER) == False or os.path.getsize(COMPILER) == 0:
        print("NOT FIND COMPILER")
        sys.exit(1)

    # Check spvgen
    if platform.system() != "Windows":
        if os.path.isfile(SPVGEN + "/spvgen.so") == False or os.path.getsize(SPVGEN + "/spvgen.so") == 0:
            if os.path.isfile(SPVGEN + "/spvgen.dylib") == False or os.path.getsize(SPVGEN + "/spvgen.dylib") == 0:
                print("NOT FIND SPVGEN")
                sys.exit(1)
    elif os.path.isfile(SPVGEN + "\\spvgen.dll") == False or os.path.getsize(SPVGEN + "\\spvgen.dll") == 0:
            print("NOT FIND SPVGEN")
            sys.exit(1)

    # Update path for test
    if platform.system() != "Windows":
        os.putenv("LD_LIBRARY_PATH", SPVGEN)
    else:
        os.putenv("PATH", "%PATH%;"+SPVGEN)

    # Prepare result folder
    if os.path.exists(RESULT):
       shutil.rmtree(RESULT)
    os.mkdir(RESULT)

    for gfx in GFX_DIRS:
       if not os.path.exists(RESULT + "/" + gfx):
         os.mkdir(RESULT + "/" + gfx)

    # If everything is ok, return true
    return True

# Main function
if __name__=='__main__':

    if prepareTesting():

        print("\n=================================  RUN TESTS  =================================")
        start_time = time.time()
        for gfx in GFX_DIRS:
            if not os.path.exists(SHADER_SRC + "/" + gfx):
                continue

            if gfx.startswith("gfx"):
                if  GFXIP and gfx[3] > GFXIP:
                    continue
                print(">>>  " + gfx.upper() + " TESTS")
            else:
                print(">>>  GENERIC TESTS")

            process_pool = Pool(8)
            result_msg = []
            sub_index = 0
            for f in os.listdir(SHADER_SRC + "/" + gfx):
                if f.endswith(".vert") or f.endswith(".tesc") or f.endswith(".tese") or f.endswith(".frag") or f.endswith(".geom") or f.endswith(".comp") or f.endswith(".spvas") or f.endswith(".pipe"):
                    # Build test command
                    gfxip = " "
                    if GFXIP:
                       gfxip = gfxip_str + GFXIP
                    elif gfx.startswith("gfx"):
                       gfxip = gfxip_str + gfx[3]

                    if compile_name == "amdllpc":
                        cmd = COMPILER + gfxip + " -enable-outs=0 " + SHADER_SRC + "/" + gfx + "/" + f + " 2>&1 >> " + RESULT + "/" + gfx + "/" + f + ".log"
                    if sub_index == 0 :
                        # Run test in sync-compile mode to setup context cache
                        result = compile(cmd, gfx, f, compile_name)
                        print(result)
                        if re.search("(FAIL)", result):
                            fail_count = fail_count + 1
                    else :
                        # Run test in async-compile mode
                        result_msg.append(process_pool.apply_async(compile, args=(cmd, gfx, f, compile_name)))

                    # Check result delayed with max process pool
                    if (sub_index > 8) :
                        result = result_msg[sub_index - 8].get()
                        print(result)
                        if re.search("(FAIL)", result):
                            fail_count = fail_count + 1

                    sub_index = sub_index + 1;
                    total_count = total_count + 1

            process_pool.close()
            process_pool.join()
            end_time = time.time()
            # Check remaining results
            i = sub_index - 8
            while (i >= 0) and i < len(result_msg) :
                result = result_msg[i].get()
                print(result)
                if re.search("(FAIL)", result):
                    fail_count = fail_count + 1
                i += 1
            print("")

        # Clean up
        for f in os.listdir("./"):
            if f.endswith(".spv") or f.endswith(".ll") or f.endswith(".txt") or f.endswith(".elf"):
                os.remove(f)
        print("================================  TEST SUMMARY  ===============================")
        print("Total time: " + str(end_time - start_time))
        if fail_count == 0:
            print(compile_name.upper() + " TEST PASS (TOTAL: " + str(total_count) + ")")
            sys.exit(0)
        else:
            print(compile_name.upper() + " TEST FAIL (TOTAL FAIL: " + str(fail_count) + " in " + str(total_count) + ")")
            sys.exit(1)
