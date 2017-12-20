/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  llpcInternal.cpp
 * @brief LLPC source file: contains implementation of LLPC internal-use utility functions.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-internal"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_os_ostream.h"
#include "spirv.hpp"

    #include <sys/stat.h>
    #include <time.h>
    #include <unistd.h>

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcDebug.h"
#include "llpcElf.h"
#include "llpcInternal.h"

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// Gets the entry point (valid for AMD GPU) of a LLVM module.
Function* GetEntryPoint(
    Module* pModule) // [in] LLVM module
{
    Function* pEntryPoint = nullptr;

    for (auto pFunc = pModule->begin(), pEnd = pModule->end(); pFunc != pEnd; ++pFunc)
    {
        const auto callConv = pFunc->getCallingConv();
        if ((callConv == CallingConv::AMDGPU_VS) ||
            (callConv == CallingConv::AMDGPU_HS) ||
            (callConv == CallingConv::AMDGPU_GS) ||
            (callConv == CallingConv::AMDGPU_PS) ||
            (callConv == CallingConv::AMDGPU_CS))
        {
            pEntryPoint = &*pFunc;
            break;
        }
    }

    LLPC_ASSERT(pEntryPoint != nullptr);
    return pEntryPoint;
}

// =====================================================================================================================
// Emits a LLVM function call (inserted before the specified instruction), builds it automically based on return type
// and its parameters.
Value* EmitCall(
    Module*                       pModule,          // [in] LLVM module
    StringRef                     funcName,         // Name string of the function
    Type*                         pRetTy,           // [in] Return type
    ArrayRef<Value *>             args,             // [in] Parameters
    ArrayRef<Attribute::AttrKind> attribs,          // Attributes
    Instruction*                  pInsertPos)       // [in] Where to insert this call
{
    Function* pFunc = dyn_cast_or_null<Function>(pModule->getFunction(funcName));
    if (pFunc == nullptr)
    {
        std::vector<Type*> argTys;
        for (auto arg : args)
        {
            argTys.push_back(arg->getType());
        }

        auto pFuncTy = FunctionType::get(pRetTy, argTys, false);
        pFunc = Function::Create(pFuncTy, GlobalValue::ExternalLinkage, funcName, pModule);

        pFunc->setCallingConv(CallingConv::C);
        pFunc->addFnAttr(Attribute::NoUnwind);

        for (auto attrib : attribs)
        {
            pFunc->addFnAttr(attrib);
        }
    }

    auto pCallInst = CallInst::Create(pFunc, args, "", pInsertPos);
    pCallInst->setCallingConv(CallingConv::C);
    pCallInst->setAttributes(pFunc->getAttributes());

    return pCallInst;
}

// =====================================================================================================================
// Emits a LLVM function call (inserted at the end of the specified basic block), builds it automically based on return
// type and its parameters.
Value* EmitCall(
    Module*                       pModule,          // [in] LLVM module
    StringRef                     funcName,         // Name string of the function
    Type*                         pRetTy,           // [in] Return type
    ArrayRef<Value *>             args,             // [in] Parameters
    ArrayRef<Attribute::AttrKind> attribs,          // Attributes
    BasicBlock*                   pInsertAtEnd)     // [in] Which block to insert this call at the end
{
    Function* pFunc = dyn_cast_or_null<Function>(pModule->getFunction(funcName));
    if (pFunc == nullptr)
    {
        std::vector<Type*> argTys;
        for (auto arg : args)
        {
            argTys.push_back(arg->getType());
        }

        auto pFuncTy = FunctionType::get(pRetTy, argTys, false);
        pFunc = Function::Create(pFuncTy, GlobalValue::ExternalLinkage, funcName, pModule);

        pFunc->setCallingConv(CallingConv::C);
        pFunc->addFnAttr(Attribute::NoUnwind);

        for (auto attrib : attribs)
        {
            pFunc->addFnAttr(attrib);
        }
    }

    auto pCallInst = CallInst::Create(pFunc, args, "", pInsertAtEnd);
    pCallInst->setCallingConv(CallingConv::C);
    pCallInst->setAttributes(pFunc->getAttributes());

    return pCallInst;
}

