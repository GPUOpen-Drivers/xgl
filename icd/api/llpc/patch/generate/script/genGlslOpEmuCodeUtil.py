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

BASIC_TYPES = ["half", "float", "double", "i1","i16", "i32", "int", "i64"]
TYPE_PREFIX_TO_BASIC = {"f":"float", "d":"double", "u":"i32", "i":"i32", "b":"i1", "f16":"half", "u16":"i16", "i16":"i16", "i64":"i64", "u64":"i64"}
TYPES_MANGLE = {"half":"Dh", "float":"f", "double":"d", "i1":"b","i16":"s", "i32":"i", "i64":"l"}
LINE_END = "\n"
LINE_TAB = "    "

class LineNo(object):
    def __init__(self):
        self.line = 1
    def getLineNoStr(self):
        strval = "%" + str(self.line)
        self.line += 1
        return strval

class Scalar(object):
    def __init__(self, compType):
        if compType == "int":
            self.compType = "i32"
        elif compType in TYPE_PREFIX_TO_BASIC:
            self.compType = TYPE_PREFIX_TO_BASIC[compType]
        else:
            self.compType = compType
    def getTypeStr(self):
        return self.compType
    def getCompTyeStr(self):
        return self.getTypeStr()
    def __eq__(self, other):
        return type(self) == type(other) and self.compType == other.compType
    def __ne__(self, other):
        return not self.__eq__(other)
    def isFloatType(self):
        return self.compType == "float" or self.compType == "double" or self.compType == "half"
    def getMangle(self):
        return TYPES_MANGLE[self.compType]

class Vector(object):
    def __init__(self, compType, compCount):
        self.compType = compType
        self.compCount = compCount
    def __eq__(self, other):
        return type(self) == type(other) and self.compType == other.compType and self.compCount == other.compCount
    def __ne__(self, other):
        return not self.__eq__(other)
    def getTypeStr(self):
        typeStr = "<" + str(self.compCount) + " x " + self.compType.getTypeStr() + ">"
        return typeStr
    def getMangle(self):
        mangle = "Dv" + str(self.compCount) + "_" + self.compType.getMangle()
        return mangle
    def getCompTyeStr(self):
        return self.compType.getTypeStr()

class Matrix(object):
    def __init__(self, colType, colCount):
        self.colType = colType
        self.compType = colType.compType
        self.colCount = colCount
    def getTypeStr(self):
        typeStr = "[" + str(self.colCount)+" x "+self.colType.getTypeStr()+"]"
        return typeStr
    def __eq__(self, other):
        return type(self) == type(other) and self.colType == other.colType and self.colCount == other.colCount
    def __ne__(self, other):
        return not self.__eq__(other)
    def getMangle(self):
        mangle = "Dv" + str(self.colCount)+ "_" + self.colType.getMangle()
        return mangle

def getMangledName(unmangledName, argTypes):
    mangledName = "@_Z" + str(len(unmangledName)) + unmangledName
    for argType in argTypes:
        mangledName += argType.getMangle()
    return mangledName

def getVariable(var):
    return "%" + var

def getTypeObject(arg):
    if arg in BASIC_TYPES:
        return Scalar(arg)
    elif "vec" in arg:
        if (arg.startswith("vec")):
            scalarObj = Scalar("f")
        else:
            scalarObj = Scalar(arg[0 : arg.find("vec")])
        return Vector(scalarObj, int(arg[len(arg) - 1]))
    elif "mat" in arg:
        if (arg.startswith("mat")):
            scalarObj = Scalar("f")
        else:
            scalarObj = Scalar(arg[0 : arg.find("mat")])
        scalarObj = Scalar(arg[0])
        vecCount = int(arg[len(arg) - 1])
        colCount = vecCount
        if "x" in arg:
            colCount = int(arg[len(arg) - 3])
        vectorObj = Vector(scalarObj, vecCount)
        return Matrix(vectorObj, colCount)

def constructFuncHeader(retVal, mangledName, args):
    funcHeader = "define spir_func " + retVal.getTypeStr() + " " + mangledName + "("
    funcHeader += LINE_END
    funcHeader += LINE_TAB
    for i, arg in enumerate(args):
        funcHeader += getTypeObject(arg[0]).getTypeStr()
        funcHeader += " "
        funcHeader += getVariable(arg[1])
        if i < len(args) - 1:
            funcHeader += ", "
    funcHeader += ") #0" + LINE_END
    return funcHeader

def extractElement(retVal, vec, vecType, elemIdx):
    elemExtract = retVal + " = extractelement " + vecType.getTypeStr() + " " + vec + ", i32 " + str(elemIdx)
    return elemExtract

def extractElementFromVector(vec, vecType):
    vecExtract = ""
    for i in range(vecType.compCount):
        vecExtract += LINE_TAB + extractElement(vec + str(i), vec, vecType, i) + LINE_END
    vecExtract += LINE_END
    return vecExtract

# Runs a C-style macro expansion in srcFile, generates destFile, takes defs as gcc style marcro definitions
#
# NOTE: Not used currently, maybe used in the future.
def expandCMacro(osType, srcFile, destFile, defs):
    dkRoot = os.environ.get("DK_ROOT")
    if osType == "win":
        gcc = os.path.join(dkRoot, "gcc/gcc-4.5.2-mingw32-amd64/bin/gcc")
    else:
        gcc = "TODO: add linux gcc path"

    cmd = "%s -E -P -x c -o %s %s %s" % (gcc, destFile, defs, srcFile)

    if osType == "win":
        subprocess.call(cmd)
    else:
        subprocess.call(cmd, shell = True)

