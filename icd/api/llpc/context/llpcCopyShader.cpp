/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
 ***********************************************************************************************************************
 * @file  llpcCopyShader.cpp
 * @brief LLPC source file: contains implementation of class Llpc::CopyShader.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-copy-shader"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"

#include "llpcCodeGenManager.h"
#include "llpcContext.h"
#include "llpcCopyShader.h"
#include "llpcDebug.h"
#include "llpcInternal.h"
#include "llpcPassDeadFuncRemove.h"
#include "llpcPatch.h"
#include "llpcPatchAddrSpaceMutate.h"
#include "llpcPatchDescriptorLoad.h"
#include "llpcPatchInOutImportExport.h"

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// Initializes static member.
static const uint8_t GlslCopyShaderLib[]=
{
    #include "generate/g_llpcGlslCopyShaderEmuLib.h"
};

// =====================================================================================================================
CopyShader::CopyShader(
    Context* pContext)  // [in] LLPC context
    :
    m_pModule(nullptr),
    m_pContext(pContext),
    m_pEntryPoint(nullptr)
{
}

// =====================================================================================================================
// Executes copy shader generation and outputs its LLVM module.
Result CopyShader::Run(
    Module** ppModule)  // [out] Output module for this copy shader
{
    Result result = Result::Success;
    Module *pModule = nullptr;

    // Load LLVM external library (copy shader skeleton)
    result = LoadLibrary(pModule);
    auto pInsertPos = &*m_pEntryPoint->begin()->getFirstInsertionPt();

    // Load GS-VS ring buffer descriptor
    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageCopyShader);
    Value* pInternalTablePtrLo = GetFunctionArgument(m_pEntryPoint, EntryArgIdxInternalTablePtrLow);
    std::vector<Value*> args;
    args.push_back(pInternalTablePtrLo);
    args.push_back(ConstantInt::get(m_pContext->Int64Ty(), SI_DRV_TABLE_VS_RING_IN_OFFS));
    auto pGsVsRingBufDesc = EmitCall(m_pModule,
                                     LlpcName::DescriptorLoadGsVsRingBuffer,
                                     m_pContext->Int32x4Ty(),
                                     args,
                                     NoAttrib,
                                     pInsertPos);

    auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageCopyShader)->inOutUsage;
    inOutUsage.gs.pGsVsRingBufDesc = pGsVsRingBufDesc;

    if (m_pContext->IsGsOnChip())
    {
        // Construct LDS type: [ldsSize * i32], address space 3
        auto ldsSize = m_pContext->GetGpuProperty()->ldsSizePerCu;
        auto pLdsTy = ArrayType::get(m_pContext->Int32Ty(), ldsSize / sizeof(uint32_t));

        m_pLds = new GlobalVariable(*m_pModule,
                                    pLdsTy,
                                    false,
                                    GlobalValue::ExternalLinkage,
                                    nullptr,
                                    "lds",
                                    nullptr,
                                    GlobalValue::NotThreadLocal,
                                    ADDR_SPACE_LOCAL);
        LLPC_ASSERT(m_pLds != nullptr);
        m_pLds->setAlignment(sizeof(uint32_t));
    }

    // Export GS outputs to FS
    if (result == Result::Success)
    {
        ExportOutput();
    }

    // Do LLVM patching operations
    if (result == Result::Success)
    {
        result = DoPatch();
    }

    if (result != Result::Success)
    {
        delete pModule;
        pModule = nullptr;
    }

    *ppModule = pModule;

    return result;
}