// =====================================================================================================================
// Gets LLVM-style name for scalar or vector type.
std::string GetTypeNameForScalarOrVector(
    Type* pTy)  // [in] Type to get mangle name
{
    LLPC_ASSERT(pTy->isSingleValueType());

    std::string name;
    raw_string_ostream nameStream(name);

    const Type* pScalarTy = pTy->getScalarType();
    LLPC_ASSERT(pScalarTy->isFloatingPointTy() || pScalarTy->isIntegerTy());

    if (pTy->isVectorTy())
    {
        nameStream << "v" << pTy->getVectorNumElements();
    }

    nameStream << (pScalarTy->isFloatingPointTy() ? "f" : "i") << pScalarTy->getScalarSizeInBits();
    return nameStream.str();
}

// =====================================================================================================================
// Gets the shader stage from the specified LLVM module.
ShaderStage GetShaderStageFromModule(
    Module* pModule)  // [in] LLVM module
{
    ShaderStage stage = ShaderStageInvalid;

    Function* pEntryPoint = GetEntryPoint(pModule);
    MDNode* pExecModelNode = pEntryPoint->getMetadata(gSPIRVMD::ExecutionModel);
    LLPC_ASSERT(pExecModelNode != nullptr);
    auto execModel = mdconst::dyn_extract<ConstantInt>(pExecModelNode->getOperand(0))->getZExtValue();

    switch (execModel)
    {
    case ExecutionModelVertex:
        stage = ShaderStageVertex;
        break;
    case ExecutionModelTessellationControl:
        stage = ShaderStageTessControl;
        break;
    case ExecutionModelTessellationEvaluation:
        stage = ShaderStageTessEval;
        break;
    case ExecutionModelGeometry:
        stage = ShaderStageGeometry;
        break;
    case ExecutionModelFragment:
        stage = ShaderStageFragment;
        break;
    case ExecutionModelGLCompute:
        stage = ShaderStageCompute;
        break;
    case ExecutionModelCopyShader:
        stage = ShaderStageCopyShader;
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }
    return stage;
}

// =====================================================================================================================
// Gets the name string of shader stage.
const char* GetShaderStageName(
    ShaderStage shaderStage)  // Shader stage
{
    const char* pName = nullptr;

    if (shaderStage == ShaderStageCopyShader)
    {
        pName = "copy";
    }
    else
    {
        LLPC_ASSERT(shaderStage < ShaderStageCount);

        static const char* ShaderStageNames[] =
        {
            "vertex",
            "tessellation control",
            "tessellation evaluation",
            "geometry",
            "fragment",
            "compute",
        };

        pName = ShaderStageNames[static_cast<uint32_t>(shaderStage)];
    }

    return pName;
}

// =====================================================================================================================
// Gets name string of the abbreviation for the specified shader stage
const char* GetShaderStageAbbreviation(
    ShaderStage shaderStage,  // Shader stage
    bool        upper)        // Whether to use uppercase for the abbreviation (default is lowercase)
{
    const char* pAbbr = nullptr;

    if (shaderStage == ShaderStageCopyShader)
    {
        pAbbr = upper ? "COPY" : "Copy";
    }
    else
    {
        LLPC_ASSERT(shaderStage < ShaderStageCount);

        if (upper)
        {
            static const char* ShaderStageAbbrs[] =
            {
                "VS",
                "TCS",
                "TES",
                "GS",
                "FS",
                "CS",
            };

            pAbbr = ShaderStageAbbrs[static_cast<uint32_t>(shaderStage)];
        }
        else
        {
            static const char* ShaderStageAbbrs[] =
            {
                "Vs",
                "Tcs",
                "Tes",
                "Gs",
                "Fs",
                "Cs",
            };

            pAbbr = ShaderStageAbbrs[static_cast<uint32_t>(shaderStage)];
        }
    }

    return pAbbr;
}

// =====================================================================================================================
// Gets the argument from the specified function according to the argument index.
Value* GetFunctionArgument(
    Function* pFunc,    // [in] LLVM function
    uint32_t  idx)      // Index of the query argument
{
    auto pArg = pFunc->arg_begin();
    while (idx-- > 0)
    {
        ++pArg;
    }
    return &*pArg;
}

