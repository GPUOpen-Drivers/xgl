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
import string

### Global definitions
# Contains LLVM function attributes
LLVM_ATTRIBUTES = \
[\
    "attributes #0 = { nounwind readonly }", \
    "attributes #1 = { nounwind writeonly}", \
    "attributes #2 = { nounwind }" \
]

# Contains LLVM declarations
LLVM_DECLS = {}

# Image opcode traits are encoded in function name using these tokens
SPIRV_IMAGE_PREFIX                          = "llpc"
SPIRV_IMAGE_MODIFIER                        = "image"
SPIRV_IMAGE_SPARSE_MODIFIER                 = "sparse"
SPIRV_IMAGE_OPERAND_DREF_MODIFIER           = "dref"
SPIRV_IMAGE_OPERAND_PROJ_MODIFIER           = "proj"
SPIRV_IMAGE_OPERAND_BIAS_MODIFIER           = "bias"
SPIRV_IMAGE_OPERAND_LOD_MODIFIER            = "lod"
SPIRV_IMAGE_OPERAND_LODLODZ_MODIFIER        = "lod|lodz"
SPIRV_IMAGE_OPERAND_LODZ_MODIFIER           = "lodz"
SPIRV_IMAGE_OPERAND_GRAD_MODIFIER           = "grad"
SPIRV_IMAGE_OPERAND_CONSTOFFSET_MODIFIER    = "constoffset"
SPIRV_IMAGE_OPERAND_OFFSET_MODIFIER         = "offset"
SPIRV_IMAGE_OPERAND_CONSTOFFSETS_MODIFIER   = "constoffsets"
SPIRV_IMAGE_OPERAND_SAMPLE_MODIFIER         = "sample"
SPIRV_IMAGE_OPERAND_MINLOD_MODIFIER         = "minlod"
SPIRV_IMAGE_OPERAND_FMASKBASED_MODIFIER     = "fmaskbased"
SPIRV_IMAGE_OPERAND_FMASKID_MODIFIER        = "fmaskid"
SPIRV_IMAGE_DIM_PREFIX                      = "Dim"
SPIRV_IMAGE_ARRAY_MODIFIER                  = "Array"

LLPC_DESCRIPTOR_LOAD_RESOURCE               = "llpc.descriptor.load.resource"
LLPC_DESCRIPTOR_LOAD_TEXELBUFFER            = "llpc.descriptor.load.texelbuffer"
LLPC_DESCRIPTOR_LOAD_SAMPLER                = "llpc.descriptor.load.sampler"
LLPC_DESCRIPTOR_LOAD_FMASK                  = "llpc.descriptor.load.fmask"

LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE = "llpc.patch.image.readwriteatomic.descriptor.cube"

# LLVM image load/store intrinsic flags, meaning of these flags are:
# glc, slc, lwe
LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS         = "i1 0, i1 0, i1 0"
LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC = "i1 %glc, i1 %slc, i1 0"
# LLVM buffer load/store intrinsic flags, meaning of these flags are:
# glc, slc
LLVM_BUFFER_INTRINSIC_LOADSTORE_FLAGS         = "i1 0, i1 0"
LLVM_BUFFER_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC = "i1 %glc, i1 %slc"

# GFX level
GFX6 = 6.0
GFX9 = 9.0

LITERAL_INT_ZERO                = "0"
LITERAL_FLOAT_ZERO              = "0.0"
LITERAL_FLOAT_ZERO_POINT_FIVE   = "0.5"

# Utilities
def shouldNeverCall(msg):
    assert False, "Should never reach here " + msg

def getClassMemberName(cls, m):
    for i in dir(cls):
        if getattr(cls, i) is m:
            return i
    return "getClassMemberName() find nothing"

def setupClassNameToAttrMap(cls):
    ret = {}
    for i in dir(cls):
        if not i.startswith("__"):
            ret[i] = getattr(cls, i)
    return ret

def rFind(dict, v):
    for i in dict.items():
        if v == i[1]:
            return i[0]
    return "rFind() find nothing"

# Enums and corresponding name map
class SpirvDim:
    Dim1D          = 0
    Dim2D          = 1
    Dim3D          = 2
    DimCube        = 3
    DimRect        = 4
    DimBuffer      = 5
    DimSubpassData = 6
    Undef          = 7
SPIRV_DIM_DICT = setupClassNameToAttrMap(SpirvDim)

class SpirvDimToCoordNum:
    Dim1D          = 1
    Dim2D          = 2
    Dim3D          = 3
    DimCube        = 3
    DimRect        = 2
    DimBuffer      = 1
    DimSubpassData = 2
SPIRV_DIM_TO_COORD_NUM_DICT = setupClassNameToAttrMap(SpirvDimToCoordNum)

class SpirvImageOpKind:
    sample              = 0
    fetch               = 1
    gather              = 2
    querynonlod         = 3
    querylod            = 4
    read                = 5
    write               = 6
    atomicexchange      = 7
    atomiccompexchange  = 8
    atomiciincrement    = 9
    atomicidecrement    = 10
    atomiciadd          = 11
    atomicisub          = 12
    atomicsmin          = 13
    atomicumin          = 14
    atomicsmax          = 15
    atomicumax          = 16
    atomicand           = 17
    atomicor            = 18
    atomicxor           = 19
    undef               = 20
SPIRV_IMAGE_INST_KIND_DICT = setupClassNameToAttrMap(SpirvImageOpKind)

# Function attributes based on image operation kind
#    #0 : nounwind readonly
#    #1 : nounwind writeonly
#    #2 : nounwind
SPIRV_IMAGE_INST_KIND_ATTR_DICT = { \
    SpirvImageOpKind.sample              : "#0", \
    SpirvImageOpKind.fetch               : "#0", \
    SpirvImageOpKind.gather              : "#0", \
    SpirvImageOpKind.querynonlod         : "#0", \
    SpirvImageOpKind.querylod            : "#0", \
    SpirvImageOpKind.read                : "#0", \
    SpirvImageOpKind.write               : "#1", \
    SpirvImageOpKind.atomicexchange      : "#2", \
    SpirvImageOpKind.atomiccompexchange  : "#2", \
    SpirvImageOpKind.atomiciincrement    : "#2", \
    SpirvImageOpKind.atomicidecrement    : "#2", \
    SpirvImageOpKind.atomiciadd          : "#2", \
    SpirvImageOpKind.atomicisub          : "#2", \
    SpirvImageOpKind.atomicsmin          : "#2", \
    SpirvImageOpKind.atomicumin          : "#2", \
    SpirvImageOpKind.atomicsmax          : "#2", \
    SpirvImageOpKind.atomicumax          : "#2", \
    SpirvImageOpKind.atomicand           : "#2", \
    SpirvImageOpKind.atomicor            : "#2", \
    SpirvImageOpKind.atomicxor           : "#2"}

class SpirvSampledType:
    f32         = 0
    i32         = 1
    u32         = 2
    f16         = 3
SPIRV_SAMPLED_TYPE_DICT = setupClassNameToAttrMap(SpirvSampledType)

class VarNames:
    sampler          = "%sampler"
    resource         = "%resource"
    fmask            = "%fmask"
    samplerSet       = "%samplerDescSet"
    samplerBinding   = "%samplerBinding"
    samplerIndex     = "%samplerIdx"
    resourceSet      = "%resourceDescSet"
    resourceBinding  = "%resourceBinding"
    resourceIndex    = "%resourceIdx"
    coord            = "%coord"
    texel            = "%texel"
    comp             = "%comp"
    dRef             = "%dRef"
    proj             = "%proj"
    bias             = "%bias"
    lod              = "%lod"
    gradX            = "%gradX"
    gradY            = "%gradY"
    offset           = "%offset"
    constOffsets     = "%constOffsets"
    sample           = "%sample"
    minlod           = "%minlod"
    atomicData       = "%atomicData"
    atomicComparator = "%atomicComparator"

# Helper functions (internally used)
LLPC_TRANSFORM_CUBE_GRAD = "llpc.image.transformCubeGrad"

### End Global definitions

