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
# @file  genGlslMatrixOpEmu.py.py
# @brief LLPC python script file: generates LLVM-IR to emulate GLSL matrix operations.
#**********************************************************************************************************************

import os
import sys

BASICTYPES = ["float", "double", "uint", "int"]
DIGTYPE = {"v":"float", "m":"float", "u":"i32", "i":"i32", "d":"double"}
LE = "\n"
LB = "    "

class LineVar(object):
    def __init__(self):
        self.line = 1
    def getVar(self):
        strval = "%" + str(self.line)
        self.line += 1
        return strval

class Scalar(object):
    def __init__(self, compType):
        if compType == "int" or compType == "uint":
            self.compType = "i32"
        elif len(compType) == 1:
            self.compType = DIGTYPE[compType]
        else:
            self.compType = compType
    def getTypeStr(self):
        if self.floatTye():
            return "{type}"
        else:
            return self.compType

    def __eq__(self, other):
        return type(self) == type(other) and self.compType == other.compType
    def __ne__(self, other):
        return not self.__eq__(other)
    def floatTye(self):
        return self.compType == "float" or self.compType == "double"

    def getMangle(self):
        if self.floatTye():
            return "{abbr}"
        else:
            return self.compType[0]

class Vec(object):
    def __init__(self, compType, compCount):
        self.compType = compType
        self.compCount = compCount
    def __eq__(self, other):
        return type(self) == type(other) and self.compType == other.compType and self.compCount == other.compCount
    def __ne__(self, other):
        return not self.__eq__(other)
    def getTypeStr(self):
        llvmType = "<" + str(self.compCount) + " x " + self.compType.getTypeStr() + ">"
        return llvmType
    def getMangle(self):
        strv = "Dv" + str(self.compCount) + "_" + self.compType.getMangle()
        return strv

class Mat(object):
    def __init__(self, colType, colCount):
        self.colType = colType
        self.compType = colType.compType
        self.colCount = colCount

    def getTypeStr(self):
        llvmType = "[" + str(self.colCount)+" x "+self.colType.getTypeStr()+"]"
        return llvmType
    def __eq__(self, other):
        return type(self) == type(other) and self.colType == other.colType and self.colCount == other.colCount

    def __ne__(self, other):
        return not self.__eq__(other)

    def getMangle(self):
        strv = "Dv" + str(self.colCount)+ "_" + self.colType.getMangle()
        return strv

def alloc(rv, ty):
    strv = rv + " = alloca " + ty.getTypeStr()
    return strv

def getElementPtr(rv , m, ty, idx):
    strv = rv + " = getelementptr inbounds " + ty.getTypeStr() + ", " + ty.getTypeStr() + \
    "* " + m + ", i32 0, i32 " + str(idx)
    return strv

def extractElement(rv, v, ty, idx):
    strv = rv + " = extractelement " + ty.getTypeStr() + " " + v + ", i32 " + str(idx)
    return strv

def extractValue(rv, v , ty, idx):
    strv = rv + " = extractvalue " + ty.getTypeStr() + " " + v + ", "+ str(idx)
    return strv

def fadd(rv, v0, v1, ty):
    strv = rv + " = fadd "  + ty.getTypeStr() + " " + v0 + ", " + v1
    return strv

def fmul(rv, v0, v1, ty):
    strv = rv + " = fmul "  + ty.getTypeStr() + " " + v0 + ", " + v1
    return strv

def store(v0, v1, ty):
    strv = "store " + ty.getTypeStr() + " " + v0 + ", " + ty.getTypeStr() + "* " + v1
    return strv

def load(rv, v, ty):
    strv = rv + " = load " + ty.getTypeStr()+", " + ty.getTypeStr()+"* " + v
    return strv

def retn(v, ty):
    strv = "ret " + ty.getTypeStr() + " " + v
    return strv

def shufflevector(rv, v0, v0ty, v1, v1ty, idx):
    sTye = Scalar("i32")
    idxtye = Vec(sTye, len(idx))
    strv = rv + " = shufflevector " + v0ty.getTypeStr() + " " + v0 +", " + v1ty.getTypeStr() + " " + v1+ ", " + idxtye.getTypeStr() + " <"
    i = 0
    while i < len(idx) - 1:
        strv += "i32" + " " + str(idx[i])+", "
        i+= 1
    strv += "i32" + " " + str(idx[i]) +">"
    return strv

def callFunc(rv, rty, funcName, argTys, argName):
    strv = rv + " = call " + rty.getTypeStr() + " " + funcName + "("
    i = 0
    while i < len(argTys)-1:
        strv += argTys[i].getTypeStr() + " "
        strv += argName[i] + ", "
        i += 1
    strv += argTys[i].getTypeStr() + " "
    strv += argName[i] + ")"
    return strv