// =====================================================================================================================
// Checks if one type can be bitcasted to the other (type1 -> type2, valid for scalar or vector type).
bool CanBitCast(
    const Type* pTy1,   // [in] One type
    const Type* pTy2)   // [in] The other type
{
    bool valid = false;

    if (pTy1 == pTy2)
    {
        valid = true;
    }
    else if (pTy1->isSingleValueType() && pTy2->isSingleValueType())
    {
        const Type* pCompTy1 = pTy1->isVectorTy() ? pTy1->getVectorElementType() : pTy1;
        const Type* pCompTy2 = pTy2->isVectorTy() ? pTy2->getVectorElementType() : pTy2;
        if ((pCompTy1->isFloatingPointTy() || pCompTy1->isIntegerTy()) &&
            (pCompTy2->isFloatingPointTy() || pCompTy2->isIntegerTy()))
        {
            const uint32_t compCount1 = pTy1->isVectorTy() ? pTy1->getVectorNumElements() : 1;
            const uint32_t compCount2 = pTy2->isVectorTy() ? pTy2->getVectorNumElements() : 1;

            valid = (compCount1 * pCompTy1->getScalarSizeInBits() == compCount2 * pCompTy2->getScalarSizeInBits());
        }
    }

    return valid;
}

// =====================================================================================================================
// Gets the symbol name for .text section.
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForTextSection(
    ShaderStage stage,      // Shader stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::LsMainEntry)];
        }
        else if (hasGs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::EsMainEntry)];
        }
        else
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        }
        break;
    case ShaderStageTessControl:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
           static_cast<uint32_t>(Util::Abi::PipelineSymbolType::HsMainEntry)];
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::EsMainEntry)];
        }
        else
        {
            pName = Util::Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        }
        break;
    case ShaderStageGeometry:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
           static_cast<uint32_t>(Util::Abi::PipelineSymbolType::GsMainEntry)];
        break;
    case ShaderStageFragment:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::PsMainEntry)];
        break;
    case ShaderStageCompute:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::CsMainEntry)];
        break;
    case ShaderStageCopyShader:
        pName = Util::Abi::PipelineAbiSymbolNameStrings[
            static_cast<uint32_t>(Util::Abi::PipelineSymbolType::VsMainEntry)];
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// =====================================================================================================================
// Gets the symbol name for .AMDGPU.disasm section.
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForDisasmSection(
    ShaderStage stage,      // Shader Stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = DebugSymNames::LsDisasm;
        }
        else if (hasGs)
        {
            pName = DebugSymNames::EsDisasm;
        }
        else
        {
            pName = DebugSymNames::VsDisasm;
        }
        break;
    case ShaderStageTessControl:
        pName = DebugSymNames::HsDisasm;
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = DebugSymNames::EsDisasm;
        }
        else
        {
            pName = DebugSymNames::VsDisasm;
        }
        break;
    case ShaderStageGeometry:
        pName = DebugSymNames::GsDisasm;
        break;
    case ShaderStageFragment:
        pName = DebugSymNames::PsDisasm;
        break;
    case ShaderStageCompute:
        pName = DebugSymNames::CsDisasm;
        break;
    case ShaderStageCopyShader:
        pName = DebugSymNames::VsDisasm;
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// =====================================================================================================================
// Gets the symbol name for .AMDGPU.csdata section
//
// TODO: We need to use hardware shader stage here.
const char* GetSymbolNameForCsdataSection(
    ShaderStage stage,      // Shader stage
    uint32_t    stageMask)  // Mask of active shader stages
{
    const char* pName = nullptr;

    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    switch (stage)
    {
    case ShaderStageVertex:
        if (hasTs)
        {
            pName = DebugSymNames::LsCsdata;
        }
        else if (hasGs)
        {
            pName = DebugSymNames::EsCsdata;
        }
        else
        {
            pName = DebugSymNames::VsCsdata;
        }
        break;
    case ShaderStageTessControl:
        pName = DebugSymNames::HsCsdata;
        break;
    case ShaderStageTessEval:
        if (hasGs)
        {
            pName = DebugSymNames::EsCsdata;
        }
        else
        {
            pName = DebugSymNames::VsCsdata;
        }
        break;
    case ShaderStageGeometry:
        pName = DebugSymNames::GsCsdata;
        break;
    case ShaderStageFragment:
        pName = DebugSymNames::PsCsdata;
        break;
    case ShaderStageCompute:
        pName = DebugSymNames::CsCsdata;
        break;
    case ShaderStageCopyShader:
        pName = DebugSymNames::VsCsdata;
        break;
    default:
        LLPC_NOT_IMPLEMENTED();
        break;
    }

    return pName;
}