# Represents an image function definition base.
class FuncDef(object):
    INVALID_VAR = ""

    def copyFrom(self, other):
        self._opKind            = other._opKind
        self._sampledType       = other._sampledType
        self._dim               = other._dim
        self._arrayed           = other._arrayed
        self._hasDref           = other._hasDref
        self._hasProj           = other._hasProj
        self._hasBias           = other._hasBias
        self._hasLod            = other._hasLod
        self._hasGrad           = other._hasGrad
        self._hasConstOffset    = other._hasConstOffset
        self._hasOffset         = other._hasOffset
        self._hasConstOffsets   = other._hasConstOffsets
        self._hasSample         = other._hasSample
        self._hasMinLod         = other._hasMinLod
        self._isFmaskBased      = other._isFmaskBased
        self._returnFmaskId     = other._returnFmaskId
        self._attr              = other._attr
        self._localVarCounter   = other._localVarCounter
        self._coordXYZW[0]      = other._coordXYZW[0]
        self._coordXYZW[1]      = other._coordXYZW[1]
        self._coordXYZW[2]      = other._coordXYZW[2]
        self._coordXYZW[3]      = other._coordXYZW[3]

        self._dRef              = other._dRef
        self._invProjDivisor    = other._invProjDivisor
        self._bias              = other._bias
        self._lod               = other._lod
        self._gradX             = other._gradX

        self._gradY             = other._gradY

        self._packedOffset      = other._packedOffset
        self._packedOffsets     = other._packedOffsets

        self._sample            = other._sample
        self._minlod            = other._minlod
        self._cubeMa            = other._cubeMa
        self._cubeId            = other._cubeId
        self._atomicData        = other._atomicData

        self._supportLzOptimization    = other._supportLzOptimization
        self._supportSparse     = other._supportSparse
        self._mangledName       = other._mangledName
        pass

    def __init__(self, mangledName, sampledType):
        self._opKind            = SpirvImageOpKind.undef
        self._sampledType       = sampledType
        self._dim               = SpirvDim.Undef
        self._arrayed           = False
        self._hasDref           = False
        self._hasProj           = False
        self._hasBias           = False
        self._hasLod            = False
        self._hasGrad           = False
        self._hasConstOffset    = False
        self._hasOffset         = False
        self._hasConstOffsets   = False
        self._hasSample         = False
        self._hasMinLod         = False
        self._isFmaskBased      = False
        self._returnFmaskId     = False
        self._attr              = ""
        self._localVarCounter   = 1
        self._coordXYZW         = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                                   FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]
        self._dRef              = FuncDef.INVALID_VAR
        self._invProjDivisor    = FuncDef.INVALID_VAR
        self._bias              = FuncDef.INVALID_VAR
        self._lod               = FuncDef.INVALID_VAR
        self._gradX             = [FuncDef.INVALID_VAR,FuncDef.INVALID_VAR, \
                                   FuncDef.INVALID_VAR]
        self._gradY             = [FuncDef.INVALID_VAR,FuncDef.INVALID_VAR, \
                                   FuncDef.INVALID_VAR]
        self._packedOffset      = FuncDef.INVALID_VAR
        self._packedOffsets     = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                                   FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]
        self._sample            = VarNames.sample
        self._minlod            = FuncDef.INVALID_VAR
        self._cubeMa            = FuncDef.INVALID_VAR
        self._cubeId            = FuncDef.INVALID_VAR
        self._atomicData        = FuncDef.INVALID_VAR

        # For zero-LOD optimization, will generate 2 version of function, a lz optimized version which uses
        # zero-LOD instruction, and a normal version uses lod instruction.
        self._supportLzOptimization    = (mangledName.find(SPIRV_IMAGE_OPERAND_LODLODZ_MODIFIER) != -1)
        self._supportSparse     = (mangledName.find(SPIRV_IMAGE_SPARSE_MODIFIER) != -1)
        self._mangledName       = mangledName
        pass

    # Parses and store all traits of an image function.
    def parse(self):
        # Gets each image opcode trait token from function's mangled name
        tokens         = self._mangledName.split('.')
        # Parses SpirvImageOpKind
        opKind         = tokens[2]
        assert opKind in SPIRV_IMAGE_INST_KIND_DICT, "Error: " + self._mangledName
        self._opKind = SPIRV_IMAGE_INST_KIND_DICT[opKind]
        self._attr = SPIRV_IMAGE_INST_KIND_ATTR_DICT[self._opKind]

        # Parses dimension
        dimName        = tokens[3]
        arrayed        = False
        if dimName.find(SPIRV_IMAGE_ARRAY_MODIFIER) != -1:
            arrayed    = True
            dimName    = dimName.replace(SPIRV_IMAGE_ARRAY_MODIFIER, "")
        assert dimName in SPIRV_DIM_DICT, "Error: " + self._mangledName
        self._dim           = SPIRV_DIM_DICT[dimName]
        self._arrayed       = arrayed

        # Parses other traits
        for t in tokens[4:]:
            if t == SPIRV_IMAGE_OPERAND_DREF_MODIFIER:
                self._hasDref           = True
            elif t == SPIRV_IMAGE_OPERAND_PROJ_MODIFIER:
                self._hasProj           = True
            elif t == SPIRV_IMAGE_OPERAND_BIAS_MODIFIER:
                self._hasBias           = True
            elif t == SPIRV_IMAGE_OPERAND_LOD_MODIFIER:
                self._hasLod            = True
            elif t == SPIRV_IMAGE_OPERAND_LODLODZ_MODIFIER:
                self._hasLod            = True
            elif t == SPIRV_IMAGE_OPERAND_LODZ_MODIFIER:
                self._hasLod            = True
            elif t == SPIRV_IMAGE_OPERAND_GRAD_MODIFIER:
                self._hasGrad           = True
            elif t == SPIRV_IMAGE_OPERAND_CONSTOFFSET_MODIFIER:
                self._hasConstOffset    = True
            elif t == SPIRV_IMAGE_OPERAND_OFFSET_MODIFIER:
                self._hasOffset         = True
            elif t == SPIRV_IMAGE_OPERAND_CONSTOFFSETS_MODIFIER:
                self._hasConstOffsets   = True
            elif t == SPIRV_IMAGE_OPERAND_SAMPLE_MODIFIER:
                self._hasSample         = True
            elif t == SPIRV_IMAGE_OPERAND_MINLOD_MODIFIER:
                self._hasMinLod         = True
            elif t == SPIRV_IMAGE_OPERAND_FMASKBASED_MODIFIER:
                self._isFmaskBased      = True
            elif t == SPIRV_IMAGE_OPERAND_FMASKID_MODIFIER:
                self._returnFmaskId     = True
            else:
                shouldNeverCall(self._mangledName)
        pass

    def isAtomicOp(self):
        return self._opKind >= SpirvImageOpKind.atomicexchange and \
            self._opKind <= SpirvImageOpKind.atomicxor

    # Returns string representation of this object, used for debug purpose.
    def __repr__(self):
        traits = ["FuncDef:", rFind(SPIRV_IMAGE_INST_KIND_DICT, self._opKind), rFind(SPIRV_DIM_DICT, self._dim),\
                  "arrayed: %d" % (self._arrayed)]
        if self._hasDref == True:
            traits.append(SPIRV_IMAGE_OPERAND_DREF_MODIFIER)
        elif self._hasProj == True:
            traits.append(SPIRV_IMAGE_OPERAND_PROJ_MODIFIER)
        elif self._hasBias == True:
            traits.append(SPIRV_IMAGE_OPERAND_BIAS_MODIFIER)
        elif self._hasLod == True:
            traits.append(SPIRV_IMAGE_OPERAND_LOD_MODIFIER)
        elif self._hasGrad == True:
            traits.append(SPIRV_IMAGE_OPERAND_GRAD_MODIFIER)
        elif self._hasConstOffset == True:
            traits.append(SPIRV_IMAGE_OPERAND_CONSTOFFSET_MODIFIER)
        elif self._hasOffset == True:
            traits.append(SPIRV_IMAGE_OPERAND_OFFSET_MODIFIER)
        elif self._hasConstOffsets == True:
            traits.append(SPIRV_IMAGE_OPERAND_CONSTOFFSETS_MODIFIER)
        elif self._hasSample == True:
            traits.append(SPIRV_IMAGE_OPERAND_SAMPLE_MODIFIER)
        elif self._hasMinLod == True:
            traits.append(SPIRV_IMAGE_OPERAND_MINLOD_MODIFIER)

        return " ,".join(traits)

