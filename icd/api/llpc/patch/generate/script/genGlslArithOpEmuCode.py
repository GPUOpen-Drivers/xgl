##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

import os
import sys
from genGlslOpEmuCodeUtil import *

VEC_SWIZ = [ 'x', 'y', 'z', 'w' ]
GEN_TYPE_TO_SCALAR = \
{ \
    "genType"    : "float",     \
    "genDType"   : "double",    \
    "genIType"   : "i32",       \
    "genUType"   : "i32",       \
    "genF16Type" : "half",      \
    "genI16Type" : "i16",       \
    "genU16Type" : "i16",       \
    "genI64Type" : "i64",       \
    "genU64Type" : "i64",       \
    "genBType"   : "i1",        \
    "float"      : "float",     \
    "double"     : "double",    \
    "int"        : "i32",       \
    "uint"       : "i32",       \
    "int64_t"    : "i64",       \
    "uint64_t"   : "i64",       \
    "bool"       : "i1"         \
}

GEN_TYPE_TO_VECTOR = \
{ \
    "genType"    : "vec",       \
    "genDType"   : "dvec",      \
    "genIType"   : "ivec",      \
    "genUType"   : "uvec",      \
    "genF16Type" : "f16vec",    \
    "genI16Type" : "i16vec",    \
    "genU16Type" : "u16vec",    \
    "genI64Type" : "i64vec",    \
    "genU64Type" : "u64vec",    \
    "genBType"   : "bvec"       \
}

# Gets scalar or vector type from the specified generic type.
def getScalarOrVectorType(genType, count):
    if genType in GEN_TYPE_TO_VECTOR:
        if count > 1:
            return GEN_TYPE_TO_VECTOR[genType] + str(count)
        else:
            return GEN_TYPE_TO_SCALAR[genType]
    else:
        return genType

""" type abbreviation rules for mangled name
    "b",  // BOOL
    "h",  // UCHAR
    "c",  // CHAR
    "t",  // USHORT
    "s",  // SHORT
    "j",  // UINT
    "i",  // INT
    "m",  // ULONG
    "l",  // LONG
    "Dh", // HALF
    "f",  // FLOAT
    "d",  // DOUBLE
    "v",  // VOID
    "z",  // VarArg
"""

# Generates LLVM function (for the specified component count) to emulate a GLSL arithmetic operation.
def genLlvmArithFunc(compCount, retType, funcName, genTypedArgs, intrinsic):
    retType = getScalarOrVectorType(retType, compCount)
    argTypes = []
    args = []
    for arg in genTypedArgs:
        newArg = []
        newArgType = getScalarOrVectorType(arg[0], compCount)
        newArg.append(newArgType)
        newArg.append(arg[1])
        args.append(newArg)
        argTypes.append(getTypeObject(newArgType))

    ret = getTypeObject(retType)
    operandType = ret.getTypeStr()
    compType = ret.getCompTyeStr()
    mangledName = getMangledName(funcName, argTypes)

    func = "; " + operandType + " " + funcName + "()  =>  " + intrinsic + "\n"
    func += constructFuncHeader(ret, mangledName, args)
    func += "{" + LINE_END
    tmpVar = 1
    if compCount > 1:
        # Extract components from source vectors
        func += "    ; Extract components from source vectors\n"
        for i, arg in enumerate(zip(args, argTypes)):
            if isinstance(arg[1], Vector):
                argName = getVariable(arg[0][1])
                func += extractElementFromVector(argName, arg[1])

        # Call LLVM instrinsic, do component-wise computation
        func += "    ; Call LLVM/LLPC instrinsic, do component-wise computation\n"
        for i in range(compCount):
            func += "    %" + str(tmpVar) + " = call " + compType + " @" + intrinsic + "("
            for j, arg in enumerate(zip(args, argTypes)):
                if j > 0:
                    func += ", "
                argName = getVariable(arg[0][1])
                if isinstance(arg[1], Vector):
                    argName += str(i)
                func += arg[1].getCompTyeStr() + " " + argName
            func += ")\n"
            tmpVar += 1

        # Insert computed components into the destination vector
        func += "\n    ; Insert computed components into the destination vector\n"
        func += "    %" + str(tmpVar) + " = alloca " + operandType + "\n"
        tmpVar += 1
        func += "    %" + str(tmpVar) + " = load " + operandType + ", " + operandType + "* %" + str(tmpVar - 1) + "\n"
        for i in range(compCount):
            func += "    %" + str(tmpVar + 1) + " = insertelement " + operandType + \
                    " %" + str(tmpVar) + ", " + compType + " %" + str(i + 1) + ", i32 " + str(i) + "\n"
            tmpVar += 1
    else:
        func += "    %" + str(tmpVar) + " = call " + compType + " @" + intrinsic + "("
        for j, arg in enumerate(args):
            if j > 0:
                func += ", "
            func += getTypeObject(arg[0]).getCompTyeStr() + " %" + arg[1]
        func += ")\n"

    func += "\n    ret " +  operandType + " %" + str(tmpVar) + "\n"
    func += "}\n\n"

    return func