// Represents the special header of SPIR-V token stream (the first DWORD).
struct SpirvHeader
{
    uint32_t    magicNumber;        // Magic number of SPIR-V module
    uint32_t    spvVersion;         // SPIR-V version number
    uint32_t    genMagicNumber;     // Generator's magic number
    uint32_t    idBound;            // Upbound (X) of all IDs used in SPIR-V (0 < ID < X)
    uint32_t    reserved;           // Reserved word
};

// =====================================================================================================================
// Checks whether input binary data is SPIR-V binary
bool IsSpirvBinary(
    const BinaryData*  pShaderBin)  // [in] Shader binary codes
{
    bool isSpvBinary = false;
    if (pShaderBin->codeSize > sizeof(SpirvHeader))
    {
        const SpirvHeader* pHeader = reinterpret_cast<const SpirvHeader*>(pShaderBin->pCode);
        if ((pHeader->magicNumber == MagicNumber) && (pHeader->spvVersion <= spv::Version) && (pHeader->reserved == 0))
        {
            isSpvBinary = true;
        }
    }

    return isSpvBinary;
}

// =====================================================================================================================
// Checks whether input binary data is LLVM bitcode.
bool IsLlvmBitcode(
    const BinaryData*  pShaderBin)  // [in] Shader binary codes
{
    static uint32_t BitcodeMagicNumber = 0xDEC04342; // 0x42, 0x43, 0xC0, 0xDE
    bool isLlvmBitcode = false;
    if ((pShaderBin->codeSize > 4) &&
        (*reinterpret_cast<const uint32_t*>(pShaderBin->pCode) == BitcodeMagicNumber))
    {
        isLlvmBitcode = true;
    }

    return isLlvmBitcode;
}

// =====================================================================================================================
// Gets the shader stage mask from the SPIR-V binary according to the specified entry-point.
//
// Returns 0 on error, or the stage mask of the specified entry-point on success.
uint32_t GetStageMaskFromSpirvBinary(
    const BinaryData*  pSpvBin,      // [in] SPIR-V binary
    const char*        pEntryName)   // [in] Name of entry-point
{
    uint32_t stageMask = 0;

    const uint32_t* pCode = reinterpret_cast<const uint32_t*>(pSpvBin->pCode);
    const uint32_t* pEnd  = pCode + pSpvBin->codeSize / sizeof(uint32_t);

    if (IsSpirvBinary(pSpvBin))
    {
        // Skip SPIR-V header
        const uint32_t* pCodePos = pCode + sizeof(SpirvHeader) / sizeof(uint32_t);

        while (pCodePos < pEnd)
        {
            uint32_t opCode = (pCodePos[0] & OpCodeMask);
            uint32_t wordCount = (pCodePos[0] >> WordCountShift);

            if ((wordCount == 0) || (pCodePos + wordCount > pEnd))
            {
                LLPC_ERRS("Invalid SPIR-V binary\n");
                stageMask = 0;
                break;
            }

            if (opCode == OpEntryPoint)
            {
                LLPC_ASSERT(wordCount >= 4);

                // The fourth word is start of the name string of the entry-point
                const char* pName = reinterpret_cast<const char*>(&pCodePos[3]);
                if (strcmp(pEntryName, pName) == 0)
                {
                    // An matching entry-point is found
                    stageMask |= ShaderStageToMask(static_cast<ShaderStage>(pCodePos[1]));
                }
            }

            // All "OpEntryPoint" are before "OpFunction"
            if ((opCode == OpFunction))
            {
                break;
            }

            pCodePos += wordCount;
        }
    }
    else
    {
        LLPC_ERRS("Invalid SPIR-V binary\n");
    }

    return stageMask;
}