// =====================================================================================================================
// Loads LLVM external library for copy shader (the skeleton).
Result CopyShader::LoadLibrary(
    Module*& pModule)   // [in,out] Copy shader module
{
    Result result = Result::Success;

    auto pMemBuffer = MemoryBuffer::getMemBuffer(StringRef(reinterpret_cast<const char*>(&GlslCopyShaderLib[0]),
                                                 sizeof(GlslCopyShaderLib)),
                                                 "",
                                                 false);

    Expected<std::unique_ptr<Module>> moduleOrErr = getLazyBitcodeModule(pMemBuffer->getMemBufferRef(), *m_pContext);
    if (!moduleOrErr)
    {
        Error error = moduleOrErr.takeError();
        LLPC_ERRS("Fails to load LLVM bitcode (copy shader)\n");
        result = Result::ErrorInvalidShader;
    }
    else
    {
        if (llvm::Error errCode = (*moduleOrErr)->materializeAll())
        {
            LLPC_ERRS("Fails to materialize (copy shader)\n");
            result = Result::ErrorInvalidShader;
        }
    }

    if (result == Result::Success)
    {
        pModule = moduleOrErr->release();
        m_pModule = pModule;
        m_pEntryPoint = GetEntryPoint(m_pModule);
    }

    return result;
}

// =====================================================================================================================
// Exports outputs of geometry shader, inserting buffer-load/output-export calls.
void CopyShader::ExportOutput()
{
    std::vector<Value*> args;

    std::string instName;

    Value* pOutputValue = nullptr;

    LLPC_ASSERT(m_pEntryPoint->getBasicBlockList().size() == 1); // Must have only one block
    auto pInsertPos = &*(m_pEntryPoint->back().getTerminator());

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry);
    auto& builtInUsage = pResUsage->builtInUsage.gs;
    const auto& genericOutByteSizes = pResUsage->inOutUsage.gs.genericOutByteSizes;

    for (auto& byteSizeMap : genericOutByteSizes)
    {
        // <location, <component, byteSize>>
        uint32_t loc = byteSizeMap.first;

        uint32_t byteSize = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            byteSize += byteSizeMap.second[i];
        }

        LLPC_ASSERT(byteSize % 4 == 0);
        uint32_t dwordSize = byteSize / 4;
        auto pOutputTy = VectorType::get(m_pContext->FloatTy(), dwordSize);
        pOutputValue = UndefValue::get(pOutputTy);

        for (uint32_t i = 0; i < dwordSize; ++i)
        {
            auto pLoadValue = LoadValueFromGsVsRingBuffer(loc + i / 4, i % 4, pInsertPos);
            pOutputValue = InsertElementInst::Create(pOutputValue,
                                                     pLoadValue,
                                                     ConstantInt::get(m_pContext->Int32Ty(), i),
                                                     "",
                                                     pInsertPos);
        }

        ExportGenericOutput(pOutputValue, loc, pInsertPos);
    }

    if (builtInUsage.position)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInPosition) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInPosition];
        pOutputValue = UndefValue::get(m_pContext->Floatx4Ty());

        for (uint32_t i = 0; i < 4; ++i)
        {
            auto pLoadValue = LoadValueFromGsVsRingBuffer(loc, i, pInsertPos);
            pOutputValue = InsertElementInst::Create(pOutputValue,
                                                     pLoadValue,
                                                     ConstantInt::get(m_pContext->Int32Ty(), i),
                                                     "",
                                                     pInsertPos);
        }

        ExportBuiltInOutput(pOutputValue, BuiltInPosition, pInsertPos);
    }

    if (builtInUsage.pointSize)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInPointSize) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInPointSize];

        auto pLoadValue = LoadValueFromGsVsRingBuffer(loc, 0, pInsertPos);

        ExportBuiltInOutput(pLoadValue, BuiltInPointSize, pInsertPos);
    }

    if (builtInUsage.clipDistance > 0)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInClipDistance) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInClipDistance];

        pOutputValue = UndefValue::get(ArrayType::get(m_pContext->FloatTy(), builtInUsage.clipDistance));

        for (uint32_t i = 0; i < builtInUsage.clipDistance; ++i)
        {
            auto pLoadValue = LoadValueFromGsVsRingBuffer(loc + i / 4, i % 4, pInsertPos);
            std::vector<uint32_t> idxs;
            idxs.push_back(i);
            pOutputValue = InsertValueInst::Create(pOutputValue,
                                                   pLoadValue,
                                                   idxs,
                                                   "",
                                                   pInsertPos);
        }

        ExportBuiltInOutput(pOutputValue, BuiltInClipDistance, pInsertPos);
    }

    if (builtInUsage.cullDistance > 0)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInCullDistance) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInCullDistance];

        pOutputValue = UndefValue::get(ArrayType::get(m_pContext->FloatTy(), builtInUsage.cullDistance));

        for (uint32_t i = 0; i < builtInUsage.cullDistance; ++i)
        {
            auto pLoadValue = LoadValueFromGsVsRingBuffer(loc + i / 4, i % 4, pInsertPos);
            std::vector<uint32_t> idxs;
            idxs.push_back(i);
            pOutputValue = InsertValueInst::Create(pOutputValue,
                                                   pLoadValue,
                                                   idxs,
                                                   "",
                                                   pInsertPos);
        }

        ExportBuiltInOutput(pOutputValue, BuiltInCullDistance, pInsertPos);
    }

    if (builtInUsage.primitiveId)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInPrimitiveId) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInPrimitiveId];

        auto pLoadValue = LoadValueFromGsVsRingBuffer(loc, 0, pInsertPos);
        pLoadValue = new BitCastInst(pLoadValue, m_pContext->Int32Ty(), "", pInsertPos);

        ExportBuiltInOutput(pLoadValue, BuiltInPrimitiveId, pInsertPos);
    }

    const auto enableMultiView = (reinterpret_cast<const GraphicsPipelineBuildInfo*>(
        m_pContext->GetPipelineBuildInfo()))->iaState.enableMultiView;
    if (builtInUsage.layer || enableMultiView)
    {
        // NOTE: If mult-view is enabled, always export gl_ViewIndex rather than gl_Layer.
        auto builtInId = enableMultiView ? BuiltInViewIndex : BuiltInLayer;
        auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;
        LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());

        uint32_t loc = builtInOutLocMap[builtInId];

        auto pLoadValue = LoadValueFromGsVsRingBuffer(loc, 0, pInsertPos);
        pLoadValue = new BitCastInst(pLoadValue, m_pContext->Int32Ty(), "", pInsertPos);

        ExportBuiltInOutput(pLoadValue, BuiltInLayer, pInsertPos);
    }

    if (builtInUsage.viewportIndex)
    {
        LLPC_ASSERT(pResUsage->inOutUsage.builtInOutputLocMap.find(BuiltInViewportIndex) !=
            pResUsage->inOutUsage.builtInOutputLocMap.end());

        uint32_t loc = pResUsage->inOutUsage.builtInOutputLocMap[BuiltInViewportIndex];

        auto pLoadValue = LoadValueFromGsVsRingBuffer(loc, 0, pInsertPos);
        pLoadValue = new BitCastInst(pLoadValue, m_pContext->Int32Ty(), "", pInsertPos);

        ExportBuiltInOutput(pLoadValue, BuiltInViewportIndex, pInsertPos);
    }

    LLPC_OUTS("===============================================================================\n");
    LLPC_OUTS("// LLPC GS output export results (" << GetShaderStageName(ShaderStageCopyShader) << " shader)\n");
    LLPC_OUTS(*m_pModule);
    LLPC_OUTS("\n");
}