# Represents code generation functionality
class CodeGen(FuncDef):
    def __init__(self, funcDefBase, gfxLevel):
        super(CodeGen, self).__init__(funcDefBase._mangledName, funcDefBase._sampledType)
        super(CodeGen, self).copyFrom(funcDefBase)
        self._gfxLevel = gfxLevel
        pass

    # Start code generation
    def gen(self, irOut):
        self._genWithSparse(irOut)
        pass

    # Generate both normal and sparse version
    def _genWithSparse(self, irOut):
        if self._supportSparse:
            # Generate sparse version
            codeGen = CodeGen(self, self._gfxLevel)
            codeGen._genWithLzOptimization(irOut)

            # Turn off sparse support
            self._supportSparse = False

        # Generate normal version
        codeGen = CodeGen(self, self._gfxLevel)
        codeGen._genWithLzOptimization(irOut)
        pass

    # Generate both normal and zero-LOD optimized version
    def _genWithLzOptimization(self, irOut):
        if self._supportLzOptimization:
            # Generate zero-LOD optimized version
            self._mangledName = self._mangledName.replace(SPIRV_IMAGE_OPERAND_LODLODZ_MODIFIER,
                                                          SPIRV_IMAGE_OPERAND_LODZ_MODIFIER)
            codeGen = CodeGen(self, self._gfxLevel)
            codeGen._genInternal(irOut)

            # Turn off zero-LOD optimization
            self._supportLzOptimization = False
            self._mangledName = self._mangledName.replace(SPIRV_IMAGE_OPERAND_LODZ_MODIFIER,
                                                          SPIRV_IMAGE_OPERAND_LOD_MODIFIER)
        # Generate normal version
        codeGen = CodeGen(self, self._gfxLevel)
        codeGen._genInternal(irOut)
        pass

    # Generates image function implementation.
    def _genInternal(self, irOut):
        retType = self._supportSparse and self.getSparseReturnType(self.getReturnType()) or self.getReturnType()
        irFuncDef = "define %s @%s(%s) %s\n" % (retType,
                                                self.getFunctionName(),
                                                self.getParamList(),
                                                self._attr)
        irOut.write(irFuncDef)
        irOut.write('{\n')
        self.genLoadSamplerAndResource(irOut)
        self.genCoord(irOut)
        if self._hasDref:
            self.genDref(irOut)
        self.genImageOperands(irOut)

        if (self._gfxLevel == GFX9):
            self.patchGfx91Dand1DArray()

        # Create AMDGCN intrinsics based code generator
        if self._dim == SpirvDim.DimBuffer:
            if self.isAtomicOp():
                intrinGen = BufferAtomicGen(self)
            elif self._opKind == SpirvImageOpKind.read or self._opKind == SpirvImageOpKind.fetch:
                intrinGen = BufferLoadGen(self)
            elif self._opKind == SpirvImageOpKind.write:
                intrinGen = BufferStoreGen(self)
            else:
                shouldNeverCall()
        else:
            if self.isAtomicOp():
                intrinGen = ImageAtomicGen(self)
            elif self._opKind == SpirvImageOpKind.read or self._opKind == SpirvImageOpKind.fetch:
                intrinGen = ImageLoadGen(self)
            elif self._opKind == SpirvImageOpKind.write:
                intrinGen = ImageStoreGen(self)
            elif self._opKind == SpirvImageOpKind.sample or self._opKind == SpirvImageOpKind.gather or \
                    self._opKind == SpirvImageOpKind.querylod:
                intrinGen = ImageSampleGen(self)
            else:
                shouldNeverCall()

        if intrinGen._returnFmaskId:
            # Fetch fmask only
            fmaskVal    = intrinGen.genLoadFmask(irOut)
            retVal      = intrinGen.genReturnFmaskValue(fmaskVal, irOut)
            irOut.write("    ret %s %s\n" % (intrinGen.getReturnType(), retVal))
        else:
            if intrinGen._isFmaskBased:
                # Get sampleId from fmask value
                fmaskVal            = intrinGen.genLoadFmask(irOut)
                intrinGen._sample   = intrinGen.genGetSampleIdFromFmaskValue(fmaskVal, irOut)

            retVal    = intrinGen.genIntrinsicCall(irOut)
            intrinGen.processReturn(retVal, intrinGen, irOut)

        irOut.write('}\n\n')

    # Gets return type of image operation, which is type of texel.
    def getReturnType(self):
        ret = "void"
        if self._opKind == SpirvImageOpKind.write:
            pass
        elif self.isAtomicOp():
            ret = self._sampledType == SpirvSampledType.f32 and "float" or "i32"
        elif self._opKind == SpirvImageOpKind.querylod:
            ret = "<2 x float>"
        elif self._hasDref and self._opKind != SpirvImageOpKind.gather:
            if self._sampledType == SpirvSampledType.f32:
                ret = "float"
            elif self._sampledType == SpirvSampledType.f16:
                ret = "half"
            else:
                assert "Unsupported return type"
        else:
            if self._sampledType == SpirvSampledType.f32:
                ret = "<4 x float>"
            elif self._sampledType == SpirvSampledType.f16:
                ret = "<4 x half>"
            elif self._sampledType == SpirvSampledType.i32:
                ret = "<4 x i32>"
            elif self._sampledType == SpirvSampledType.u32:
                ret = "<4 x i32>"
            else:
                shouldNeverCall()

        return ret

    def getSparseReturnType(self, dataReturnType):
        assert self._supportSparse
        return "{ i32, %s }" % (dataReturnType)

    # Gets image function name.
    def getFunctionName(self):
        tokens = self._mangledName.split('.')
        assert tokens[0] == SPIRV_IMAGE_PREFIX
        assert tokens[3].startswith(SPIRV_IMAGE_DIM_PREFIX)

        # Setup image sparse modifier in function name
        tokens[1] = self._supportSparse and SPIRV_IMAGE_MODIFIER + SPIRV_IMAGE_SPARSE_MODIFIER \
                                        or  SPIRV_IMAGE_MODIFIER

        # Remove dim prefix in function name
        tokens[3] = tokens[3][len(SPIRV_IMAGE_DIM_PREFIX):]
        sampledTypeName = rFind(SPIRV_SAMPLED_TYPE_DICT, self._sampledType)
        tokens.insert(3, sampledTypeName)
        funcName = '.'.join(tokens)

        return funcName

    # Gets image function parameter list.
    def getParamList(self):
        params = []

        # Sampler and resource binding parameters
        samplerBindings = ""
        if self._opKind == SpirvImageOpKind.sample or \
           self._opKind == SpirvImageOpKind.gather or \
           self._opKind == SpirvImageOpKind.querylod:
            samplerBindings = "i32 %s, i32 %s, i32 %s, " % (VarNames.samplerSet, \
                    VarNames.samplerBinding, VarNames.samplerIndex, )

        resourceBindings = "i32 %s, i32 %s, i32 %s" % (VarNames.resourceSet, VarNames.resourceBinding, \
                VarNames.resourceIndex)

        allBindings = samplerBindings + resourceBindings
        params.append(allBindings)

        # Coordinate parameter
        coordParam = "%s %s" % (self.getCoordType(True, True, True), VarNames.coord)
        params.append(coordParam)

        # Gather component
        if self._opKind == SpirvImageOpKind.gather and not self._hasDref:
            params.append("i32 %s" % (VarNames.comp))

        # Image write texel
        if self._opKind == SpirvImageOpKind.write:
            if self._sampledType == SpirvSampledType.f32:
                params.append("<4 x float> %s" % (VarNames.texel))
            elif self._sampledType == SpirvSampledType.f16:
                params.append("<4 x half> %s" % (VarNames.texel))
            elif self._sampledType == SpirvSampledType.i32:
                params.append("<4 x i32> %s" % (VarNames.texel))
            elif self._sampledType == SpirvSampledType.u32:
                params.append("<4 x i32> %s" % (VarNames.texel))
            else:
                shouldNeverCall()

        # Other parameters
        if self._hasDref:
            params.append("float %s" % (VarNames.dRef))

        if self._hasBias:
            params.append("float %s" % (VarNames.bias))

        if self._hasLod:
            if self._opKind == SpirvImageOpKind.fetch or \
               self._opKind == SpirvImageOpKind.read or \
               self._opKind == SpirvImageOpKind.write:
                params.append("i32 %s" % (VarNames.lod))
            else:
                params.append("float %s" % (VarNames.lod))

        if self._hasGrad:
            params.append("%s %s" % (self.getCoordType(False, False, False), VarNames.gradX))
            params.append("%s %s" % (self.getCoordType(False, False, False), VarNames.gradY))

        if self._hasConstOffset or self._hasOffset:
            params.append("%s %s" % (self.getOffsetType(), VarNames.offset))

        if self._hasConstOffsets:
            params.append("[4 x <2 x i32>] %s" % (VarNames.constOffsets))
            pass

        if self._hasSample:
            params.append("i32 %s" % (self._sample))

        if self._hasMinLod:
            params.append("float %s" % (VarNames.minlod))

        if self.isAtomicOp():
            if self._sampledType == SpirvSampledType.f32:
                params.append("float %s" % (VarNames.atomicData))
            else:
                params.append("i32 %s" % (VarNames.atomicData))

        if self._opKind == SpirvImageOpKind.atomiccompexchange:
            params.append("i32 %s" % (VarNames.atomicComparator))

        if self.isAtomicOp():
            params.append("i1 %slc")
        elif self._opKind == SpirvImageOpKind.read or self._opKind == SpirvImageOpKind.write:
            params.append("i1 %glc")
            params.append("i1 %slc")

        # imageCallMeta isn't used by generated code, it is for image patch pass
        params.append("i32 %imageCallMeta")
        return " ,".join(params)

    # Generates sampler and resource loading code.
    def genLoadSamplerAndResource(self, irOut):
        if self._opKind == SpirvImageOpKind.sample or \
           self._opKind == SpirvImageOpKind.gather or \
           self._opKind == SpirvImageOpKind.querylod:
            loadSampler = "    %s = call <4 x i32> @%s(i32 %s, i32 %s, i32 %s)\n" % \
                    (VarNames.sampler, LLPC_DESCRIPTOR_LOAD_SAMPLER, VarNames.samplerSet, \
                    VarNames.samplerBinding, VarNames.samplerIndex)
            irOut.write(loadSampler)

        if not self._returnFmaskId:
            if self._dim == SpirvDim.DimBuffer:
                loadResource = "    %s = call <4 x i32> @%s(i32 %s, i32 %s, i32 %s)\n" % \
                        (VarNames.resource, LLPC_DESCRIPTOR_LOAD_TEXELBUFFER, VarNames.resourceSet, \
                        VarNames.resourceBinding, VarNames.resourceIndex)
            else:
                loadResource = "    %s = call <8 x i32> @%s(i32 %s, i32 %s, i32 %s)\n" % \
                        (VarNames.resource, LLPC_DESCRIPTOR_LOAD_RESOURCE, VarNames.resourceSet, \
                        VarNames.resourceBinding, VarNames.resourceIndex)
            irOut.write(loadResource)

        if self._isFmaskBased or self._returnFmaskId:
            loadFMask = "    %s = call <8 x i32> @%s(i32 %s, i32 %s, i32 %s)\n" % \
                    (VarNames.fmask, LLPC_DESCRIPTOR_LOAD_FMASK, VarNames.resourceSet, \
                    VarNames.resourceBinding, VarNames.resourceIndex)
            irOut.write(loadFMask)

    def processReturn(self, retVal, intrinGen, irOut):
        # Casts instrinsic return type to function return type
        if self._opKind == SpirvImageOpKind.querylod:
            # Extracts first 2 componenets of return value for op query lod
            oldRetVal = retVal
            mipmap = self.acquireLocalVar()
            irOut.write("    %s = extractelement %s %s, i32 0\n" % (mipmap, \
                                                                    intrinGen.getBackendRetType(), \
                                                                    oldRetVal))
            lod = self.acquireLocalVar()
            irOut.write("    %s = extractelement %s %s, i32 1\n" % (lod, \
                                                                    intrinGen.getBackendRetType(), \
                                                                    oldRetVal))
            tempRetVal = self.acquireLocalVar()
            irOut.write("    %s = insertelement <2 x float> undef, float %s, i32 0\n" % (tempRetVal, \
                                                                                         mipmap))
            retVal = self.acquireLocalVar()
            irOut.write("    %s = insertelement <2 x float> %s, float %s, i32 1\n" % (retVal, \
                                                                                      tempRetVal, \
                                                                                      lod))

        elif self._hasDref and self._opKind != SpirvImageOpKind.gather:
            # Extracts first component of return value for shadow texture
            oldRetVal = retVal
            retVal = self.acquireLocalVar()
            irOut.write("    %s = extractelement %s %s, i32 0\n" % (retVal, \
                                                                    intrinGen.getBackendRetType(), \
                                                                    oldRetVal))
        retType = self.getReturnType()

        if self._supportSparse:
            # Return value of sparse instruction is struct
            sparseRetType = self.getSparseReturnType(retType)
            tempRetVal = self.acquireLocalVar()
            irOut.write("    %s = insertvalue %s undef, i32 1, 0\n" % (tempRetVal, sparseRetType))
            dataRetVal = retVal
            retVal = self.acquireLocalVar()
            irOut.write("    %s = insertvalue %s %s, %s %s, 1\n" % (retVal, sparseRetType, tempRetVal, retType, dataRetVal))
            irOut.write("    ret %s %s\n" % (sparseRetType, retVal))
            pass
        else:
            irOut.write("    ret %s %s\n" % (retType, retVal))
            pass

    # Generates coordinate parameters.
    def genCoord(self, irOut):
        coordNum = self.getCoordNumComponents(False, False, True)
        coordNumAll = self.getCoordNumComponents(True, True, True)
        if coordNumAll == 1:
            self._coordXYZW[0] = VarNames.coord
        else:
            for i in range(coordNumAll):
                # Extracts coordinate components
                self._coordXYZW[i] = self.acquireLocalVar()
                coordType = self.getCoordType(True, True, True)
                irExtractCoord = "    %s = extractelement %s %s, i32 %d\n" % (self._coordXYZW[i], coordType, \
                        VarNames.coord, i)
                irOut.write(irExtractCoord)
                if self._arrayed and self.getCoordElemType() == "float" and i == coordNumAll - 1:
                    # Round array layer
                    temp        = self.acquireLocalVar()
                    irRound     = "    %s = call float @llpc.round.f32(float %s)\n" % (temp, self._coordXYZW[i])
                    irOut.write(irRound)
                    self._coordXYZW[i]  = temp
            if self._dim == SpirvDim.DimCube and \
               (self._opKind == SpirvImageOpKind.sample or \
                self._opKind == SpirvImageOpKind.gather or \
                self._opKind == SpirvImageOpKind.querylod):
                # Process cube coordinates
                cubesc          = self.acquireLocalVar()
                irCubesc        = "    %s = call float @llvm.amdgcn.cubesc(float %s, float %s, float %s)\n" % ( \
                        cubesc, self._coordXYZW[0], self._coordXYZW[1], self._coordXYZW[2])
                irOut.write(irCubesc)
                cubetc          = self.acquireLocalVar()
                irCubetc        = "    %s = call float @llvm.amdgcn.cubetc(float %s, float %s, float %s)\n" % ( \
                        cubetc, self._coordXYZW[0], self._coordXYZW[1], self._coordXYZW[2])
                irOut.write(irCubetc)
                cubeMa          = self.acquireLocalVar()
                irCubeMa        = "    %s = call float @llvm.amdgcn.cubema(float %s, float %s, float %s)\n" % ( \
                        cubeMa, self._coordXYZW[0], self._coordXYZW[1], self._coordXYZW[2])
                irOut.write(irCubeMa)
                cubeId          = self.acquireLocalVar()
                irCubeId        = "    %s = call float @llvm.amdgcn.cubeid(float %s, float %s, float %s)\n" % ( \
                        cubeId, self._coordXYZW[0], self._coordXYZW[1], self._coordXYZW[2])
                irOut.write(irCubeId)
                fabsMa          = self.acquireLocalVar()
                irFabsMa        = "    %s = call float @llvm.fabs.f32(float %s)\n" % (fabsMa, cubeMa)
                irOut.write(irFabsMa)
                invFabsMa       = self.acquireLocalVar()
                irInvFabsMa     = "    %s = fdiv float 1.0, %s\n" % (invFabsMa, fabsMa)
                irOut.write(irInvFabsMa)

                tempCubeCoord = (cubesc, cubetc)
                for i in range(2):
                    temp0 = self.acquireLocalVar()
                    irMul = "    %s = fmul float %s, %s\n" % (temp0, tempCubeCoord[i], invFabsMa)
                    irOut.write(irMul)
                    temp1 = self.acquireLocalVar()
                    irAdd = "    %s = fadd float %s, 1.5\n" % (temp1, temp0)
                    irOut.write(irAdd)
                    self._coordXYZW[i] = temp1

                # Store return value of cubema and cubeid, they will be used when transforming cube gradient value
                self._cubeMa = cubeMa
                self._cubeId = cubeId

                if self._arrayed:
                    # For cube array, cubeId = faceId * 8 + cubeId
                    temp = self.acquireLocalVar()
                    irMul = "    %s = fmul float %s, 8.0\n" % (temp, self._coordXYZW[3])
                    irOut.write(irMul)
                    temp1 = self.acquireLocalVar()
                    irAdd = "    %s = fadd float %s, %s\n" % (temp1, temp, cubeId)
                    irOut.write(irAdd)
                    cubeId = temp1

                temp = self.acquireLocalVar()
                irRound = "    %s = call float @llvm.rint.f32(float %s)\n" % (temp, cubeId)
                irOut.write(irRound)
                self._coordXYZW[2] = temp
                pass

        if self._hasProj:
            # Divide coordinates with projection divisor
            invProjDivisor = self.getInverseProjDivisor(irOut)
            for i in range(coordNum):
                coordComp = self._coordXYZW[i]
                self._coordXYZW[i] = self.acquireLocalVar()
                irMul = "    %s = fmul float %s, %s\n" % (self._coordXYZW[i], coordComp, invProjDivisor)
                irOut.write(irMul)
        pass

    # Generates dref parameter.
    def genDref(self, irOut):
        # Generates dRef parameter
        self._dRef = VarNames.dRef
        if self._hasProj:
            # Divides dRef with projection divisor
            oldDRef = self._dRef
            self._dRef = self.acquireLocalVar()
            invProjDivisor = self.getInverseProjDivisor(irOut)
            irMul = "    %s = fmul float %s, %s\n" % (self._dRef, oldDRef, invProjDivisor)
            irOut.write(irMul)
        pass

    # Generates all image operands.
    def genImageOperands(self, irOut):
        # Generates optional image operands
        if self._hasBias:
            self._bias = VarNames.bias

        if self._hasLod:
            self._lod = VarNames.lod

        if self._hasGrad:
            gradNum  = self.getCoordNumComponents(False, False, False)
            gradType = self.getCoordType(False, False, False)
            if gradNum == 1:
                self._gradX[0] = VarNames.gradX
                self._gradY[0] = VarNames.gradY
            else:
                for i in range(gradNum):
                    tempX = self.acquireLocalVar()
                    tempY = self.acquireLocalVar()
                    irExtractX = "    %s = extractelement %s %s, i32 %d\n" % (tempX, gradType, VarNames.gradX, i)
                    irExtractY = "    %s = extractelement %s %s, i32 %d\n" % (tempY, gradType, VarNames.gradY, i)
                    irOut.write(irExtractX)
                    irOut.write(irExtractY)
                    self._gradX[i] = tempX
                    self._gradY[i] = tempY

            if self._dim == SpirvDim.DimCube:
                # Transform cube gradient vector to cube face gradient value
                temp = self.acquireLocalVar()
                irOut.write("    %s = call <4 x float> @%s(float %s, float %s, float %s, float %s, \
float %s, float %s, float %s, float %s, float %s, float %s)\n" % \
                            (temp, \
                             LLPC_TRANSFORM_CUBE_GRAD, \
                             self._cubeId, \
                             self._cubeMa, \
                             self._coordXYZW[0], \
                             self._coordXYZW[1], \
                             self._gradX[0], \
                             self._gradX[1], \
                             self._gradX[2], \
                             self._gradY[0], \
                             self._gradY[1], \
                             self._gradY[2]))

                self._gradX[0] = self.acquireLocalVar()
                irOut.write("    %s = extractelement <4 x float> %s, i32 0\n" % (self._gradX[0], temp))
                self._gradX[1] = self.acquireLocalVar()
                irOut.write("    %s = extractelement <4 x float> %s, i32 1\n" % (self._gradX[1], temp))
                self._gradY[0] = self.acquireLocalVar()
                irOut.write("    %s = extractelement <4 x float> %s, i32 2\n" % (self._gradY[0], temp))
                self._gradY[1] = self.acquireLocalVar()
                irOut.write("    %s = extractelement <4 x float> %s, i32 3\n" % (self._gradY[1], temp))
                pass

        if self._hasConstOffset or self._hasOffset:
            offsetNum = self.getCoordNumComponents(False, False, False)
            offsetType  = self.getOffsetType()
            if self._opKind == SpirvImageOpKind.fetch:
                # Offset for OpImageFetch is added to coordinate per component
                if offsetNum == 1:
                    tempOffset = VarNames.offset
                    tempCoord  = self.acquireLocalVar()
                    irAdd = "    %s = add %s %s, %s\n" % (tempCoord, self.getCoordElemType(), tempOffset, self._coordXYZW[0])
                    self._coordXYZW[0] = tempCoord
                    irOut.write(irAdd)
                else:
                    for i in range(offsetNum):
                        tempOffset = self.acquireLocalVar()
                        irExtract = "    %s = extractelement %s %s, i32 %d\n" % (tempOffset, offsetType, VarNames.offset, i)
                        irOut.write(irExtract)
                        tempCoordComp = self.acquireLocalVar()
                        irAdd = "    %s = add %s %s, %s\n" % (tempCoordComp, self.getCoordElemType(), tempOffset, self._coordXYZW[i])
                        self._coordXYZW[i] = tempCoordComp
                        irOut.write(irAdd)
            else:
                self._packedOffset = self.genPackedOffset(VarNames.offset, offsetNum, offsetType, irOut)

        if self._hasConstOffsets:
            offsetNum   = self.getCoordNumComponents(False, False, False)
            offsetType  = self.getOffsetType()
            tempOffsets = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                            FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]
            for i in range(4):
                tempOffsets[i] = self.acquireLocalVar()
                irExtractValue = "    %s = extractvalue [4 x <2 x i32>] %s, %d\n" % (tempOffsets[i], VarNames.constOffsets, i)
                irOut.write(irExtractValue)
            for i in range(4):
                self._packedOffsets[i] = self.genPackedOffset(tempOffsets[i], offsetNum, offsetType, irOut)

        if self._hasMinLod:
            self._minlod = VarNames.minlod

    # Generates packed offset.
    def genPackedOffset(self, offset, offsetNum, offsetType, irOut):
        # Offset for image operations other than OpImageFetch are packed into 1 dword which is: x | (y << 8) | (z << 16)
        ret = ""
        if offsetNum == 1:
            ret = self.genBitcastInst(offset, irOut)
        else:
            # Extract offset components
            tempOffsets = ["", "", ""]
            for i in range(offsetNum):
                tempOffsets[i] = self.acquireLocalVar()
                irExtract = "    %s = extractelement %s %s, i32 %d\n" % (tempOffsets[i], offsetType, offset, i)
                irOut.write(irExtract)
            # Extract 6 low bits of each offset components
            tempOffsetsAnded = ["", "", ""]
            for i in range(offsetNum):
                tempOffsetsAnded[i] = self.acquireLocalVar()
                irAnd = "    %s = and i32 %s, %d\n" % (tempOffsetsAnded[i], tempOffsets[i], 63)
                irOut.write(irAnd)
            # Shift offset components
            tempOffsetsShifted = [tempOffsetsAnded[0], "", ""]
            shiftBits          = [0, 8, 16]
            for i in range(offsetNum)[1:]:
                tempOffsetsShifted[i] = self.acquireLocalVar()
                irShiftLeft = "    %s = shl i32 %s, %d\n" % (tempOffsetsShifted[i], tempOffsetsAnded[i], shiftBits[i])
                irOut.write(irShiftLeft)
            # Pack shifted offset components
            tempPackedOffset = tempOffsetsShifted[0]
            for i in range(offsetNum)[1:]:
                temp2 = self.acquireLocalVar()
                irBitwiseOr = "    %s = or i32 %s, %s\n" % (temp2, tempPackedOffset, tempOffsetsShifted[i])
                tempPackedOffset = temp2
                irOut.write(irBitwiseOr)
            ret = self.genBitcastInst(tempPackedOffset, irOut)
        return ret

    # Generates bitcast to float instruction for one parameter.
    def genBitcastInst(self, var, irOut):
        retVal = self.acquireLocalVar()
        irCast = "    %s = bitcast i32 %s to float\n" % (retVal, var)
        irOut.write(irCast)
        return retVal

    # Gets coordinate components number.
    def getCoordNumComponents(self, includeArraySlice, includeProj, preCubeCoordTransform):
        coordNum = 0
        if self._dim == SpirvDim.DimCube:
            if self._arrayed and \
               (self._opKind == SpirvImageOpKind.sample or \
                self._opKind == SpirvImageOpKind.fetch or \
                self._opKind == SpirvImageOpKind.gather or \
                self._opKind == SpirvImageOpKind.querylod):
                if preCubeCoordTransform:
                    coordNum = 4
                else:
                    coordNum = 3
            else:
                coordNum = 3
        else:
            dimStr = rFind(SPIRV_DIM_DICT, self._dim)
            coordNum = SPIRV_DIM_TO_COORD_NUM_DICT[dimStr]
            if includeArraySlice and self._arrayed:
                coordNum += 1
            if includeProj and self._hasProj:
                coordNum += 1
        return coordNum

    # Gets inverse of projection divisor.
    def getInverseProjDivisor(self, irOut):
        assert self._hasProj
        assert self._arrayed == False
        if self._invProjDivisor != FuncDef.INVALID_VAR:
            # Inversed projection divisor has been initialzied, return it directly
            return self._invProjDivisor
        else:
            # Initialize inverse projection divisor
            coordNum = self.getCoordNumComponents(False, False, False)
            assert self._coordXYZW[coordNum] != FuncDef.INVALID_VAR
            self._invProjDivisor = self.acquireLocalVar()
            irInvProjDivisor = "    %s = fdiv float 1.0, %s\n" % (self._invProjDivisor, \
                    self._coordXYZW[coordNum])
            irOut.write(irInvProjDivisor)
            return self._invProjDivisor

    # Gets coordinate element type.
    def getCoordElemType(self):
        coordElemType = "float"
        if self._opKind == SpirvImageOpKind.fetch or                \
           self._opKind == SpirvImageOpKind.read or                 \
           self._opKind == SpirvImageOpKind.write or                \
           self._opKind == SpirvImageOpKind.atomicexchange or       \
           self._opKind == SpirvImageOpKind.atomiccompexchange or   \
           self._opKind == SpirvImageOpKind.atomiciincrement or     \
           self._opKind == SpirvImageOpKind.atomicidecrement or     \
           self._opKind == SpirvImageOpKind.atomiciadd or           \
           self._opKind == SpirvImageOpKind.atomicisub or           \
           self._opKind == SpirvImageOpKind.atomicsmin or           \
           self._opKind == SpirvImageOpKind.atomicumin or           \
           self._opKind == SpirvImageOpKind.atomicsmax or           \
           self._opKind == SpirvImageOpKind.atomicumax or           \
           self._opKind == SpirvImageOpKind.atomicand or            \
           self._opKind == SpirvImageOpKind.atomicor or             \
           self._opKind == SpirvImageOpKind.atomicxor:
            coordElemType = "i32"
        return coordElemType

    # Gets coordinate type.
    def getCoordType(self, includeArraySlice, includeProj, preCubeCoordTransform):
        coordElemType = self.getCoordElemType()
        coordNum      = self.getCoordNumComponents(includeArraySlice, includeProj, preCubeCoordTransform)
        if coordNum == 1:
            coordType = coordElemType
        else:
            coordType = "<%d x %s>" % (coordNum, coordElemType)
        return coordType

    # Gets type of offset parameter.
    def getOffsetType(self):
        coordNum = self.getCoordNumComponents(False, False, False)
        if coordNum == 1:
            offsetType = "i32"
        else:
            offsetType = "<%d x i32>" % (coordNum)
        return offsetType

    # Allocates a local variable index.
    def acquireLocalVar(self):
        ret = self._localVarCounter
        self._localVarCounter += 1
        return "%%%d" % (ret)

    # Generates loading of F-mask value.
    def genLoadFmask(self, irOut):
        vaddrRegType    = self.getVAddrRegType()
        vaddrReg        = self.genFillVAddrReg(0, True, irOut)
        fmaskDesc       = "<8 x i32> %s," % (VarNames.fmask)
        fmaskVal        = self.acquireLocalVar()
        flags           = LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS + ", i1 0"
        loadFmask       = "    %s = call <4 x float> @llvm.amdgcn.image.load.v4f32.v4i32.v8i32(%s %s, %s i32 15, %s)\n" % \
                      (fmaskVal, \
                       vaddrRegType, \
                       vaddrReg, \
                       fmaskDesc, \
                       flags)
        irOut.write(loadFmask)
        temp        = self.acquireLocalVar()
        bitcast     = "    %s = bitcast <4 x float> %s to <4 x i32>\n" % (temp, fmaskVal)
        irOut.write(bitcast)
        return temp

    # Generates return value for F-mask only fetch.
    def genReturnFmaskValue(self, fmaskVal, irOut):
        descriptorDataFormat = self.genExtractDescriptorDataFormat(irOut)
        comment = "    ; Check whether the descriptor is valid, 0 = BUF_DATA_FORMAT_INVALID\n"
        irOut.write(comment)
        temp1 = self.acquireLocalVar()
        icmp = "    %s = icmp ne i32 %s, 0\n" % (temp1, descriptorDataFormat)
        irOut.write(icmp)
        temp2 = self.acquireLocalVar()
        # Generate default sample mask if F-mask descriptor is invalid,
        # default value is <0x76543210, undef, undef, undef>
        insertelement = "    %s = insertelement <4 x i32> undef, i32 1985229328, i32 0\n" % (temp2)
        irOut.write(insertelement)
        temp3 = self.acquireLocalVar()
        select = "    %s = select i1 %s, <4 x i32> %s, <4 x i32> %s\n" % (temp3, temp1, fmaskVal, temp2)
        irOut.write(select)
        temp4 = self.acquireLocalVar()
        bitcast = "    %s = bitcast <4 x i32> %s to <4 x float>\n" % (temp4, temp3)
        irOut.write(bitcast)
        retVal = self.genCastReturnValToSampledType(temp4, irOut)
        return retVal

    # Generates sample ID based on F-mask value.
    def genGetSampleIdFromFmaskValue(self, fmaskVal, irOut):
        # Extract sample mask
        temp1 = self.acquireLocalVar()
        extractElement = "    %s = extractelement <4 x i32> %s, i32 0\n" % (temp1, fmaskVal)
        irOut.write(extractElement)

        # Extract sample ID through sample mask
        temp2 = self.acquireLocalVar()
        mul = "    %s = shl i32 %s, 2\n" % (temp2, self._sample)
        irOut.write(mul)
        temp3 = self.acquireLocalVar()
        lshr = "    %s = lshr i32 %s, %s\n" % (temp3, temp1, temp2)
        irOut.write(lshr)
        temp4 = self.acquireLocalVar()
        bitAnd = "    %s = and i32 %s, 15\n" % (temp4, temp3)
        irOut.write(bitAnd)

        # Check F-mask descriptor validity
        descriptorDataFormat = self.genExtractDescriptorDataFormat(irOut)
        temp5 = self.acquireLocalVar()
        comment = "    ; Check whether the descriptor is valid, 0 = BUF_DATA_FORMAT_INVALID\n"
        irOut.write(comment)
        icmp = "    %s = icmp ne i32 %s, 0\n" % (temp5, descriptorDataFormat)
        irOut.write(icmp)
        temp6 = self.acquireLocalVar()
        select = "    %s = select i1 %s, i32 %s, i32 %s\n" % (temp6, temp5, temp4, self._sample)
        irOut.write(select)
        return temp6

    # Generates instructions to extract data format field from a resource descriptor.
    def genExtractDescriptorDataFormat(self, irOut):
        comment = "    ; Extract DATA_FORMAT field from resource descriptor (DWORD[1].bit[20-25])\n"
        irOut.write(comment)
        temp1 = self.acquireLocalVar()
        extractDword = "    %s = extractelement <8 x i32> %s, i32 1\n" % (temp1, VarNames.fmask)
        irOut.write(extractDword)
        temp2 = self.acquireLocalVar()
        lShr = "    %s = lshr i32 %s, 20\n" % (temp2, temp1)
        irOut.write(lShr)
        temp3 = self.acquireLocalVar()
        bitAnd = "    %s = and i32 %s, 63\n" % (temp3, temp2)
        irOut.write(bitAnd)
        return temp3

    # Gets insertelement assembly code.
    def getInsertElement(self, vaddrReg, vaddrRegType, vaddrRegCompType, var, index):
        oldVaddrReg = vaddrReg
        vaddrReg = self.acquireLocalVar()
        irInsertElement = "    %s = insertelement %s %s, %s %s, i32 %d\n" % (vaddrReg, vaddrRegType, oldVaddrReg, \
                                                                             vaddrRegCompType, var, index)
        return (vaddrReg, irInsertElement)

    # Generates vaddr register setup
    def genFillVAddrReg(self, constOffsetsIndex, isFetchingFromFmask, irOut):
        index = 0
        vaddrRegType = self.getVAddrRegType()
        vaddrRegCompType = self.getVAddrRegCompType()
        vaddrReg = "undef"

        if self._hasConstOffset or self._hasOffset:
            if self._opKind != SpirvImageOpKind.fetch:
                # Fetch offset has already been added to coordinate per component
                ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._packedOffset, index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])

        if self._hasConstOffsets:
            ret = self.getInsertElement(vaddrReg,
                                        vaddrRegType,
                                        vaddrRegCompType,
                                        self._packedOffsets[constOffsetsIndex],
                                        index)
            vaddrReg = ret[0]
            index += 1
            irOut.write(ret[1])

        if self._hasBias:
            ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._bias, index)
            vaddrReg = ret[0]
            index += 1
            irOut.write(ret[1])

        if self._dRef:
            ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._dRef, index)
            vaddrReg = ret[0]
            index += 1
            irOut.write(ret[1])

        if self._gradX[0]:
            numGradComponents = self.getCoordNumComponents(False, False, False)
            if self._dim == SpirvDim.DimCube:
                numGradComponents = 2

            for i in range(numGradComponents):
                ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._gradX[i], index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])
            for i in range(numGradComponents):
                ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._gradY[i], index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])

        if self.getVAddrRegSize() == 1:
            vaddrReg = self._coordXYZW[0]
            index += 1
            pass
        else:
            for i in range(self.getCoordNumComponents(True, False, False)):
                coordComp = self._coordXYZW[i]
                ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, coordComp, index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])
            # SampleNumber is appended to last coordinate component
            if self._hasSample:
                if isFetchingFromFmask:
                    ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, "0", index)
                    pass
                else:
                    ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._sample, index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])
            # MinLod is appended to last coordinate component
            if self._minlod:
                ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._minlod, index)
                vaddrReg = ret[0]
                index += 1
                irOut.write(ret[1])

        if self._hasLod and not self._supportLzOptimization:
            ret = self.getInsertElement(vaddrReg, vaddrRegType, vaddrRegCompType, self._lod, index)
            vaddrReg = ret[0]
            index += 1
            irOut.write(ret[1])
        return vaddrReg

    # Gets size of vaddr register
    def getVAddrRegSize(self):
        size = self.getCoordNumComponents(True, False, False)
        if self._hasDref:
            size += 1
        if self._hasBias:
            size += 1
        if self._hasLod and not self._supportLzOptimization:
            size += 1
        if self._hasGrad:
            size += self.getCoordNumComponents(False, False, False) * 2
        if self._hasConstOffset or self._hasOffset or self._hasConstOffsets:
            if self._opKind != SpirvImageOpKind.fetch:
                size += 1
        if self._hasSample:
            size += 1
        if self._hasMinLod:
            size += 1

        # Round up size to 1, 2, 4, 8, 16
        sizes = (1, 2, 4, 8, 16)
        for i in sizes:
            if size <= i:
                size = i
                break
        assert size in sizes

        return size

    # Gets type of vaddr register.
    def getVAddrRegType(self):
        vaddrRegSize = self.getVAddrRegSize()
        if self._opKind == SpirvImageOpKind.fetch or \
           self._opKind == SpirvImageOpKind.read or \
           self._opKind == SpirvImageOpKind.write or \
           self.isAtomicOp():
            return vaddrRegSize == 1 and "i32" or "<%d x i32>" % (vaddrRegSize)
        else:
            return vaddrRegSize == 1 and "float" or "<%d x float>" % (vaddrRegSize)

    # Gets component type of vaddr register
    def getVAddrRegCompType(self):
        if self._opKind == SpirvImageOpKind.fetch or \
           self._opKind == SpirvImageOpKind.read or \
           self._opKind == SpirvImageOpKind.write or \
           self.isAtomicOp():
            return "i32"
        else:
            return "float"

    # Generates instruction to cast return value to sampled type.
    def genCastReturnValToSampledType(self, retVal, irOut):
        newRetVal = retVal
        if self._sampledType == SpirvSampledType.i32 or \
           self._sampledType == SpirvSampledType.u32:
            newRetVal = self.acquireLocalVar()
            irBitcast = "    %s = bitcast <4x float> %s to <4 x i32>\n" % (newRetVal, retVal)
            irOut.write(irBitcast)
        return newRetVal

    # Generates instruction to cast texel value from sampled type to type which is required by LLVM intrinsic.
    def genCastTexelToSampledType(self, texel, irOut):
        paramTexel = texel
        if self._sampledType == SpirvSampledType.i32 or \
           self._sampledType == SpirvSampledType.u32:
            paramTexel = self.acquireLocalVar()
            irCast = "    %s = bitcast <4 x i32> %s to <4 x float>\n" % (paramTexel, texel)
            irOut.write(irCast)
        return paramTexel

    # Gets return type of llvm image intrinsic.
    def getBackendRetType(self):
        if self._opKind == SpirvImageOpKind.write:
            return "void"
        elif self.isAtomicOp():
            return "i32"
        else:
            if self._sampledType == SpirvSampledType.f16:
                return "<4 x half>"
            else:
                return "<4 x float>"

    def getDAFlag(self):
        if self._arrayed or \
           (self._dim == SpirvDim.DimCube and \
            (self._opKind == SpirvImageOpKind.read or \
             self._opKind == SpirvImageOpKind.write or \
             self.isAtomicOp())):
            return ", i1 1"
        else:
            return ", i1 0"

    # GFX9 HW requires 1D and 1DArray's coordinate to be treated as 2D and 2DArray, we need to patch related operands.
    def patchGfx91Dand1DArray(self):
        if self._dim == SpirvDim.Dim1D:
            # Patch coordinate
            patchedValue = self.getGfx91Dand1DArrayPatchedCoordValue();
            if self._arrayed:
                assert self._coordXYZW[2] == FuncDef.INVALID_VAR
                self._coordXYZW[2] = self._coordXYZW[1]
            else:
                assert self._coordXYZW[1] == FuncDef.INVALID_VAR or self._hasProj
            self._coordXYZW[1] = patchedValue

            # Patch Grad
            if self._hasGrad:
                self._gradX[1] = LITERAL_FLOAT_ZERO
                self._gradY[1] = LITERAL_FLOAT_ZERO

            # Patch Dimension
            self._dim = SpirvDim.Dim2D

    # Gets patched value for GFX9 1D and 1DArray coordinate
    def getGfx91Dand1DArrayPatchedCoordValue(self):
        if self._opKind == SpirvImageOpKind.sample or \
           self._opKind == SpirvImageOpKind.gather or \
           self._opKind == SpirvImageOpKind.querylod:
               return LITERAL_FLOAT_ZERO_POINT_FIVE
        else:
            return LITERAL_INT_ZERO