def extractMatrixPtr(mn, mat):
    """ extract the matrix down to the float pointer """
    strv = ""
    for i in range(mat.colCount):
        mc = mn + str(i)
        strv += LB + getElementPtr(mc, mn, mat, i) + LE
        for j in range(mat.colType.compCount):
            strv += LB + getElementPtr(mc +str(j), mc, mat.colType, j) + LE
        strv += LE
    return strv

def extractVectorPtr(vn, vecTye):
    """ extract the matrix down to the float pointer """
    strv = ""
    for i in range(vecTye.compCount):
        vp = vn + "p"+ str(i)
        strv += LB + getElementPtr(vp, vn, vecTye, i) + LE
    strv += LE
    return strv

def extractMatrix(mn, mat):
    """ extract the matrix """
    strv = ""
    for i in range(mat.colCount):
        mcv = mn + str(i) + "v"
        strv += LB + extractValue(mcv, mn, mat, i) + LE
        for j in range(mat.colType.compCount):
            mcve = mcv + str(j)
            strv += LB + extractElement(mcve, mcv, mat.colType, j) + LE
        strv += LE
    return strv

def extractMatrixVector(mn, mat):
    """ extract the matrix """
    strv = ""
    for i in range(mat.colCount):
        mcv = mn + str(i) + "v"
        strv += LB + extractValue(mcv, mn, mat, i) + LE
    strv += LE
    return strv

def extractFunctionHeader(ret, mangledName, args):
    strv = "define spir_func " + ret.getTypeStr() + " " + mangledName + "("
    strv += LE
    strv += LB
    for i, arg in enumerate(args):
        strv += getTypeObj(arg[0]).getTypeStr()
        strv += " "
        strv += getVar(arg[1])
        if i < len(args) - 1:
            strv += ", "
    strv += ") #0" + LE
    return strv

def extractFunctionDeclare(ret, mangledName, args):
    strv = "declare spir_func " + ret.getTypeStr() + " " + mangledName + "("
    for i, arg in enumerate(args):
        strv += getTypeObj(arg[0]).getTypeStr()
        strv += " "
        if i < len(args) - 1:
            strv += ", "
    strv += ") #0" + LE
    return strv

def extractVector(vn, vec):
    strv = ""
    for i in range(vec.compCount):
        strv += LB + extractElement(vn + str(i), vn, vec, i) + LE
    strv += LE
    return strv

def getTypeObj(arg):
    if arg in BASICTYPES:
        return Scalar(arg)
    elif "vec" in arg:
        sobj = Scalar(arg[0])
        return Vec(sobj, int(arg[len(arg)-1]))
    elif "mat" in arg:
        sobj = Scalar(arg[0])
        vecCount = int(arg[len(arg)-1])
        colCount = vecCount
        if "x" in arg:
            colCount = int(arg[len(arg)-3])
        vobj = Vec(sobj, vecCount)
        return Mat(vobj, colCount)

def getComment(ret, funcname, args):
    strv = "; GLSL: " + ret + " = "
    if "Times" in funcname:
        strv += args[0][0] + " * " + args[1][0]
    else:
        strv += funcname +"("
        i = 0
        while i < len(args) -1:
            strv += args[i][0]
            strv += ", "
            i+= 1
        strv += args[i][0]
        strv += ")"
    strv += LE
    return strv

def getMangleName(unmangleName, argTys):
    """ get mangeled name from unmangled name """
    mangleName = "@_Z" + str(len(unmangleName)) + unmangleName

    for ty in argTys:
        mangleName += ty.getMangle()

    return mangleName

def getVar(inv):
    return "%" + inv

def getMatEle(matName, colID, rowID):
    return matName + str(colID) + "v" + str(rowID)

def getMatPtr(matName, colID, rowID):
    return matName + str(colID) + str(rowID)

def getMatVec(matName, colID):
    return matName + str(colID) + "v"

def getVecPtr(vecName, id):
    return vecName + "p" + str(id)