# Generates a group of LLVM functions (vector components: 1~4) to emulate a GLSL arithmetic operation.
def genLlvmArithFuncGroup(retType, funcName, args, intrinsic, attr):

    # Generate LLVM function group
    compType = getTypeObject(getScalarOrVectorType(retType, 1)).getCompTyeStr()
    funcGroup  = genLlvmArithFunc(1, retType, funcName, args, intrinsic)
    funcGroup += genLlvmArithFunc(2, retType, funcName, args, intrinsic)
    funcGroup += genLlvmArithFunc(3, retType, funcName, args, intrinsic)
    funcGroup += genLlvmArithFunc(4, retType, funcName, args, intrinsic)

    # Declare the called LLVM/LLPC intrinsic
    funcArgs = "("
    for j, arg in enumerate(args):
        if j > 0:
            funcArgs += ", "
        funcArgs += getTypeObject(getScalarOrVectorType(arg[0], 1)).getCompTyeStr()
    funcArgs += ")"
    funcGroup += "declare " + compType + " @" + intrinsic + funcArgs + " " + attr + "\n\n"

    return funcGroup

# Parses each line of the list file.
def parseLine(line, funcNo):
    tokens = line.split()
    if tokens == []:
        return ("", funcNo)

    funcNo += 1

    # Analyze the splitted tokens
    description = tokens[0] + " " + tokens[1] + "(" # Description for the processed line
    retType = tokens[0]
    funcName = tokens[1]

    idx = 2
    args = []
    while tokens[idx] != ':':
        if idx != 2:
            description += ", "
        description += tokens[idx + 1]

        arg = [] # [ ArgType, ArgName ]
        arg.append(tokens[idx])
        arg.append(tokens[idx + 1])
        args.append(arg)
        idx += 2
    instrinsic = tokens[idx + 1]
    attr = ""
    if idx  == len(tokens) - 2 :
        attr = "#0"
    else:
        attr = tokens[len(tokens) - 1]

    description += ")  =>  " + instrinsic
    print(">>>  " + description)

    # Generate LLVM function group
    funcGroup = genLlvmArithFuncGroup(retType, funcName, args, instrinsic, attr)
    return (funcGroup, funcNo)

# Main function.
def main(inFile, outFile, dataType):
    print("===  Process list: " + os.path.split(inFile)[1])

    fList = open(inFile, "rt")
    irOut = open(outFile, "wt")

    header ="""\
;**********************************************************************************************************************
;*
;*  Trade secret of Advanced Micro Devices, Inc.
;*  Copyright (c) 2017, Advanced Micro Devices, Inc., (unpublished)
;*
;*  All rights reserved. This notice is intended as a precaution against inadvertent publication and does not imply
;*  publication or any waiver of confidentiality. The year included in the foregoing notice is the year of creation of
;*  the work.
;*
;**********************************************************************************************************************

;**********************************************************************************************************************
;* @file  {0}
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL arithmetic operations ({1}).
;*
;* @note  This file has been generated automatically. Do not hand-modify this file. When changes are needed, modify the
;*        generating script genGlslArithOpEmuCode.py.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-\
v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

"""
    header = header.format(outFile, dataType)
    irOut.write(header)
    funcNo = 0
    for line in fList:
        if not line.startswith('#'):
            (funcGroup, funcNo) = parseLine(line, funcNo)
            irOut.write(funcGroup)
        else:
            irOut.write("; " + "=" * 117 + "\n")
            irOut.write("; >>>" + line.replace('#', ' '))
            irOut.write("; " + "=" * 117 + "\n\n")

    irOut.write("attributes #0 = { nounwind }\n")
    irOut.write("attributes #1 = { nounwind readnone }\n")

    fList.close()
    irOut.close()

    print("===  " + str(funcNo) + " functions processed\n")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