# Represents buffer atomic intrinsic based code generation.
class BufferAtomicGen(CodeGen):
    def __init__(self, codeGenBase):
        super(BufferAtomicGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self.isAtomicOp()
        assert self._dim == SpirvDim.DimBuffer

        retType     = self.getBackendRetType()
        retVal      = FuncDef.INVALID_VAR
        funcName    = self.getFuncName()

        # Process buffer atomic operations
        atomicData = VarNames.atomicData
        if self._sampledType == SpirvSampledType.f32:
            # Cast atomic value operand type to i32
            atomicData = self.acquireLocalVar()
            irBitcast = "    %s = bitcast float %s to i32\n" % (atomicData, VarNames.atomicData)
            irOut.write(irBitcast)

        vaddrReg   = self._coordXYZW[0]
        comp       = self._opKind == SpirvImageOpKind.atomiccompexchange \
                        and "i32 " + VarNames.atomicComparator + "," \
                        or  ""
        # Set slc flag
        flags  = "i1 %slc"

        retVal = self.acquireLocalVar()
        irCall = "    %s = call %s @%s(i32 %s, %s <4 x i32> %s, i32 %s, i32 0, %s)\n" % \
                 (retVal,
                  retType,
                  funcName,
                  atomicData,
                  comp,
                  VarNames.resource,
                  vaddrReg,
                  flags)
        irOut.write(irCall)

        if self._sampledType == SpirvSampledType.f32:
            oldRetVal = retVal
            retVal = self.acquireLocalVar()
            irBitcast = "    %s = bitcast i32 %s to float\n" % (retVal, oldRetVal)
            irOut.write(irBitcast)

        return retVal

    # Gets buffer atomic intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn.buffer.atomic"

        intrinsicNames = { \
            SpirvImageOpKind.atomicexchange      : ".swap",     \
            SpirvImageOpKind.atomiccompexchange  : ".cmpswap",  \
            SpirvImageOpKind.atomiciincrement    : ".add",      \
            SpirvImageOpKind.atomicidecrement    : ".sub",      \
            SpirvImageOpKind.atomiciadd          : ".add",      \
            SpirvImageOpKind.atomicisub          : ".sub",      \
            SpirvImageOpKind.atomicsmin          : ".smin",     \
            SpirvImageOpKind.atomicumin          : ".umin",     \
            SpirvImageOpKind.atomicsmax          : ".smax",     \
            SpirvImageOpKind.atomicumax          : ".umax",     \
            SpirvImageOpKind.atomicand           : ".and",      \
            SpirvImageOpKind.atomicor            : ".or",       \
            SpirvImageOpKind.atomicxor           : ".xor",      \
        }

        funcName += intrinsicNames[self._opKind]

        compareType = ""
        if self._opKind == SpirvImageOpKind.atomiccompexchange:
            compareType = "i32,"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            funcExternalDecl = "declare %s @%s(i32, %s <4 x i32>, i32, i32, i1) %s\n" % \
                               (self.getBackendRetType(), funcName, compareType, self._attr)
            addLlvmDecl(funcName, funcExternalDecl)

        return funcName

# Represents image atomic intrinsic based code generation.
class ImageAtomicGen(CodeGen):
    def __init__(self, codeGenBase):
        super(ImageAtomicGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self.isAtomicOp()
        assert self._dim != SpirvDim.DimBuffer

        retType     = self.getBackendRetType()
        retVal      = FuncDef.INVALID_VAR
        funcName    = self.getFuncName()

        # Process image atomic operations
        atomicData = VarNames.atomicData
        if self._sampledType == SpirvSampledType.f32:
            # Cast atomic value operand type to i32
            atomicData = self.acquireLocalVar()
            irBitcast = "    %s = bitcast float %s to i32\n" % (atomicData, VarNames.atomicData)
            irOut.write(irBitcast)

        vaddrRegType = self.getVAddrRegType()
        vaddrReg     = self.genFillVAddrReg(0, False, irOut)
        comp         = self._opKind == SpirvImageOpKind.atomiccompexchange \
                        and "i32 " + VarNames.atomicComparator + "," \
                        or  ""
        retVal     = self.acquireLocalVar()

        # Set r128 bit flag
        flags = ", i1 0"

        # Set DA flag for arrayed resource
        flags += self.getDAFlag()
        # Set slc flag
        flags += ", i1 %slc"

        resourceName = VarNames.resource;
        if self._dim == SpirvDim.DimCube:
            resourceName = "%patchedResource"
            irPatchCall = "    %s = call <8 x i32> @%s(<8 x i32> %s)\n" \
                % (resourceName,
                   LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE,
                   VarNames.resource)
            irOut.write(irPatchCall)

        irCall = "    %s = call i32 @%s(i32 %s, %s %s %s, <8 x i32> %s %s)\n" \
            % (retVal,
               funcName,
               atomicData,
               comp,
               vaddrRegType,
               vaddrReg,
               resourceName,
               flags)
        irOut.write(irCall)

        if self._sampledType == SpirvSampledType.f32:
            oldRetVal = retVal
            retVal = self.acquireLocalVar()
            irBitcast = "    %s = bitcast i32 %s to float\n" % (retVal, oldRetVal)
            irOut.write(irBitcast)

        return retVal

    # Gets buffer atomic intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn.image.atomic"

        intrinsicNames = { \
            SpirvImageOpKind.atomicexchange      : ".swap",     \
            SpirvImageOpKind.atomiccompexchange  : ".cmpswap",  \
            SpirvImageOpKind.atomiciincrement    : ".add",      \
            SpirvImageOpKind.atomicidecrement    : ".sub",      \
            SpirvImageOpKind.atomiciadd          : ".add",      \
            SpirvImageOpKind.atomicisub          : ".sub",      \
            SpirvImageOpKind.atomicsmin          : ".smin",     \
            SpirvImageOpKind.atomicumin          : ".umin",     \
            SpirvImageOpKind.atomicsmax          : ".smax",     \
            SpirvImageOpKind.atomicumax          : ".umax",     \
            SpirvImageOpKind.atomicand           : ".and",      \
            SpirvImageOpKind.atomicor            : ".or",       \
            SpirvImageOpKind.atomicxor           : ".xor",      \
        }

        funcName += intrinsicNames[self._opKind]

        vaddrRegSize = self.getVAddrRegSize()
        funcName += vaddrRegSize == 1 and ".i32" or ".v%di32" % (vaddrRegSize)

        compareType = ""
        if self._opKind == SpirvImageOpKind.atomiccompexchange:
            compareType = "i32,"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            funcExternalDecl = "declare %s @%s(i32, %s %s, <8 x i32>, i1, i1, i1) %s\n" % \
                               (self.getBackendRetType(), funcName, compareType, self.getVAddrRegType(), \
                                self._attr)
            addLlvmDecl(funcName, funcExternalDecl)

        return funcName
        pass

# Represents buffer load intrinsic based code generation.
class BufferLoadGen(CodeGen):
    def __init__(self, codeGenBase):
        super(BufferLoadGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self._dim == SpirvDim.DimBuffer
        assert self._opKind == SpirvImageOpKind.read or self._opKind == SpirvImageOpKind.fetch

        retType     = self.getBackendRetType()
        retVal      = FuncDef.INVALID_VAR
        funcName    = self.getFuncName()

        flags  = LLVM_BUFFER_INTRINSIC_LOADSTORE_FLAGS
        if self._opKind == SpirvImageOpKind.read:
            flags = LLVM_BUFFER_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC

        # Process buffer load
        retVal = self.acquireLocalVar()
        irCall = "    %s = call %s @%s(<4 x i32> %s, i32 %s, i32 0, %s)\n" % \
                 (retVal,
                  retType,
                  funcName,
                  VarNames.resource,
                  self._coordXYZW[0],
                  flags)
        irOut.write(irCall)
        retVal = self.genCastReturnValToSampledType(retVal, irOut)

        return retVal

    # Gets buffer load intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn.buffer.load.format"

        if self._sampledType == SpirvSampledType.f16:
            funcName += ".v4f16"
        else:
            funcName += ".v4f32"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            funcExternalDecl = "declare %s @%s(<4 x i32>, i32, i32, i1, i1) %s\n" % \
                               (self.getBackendRetType(), funcName, self._attr)
            addLlvmDecl(funcName, funcExternalDecl)

        return funcName

# Represents buffer store intrinsic based code generation.
class BufferStoreGen(CodeGen):
    def __init__(self, codeGenBase):
        super(BufferStoreGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self._dim == SpirvDim.DimBuffer
        assert self._opKind == SpirvImageOpKind.write

        retType     = self.getBackendRetType()
        funcName    = self.getFuncName()

        flags = LLVM_BUFFER_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC

        # Process image buffer store
        paramTexel = self.genCastTexelToSampledType(VarNames.texel, irOut)
        irCall = "    call void @%s(<4 x float> %s, <4 x i32> %s, i32 %s, i32 0, %s)\n" % \
                 (funcName,
                  paramTexel,
                  VarNames.resource,
                  self._coordXYZW[0],
                  flags)
        irOut.write(irCall)
        return FuncDef.INVALID_VAR

    # Gets buffer store intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn"
        if self._opKind == SpirvImageOpKind.fetch or self._opKind == SpirvImageOpKind.read:
            funcName += ".buffer.load.format"
        elif self._opKind == SpirvImageOpKind.write:
            funcName += ".buffer.store.format"
        else:
            shouldNeverCall("")

        funcName += ".v4f32"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            if self._opKind == SpirvImageOpKind.fetch or self._opKind == SpirvImageOpKind.read:
                funcExternalDecl = "declare %s @%s(<4 x i32>, i32, i32, i1, i1) %s\n" % \
                                   (self.getBackendRetType(), funcName, self._attr)
            elif self._opKind == SpirvImageOpKind.write:
                funcExternalDecl = "declare %s @%s(<4 x float>, <4 x i32>, i32, i32, i1, i1) %s\n" % \
                                   (self.getBackendRetType(), funcName, self._attr)
            else:
                shouldNeverCall()
            addLlvmDecl(funcName, funcExternalDecl)

        return funcName

# Represents image load intrinsic based code generation.
class ImageLoadGen(CodeGen):
    def __init__(self, codeGenBase):
        super(ImageLoadGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self._dim != SpirvDim.DimBuffer
        assert self._opKind == SpirvImageOpKind.read or self._opKind == SpirvImageOpKind.fetch

        retType     = self.getBackendRetType()
        retVal      = FuncDef.INVALID_VAR
        funcName    = self.getFuncName()

        # Process image sample, image gather, image fetch
        retVals = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                   FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]

        vaddrReg    = self.genFillVAddrReg(0, False, irOut)
        retVal      = self.acquireLocalVar()

        if self._opKind == SpirvImageOpKind.fetch:
            flags = LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS
        else:
            flags = LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC

        # Set DA flag for arrayed resource
        flags += self.getDAFlag()

        resourceName = VarNames.resource;
        if self._dim == SpirvDim.DimCube:
            resourceName = "%patchedResource"
            irPatchCall = "    %s = call <8 x i32> @%s(<8 x i32> %s)\n" \
                % (resourceName,
                   LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE,
                   VarNames.resource)
            irOut.write(irPatchCall)

        irCall = "    %s = call %s @%s(%s %s, <8 x i32> %s,  i32 15, %s)\n" \
            % (retVal,
               retType,
               funcName,
               self.getVAddrRegType(),
               vaddrReg,
               resourceName,
               flags)
        irOut.write(irCall)

        retVal = self.genCastReturnValToSampledType(retVal, irOut)

        return retVal

    # Gets image load intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn.image.load"

        if self._hasLod:
            funcName += ".mip"

        if self._hasConstOffset:
            if self._opKind != SpirvImageOpKind.fetch:
                funcName += ".o"

        if self._sampledType == SpirvSampledType.f16:
            funcName += ".v4f16"
        else:
            funcName += ".v4f32"

        vaddrRegSize = self.getVAddrRegSize()
        funcName += vaddrRegSize == 1 and ".i32" or ".v%di32" % (vaddrRegSize)
        funcName += ".v8i32"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            if self._opKind != SpirvImageOpKind.write:
                funcExternalDecl = "declare %s @%s(%s, <8 x i32>,  i32, i1, i1, i1, i1) %s\n" % \
                                   (self.getBackendRetType(), funcName, self.getVAddrRegType(), \
                                    self._attr)
            addLlvmDecl(funcName, funcExternalDecl)
        return funcName
        pass

# Represents image store intrinsic based code generation.
class ImageStoreGen(CodeGen):
    def __init__(self, codeGenBase):
        super(ImageStoreGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self._dim != SpirvDim.DimBuffer
        assert self._opKind == SpirvImageOpKind.write

        retType     = self.getBackendRetType()
        funcName    = self.getFuncName()

        # Process image write
        vaddrReg = self.genFillVAddrReg(0, False, irOut)
        vdataReg = self.genCastTexelToSampledType(VarNames.texel, irOut)
        flag = LLVM_IMAGE_INTRINSIC_LOADSTORE_FLAGS_DYNAMIC + self.getDAFlag()
        resourceName = VarNames.resource;
        if self._dim == SpirvDim.DimCube:
            resourceName = "%patchedResource"
            irPatchCall = "    %s = call <8 x i32> @%s(<8 x i32> %s)\n" \
                % (resourceName,
                   LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE,
                   VarNames.resource)
            irOut.write(irPatchCall)

        irCall = "    call void @%s(<4 x float> %s, %s %s, <8 x i32> %s, i32 15, %s)\n" \
            % (funcName,
               vdataReg,
               self.getVAddrRegType(),
               vaddrReg,
               resourceName,
               flag)
        irOut.write(irCall)

        return FuncDef.INVALID_VAR

    # Gets image store intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn.image.store"

        if self._hasLod:
            funcName += ".mip"

        funcName += ".v4f32"
        vaddrRegSize = self.getVAddrRegSize()
        funcName += vaddrRegSize == 1 and ".i32" or ".v%di32" % (vaddrRegSize)

        funcName += ".v8i32"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            funcExternalDecl = "declare %s @%s(<4 x float>, %s, <8 x i32>,  i32, i1, i1, i1, i1) %s\n" % \
                               (self.getBackendRetType(), funcName, self.getVAddrRegType(), \
                                self._attr)
            addLlvmDecl(funcName, funcExternalDecl)
        return funcName

# Represents image sample intrinsic based code generation.
class ImageSampleGen(CodeGen):
    def __init__(self, codeGenBase):
        super(ImageSampleGen, self).__init__(codeGenBase, codeGenBase._gfxLevel)
        pass

    # Generates call to amdgcn intrinsic function.
    def genIntrinsicCall(self, irOut):
        assert self._dim != SpirvDim.DimBuffer
        assert self._opKind == SpirvImageOpKind.sample or self._opKind == SpirvImageOpKind.gather or \
                self._opKind == SpirvImageOpKind.querylod

        retType     = self.getBackendRetType()
        retElemType = self._sampledType == SpirvSampledType.f16 and "half" or "float"
        funcName    = self.getFuncName()
        resourceName = VarNames.resource
        # Dmask for gather4 instructions
        dmask = "15"
        if self._opKind == SpirvImageOpKind.gather:
            if not self._hasDref:
                # Gather component transformed in to dmask as: dmask = 1 << comp
                dmask = self.acquireLocalVar()
                irShiftLeft = "    %s = shl i32 1, %s\n" % (dmask, VarNames.comp)
                irOut.write(irShiftLeft)
            else:
                # Gather component is 1 for shadow textures
                dmask = "1"
            # Patch descriptor for gather
            if self._gfxLevel < GFX9:
                if self._sampledType == SpirvSampledType.i32:
                    resourceName = "%patchedResource"
                    irPatchCall = "    %s = call <8 x i32> @llpc.patch.image.gather.descriptor.u32(<8 x i32> %s)\n" \
                        % (resourceName,
                           VarNames.resource)
                    irOut.write(irPatchCall)
                elif self._sampledType == SpirvSampledType.u32:
                    resourceName = "%patchedResource"
                    irPatchCall = "    %s = call <8 x i32> @llpc.patch.image.gather.descriptor.u32(<8 x i32> %s)\n" \
                        % (resourceName,
                           VarNames.resource)
                    irOut.write(irPatchCall)

        # Process image sample, image gather, image fetch
        retVals = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                   FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]

        patchedRetVals =  [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                           FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]

        # Remove sampler descriptor for OpImageFetch
        samplerDesc = "<4 x i32> %s," % (VarNames.sampler)

        # First parameter
        vaddrRegs       = [FuncDef.INVALID_VAR, FuncDef.INVALID_VAR, \
                           FuncDef.INVALID_VAR, FuncDef.INVALID_VAR]
        # For OpImageGather* with offsets, we need to generate 4 gather4 instructions, thus introduce 4 vaddrRegs,
        # other image instructions will only use the first element
        callNum     = self._hasConstOffsets and 4 or 1
        for i in range(callNum):
            vaddrRegs[i]    = self.genFillVAddrReg(i, False, irOut)
            retVals[i]      = self.acquireLocalVar()

            # Instruction flags: unorm, glc, slc, lwe
            flags = "i1 0, i1 0, i1 0, i1 0"

            # Set DA flag for arrayed resource
            flags += self.getDAFlag()

            irCall     = "    %s = call %s @%s(%s %s, <8 x i32> %s, %s i32 %s, %s)\n" \
                % (retVals[i],
                   retType,
                   funcName,
                   self.getVAddrRegType(),
                   vaddrRegs[i],
                   resourceName,
                   samplerDesc,
                   dmask,
                   flags)
            irOut.write(irCall)
            if self._gfxLevel < GFX9 and self._opKind == SpirvImageOpKind.gather:
                # Patch descriptor for gather
                if self._sampledType == SpirvSampledType.i32:
                    patchedRetVals[i]  = self.acquireLocalVar()
                    irPatchCall = "    %s = call <4 x float> @llpc.patch.image.gather.texel.i32(<8 x i32> %s, <4 x float> %s)\n" \
                        % (patchedRetVals[i],
                           VarNames.resource,
                           retVals[i])
                    irOut.write(irPatchCall)
                    retVals[i] = patchedRetVals[i]
                elif self._sampledType == SpirvSampledType.u32:
                    patchedRetVals[i]  = self.acquireLocalVar()
                    irPatchCall = "    %s = call <4 x float> @llpc.patch.image.gather.texel.u32(<8 x i32> %s, <4 x float> %s)\n" \
                        % (patchedRetVals[i],
                           VarNames.resource,
                           retVals[i])
                    irOut.write(irPatchCall)
                    retVals[i] = patchedRetVals[i]

        if not self._hasConstOffsets:
            retVal  = self.genCastReturnValToSampledType(retVals[0], irOut)
        else:
            # Extract w component which is i0j0 texel from each gather4 result, and construct the return value
            tempRetVal = "undef"
            for i in range(callNum):
                tempComp   = self.acquireLocalVar()
                irExtract  = "    %s = extractelement %s %s, i32 3\n" % (tempComp, retType, retVals[i])
                irOut.write(irExtract)
                retVal = self.acquireLocalVar()
                irInsert   = "    %s = insertelement %s %s, %s %s, i32 %d\n" % (retVal, retType, tempRetVal, retElemType, tempComp, i)
                tempRetVal = retVal
                irOut.write(irInsert)
            retVal = self.genCastReturnValToSampledType(retVal, irOut)

        return retVal

    # Gets image sample intrinsic function name
    def getFuncName(self):
        funcName = "llvm.amdgcn"
        if self._opKind == SpirvImageOpKind.sample:
            funcName += ".image.sample"
        elif self._opKind == SpirvImageOpKind.gather:
            funcName += ".image.gather4"
        elif self._opKind == SpirvImageOpKind.querylod:
            funcName += ".image.getlod"
        else:
            shouldNeverCall()

        if self._hasDref:
            funcName += ".c"

        if self._opKind == SpirvImageOpKind.gather and not self._hasLod and not self._hasBias:
            funcName += ".lz"

        if self._hasBias:
            funcName += ".b"
        elif self._hasLod and not self._supportLzOptimization:
            funcName += ".l"
        elif self._hasLod and self._supportLzOptimization:
            funcName += ".lz"
        elif self._hasGrad:
            funcName += ".d"

        if self._hasMinLod:
            funcName += ".cl"

        if self._hasConstOffset or self._hasOffset or self._hasConstOffsets:
            funcName += ".o"

        if self._sampledType == SpirvSampledType.f16:
            funcName += ".v4f16"
        else:
            funcName += ".v4f32"
        vaddrRegSize = self.getVAddrRegSize()
        funcName += vaddrRegSize == 1 and ".f32" or ".v%df32" % (vaddrRegSize)
        funcName += ".v8i32"

        if not hasLlvmDecl(funcName):
            # Adds LLVM declaration for this function
            funcExternalDecl = "declare %s @%s(%s, <8 x i32>, <4 x i32>, i32, i1, i1, i1, i1, i1) %s\n" % \
                               (self.getBackendRetType(), funcName, self.getVAddrRegType(), \
                                self._attr)
            addLlvmDecl(funcName, funcExternalDecl)
        return funcName

# Processess a mangled function configuration.
def processLine(irOut, funcConfig, gfxLevel):
    # A mangled function configuration looks like:
    # llpc.image.sample.Dim1D|2D|3D|Rect.proj.dref.bias.constoffset
    # Supported configuration tokens:   (All tokens must follow this order)
    # 0.  llpc                                              (mandatory)
    # 1.  image or image|sparse                             (mandatory)
    #         image|sparse means sparse instruction is supported for this function, an additional sparse version
    #         will be generated.
    # 2.  One of:                                           (mandatory)
    #       sample
    #       fetch
    #       gather
    #       querylod
    #       read
    #       write
    #       atomicexchange
    #       atomiccompExchange
    #       atomiciincrement
    #       atomicidecrement
    #       atomiciadd
    #       atomicisub
    #       atomicsmin
    #       atomicumin
    #       atomicsmax
    #       atomicumax
    #       atomicand
    #       atomicor
    #       atomicxor
    # 3.  Dimension string                                  (mandatory, see below)
    # 4.  proj                                              (optional)
    # 5.  dref                                              (optional)
    # 6.  bias                                              (optional)
    # 7.  lod or lod|lodz                                   (optional)
    #         lod|lodz means lz optimization is enabled for this function, besides normal lod version, an additional
    #         lodz version will also be generated, which leverages hardware lz instructions.
    # 8.  grad                                              (optional)
    # 9.  constoffset                                       (optional)
    # 10. offset                                            (optional)
    # 11. constoffsets                                      (optional)
    # 12. sample                                            (optional)
    # 13. minlod                                            (optional)
    # 14. fmaskbased                                        (optional)
    # 15. fmaskid                                           (optional)
    #         returns index of fragment which is encoded in Fmask.
    # Dimension string: All supported dimensions are packed in a dimension string, as a configuration token.
    # Dimension string format:
    # Dim1D|2D|3D|1DArray|2DArray|Cube|CubeArray|Rect|Buffer|SubpassData

    print(">>>  %s" % (funcConfig))

    mangledTokens  = funcConfig.split('.')
    # check token 0
    assert funcConfig.startswith(SPIRV_IMAGE_PREFIX), "Error prefix: " + funcConfig

    # check token 1
    assert mangledTokens[1] in (SPIRV_IMAGE_MODIFIER, SPIRV_IMAGE_MODIFIER + '|' + SPIRV_IMAGE_SPARSE_MODIFIER), \
            "Error image modifier" + funcConfig

    # Extract dimensions from dimension string
    dimString = mangledTokens[3]
    assert dimString.startswith(SPIRV_IMAGE_DIM_PREFIX), "" + dimString
    dims = dimString[3:].split('|')
    opKind = SPIRV_IMAGE_INST_KIND_DICT[mangledTokens[2]]

    # Generate function definition for each dimension
    for dim in dims:
        mangledTokens[3] = SPIRV_IMAGE_DIM_PREFIX + dim
        mangledName = '.'.join(mangledTokens)
        if opKind in (SpirvImageOpKind.sample, SpirvImageOpKind.fetch, SpirvImageOpKind.gather, \
                      SpirvImageOpKind.querylod, SpirvImageOpKind.read, SpirvImageOpKind.write, \
                      SpirvImageOpKind.atomicexchange):
            # Support float return type for all non-atomic image operations, plus atomicexchange
            funcDef = FuncDef(mangledName, SpirvSampledType.f32)
            funcDef.parse()
            codeGen = CodeGen(funcDef, gfxLevel)
            codeGen.gen(irOut)

            # Support float16 return type for image sample, gather, read, write operations for GFX9+
            if gfxLevel >= GFX9 and opKind in (SpirvImageOpKind.sample, \
                                               SpirvImageOpKind.fetch, \
                                               SpirvImageOpKind.gather):
                funcDef = FuncDef(mangledName, SpirvSampledType.f16)
                funcDef.parse()
                if not funcDef._returnFmaskId:
                    codeGen = CodeGen(funcDef, gfxLevel)
                    codeGen.gen(irOut)

        if funcConfig.find('dref') == -1 and opKind != SpirvImageOpKind.querylod:
            # Support integer return type for non-shadowed image operations
            funcDef = FuncDef(mangledName, SpirvSampledType.i32)
            funcDef.parse()
            codeGen = CodeGen(funcDef, gfxLevel)
            codeGen.gen(irOut)
            funcDef = FuncDef(mangledName, SpirvSampledType.u32)
            funcDef.parse()
            codeGen = CodeGen(funcDef, gfxLevel)
            codeGen.gen(irOut)
    pass

# Adds a LLVM function declaration.
def addLlvmDecl(n, d):
    global LLVM_DECLS
    LLVM_DECLS[n] = d
    pass

# Whether a LLVM function declaration has been added.
def hasLlvmDecl(n):
    return n in LLVM_DECLS

# Outputs LLVM function declarations.
def outputLlvmDecls(irOut):
    items = list(LLVM_DECLS.items())
    items.sort()
    for i in items:
        irOut.write("%s\n" % (i[1]))
    pass

# Outputs LLVM metadata.
def outputLlvmMetadata(irOut):
    irOut.write("!0 = !{float 2.5}\n")
    pass

# Outputs LLVM attributes.
def outputLlvmAttributes(irOut):
    for i in range(len(LLVM_ATTRIBUTES)):
        irOut.write("%s\n" % (LLVM_ATTRIBUTES[i]))
    pass

def outputPreDefinedFunctions(irOut):
    transformCubeGrad = """\
define <4 x float> @llpc.image.transformCubeGrad(float %cubeId, float %cubeMa, float %faceCoordX, float %faceCoordY, float %gradX.x, float %gradX.y, float %gradX.z, float %gradY.x, float %gradY.y, float %gradY.z) #0
{
    ; When sampling cubemap with explicit gradient value, API supplied gradients are cube vectors,
    ; need to transform them to face gradients for the selected face.
    ; Mapping of MajorAxis, U-Axis, V-Axis is (according to DXSDK doc and refrast):
    ;   face_id | MajorAxis | FaceUAxis | FaceVAxis
    ;   0       | +X        | -Z        | -Y
    ;   1       | -X        | +Z        | -Y
    ;   2       | +Y        | +X        | +Z
    ;   3       | -Y        | +X        | -Z
    ;   4       | +Z        | +X        | -Y
    ;   5       | -Z        | -X        | -Y
    ;   (Major Axis is defined by enum D3D11_TEXTURECUBE_FACE in d3d ddk header file (d3d11.h in DX11DDK).)
    ;
    ; Parameters used to convert cube gradient vector to face gradient (face ids are in floats because HW returns floats):
    ;   face_id | faceidPos    | faceNeg   | flipU | flipV
    ;   0.0     | 0.0          | false     | true  | true
    ;   1.0     | 0.0          | true      | false | true
    ;   2.0     | 1.0          | false     | false | false
    ;   3.0     | 1.0          | true      | false | true
    ;   4.0     | 2.0          | false     | false | true
    ;   5.0     | 2.0          | true      | true  | true

    ; faceidHalf = faceid * 0.5
    %1 = fmul float %cubeId, 0.5
    ; faceidPos = round_zero(faceidHalf)
    ;   faceidPos is: 0.0 (X axis) when face id is 0.0 or 1.0;
    ;                 1.0 (Y axis) when face id is 2.0 or 3.0;
    ;                 2.0 (Z axis) when face id is 4.0 or 5.0;
    %2 = call float @llvm.trunc.f32(float %1)
    ; faceNeg = (faceIdPos != faceIdHalf)
    ;   faceNeg is true when major axis is negative, this corresponds to             face id being 1.0, 3.0, or 5.0
    %3 = fcmp one float %2, %1
    ; faceIsY = (faceidPos == 1.0);
    %4 = fcmp oeq float %2, 1.0
    ; flipU is true when U-axis is negative, this corresponds to face id being 0.0 or 5.0.
    %5 = fcmp oeq float %cubeId, 5.0
    %6 = fcmp oeq float %cubeId, 0.0
    %7 = or i1 %5, %6
    ; flipV is true when V-axis is negative, this corresponds to face id being             anything other than 2.0.
    ; flipV = (faceid != 2.0);
    %8 = fcmp one float %cubeId, 2.0
    ; major2.x = 1/major.x * 1/major.x * 0.5;
    ;          = 1/(2*major.x) * 1/(2*major.x) * 2
    %9 = fdiv float 1.0, %cubeMa
    %10 = fmul float %9, %9
    %11 = fmul float %10, 2.0
    ; majorDeriv.x = (faceidPos == 0.0) ? grad.x : grad.z;
    %12 = fcmp oeq float %2, 0.0
    %13 = select i1 %12, float %gradX.x, float %gradX.z
    ; majorDeriv.x = (faceIsY == 0) ? majorDeriv.x : grad.y;
    %14 = icmp eq i1 %4, 0
    %15 = select i1 %14, float %13, float %gradX.y
    ; majorDeriv.x = (faceNeg == 0.0) ? majorDeriv.x : (-majorDeriv.x);
    %16 = icmp eq i1 %3, 0
    %17 = fmul float %15, -1.0
    %18 = select i1 %16, float %15, float %17
    ; faceDeriv.x = (faceidPos == 0.0) ? grad.z : grad.x;
    %19 = fcmp oeq float %2, 0.0
    %20 = select i1 %19, float %gradX.z, float %gradX.x
    ; faceDeriv.x = (flipU == 0) ? faceDeriv.x : (-faceDeriv.x);
    %21 = icmp eq i1 %7, 0
    %22 = fmul float %20, -1.0
    %23 = select i1 %21, float %20, float %22
    ; faceDeriv.y = (faceIsY == 0) ? grad.y : grad.z;
    %24 = icmp eq i1 %4, 0
    %25 = select i1 %24, float %gradX.y, float %gradX.z
    ; faceDeriv.y = (flipV == 0) ? faceDeriv.y : (-faceDeriv.y);
    %26 = icmp eq i1 %8, 0
    %27 = fmul float %25, -1.0
    %28 = select i1 %26, float %25, float %27
    ; faceDeriv.xy = major.xx * faceDeriv.xy;
    %29 = fmul float %cubeMa, 0.5
    %30 = fmul float %23, %29
    %31 = fmul float %28, %29
    ; faceDeriv.xy = (-faceCrd.xy) * majorDeriv.xx + faceDeriv.xy;
    %32 = fmul float %faceCoordX, -1.0
    %33 = fmul float %faceCoordY, -1.0
    %34 = fmul float %32, %18
    %35 = fmul float %33, %18
    %36 = fadd float %34, %30
    %37 = fadd float %34, %31
    ; grad.xy = faceDeriv.xy * major2.xx;
    %38 = fmul float %36, %11
    %39 = fmul float %37, %11
    ; majorDeriv.x = (faceidPos == 0.0) ? grad.x : grad.z;
    %40 = fcmp oeq float %2, 0.0
    %41 = select i1 %40, float %gradY.x, float %gradY.z
    ; majorDeriv.x = (faceIsY == 0) ? majorDeriv.x : grad.y;
    %42 = icmp eq i1 %4, 0
    %43 = select i1 %42, float %41, float %gradY.y
    ; majorDeriv.x = (faceNeg == 0.0) ? majorDeriv.x : (-majorDeriv.x);
    %44 = icmp eq i1 %3, 0
    %45 = fmul float %43, -1.0
    %46 = select i1 %44, float %43, float %45
    ; faceDeriv.x = (faceidPos == 0.0) ? grad.z : grad.x;
    %47 = fcmp oeq float %2, 0.0
    %48 = select i1 %47, float %gradY.z, float %gradY.x
    ; faceDeriv.x = (flipU == 0) ? faceDeriv.x : (-faceDeriv.x);
    %49 = icmp eq i1 %7, 0
    %50 = fmul float %48, -1.0
    %51 = select i1 %49, float %48, float %50
    ; faceDeriv.y = (faceIsY == 0) ? grad.y : grad.z;
    %52 = icmp eq i1 %4, 0
    %53 = select i1 %52, float %gradY.y, float %gradY.z
    ; faceDeriv.y = (flipV == 0) ? faceDeriv.y : (-faceDeriv.y);
    %54 = icmp eq i1 %8, 0
    %55 = fmul float %53, -1.0
    %56 = select i1 %54, float %53, float %55
    ; faceDeriv.xy = major.xx * faceDeriv.xy;
    %57 = fmul float %cubeMa, 0.5
    %58 = fmul float %51, %57
    %59 = fmul float %56, %57
    ; faceDeriv.xy = (-faceCrd.xy) * majorDeriv.xx + faceDeriv.xy;
    %60 = fmul float %faceCoordX, -1.0
    %61 = fmul float %faceCoordY, -1.0
    %62 = fmul float %60, %46
    %63 = fmul float %61, %46
    %64 = fadd float %62, %58
    %65 = fadd float %62, %59
    ; grad.xy = faceDeriv.xy * major2.xx;
    %66 = fmul float %64, %11
    %67 = fmul float %65, %11
    %68 = insertelement <4 x float> undef, float %38, i32 0
    %69 = insertelement <4 x float> %68, float %39, i32 1
    %70 = insertelement <4 x float> %69, float %66, i32 2
    %71 = insertelement <4 x float> %70, float %67, i32 3
    ret <4 x float> %71
}

"""
    irOut.write(transformCubeGrad)

# Outputs header contents.
def outputLlvmHeader(irOut):
    header = """\
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
;* @brief LLVM IR file: contains emulation codes for GLSL image operations.
;*
;* @note  This file has been generated automatically. Do not hand-modify this file. When changes are needed, modify the
;*        generating script genGlslImageOpEmuCode.py.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-\
v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

"""
    irOut.write(header.format(irOut.name))
    pass

# Processes each listed image function configuration
def processList(irOut, listIn, gfxLevel):
    print("===  Process list: " + os.path.split(listIn)[1])

    fList = open(listIn, 'rt')
    for line in fList:
        line = line.strip()
        isComment = line.startswith('#')
        isEmpty = len(line.strip()) == 0
        if not isComment and not isEmpty:
            processLine(irOut, line, gfxLevel)
    fList.close()

    print("")
    pass

# Initializes necessary LLVM function declarations.
def initLlvmDecls(gfxLevel):
    addLlvmDecl(LLPC_DESCRIPTOR_LOAD_SAMPLER, "declare <4 x i32> @%s(i32 , i32 , i32) #0\n" % (\
            LLPC_DESCRIPTOR_LOAD_SAMPLER))
    addLlvmDecl(LLPC_DESCRIPTOR_LOAD_RESOURCE, "declare <8 x i32> @%s(i32 , i32 , i32) #0\n" % (\
            LLPC_DESCRIPTOR_LOAD_RESOURCE))
    addLlvmDecl(LLPC_DESCRIPTOR_LOAD_TEXELBUFFER, "declare <4 x i32> @%s(i32 , i32 , i32) #0\n" % (\
            LLPC_DESCRIPTOR_LOAD_TEXELBUFFER))
    addLlvmDecl(LLPC_DESCRIPTOR_LOAD_FMASK, "declare <8 x i32> @%s(i32 , i32 , i32) #0\n" % (\
            LLPC_DESCRIPTOR_LOAD_FMASK))
    addLlvmDecl("llvm.amdgcn.cubetc", "declare float @llvm.amdgcn.cubetc(float, float, float) #0\n")
    addLlvmDecl("llvm.amdgcn.cubesc", "declare float @llvm.amdgcn.cubesc(float, float, float) #0\n")
    addLlvmDecl("llvm.amdgcn.cubema", "declare float @llvm.amdgcn.cubema(float, float, float) #0\n")
    addLlvmDecl("llvm.amdgcn.cubeid", "declare float @llvm.amdgcn.cubeid(float, float, float) #0\n")
    addLlvmDecl("llvm.fabs.f32", "declare float @llvm.fabs.f32(float) #0\n")
    addLlvmDecl("llvm.rint.f32", "declare float @llvm.rint.f32(float) #0\n")
    addLlvmDecl("llpc.round.f32", "declare float @llpc.round.f32(float) #0\n")
    addLlvmDecl("llvm.trunc.f32", "declare float @llvm.trunc.f32(float) #0\n")
    addLlvmDecl(LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE, "declare <8 x i32> @%s(<8 x i32>) #0\n" % (\
            LLPC_PATCH_IMAGE_READWRITEATOMIC_DESCRIPTOR_CUBE))
    if gfxLevel < GFX9:
        addLlvmDecl("llpc.patch.image.gather.descriptor.u32",
                    "declare <8 x i32> @llpc.patch.image.gather.descriptor.u32(<8 x i32>) #0\n")
        addLlvmDecl("llpc.patch.image.gather.descriptor.i32",
                    "declare <8 x i32> @llpc.patch.image.gather.descriptor.i32(<8 x i32>) #0\n")
        addLlvmDecl("llpc.patch.image.gather.texel.u32",
                    "declare <4 x float> @llpc.patch.image.gather.texel.u32(<8 x i32>, <4 x float>) #0\n")
        addLlvmDecl("llpc.patch.image.gather.texel.i32",
                    "declare <4 x float> @llpc.patch.image.gather.texel.i32(<8 x i32>, <4 x float>) #0\n")

def main(inFile, outFile, gfxLevelStr):
    gfxLevel = float(gfxLevelStr[3:])
    irOut = open(outFile, 'wt')

    initLlvmDecls(gfxLevel)
    outputLlvmHeader(irOut)
    outputPreDefinedFunctions(irOut)

    processList(irOut, inFile, gfxLevel)

    outputLlvmDecls(irOut)
    outputLlvmAttributes(irOut)
    outputLlvmMetadata(irOut)
    irOut.close()

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