def declare(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    ret = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # define function Name
    strv = extractFunctionDeclare(ret, mangledName, args) + LE
    return strv

def getArgTys(args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    return argTys

def outerProduct(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))

    ret = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # define function Name
    strv = getComment(retType, "outerProduct", args)
    strv += extractFunctionHeader(ret, mangledName, args)
    strv += "{" + LE
    mn = "%m"
    mat = Mat(argTys[0], argTys[1].compCount)
    strv += LB + alloc(mn, mat) + LE
    # extract the matrix
    strv += extractMatrixPtr(mn, mat)
    # extract the vector
    nm0 = getVar(args[0][1])
    nm1 = getVar(args[1][1])
    strv += extractVector(nm0, argTys[0])
    strv += extractVector(nm1, argTys[1])
    lv = LineVar()
    for i in range(mat.colCount):
        v1 = nm1 + str(i)
        for j in range(mat.colType.compCount):
            v0 = nm0 + str(j)
            rv = lv.getVar()
            strv += LB + fmul(rv, v0, v1, argTys[0].compType) + LE
            mptr = mn + str(i) + str(j)
            strv += LB + store(rv, mptr, argTys[0].compType) + LE
    rv = lv.getVar()
    strv += LB + load(rv, mn, mat) + LE + LE
    strv += LB + retn(rv, mat) + LE
    strv += "}" + LE + LE
    return strv

def transpose(retType, funcName, args):
    argTys = getArgTys(args)
    nMatTys = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # output comment system
    strv = getComment(retType, "transpose", args)
    # define function Name
    strv += extractFunctionHeader(nMatTys, mangledName, args)
    strv += "{" + LE
    newmn = "%nm"
    oldmn = "%" + args[0][1]
    # transpose matrix type
    strv += LB + alloc(newmn, nMatTys) + LE
    strv += extractMatrixPtr(newmn, nMatTys)
    oTys = argTys[0]
    strv += extractMatrix(oldmn, oTys)
    for i in range(nMatTys.colCount):
        for j in range(nMatTys.colType.compCount):
            strv += LB + store(getMatEle(oldmn, j, i), getMatPtr(newmn, i, j), nMatTys.colType.compType) + LE
    strv += LB + load("%nmv", newmn, nMatTys) + LE
    strv += LB + retn("%nmv", nMatTys) + LE
    strv += "}" + LE + LE
    return strv

def matrixTimesScalar(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    matTye = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # output comment system
    strv = getComment(retType, funcName, args)
    # define function Name
    strv += extractFunctionHeader(matTye, mangledName, args)
    strv += "{" + LE
    oldmn = "%" + args[0][1]
    newmn = "%nm"
    sv = "%" + args[1][1]
    strv += LB + alloc(newmn, matTye) + LE
    strv += extractMatrixPtr(newmn, matTye)
    strv += extractMatrix(oldmn, matTye)
    lv = LineVar()
    for i in range(matTye.colCount):
        for j in range(matTye.colType.compCount):
            rv = lv.getVar()
            strv += LB + fmul(rv, getMatEle(oldmn, i, j), sv, argTys[1]) + LE
            strv += LB + store(rv, getMatPtr(newmn, i, j), argTys[1]) + LE
    rv = lv.getVar()
    strv += LB + load(rv, newmn, matTye) + LE
    strv += LE
    strv += LB + retn(rv, matTye) + LE
    strv += "}" + LE + LE
    return strv

def vectorTimesMatrix(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    retTye = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    vecTye = argTys[0]
    matTye = argTys[1]
    dotTys = [vecTye, vecTye]
    dotFuncName = getMangleName("dot", dotTys)
    # output comment system
    strv = getComment(retType, funcName, args)
    # define function Name
    strv += extractFunctionHeader(retTye, mangledName, args)
    strv += "{" + LE
    oldmn = "%" + args[1][1]
    newv = "%nv"
    oldv = "%" + args[0][1]
    strv += LB + alloc(newv, retTye) + LE
    strv += extractVectorPtr(newv, retTye)
    lv = LineVar()
    for i in range(matTye.colCount):
        rv = lv.getVar()
        mcv = oldmn + str(i) + "v"
        strv += LB + extractValue(mcv, oldmn, matTye, i) + LE
        strv += LB + callFunc(rv, vecTye.compType, dotFuncName, dotTys, [mcv, oldv]) + LE
        strv += LB + store(rv, getVecPtr(newv, i), vecTye.compType) + LE
    strv += LE
    rv = lv.getVar()
    strv += LB + load(rv, newv, retTye) + LE
    strv += LE
    strv += LB + retn(rv, retTye) + LE
    strv += "}" + LE + LE
    return strv

def matrixTimesVector(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    retTye = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # define function Name
    matTye = argTys[0]
    vec0Tye = matTye.colType
    vecTye = argTys[1]
    # output comment system
    strv = getComment(retType, funcName, args)
    # output function header
    strv += extractFunctionHeader(retTye, mangledName, args)
    strv += "{" + LE
    oldmn = "%" + args[0][1]
    oldv = "%" + args[1][1]
    strv += extractMatrixVector(oldmn, matTye)
    lv = LineVar()
    oldrv = ""
    for i in range(matTye.colCount):
        vecVar = oldv + str(i)
        idx = [i]*vec0Tye.compCount
        strv += LB + shufflevector(vecVar, oldv, vecTye, oldv, vecTye, idx) + LE
        rv = lv.getVar()
        strv += LB + fmul(rv, getMatVec(oldmn, i), vecVar, vec0Tye) + LE
        if i > 0:
            addrv = lv.getVar()
            strv += LB + fadd(addrv, rv, oldrv, vec0Tye) + LE
            oldrv = addrv
        else:
            oldrv = rv
    strv += LE
    strv += LB + retn(oldrv, retTye) + LE
    strv += "}" + LE + LE
    return strv

def matrixTimesMatrix(retType, funcName, args):
    argTys = []
    for arg in args:
        argTys.append(getTypeObj(arg[0]))
    retTye = getTypeObj(retType)
    mangledName = getMangleName(funcName, argTys)
    # define function Name
    mat0Tye = argTys[0]
    mat1Tye = argTys[1]
    vec0Tye = mat0Tye.colType
    vecTye = mat1Tye.colType
    # output comment system
    strv = getComment(retType, funcName, args)
    # output function header
    strv += extractFunctionHeader(retTye, mangledName, args)
    strv += "{" + LE
    oldm0 = "%" + args[0][1]
    oldm1 = "%" + args[1][1]
    newm = "%nm"
    # alloca return matrix
    strv += LB + alloc(newm, retTye) + LE
    lv = LineVar()
    # extract left matrix column vector
    strv += extractMatrixVector(oldm0, mat0Tye)
    for k in range(mat1Tye.colCount):
        mcve = getMatVec(oldm1, k)
        # extract right matrix column vector
        strv += LB + extractValue(mcve, oldm1, mat1Tye, k) + LE
        # shuffle vector
        oldrv = ""
        for i in range(mat0Tye.colCount):
            vecVar = mcve + str(i)
            idx = [i]*vec0Tye.compCount
            strv += LB + shufflevector(vecVar, mcve, vecTye, mcve, vecTye, idx) + LE
            rv = lv.getVar()
            strv += LB + fmul(rv, getMatVec(oldm0, i), vecVar, vec0Tye) + LE
            if i > 0:
                addrv = lv.getVar()
                strv += LB + fadd(addrv, rv, oldrv, vec0Tye) + LE
                oldrv = addrv
            else:
                oldrv = rv
            strv += LE
        newVecPtr = newm + str(k)
        strv += LB + getElementPtr(newVecPtr, newm, retTye, k) + LE
        strv += LB + store(oldrv, newVecPtr, vec0Tye) + LE
        strv += LE
    rv = lv.getVar()
    strv += LB + load(rv, newm, retTye) + LE
    strv += LE
    strv += LB + retn(rv, retTye) + LE
    strv += "}" + LE + LE
    return strv

DISPATCH = {
    "OuterProduct":outerProduct,
    "Transpose":transpose,
    "MatrixTimesScalar":matrixTimesScalar,
    "VectorTimesMatrix":vectorTimesMatrix,
    "MatrixTimesVector":matrixTimesVector,
    "MatrixTimesMatrix":matrixTimesMatrix
    }

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
    instrinsic = tokens[len(tokens) - 1]

    description += ")  =>  " + funcName
    print(">>>  " + description)

    if funcName in DISPATCH:
        funcGroup = DISPATCH[funcName](retType, funcName, args)
        return (funcGroup, funcNo)
    else:
        return ("", funcNo)
    return (funcGroup, funcNo)

# Main function.
def main(inFile, outFile):
    print("*******************************************************************************")
    print("                 Generate LLVM Emulation IR (GLSL Matrix)                      ")
    print("*******************************************************************************")
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
;* @file  {filename}
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL matrix operations ({type}).
;*
;* @note  This file has been generated automatically. Do not hand-modify this file. When changes are needed, modify the
;*        generating script genGlslMatrixOpEmu.py.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-\
v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

"""
    dotdeclare = """\
declare spir_func float @_Z3dotDv2_fDv2_f(<2 x float> , <2 x float> ) #0
declare spir_func float @_Z3dotDv3_fDv3_f(<3 x float> , <3 x float> ) #0
declare spir_func float @_Z3dotDv4_fDv4_f(<4 x float> , <4 x float> ) #0
declare spir_func double @_Z3dotDv2_dDv2_d(<2 x double> , <2 x double> ) #0
declare spir_func double @_Z3dotDv3_dDv3_d(<3 x double> , <3 x double> ) #0
declare spir_func double @_Z3dotDv4_dDv4_d(<4 x double> , <4 x double> ) #0

"""
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

    # irOut.write("attributes #0 = { nounwind }\n")
    irOut.write(dotdeclare)
    fList.close()
    irOut.close()

    print("===  " + str(funcNo) + " functions processed\n")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