// =====================================================================================================================
// Executes LLVM patching opertions for copy shader.
Result CopyShader::DoPatch()
{
    Result result = Result::Success;

    // Do patching opertions
    legacy::PassManager passMgr;

    // Convert SPIRAS address spaces to AMDGPU address spaces.
    passMgr.add(PatchAddrSpaceMutate::Create());

    // Function inlining
    passMgr.add(createFunctionInliningPass(InlineThreshold));

    // Remove dead functions after function inlining
    passMgr.add(PassDeadFuncRemove::Create());

    // Patch input import and output export operations
    passMgr.add(PatchInOutImportExport::Create());

    if (passMgr.run(*m_pModule) == false)
    {
        LLPC_ERRS("Fails to patch LLVM module\n");
        result = Result::ErrorInvalidShader;
    }
    else
    {
        LLPC_OUTS("===============================================================================\n");
        LLPC_OUTS("// LLPC patching results (" << GetShaderStageName(ShaderStageCopyShader) << " shader)\n");
        LLPC_OUTS(*m_pModule);
        LLPC_OUTS("\n");
    }

    return result;
}

// =====================================================================================================================
// Calculates GS to VS ring offset from input location
Value* CopyShader::CalcGsVsRingOffsetForInput(
    uint32_t        location,    // Output location
    uint32_t        compIdx,     // Output component
    Instruction*    pInsertPos)  // [in] Where to insert the instruction
{
    Value* pVertexOffset = GetFunctionArgument(m_pEntryPoint, EntryArgIdxVertexOffset);

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry);

    Value* pRingOffset = nullptr;
    if (m_pContext->IsGsOnChip())
    {
        // ringOffset = esGsLdsSize + vertexOffset + location * 4 + compIdx
        pRingOffset = ConstantInt::get(m_pContext->Int32Ty(), pResUsage->inOutUsage.gs.calcFactor.esGsLdsSize);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset, pVertexOffset, "", pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), (location * 4) + compIdx),
                                                "",
                                                pInsertPos);
    }
    else
    {
        uint32_t outputVertices = pResUsage->builtInUsage.gs.outputVertices;

        // ringOffset = vertexOffset * 4 + (location * 4 + compIdx) * 64 * maxVertices
        pRingOffset = BinaryOperator::CreateMul(pVertexOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                "",
                                                pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(),
                                                                 (location * 4 + compIdx) * 64 *
                                                                 outputVertices),
                                                "",
                                                pInsertPos);
    }

    return pRingOffset;
}