// =====================================================================================================================
// Verifies if the SPIR-V binary is valid and is supported
Result VerifySpirvBinary(
    const BinaryData* pSpvBin)  // [in] SPIR-V binary
{
    Result result = Result::Success;

#define _SPIRV_OP(x,...) Op##x,
    static const std::set<Op> OpSet{
       {
        #include "SPIRVOpCodeEnum.h"
       }
    };
#undef _SPIRV_OP

    const uint32_t* pCode = reinterpret_cast<const uint32_t*>(pSpvBin->pCode);
    const uint32_t* pEnd  = pCode + pSpvBin->codeSize / sizeof(uint32_t);

    // Skip SPIR-V header
    const uint32_t* pCodePos = pCode + sizeof(SpirvHeader) / sizeof(uint32_t);

    while (pCodePos < pEnd)
    {
        Op opCode = static_cast<Op>(pCodePos[0] & OpCodeMask);
        uint32_t wordCount = (pCodePos[0] >> WordCountShift);

        if ((wordCount == 0) || (pCodePos + wordCount > pEnd))
        {
            result = Result::ErrorInvalidShader;
            break;
        }

        if (OpSet.find(opCode) == OpSet.end())
        {
            result = Result::ErrorInvalidShader;
            break;
        }

        pCodePos += wordCount;
    }

    return result;
}

// =====================================================================================================================
// Translates shader stage to corresponding stage mask.
uint32_t ShaderStageToMask(
    ShaderStage stage)  // Shader stage
{
    LLPC_ASSERT((stage < ShaderStageCount) || (stage == ShaderStageCopyShader));
    return (1 << stage);
}

// =====================================================================================================================
// Checks if the specified value actually represents a don't-care value (0xFFFFFFFF).
bool IsDontCareValue(
    Value* pValue) // [in] Value to check
{
    bool isDontCare = false;

    if (isa<ConstantInt>(pValue))
    {
        isDontCare = (static_cast<uint32_t>(cast<ConstantInt>(pValue)->getZExtValue()) == InvalidValue);
    }

    return isDontCare;
}

// =====================================================================================================================
// Translates an integer to 32-bit integer regardless of its initial bit width.
Value* ToInt32Value(
    Context*     pContext,   // [in] LLPC context
    Value*       pValue,     // [in] Value to be translated
    Instruction* pInsertPos) // [in] Where to insert the translation instructions
{
    LLPC_ASSERT(isa<IntegerType>(pValue->getType()));
    auto pValueTy = cast<IntegerType>(pValue->getType());

    const uint32_t bitWidth = pValueTy->getBitWidth();
    if (bitWidth > 32)
    {
        // Truncated to i32 type
        pValue = CastInst::CreateTruncOrBitCast(pValue, pContext->Int32Ty(), "", pInsertPos);
    }
    else if (bitWidth < 32)
    {
        // Extended to i32 type
        pValue = CastInst::CreateZExtOrBitCast(pValue, pContext->Int32Ty(), "", pInsertPos);
    }

    return pValue;
}

// =====================================================================================================================
// Retrieves the frequency of the performance counter for CPU times.
//
// NOTE: In current implementation, the tick has been set to 1 ns.
int64_t GetPerfFrequency()
{
    constexpr uint32_t NanosecsPerSec = (1000 * 1000 * 1000);

    return NanosecsPerSec;
}

// =====================================================================================================================
// Retrieves the current value of the performance counter, which is a high resolution time stamp that can be used for
// time-interval measurements.
int64_t GetPerfCpuTime()
{
    int64_t time = 0LL;

    // clock_gettime() returns the monotonic time since EPOCH
    timespec timeSpec = { };
    if (clock_gettime(CLOCK_MONOTONIC, &timeSpec) == 0)
    {
        // NOTE: The tick takes 1 ns since we actually can hardly get timer with resolution less than 1 ns.
        constexpr int64_t NanosecsPerSec = 1000000000LL;

        time = ((timeSpec.tv_sec * NanosecsPerSec) + timeSpec.tv_nsec);
    }

    return time;
}

} // Llpc