// =====================================================================================================================
// // Loads value from GS-VS ring buffer.
Value* CopyShader::LoadValueFromGsVsRingBuffer(
    uint32_t        location,   // Output location
    uint32_t        compIdx,    // Output component
    Instruction*    pInsertPos) // [in] Where to insert the load instruction
{
    Value* pLoadValue = nullptr;
    Value* pRingOffset = CalcGsVsRingOffsetForInput(location, compIdx, pInsertPos);

    if (m_pContext->IsGsOnChip())
    {
        std::vector<Value*> idxs;
        idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        idxs.push_back(pRingOffset);

        Value* pLoadPtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
        pLoadValue = new LoadInst(pLoadPtr, "", false, m_pLds->getAlignment(), pInsertPos);

        pLoadValue = new BitCastInst(pLoadValue, m_pContext->FloatTy(), "", pInsertPos);
    }
    else
    {
        auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageCopyShader)->inOutUsage;

        std::vector<Value*> args;
        args.push_back(inOutUsage.gs.pGsVsRingBufDesc);
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        args.push_back(pRingOffset);
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));  // glc
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));  // slc
        pLoadValue = EmitCall(m_pModule,
                              "llvm.amdgcn.buffer.load.f32",
                              m_pContext->FloatTy(),
                              args,
                              NoAttrib,
                              pInsertPos);
    }

    return pLoadValue;
}

// =====================================================================================================================
// Exports generic outputs of geometry shader, inserting output-export calls.
void CopyShader::ExportGenericOutput(
    Value*       pOutputValue,  // [in] Value exported to output
    uint32_t     location,      // Location of the output
    Instruction* pInsertPos)    // [in] Where to insert the instructions
{
    auto pOutputTy = pOutputValue->getType();
    LLPC_ASSERT(pOutputTy->isSingleValueType());

    std::vector<Value*> args;

    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), location));
    args.push_back(pOutputValue);

    std::string instName = LlpcName::OutputExportGeneric;
    instName += GetTypeNameForScalarOrVector(pOutputTy);

    EmitCall(m_pModule, instName, m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
}

// =====================================================================================================================
// Exports built-in outputs of geometry shader, inserting output-export calls.
void CopyShader::ExportBuiltInOutput(
    Value*       pOutputValue,  // [in] Value exported to output
    BuiltIn      builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the instructions
{
    std::vector<Value*> args;

    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), builtInId));
    args.push_back(pOutputValue);

    std::string builtInName = getNameMap(builtInId).map(builtInId);
    LLPC_ASSERT(builtInName.find("BuiltIn")  == 0);
    std::string instName = LlpcName::OutputExportBuiltIn + builtInName.substr(strlen("BuiltIn"));

    EmitCall(m_pModule, instName, m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
}

} // Llpc
