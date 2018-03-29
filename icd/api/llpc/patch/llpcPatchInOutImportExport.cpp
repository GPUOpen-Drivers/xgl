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
 * @file  llpcPatchInOutImportExport.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchInOutImportExport.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-in-out-import-export"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_set>
#include "spirv.hpp"
#include "llpcContext.h"
#include "llpcFragColorExport.h"
#include "llpcGraphicsContext.h"
#include "llpcPatchInOutImportExport.h"
#include "llpcVertexFetch.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchInOutImportExport::ID = 0;

// =====================================================================================================================
PatchInOutImportExport::PatchInOutImportExport()
    :
    Patch(ID),
    m_pVertexFetch(nullptr),
    m_pFragColorExport(nullptr),
    m_pLastExport(nullptr),
    m_pClipDistance(nullptr),
    m_pCullDistance(nullptr),
    m_pPrimitiveId(nullptr),
    m_pFragDepth(nullptr),
    m_pFragStencilRef(nullptr),
    m_pSampleMask(nullptr),
    m_pViewportIndex(nullptr),
    m_pLayer(nullptr),
    m_hasTs(false),
    m_hasGs(false),
    m_pLds(nullptr),
    m_pThreadId(nullptr)
{
    memset(&m_gfxIp, 0, sizeof(m_gfxIp));

    initializePatchInOutImportExportPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
PatchInOutImportExport::~PatchInOutImportExport()
{
    delete m_pFragColorExport;
    delete m_pVertexFetch;
}

// =====================================================================================================================
// Executes this LLVM patching pass on the specified LLVM module.
bool PatchInOutImportExport::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-In-Out-Import-Export\n");

    Patch::Init(&module);

    m_gfxIp = m_pContext->GetGfxIpVersion();

    if (m_shaderStage == ShaderStageVertex)
    {
        // Create vertex fetch manager
        m_pVertexFetch = new VertexFetch(m_pModule);
    }
    else if (m_shaderStage == ShaderStageFragment)
    {
        // Create fragment color export manager
        m_pFragColorExport = new FragColorExport(m_pModule);
    }

    // Initialize the output value for gl_PrimitiveID
    const auto& builtInUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->builtInUsage;
    const auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs;
    if (m_shaderStage == ShaderStageVertex)
    {
        if (builtInUsage.vs.primitiveId)
        {
            m_pPrimitiveId = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.vs.primitiveId);
        }
    }
    else if (m_shaderStage == ShaderStageTessEval)
    {
        if (builtInUsage.tes.primitiveId)
        {
            // TODO: Support tessellation shader.
            m_pPrimitiveId = UndefValue::get(m_pContext->Int32Ty());
        }
    }

    // Initialize calculation factors for tessellation shader
    if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
    {
        const uint32_t stageMask = m_pContext->GetShaderStageMask();
        const bool hasTcs = ((stageMask & ShaderStageToMask(ShaderStageTessControl)) != 0);
        const bool hasTes = ((stageMask & ShaderStageToMask(ShaderStageTessEval)) != 0);

        auto& calcFactor = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs.calcFactor;
        if ((calcFactor.inVertexStride              == InvalidValue) &&
            (calcFactor.outVertexStride             == InvalidValue) &&
            (calcFactor.patchCountPerThreadGroup    == InvalidValue) &&
            (calcFactor.outPatchSize                == InvalidValue) &&
            (calcFactor.patchConstSize              == InvalidValue))
        {
            // NOTE: The LDS space is divided to three parts:
            //
            //              +----------------------------------------+
            //            / | TCS Vertex (Control Point) In (VS Out) |
            //           /  +----------------------------------------+
            //   LDS Space  | TCS Vertex (Control Point) Out         |
            //           \  +----------------------------------------+
            //            \ | TCS Patch Constant                     |
            //              +----------------------------------------+
            //
            // inPatchTotalSize = inVertexCount * inVertexStride * patchCountPerThreadGroup
            // outPatchTotalSize = outVertexCount * outVertexStride * patchCountPerThreadGroup
            // patchConstTotalSize = patchConstCount * 4 * patchCountPerThreadGroup

            const auto& tcsInOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage;
            const auto& tesInOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->inOutUsage;

            const auto& tcsBuiltInUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
            const auto& tesBuiltInUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;

            const uint32_t inLocCount = std::max(tcsInOutUsage.inputMapLocCount, 1u);
            const uint32_t outLocCount =
                hasTcs ? std::max(tcsInOutUsage.outputMapLocCount, 1u) : std::max(tesInOutUsage.inputMapLocCount, 1u);

            const auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            const uint32_t inVertexCount  = pPipelineInfo->iaState.patchControlPoints;
            const uint32_t outVertexCount = hasTcs ? tcsBuiltInUsage.outputVertices : MaxTessPatchVertices;

            calcFactor.inVertexStride = inLocCount * 4;
            calcFactor.outVertexStride = outLocCount * 4;

            const uint32_t patchConstCount =
                hasTcs ? tcsInOutUsage.perPatchOutputMapLocCount : tesInOutUsage.perPatchInputMapLocCount;
            calcFactor.patchConstSize = patchConstCount * 4;

            calcFactor.patchCountPerThreadGroup = CalcPatchCountPerThreadGroup(inVertexCount,
                                                                               calcFactor.inVertexStride,
                                                                               outVertexCount,
                                                                               calcFactor.outVertexStride,
                                                                               patchConstCount);

            const uint32_t inPatchSize = inVertexCount * calcFactor.inVertexStride;
            const uint32_t inPatchTotalSize = calcFactor.patchCountPerThreadGroup * inPatchSize;

            const uint32_t outPatchSize = outVertexCount * calcFactor.outVertexStride;
            const uint32_t outPatchTotalSize = calcFactor.patchCountPerThreadGroup * outPatchSize;

            calcFactor.outPatchSize = outPatchSize;
            calcFactor.inPatchSize = inPatchSize;

            calcFactor.onChip.outPatchStart = inPatchTotalSize;
            calcFactor.onChip.patchConstStart = inPatchTotalSize + outPatchTotalSize;

            if (m_pContext->IsTessOffChip())
            {
                calcFactor.offChip.outPatchStart = 0;
                calcFactor.offChip.patchConstStart = outPatchTotalSize;
            }

            uint32_t tessFactorStride = 0;
            switch (tesBuiltInUsage.primitiveMode)
            {
            case Triangles:
                tessFactorStride = 4;
                break;
            case Quads:
                tessFactorStride = 6;
                break;
            case Isolines:
                tessFactorStride = 2;
                break;
            default:
                LLPC_NEVER_CALLED();
                break;
            }

            calcFactor.tessFactorStride = tessFactorStride;

            LLPC_OUTS("===============================================================================\n");
            LLPC_OUTS("// LLPC tessellation calculation factor results\n\n");
            LLPC_OUTS("Patch count per thread group: " << calcFactor.patchCountPerThreadGroup << "\n");
            LLPC_OUTS("\n");
            LLPC_OUTS("Input vertex count: " << inVertexCount << "\n");
            LLPC_OUTS("Input vertex stride: " << calcFactor.inVertexStride << "\n");
            LLPC_OUTS("Input patch size: " << inPatchSize << "\n");
            LLPC_OUTS("Input patch total size: " << inPatchTotalSize << "\n");
            LLPC_OUTS("\n");
            LLPC_OUTS("Output vertex count: " << outVertexCount << "\n");
            LLPC_OUTS("Output vertex stride: " << calcFactor.outVertexStride << "\n");
            LLPC_OUTS("Output patch size: " << outPatchSize << "\n");
            LLPC_OUTS("Output patch total size: " << outPatchTotalSize << "\n");
            LLPC_OUTS("\n");
            LLPC_OUTS("Patch constant count: " << patchConstCount << "\n");
            LLPC_OUTS("Patch constant size: " << calcFactor.patchConstSize << "\n");
            LLPC_OUTS("Patch constant total size: " <<
                      calcFactor.patchConstSize * calcFactor.patchCountPerThreadGroup << "\n");
            LLPC_OUTS("\n");
            LLPC_OUTS("Tessellation factor stride: " << tessFactorStride << " (");
            switch (tesBuiltInUsage.primitiveMode)
            {
            case Triangles:
                LLPC_OUTS("triangles");
                break;
            case Quads:
                LLPC_OUTS("quads");
                tessFactorStride = 6;
                break;
            case Isolines:
                LLPC_OUTS("isolines");
                tessFactorStride = 2;
                break;
            default:
                LLPC_NEVER_CALLED();
                break;
            }
            LLPC_OUTS(")\n\n");
        }
    }

    const uint32_t stageMask = m_pContext->GetShaderStageMask();
    m_hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                             ShaderStageToMask(ShaderStageTessEval))) != 0);
    m_hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    // Thread ID will be used in on-chip GS offset calculation (ES -> GS ring is always on-chip on GFX9)
    const bool useThreadId = (m_hasGs && (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9)));
    if (useThreadId)
    {
        // Calculate and store thread ID
        auto pInsertPos = m_pEntryPoint->begin()->getFirstInsertionPt();

        std::vector<Value*> args;
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), -1));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        m_pThreadId = EmitCall(m_pModule, "llvm.amdgcn.mbcnt.lo", m_pContext->Int32Ty(), args, NoAttrib, &*pInsertPos);

        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), -1));
        args.push_back(m_pThreadId);
        m_pThreadId = EmitCall(m_pModule, "llvm.amdgcn.mbcnt.hi", m_pContext->Int32Ty(), args, NoAttrib, &*pInsertPos);
    }

    // Create the global variable that is to model LDS
    // NOTE: ES -> GS ring is always on-chip on GFX9.
    if (m_hasTs || (m_hasGs && (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9))))
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

    // Invoke handling of "call" instruction
    visit(*m_pModule);

    // Collect to-be-removed call instructions (keep unique copy)
    std::unordered_set<Function*> removedCalls;
    for (auto pCallInst : m_importCalls)
    {
        removedCalls.insert(pCallInst->getCalledFunction());
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    for (auto pCallInst : m_exportCalls)
    {
        removedCalls.insert(pCallInst->getCalledFunction());
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    // Remove unnecessary call instructions
    for (auto pCallInst : removedCalls)
    {
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "call" instruction.
void PatchInOutImportExport::visitCallInst(
    CallInst& callInst)   // [in] "Call" instruction
{
    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    auto pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    auto mangledName = pCallee->getName();

    auto importGenericInput     = LlpcName::InputImportGeneric;
    auto importBuiltInInput     = LlpcName::InputImportBuiltIn;
    auto importInterpolantInput = LlpcName::InputImportInterpolant;
    auto importGenericOutput    = LlpcName::OutputImportGeneric;
    auto importBuiltInOutput    = LlpcName::OutputImportBuiltIn;

    const bool isGenericInputImport     = mangledName.startswith(importGenericInput);
    const bool isBuiltInInputImport     = mangledName.startswith(importBuiltInInput);
    const bool isInterpolantInputImport = mangledName.startswith(importInterpolantInput);
    const bool isGenericOutputImport    = mangledName.startswith(importGenericOutput);
    const bool isBuiltInOutputImport    = mangledName.startswith(importBuiltInOutput);

    const bool isImport = (isGenericInputImport  || isBuiltInInputImport || isInterpolantInputImport ||
                           isGenericOutputImport || isBuiltInOutputImport);

    auto exportGenericOutput = LlpcName::OutputExportGeneric;
    auto exportBuiltInOutput = LlpcName::OutputExportBuiltIn;

    const bool isGenericOutputExport = mangledName.startswith(exportGenericOutput);
    const bool isBuiltInOutputExport = mangledName.startswith(exportBuiltInOutput);

    const bool isExport = (isGenericOutputExport || isBuiltInOutputExport);

    const bool isInput  = (isGenericInputImport || isBuiltInInputImport || isInterpolantInputImport);
    const bool isOutput = (isGenericOutputImport || isBuiltInOutputImport ||
                           isGenericOutputExport || isBuiltInOutputExport);

    if (isImport && isInput)
    {
        // Input imports
        Value* pInput = nullptr;
        Type* pInputTy = callInst.getType();

        // Generic value (location or SPIR-V built-in ID)
        uint32_t value = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();

        if (isGenericInputImport)
        {
            LLPC_ASSERT((importGenericInput + GetTypeNameForScalarOrVector(pInputTy) == mangledName));
        }
        DEBUG(dbgs() << "Find input import call: builtin = " << isBuiltInInputImport
                     << " value = " << value << "\n");

        m_importCalls.push_back(&callInst);

        if (isBuiltInInputImport)
        {
            const uint32_t builtInId = value;

            switch (m_shaderStage)
            {
            case ShaderStageVertex:
                {
                    pInput = PatchVsBuiltInInputImport(pInputTy, builtInId, &callInst);
                    break;
                }
            case ShaderStageTessControl:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                    Value* pElemIdx = IsDontCareValue(callInst.getOperand(1)) ? nullptr : callInst.getOperand(1);
                    Value* pVertexIdx = IsDontCareValue(callInst.getOperand(2)) ? nullptr : callInst.getOperand(2);

                    pInput = PatchTcsBuiltInInputImport(pInputTy, builtInId, pElemIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageTessEval:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                    Value* pElemIdx = IsDontCareValue(callInst.getOperand(1)) ? nullptr : callInst.getOperand(1);
                    Value* pVertexIdx = IsDontCareValue(callInst.getOperand(2)) ? nullptr : callInst.getOperand(2);

                    pInput = PatchTesBuiltInInputImport(pInputTy, builtInId, pElemIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageGeometry:
                {
                    Value* pVertexIdx = IsDontCareValue(callInst.getOperand(1)) ? nullptr : callInst.getOperand(1);
                    pInput = PatchGsBuiltInInputImport(pInputTy, builtInId, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageFragment:
                {
                    pInput = PatchFsBuiltInInputImport(pInputTy, builtInId, &callInst);
                    break;
                }
            case ShaderStageCompute:
                {
                    pInput = PatchCsBuiltInInputImport(pInputTy, builtInId, &callInst);
                    break;
                }
            default:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
        }
        else
        {
            LLPC_ASSERT(isGenericInputImport || isInterpolantInputImport);

            uint32_t loc = InvalidValue;
            Value* pLocOffset = nullptr;

            if (m_shaderStage == ShaderStageVertex)
            {
                // NOTE: For vertex shader, generic inputs are not mapped.
                loc = value;
            }
            else
            {
                if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval) ||
                    ((m_shaderStage == ShaderStageFragment) && isInterpolantInputImport))
                {
                    // NOTE: If location offset is present and is a constant, we have to add it to the unmapped
                    // location before querying the mapped location. Meanwhile, we have to adjust the location
                    // offset to 0 (rebase it).
                    pLocOffset = callInst.getOperand(1);
                    if (isa<ConstantInt>(pLocOffset))
                    {
                        auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
                        value += locOffset;
                        pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
                    }
                }

                if (m_shaderStage == ShaderStageTessEval)
                {
                    // NOTE: For generic inputs of tessellation evaluation shader, they could be per-patch ones.
                    if (pResUsage->inOutUsage.inputLocMap.find(value) !=
                        pResUsage->inOutUsage.inputLocMap.end())
                    {
                        loc = pResUsage->inOutUsage.inputLocMap[value];
                    }
                    else
                    {
                        LLPC_ASSERT(pResUsage->inOutUsage.perPatchInputLocMap.find(value) !=
                                    pResUsage->inOutUsage.perPatchInputLocMap.end());
                        loc = pResUsage->inOutUsage.perPatchInputLocMap[value];
                    }
                }
                else
                {
                    LLPC_ASSERT(pResUsage->inOutUsage.inputLocMap.find(value) !=
                                pResUsage->inOutUsage.inputLocMap.end());
                    loc = pResUsage->inOutUsage.inputLocMap[value];
                }
            }
            LLPC_ASSERT(loc != InvalidValue);

            switch (m_shaderStage)
            {
            case ShaderStageVertex:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 2);
                    const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                    pInput = PatchVsGenericInputImport(pInputTy, loc, compIdx, &callInst);
                    break;
                }
            case ShaderStageTessControl:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 4);

                    auto pElemIdx = callInst.getOperand(2);
                    LLPC_ASSERT(IsDontCareValue(pElemIdx) == false);

                    auto pVertexIdx = callInst.getOperand(3);
                    LLPC_ASSERT(IsDontCareValue(pVertexIdx) == false);

                    pInput = PatchTcsGenericInputImport(pInputTy, loc, pLocOffset, pElemIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageTessEval:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 4);

                    auto pElemIdx = callInst.getOperand(2);
                    LLPC_ASSERT(IsDontCareValue(pElemIdx) == false);

                    auto pVertexIdx = IsDontCareValue(callInst.getOperand(3)) ? nullptr : callInst.getOperand(3);

                    pInput = PatchTesGenericInputImport(pInputTy, loc, pLocOffset, pElemIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageGeometry:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 3);

                    const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();

                    Value* pVertexIdx = callInst.getOperand(2);
                    LLPC_ASSERT(IsDontCareValue(pVertexIdx) == false);

                    pInput = PatchGsGenericInputImport(pInputTy, loc, compIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageFragment:
                {
                    uint32_t interpMode = InterpModeSmooth;
                    uint32_t interpLoc = InterpLocCenter;

                    Value* pElemIdx = callInst.getOperand(1);
                    LLPC_ASSERT(IsDontCareValue(pElemIdx) == false);

                    Value* pAuxInterpValue = nullptr;

                    if (isGenericInputImport)
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 4);

                        interpMode = cast<ConstantInt>(callInst.getOperand(2))->getZExtValue();
                        interpLoc = cast<ConstantInt>(callInst.getOperand(3))->getZExtValue();
                    }
                    else
                    {
                        LLPC_ASSERT(isInterpolantInputImport);
                        LLPC_ASSERT(callInst.getNumArgOperands() == 5);

                        interpMode = cast<ConstantInt>(callInst.getOperand(3))->getZExtValue();
                        interpLoc = InterpLocUnknown;

                        pAuxInterpValue = callInst.getOperand(4);
                    }

                    pInput = PatchFsGenericInputImport(pInputTy,
                                                       loc,
                                                       pLocOffset,
                                                       pElemIdx,
                                                       pAuxInterpValue,
                                                       interpMode,
                                                       interpLoc,
                                                       &callInst);
                    break;
                }
            case ShaderStageCompute:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            default:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
        }

        callInst.replaceAllUsesWith(pInput);
    }
    else if (isImport && isOutput)
    {
        // Output imports
        LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

        Value* pOutput = nullptr;
        Type* pOutputTy = callInst.getType();

        // Generic value (location or SPIR-V built-in ID)
        uint32_t value = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();

        if (isGenericOutputImport)
        {
            LLPC_ASSERT((importGenericOutput + GetTypeNameForScalarOrVector(pOutputTy) == mangledName));
        }
        DEBUG(dbgs() << "Find output import call: builtin = " << isBuiltInOutputImport
                     << " value = " << value << "\n");

        m_importCalls.push_back(&callInst);

        if (isBuiltInOutputImport)
        {
            const uint32_t builtInId = value;

            LLPC_ASSERT(callInst.getNumArgOperands() == 3);
            Value* pElemIdx = IsDontCareValue(callInst.getOperand(1)) ? nullptr : callInst.getOperand(1);
            Value* pVertexIdx = IsDontCareValue(callInst.getOperand(2)) ? nullptr : callInst.getOperand(2);

            pOutput = PatchTcsBuiltInOutputImport(pOutputTy, builtInId, pElemIdx, pVertexIdx, &callInst);
        }
        else
        {
            LLPC_ASSERT(isGenericOutputImport);

            uint32_t loc = InvalidValue;

            // NOTE: If location offset is a constant, we have to add it to the unmapped location before querying
            // the mapped location. Meanwhile, we have to adjust the location offset to 0 (rebase it).
            Value* pLocOffset = callInst.getOperand(1);
            if (isa<ConstantInt>(pLocOffset))
            {
                auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
                value += locOffset;
                pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
            }

            // NOTE: For generic outputs of tessellation control shader, they could be per-patch ones.
            if (pResUsage->inOutUsage.outputLocMap.find(value) != pResUsage->inOutUsage.outputLocMap.end())
            {
                loc = pResUsage->inOutUsage.outputLocMap[value];
            }
            else
            {
                LLPC_ASSERT(pResUsage->inOutUsage.perPatchOutputLocMap.find(value) !=
                            pResUsage->inOutUsage.perPatchOutputLocMap.end());
                loc = pResUsage->inOutUsage.perPatchOutputLocMap[value];
            }
            LLPC_ASSERT(loc != InvalidValue);

            LLPC_ASSERT(callInst.getNumArgOperands() == 4);
            auto pElemIdx = callInst.getOperand(2);
            LLPC_ASSERT(IsDontCareValue(pElemIdx) == false);
            auto pVertexIdx = IsDontCareValue(callInst.getOperand(3)) ? nullptr : callInst.getOperand(3);

            pOutput = PatchTcsGenericOutputImport(pOutputTy, loc, pLocOffset, pElemIdx, pVertexIdx, &callInst);

        }

        callInst.replaceAllUsesWith(pOutput);
    }
    else if (isExport)
    {
        // Output exports
        LLPC_ASSERT(isOutput);

        Value* pOutput = callInst.getOperand(callInst.getNumArgOperands() - 1); // Last argument

        // Generic value (location or SPIR-V built-in ID)
        uint32_t value = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();

        if (isGenericOutputExport)
        {
            LLPC_ASSERT(exportGenericOutput + GetTypeNameForScalarOrVector(pOutput->getType()) == mangledName);
        }
        DEBUG(dbgs() << "Find output export call: builtin = " << isBuiltInOutputExport
                     << " value = " << value << "\n");

        m_exportCalls.push_back(&callInst);

        if (isBuiltInOutputExport)
        {
            const uint32_t builtInId = value;

            switch (m_shaderStage)
            {
            case ShaderStageVertex:
                {
                    PatchVsBuiltInOutputExport(pOutput, builtInId, &callInst);
                    break;
                }
            case ShaderStageTessControl:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 4);
                    Value* pElemIdx = IsDontCareValue(callInst.getOperand(1)) ? nullptr : callInst.getOperand(1);
                    Value* pVertexIdx = IsDontCareValue(callInst.getOperand(2)) ? nullptr : callInst.getOperand(2);

                    PatchTcsBuiltInOutputExport(pOutput, builtInId, pElemIdx, pVertexIdx, &callInst);
                    break;
                }
            case ShaderStageTessEval:
                {
                    PatchTesBuiltInOutputExport(pOutput, builtInId, &callInst);
                    break;
                }
            case ShaderStageGeometry:
                {
                    LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                    uint32_t streamId = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                    PatchGsBuiltInOutputExport(pOutput, builtInId, streamId, &callInst);
                    break;
                }
            case ShaderStageFragment:
                {
                    PatchFsBuiltInOutputExport(pOutput, builtInId, &callInst);
                    break;
                }
            case ShaderStageCopyShader:
                {
                    PatchCopyShaderBuiltInOutputExport(pOutput, builtInId, &callInst);
                    break;
                }
            case ShaderStageCompute:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            default:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
        }
        else
        {
            LLPC_ASSERT(isGenericOutputExport);

            bool exist = false;
            uint32_t loc = InvalidValue;
            Value* pLocOffset = nullptr;

            if (m_shaderStage == ShaderStageTessControl)
            {
                // NOTE: If location offset is a constant, we have to add it to the unmapped location before querying
                // the mapped location. Meanwhile, we have to adjust the location offset to 0 (rebase it).
                pLocOffset = callInst.getOperand(1);
                if (isa<ConstantInt>(pLocOffset))
                {
                    auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
                    value += locOffset;
                    pLocOffset = ConstantInt::get(m_pContext->Int32Ty(), 0);
                }

                // NOTE: For generic outputs of tessellation control shader, they could be per-patch ones.
                if (pResUsage->inOutUsage.outputLocMap.find(value) != pResUsage->inOutUsage.outputLocMap.end())
                {
                    exist = true;
                    loc = pResUsage->inOutUsage.outputLocMap[value];
                }
                else if (pResUsage->inOutUsage.perPatchOutputLocMap.find(value) !=
                         pResUsage->inOutUsage.perPatchOutputLocMap.end())
                {
                    exist = true;
                    loc = pResUsage->inOutUsage.perPatchOutputLocMap[value];
                }
            }
            else if (m_shaderStage == ShaderStageCopyShader)
            {
                if (pResUsage->inOutUsage.gs.genericOutByteSizes.find(value) != pResUsage->inOutUsage.gs.genericOutByteSizes.end())
                {
                    exist = true;
                    loc = value;
                }
            }
            else
            {
                if (pResUsage->inOutUsage.outputLocMap.find(value) != pResUsage->inOutUsage.outputLocMap.end())
                {
                    exist = true;
                    loc = pResUsage->inOutUsage.outputLocMap[value];
                }
            }

            if (exist)
            {
                // NOTE: Some outputs are not used by next shader stage. They must have been removed already.
                LLPC_ASSERT(loc != InvalidValue);

                switch (m_shaderStage)
                {
                case ShaderStageVertex:
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                        const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                        PatchVsGenericOutputExport(pOutput, loc, compIdx, &callInst);
                        break;
                    }
                case ShaderStageTessControl:
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 5);

                        auto pElemIdx = callInst.getOperand(2);
                        LLPC_ASSERT(IsDontCareValue(pElemIdx) == false);

                        auto pVertexIdx = IsDontCareValue(callInst.getOperand(3)) ? nullptr : callInst.getOperand(3);

                        PatchTcsGenericOutputExport(pOutput, loc, pLocOffset, pElemIdx, pVertexIdx, &callInst);
                        break;
                    }
                case ShaderStageTessEval:
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                        const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                        PatchTesGenericOutputExport(pOutput, loc, compIdx, &callInst);
                        break;
                    }
                case ShaderStageGeometry:
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 4);
                        const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                        const uint32_t streamId = cast<ConstantInt>(callInst.getOperand(2))->getZExtValue();
                        PatchGsGenericOutputExport(pOutput, loc, compIdx, streamId, &callInst);
                        break;
                    }
                case ShaderStageFragment:
                    {
                        LLPC_ASSERT(callInst.getNumArgOperands() == 3);
                        const uint32_t compIdx = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
                        PatchFsGenericOutputExport(pOutput, loc, compIdx, &callInst);
                        break;
                    }
                case ShaderStageCopyShader:
                    {
                        PatchCopyShaderGenericOutputExport(pOutput, loc, &callInst);
                        break;
                    }
                case ShaderStageCompute:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                default:
                    {
                        LLPC_NEVER_CALLED();
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // Other calls relevant to input/output import/export
        if (mangledName.startswith("llvm.amdgcn.s.sendmsg"))
        {
            // NOTE: Implicitly store the value of gl_ViewIndex to GS-VS ring buffer before emit calls.
            const auto enableMultiView = (reinterpret_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo()))->iaState.enableMultiView;

            if (enableMultiView)
            {
                LLPC_ASSERT(m_shaderStage == ShaderStageGeometry); // Must be geometry shader

                auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageGeometry)->entryArgIdxs.gs;
                auto pViewIndex = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);

                auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry);
                auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;

                LLPC_ASSERT(builtInOutLocMap.find(BuiltInViewIndex) != builtInOutLocMap.end());
                uint32_t loc = builtInOutLocMap[BuiltInViewIndex];

                StoreValueToGsVsRingBuffer(pViewIndex, loc, 0, &callInst);
            }

            bool isEmitStream0 = false;

            uint64_t message = cast<ConstantInt>(callInst.getArgOperand(0))->getZExtValue();
            if (message == GS_EMIT_STREAM0)
            {
                // NOTE: Only stream 0 is supported.
                isEmitStream0 = true;
            }

            if (isEmitStream0)
            {
                // Increment emit vertex counter
                const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
                Value* pEmitCounter = new LoadInst(inOutUsage.gs.pEmitCounterPtr, "", &callInst);
                pEmitCounter = BinaryOperator::CreateAdd(pEmitCounter,
                                                         ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                         "",
                                                         &callInst);
                new StoreInst(pEmitCounter, inOutUsage.gs.pEmitCounterPtr, &callInst);
            }
        }
    }
}

// =====================================================================================================================
// Visits "ret" instruction.
void PatchInOutImportExport::visitReturnInst(
    ReturnInst& retInst)  // [in] "Ret" instruction
{
    // We only handle the "ret" of shader entry point
    if (retInst.getParent()->getParent()->getDLLStorageClass() != GlobalValue::DLLExportStorageClass)
    {
        return;
    }

    LLPC_ASSERT(retInst.getParent()->getParent() == m_pEntryPoint);

    const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);

    // Whether this shader stage has to use "exp" instructions to export outputs
    const bool useExpInst = (((m_shaderStage == ShaderStageVertex) || (m_shaderStage == ShaderStageTessEval) ||
                              (m_shaderStage == ShaderStageCopyShader)) &&
                             ((nextStage == ShaderStageInvalid) || (nextStage == ShaderStageFragment)));

    auto pZero  = ConstantFP::get(m_pContext->FloatTy(), 0.0);
    auto pOne   = ConstantFP::get(m_pContext->FloatTy(), 1.0);
    auto pUndef = UndefValue::get(m_pContext->FloatTy());

    auto pInsertPos = &retInst;

    std::vector<Value*> args;

    if (useExpInst)
    {
        bool usePosition = false;
        bool usePointSize = false;
        bool usePrimitiveId = false;
        bool useLayer = false;
        bool useViewportIndex = false;
        uint32_t clipDistanceCount = 0;
        uint32_t cullDistanceCount = 0;

        auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;

        const auto enableMultiView = (reinterpret_cast<const GraphicsPipelineBuildInfo*>(
            m_pContext->GetPipelineBuildInfo()))->iaState.enableMultiView;

        if (m_shaderStage == ShaderStageVertex)
        {
            auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;
            auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageVertex)->entryArgIdxs.vs;

            usePosition       = builtInUsage.position;
            usePointSize      = builtInUsage.pointSize;
            usePrimitiveId    = builtInUsage.primitiveId;
            useLayer          = builtInUsage.layer;
            useViewportIndex  = builtInUsage.viewportIndex;
            clipDistanceCount = builtInUsage.clipDistance;
            cullDistanceCount = builtInUsage.cullDistance;

            if (enableMultiView)
            {
                // NOTE: If multi-view is enabled, the exported value of gl_Layer is from gl_ViewIndex.
                m_pLayer = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);
            }
        }
        else if (m_shaderStage == ShaderStageTessEval)
        {
            auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;
            auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageTessEval)->entryArgIdxs.tes;

            usePosition       = builtInUsage.position;
            usePointSize      = builtInUsage.pointSize;
            usePrimitiveId    = builtInUsage.primitiveId;
            useLayer          = builtInUsage.layer;
            useViewportIndex  = builtInUsage.viewportIndex;
            clipDistanceCount = builtInUsage.clipDistance;
            cullDistanceCount = builtInUsage.cullDistance;

            if (enableMultiView)
            {
                // NOTE: If multi-view is enabled, the exported value of gl_Layer is from gl_ViewIndex.
                m_pLayer = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);
            }
        }
        else
        {
            LLPC_ASSERT(m_shaderStage == ShaderStageCopyShader);
            auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageCopyShader)->builtInUsage.gs;
            auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageGeometry)->entryArgIdxs.gs;

            usePosition       = builtInUsage.position;
            usePointSize      = builtInUsage.pointSize;
            usePrimitiveId    = builtInUsage.primitiveId;
            useLayer          = builtInUsage.layer;
            useViewportIndex  = builtInUsage.viewportIndex;
            clipDistanceCount = builtInUsage.clipDistance;
            cullDistanceCount = builtInUsage.cullDistance;
        }

        useLayer = enableMultiView || useLayer;

        // NOTE: If gl_Position is not present in this shader stage, we have to export a dummy one.
        if (usePosition == false)
        {
            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_0)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));              // en
            args.push_back(pZero);                                                     // src0
            args.push_back(pZero);                                                     // src1
            args.push_back(pZero);                                                     // src2
            args.push_back(pOne);                                                      // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // vm

            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
        }

        // Export gl_ClipDistance[] and gl_CullDistance[] before entry-point returns
        if ((clipDistanceCount > 0) || (cullDistanceCount > 0))
        {
            LLPC_ASSERT(clipDistanceCount + cullDistanceCount <= MaxClipCullDistanceCount);

            LLPC_ASSERT((clipDistanceCount == 0) || ((clipDistanceCount > 0) && (m_pClipDistance != nullptr)));
            LLPC_ASSERT((cullDistanceCount == 0) || ((cullDistanceCount > 0) && (m_pCullDistance != nullptr)));

            // Extract elements of gl_ClipDistance[] and gl_CullDistance[]
            std::vector<Value*> clipDistance;
            for (uint32_t i = 0; i < clipDistanceCount; ++i)
            {
                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                auto pClipDistance = ExtractValueInst::Create(m_pClipDistance, idxs, "", pInsertPos);
                clipDistance.push_back(pClipDistance);
            }

            std::vector<Value*> cullDistance;
            for (uint32_t i = 0; i < cullDistanceCount; ++i)
            {
                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                auto pCullDistance = ExtractValueInst::Create(m_pCullDistance, idxs, "", pInsertPos);
                cullDistance.push_back(pCullDistance);
            }

            // Merge gl_ClipDistance[] and gl_CullDistance[]
            std::vector<Value*> clipCullDistance;
            for (auto pClipDistance : clipDistance)
            {
                clipCullDistance.push_back(pClipDistance);
            }

            for (auto pCullDistance : cullDistance)
            {
                clipCullDistance.push_back(pCullDistance);
            }

            // Do array padding
            if (clipCullDistance.size() <= 4)
            {
                while (clipCullDistance.size() < 4) // [4 x float]
                {
                    clipCullDistance.push_back(pUndef);
                }
            }
            else
            {
                while (clipCullDistance.size() < 8) // [8 x float]
                {
                    clipCullDistance.push_back(pUndef);
                }
            }

            // NOTE: When gl_PointSize, gl_Layer, or gl_ViewportIndex is used, gl_ClipDistance[] or gl_CullDistance[]
            // should start from pos2.
            uint32_t pos = (usePointSize || useLayer || useViewportIndex) ? EXP_TARGET_POS_2 : EXP_TARGET_POS_1;

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), pos));   // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));   // en
            args.push_back(clipCullDistance[0]);                            // src0
            args.push_back(clipCullDistance[1]);                            // src1
            args.push_back(clipCullDistance[2]);                            // src2
            args.push_back(clipCullDistance[3]);                            // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));

            if (clipCullDistance.size() > 4)
            {
                // Do the second exporting
                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), pos + 1));   // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));       // en
                args.push_back(clipCullDistance[4]);                                // src0
                args.push_back(clipCullDistance[5]);                                // src1
                args.push_back(clipCullDistance[6]);                                // src2
                args.push_back(clipCullDistance[7]);                                // src3
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));      // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));      // vm

                m_pLastExport = cast<CallInst>(
                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            }

            // NOTE: We have to export gl_ClipDistance[] or gl_CullDistancep[] via generic outputs as well.
            LLPC_ASSERT((nextStage == ShaderStageInvalid) || (nextStage == ShaderStageFragment));

            bool hasClipCullExport = true;
            if (nextStage == ShaderStageFragment)
            {
                const auto& nextBuiltInUsage =
                    m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

                hasClipCullExport = ((nextBuiltInUsage.clipDistance > 0) || (nextBuiltInUsage.cullDistance > 0));

                if (hasClipCullExport)
                {
                    // NOTE: We adjust the array size of gl_ClipDistance[] and gl_CullDistance[] according to their
                    // usages in fragment shader.
                    clipDistanceCount = std::min(nextBuiltInUsage.clipDistance, clipDistanceCount);
                    cullDistanceCount = std::min(nextBuiltInUsage.cullDistance, cullDistanceCount);

                    clipCullDistance.clear();
                    for (uint32_t i = 0; i < clipDistanceCount; ++i)
                    {
                        clipCullDistance.push_back(clipDistance[i]);
                    }

                    for (uint32_t i = clipDistanceCount; i < nextBuiltInUsage.clipDistance; ++i)
                    {
                        clipCullDistance.push_back(pUndef);
                    }

                    for (uint32_t i = 0; i < cullDistanceCount; ++i)
                    {
                        clipCullDistance.push_back(cullDistance[i]);
                    }

                    // Do array padding
                    if (clipCullDistance.size() <= 4)
                    {
                        while (clipCullDistance.size() < 4) // [4 x float]
                        {
                            clipCullDistance.push_back(pUndef);
                        }
                    }
                    else
                    {
                        while (clipCullDistance.size() < 8) // [8 x float]
                        {
                            clipCullDistance.push_back(pUndef);
                        }
                    }
                }
            }

            if (hasClipCullExport)
            {
                uint32_t loc = InvalidValue;
                if (m_shaderStage == ShaderStageCopyShader)
                {
                    if (inOutUsage.gs.builtInOutLocs.find(BuiltInClipDistance) !=
                        inOutUsage.gs.builtInOutLocs.end())
                    {
                        loc = inOutUsage.gs.builtInOutLocs[BuiltInClipDistance];
                    }
                    else
                    {
                        LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInCullDistance) !=
                                    inOutUsage.gs.builtInOutLocs.end());
                        loc = inOutUsage.gs.builtInOutLocs[BuiltInCullDistance];
                    }
                }
                else
                {
                    if (inOutUsage.builtInOutputLocMap.find(BuiltInClipDistance) !=
                        inOutUsage.builtInOutputLocMap.end())
                    {
                        loc = inOutUsage.builtInOutputLocMap[BuiltInClipDistance];
                    }
                    else
                    {
                        LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInCullDistance) !=
                            inOutUsage.builtInOutputLocMap.end());
                        loc = inOutUsage.builtInOutputLocMap[BuiltInCullDistance];
                    }
                }

                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                       // en
                args.push_back(clipCullDistance[0]);                                                // src0
                args.push_back(clipCullDistance[1]);                                                // src1
                args.push_back(clipCullDistance[2]);                                                // src2
                args.push_back(clipCullDistance[3]);                                                // src3
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                ++inOutUsage.expCount;

                if (clipCullDistance.size() > 4)
                {
                    // Do the second exporting
                    args.clear();
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc + 1));  // tgt
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                           // en
                    args.push_back(clipCullDistance[4]);                                                    // src0
                    args.push_back(clipCullDistance[5]);                                                    // src1
                    args.push_back(clipCullDistance[6]);                                                    // src2
                    args.push_back(clipCullDistance[7]);                                                    // src3
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                          // done
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                          // vm

                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                    ++inOutUsage.expCount;
                }
            }
        }

        // Export gl_PrimitiveID before entry-point returns
        if (usePrimitiveId)
        {
            bool hasPrimitiveIdExport = false;
            if (nextStage == ShaderStageFragment)
            {
                hasPrimitiveIdExport =
                    m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs.primitiveId;
            }
            else if (nextStage == ShaderStageInvalid)
            {
                if (m_shaderStage == ShaderStageCopyShader)
                {
                    hasPrimitiveIdExport =
                        m_pContext->GetShaderResourceUsage(ShaderStageGeometry)->builtInUsage.gs.primitiveId;
                }
            }

            if (hasPrimitiveIdExport)
            {
                uint32_t loc = InvalidValue;
                if (m_shaderStage == ShaderStageCopyShader)
                {
                    LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInPrimitiveId) !=
                        inOutUsage.gs.builtInOutLocs.end());
                    loc = inOutUsage.gs.builtInOutLocs[BuiltInPrimitiveId];
                }
                else
                {
                    LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInPrimitiveId) !=
                        inOutUsage.builtInOutputLocMap.end());
                    loc = inOutUsage.builtInOutputLocMap[BuiltInPrimitiveId];
                }

                LLPC_ASSERT(m_pPrimitiveId != nullptr);
                Value* pPrimitiveId = new BitCastInst(m_pPrimitiveId, m_pContext->FloatTy(), "", pInsertPos);

                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x1));                       // en
                args.push_back(pPrimitiveId);                                                       // src0
                args.push_back(pUndef);                                                             // src1
                args.push_back(pUndef);                                                             // src2
                args.push_back(pUndef);                                                             // src3
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                ++inOutUsage.expCount;
            }
        }
        // NOTE: If multi-view is enabled, always do exporting for gl_Layer.
        if ((m_gfxIp.major <= 8) && enableMultiView)
        {
            LLPC_ASSERT(m_pLayer != nullptr);
            AddExportInstForBuiltInOutput(m_pLayer, BuiltInLayer, pInsertPos);
        }

        // Export gl_Layer and gl_ViewportIndex before entry-point returns
        if ((m_gfxIp.major >= 9) && (useLayer || useViewportIndex))
        {
            Value* pViewportIndexAndLayer = ConstantInt::get(m_pContext->Int32Ty(), 0);

            if (useViewportIndex)
            {
                LLPC_ASSERT(m_pViewportIndex != nullptr);
                pViewportIndexAndLayer = BinaryOperator::CreateShl(m_pViewportIndex,
                                                                   ConstantInt::get(m_pContext->Int32Ty(), 16),
                                                                   "",
                                                                   pInsertPos);

            }

            if (useLayer)
            {
                LLPC_ASSERT(m_pLayer != nullptr);
                pViewportIndexAndLayer = BinaryOperator::CreateOr(pViewportIndexAndLayer,
                                                                  m_pLayer,
                                                                  "",
                                                                  pInsertPos);
            }

            pViewportIndexAndLayer = new BitCastInst(pViewportIndexAndLayer, m_pContext->FloatTy(), "", pInsertPos);

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_1));  // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x4));               // en
            args.push_back(pUndef);                                                     // src0
            args.push_back(pUndef);                                                     // src1
            args.push_back(pViewportIndexAndLayer);                                     // src2
            args.push_back(pUndef);                                                     // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));              // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));              // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));

            // NOTE: We have to export gl_ViewportIndex via generic outputs as well.
            if (useViewportIndex)
            {
                bool hasViewportIndexExport = true;
                if (nextStage == ShaderStageFragment)
                {
                    const auto& nextBuiltInUsage =
                        m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

                    hasViewportIndexExport = nextBuiltInUsage.viewportIndex;
                }

                if (hasViewportIndexExport)
                {
                    uint32_t loc = InvalidValue;
                    if (m_shaderStage == ShaderStageCopyShader)
                    {
                        LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInViewportIndex) !=
                                    inOutUsage.gs.builtInOutLocs.end());
                        loc = inOutUsage.gs.builtInOutLocs[BuiltInViewportIndex];
                    }
                    else
                    {
                        LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInViewportIndex) !=
                                    inOutUsage.builtInOutputLocMap.end());
                        loc = inOutUsage.builtInOutputLocMap[BuiltInViewportIndex];
                    }

                    Value* pViewportIndex = new BitCastInst(m_pViewportIndex, m_pContext->FloatTy(), "", pInsertPos);

                    args.clear();
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                       // en
                    args.push_back(pViewportIndex);                                                     // src0
                    args.push_back(pUndef);                                                             // src1
                    args.push_back(pUndef);                                                             // src2
                    args.push_back(pUndef);                                                             // src3
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                    ++inOutUsage.expCount;
                }
            }

            // NOTE: We have to export gl_Layer via generic outputs as well.
            if (useLayer)
            {
                bool hasLayerExport = true;
                if (nextStage == ShaderStageFragment)
                {
                    const auto& nextBuiltInUsage =
                        m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

                    hasLayerExport = nextBuiltInUsage.layer|| nextBuiltInUsage.viewIndex;
                }

                if (hasLayerExport)
                {
                    uint32_t loc = InvalidValue;
                    if (m_shaderStage == ShaderStageCopyShader)
                    {
                        LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInLayer) !=
                                    inOutUsage.gs.builtInOutLocs.end() ||
                                    inOutUsage.gs.builtInOutLocs.find(BuiltInViewIndex) !=
                                    inOutUsage.gs.builtInOutLocs.end());

                        loc = enableMultiView ? inOutUsage.gs.builtInOutLocs[BuiltInViewIndex] :
                            inOutUsage.gs.builtInOutLocs[BuiltInLayer];
                    }
                    else
                    {
                        LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInLayer) !=
                                    inOutUsage.builtInOutputLocMap.end() ||
                                    inOutUsage.builtInOutputLocMap.find(BuiltInViewIndex) !=
                                    inOutUsage.builtInOutputLocMap.end());

                        loc = enableMultiView ? inOutUsage.builtInOutputLocMap[BuiltInViewIndex]:
                            inOutUsage.builtInOutputLocMap[BuiltInLayer];
                    }

                    Value* pLayer = new BitCastInst(m_pLayer, m_pContext->FloatTy(), "", pInsertPos);

                    args.clear();
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                       // en
                    args.push_back(pLayer);                                                             // src0
                    args.push_back(pUndef);                                                             // src1
                    args.push_back(pUndef);                                                             // src2
                    args.push_back(pUndef);                                                             // src3
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                    ++inOutUsage.expCount;
                }
            }
        }

        // NOTE: If no generic outputs are present in this shader, we have to export a dummy one
        if (inOutUsage.expCount == 0)
        {
            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                  // en
            args.push_back(pUndef);                                                      // src0
            args.push_back(pUndef);                                                      // src1
            args.push_back(pUndef);                                                      // src2
            args.push_back(pUndef);                                                      // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));               // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));               // vm

            EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
            ++inOutUsage.expCount;
        }
    }
    else if (m_shaderStage == ShaderStageGeometry)
    {
        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), GS_DONE));

        auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageGeometry)->entryArgIdxs.gs;
        auto pWaveId = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.waveId);
        args.push_back(pWaveId);

        EmitCall(m_pModule, "llvm.amdgcn.s.sendmsg", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }
    else if (m_shaderStage == ShaderStageFragment)
    {
        if ((m_gfxIp.major == 6) &&
            ((m_pFragDepth != nullptr) || (m_pFragStencilRef != nullptr) || (m_pSampleMask != nullptr)))
        {
            auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;
            Value* pFragDepth = pUndef;
            Value* pFragStencilRef = pUndef;
            Value* pSampleMask = pUndef;

            uint32_t channelMask = 0x1; // Always export gl_FragDepth
            if (m_pFragDepth != nullptr)
            {
                LLPC_ASSERT(builtInUsage.fragDepth);
                pFragDepth = m_pFragDepth;
            }

            if (m_pFragStencilRef != nullptr)
            {
                LLPC_ASSERT(builtInUsage.fragStencilRef);
                channelMask |= 2;
                pFragStencilRef = m_pFragStencilRef;
            }

            if (m_pSampleMask != nullptr)
            {
                LLPC_ASSERT(builtInUsage.sampleMask);
                channelMask |= 4;
                pSampleMask = m_pSampleMask;
            }

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_Z));  // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), channelMask));           // en

            // src0 ~ src3
            args.push_back(pFragDepth);
            args.push_back(pFragStencilRef);
            args.push_back(pSampleMask);
            args.push_back(pUndef);

            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
        }

        // Export fragment colors
        for (uint32_t location = 0; location < MaxColorTargets; ++location)
        {
            auto& expFragColor = m_expFragColors[location];
            if (expFragColor.size() > 0)
            {
                Value* pOutput = nullptr;
                uint32_t compCount = expFragColor.size();
                LLPC_ASSERT(compCount <= 4);

                // Set CB shader mask
                auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment);
                const uint32_t channelMask = ((1 << compCount) - 1);
                pResUsage->inOutUsage.fs.cbShaderMask |= (channelMask << (4 * location));

                // Construct exported fragment colors
                if (compCount == 1)
                {
                    pOutput = expFragColor[0];
                }
                else
                {
                    const auto pCompTy = expFragColor[0]->getType();

                    pOutput = UndefValue::get(VectorType::get(pCompTy, compCount));
                    for (uint32_t i = 0; i < compCount; ++i)
                    {
                        LLPC_ASSERT(expFragColor[i]->getType() == pCompTy);
                        pOutput = InsertElementInst::Create(pOutput,
                                                            expFragColor[i],
                                                            ConstantInt::get(m_pContext->Int32Ty(), i),
                                                            "",
                                                            pInsertPos);
                    }
                }

                // Do fragment color exporting
                auto pExport = m_pFragColorExport->Run(pOutput, location, pInsertPos);
                if (pExport != nullptr)
                {
                    m_pLastExport = cast<CallInst>(pExport);
                }
            }
        }

        // NOTE: If outputs are present in fragment shader, we have to export a dummy one
        if (m_pLastExport == nullptr)
        {
            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_MRT_0)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x1));              // en
            args.push_back(pZero);                                                     // src0
            args.push_back(pUndef);                                                    // src1
            args.push_back(pUndef);                                                    // src2
            args.push_back(pUndef);                                                    // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));              // vm

            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
        }
    }

    if (m_pLastExport != nullptr)
    {
        // Set "done" flag
        auto exportName = m_pLastExport->getCalledFunction()->getName();
        if (exportName == "llvm.amdgcn.exp.f32")
        {
            m_pLastExport->setOperand(6, ConstantInt::get(m_pContext->BoolTy(), true));
        }
        else
        {
            LLPC_ASSERT(exportName == "llvm.amdgcn.exp.compr.v2f16");
            m_pLastExport->setOperand(4, ConstantInt::get(m_pContext->BoolTy(), true));
        }
    }
}

// =====================================================================================================================
// Patches import calls for generic inputs of vertex shader.
Value* PatchInOutImportExport::PatchVsGenericInputImport(
    Type*        pInputTy,        // [in] Type of input value
    uint32_t     location,        // Location of the input
    uint32_t     compIdx,         // Index used for vector element indexing
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    // Do vertex fetch operations (returns <n x i32>)
    LLPC_ASSERT(m_pVertexFetch != nullptr);
    auto pVertex = m_pVertexFetch->Run(pInputTy, location, compIdx, pInsertPos);

    // Cast vertex fetch results if necessary
    const Type* pVertexTy = pVertex->getType();
    if (pVertexTy != pInputTy)
    {
        LLPC_ASSERT(CanBitCast(pVertexTy, pInputTy));
        pInput = new BitCastInst(pVertex, pInputTy, "", pInsertPos);
    }
    else
    {
        pInput = pVertex;
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for generic inputs of tessellation control shader.
Value* PatchInOutImportExport::PatchTcsGenericInputImport(
    Type*        pInputTy,        // [in] Type of input value
    uint32_t     location,        // Base location of the input
    Value*       pLocOffset,      // [in] Relative location offset
    Value*       pCompIdx,        // [in] Index used for vector element indexing
    Value*       pVertexIdx,      // [in] Input array outermost index used for vertex indexing
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    LLPC_ASSERT((pCompIdx != nullptr) && (pVertexIdx != nullptr));

    auto pLdsOffset = CalcLdsOffsetForTcsInput(pInputTy, location, pLocOffset, pCompIdx, pVertexIdx, pInsertPos);
    return ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);
}

// =====================================================================================================================
// Patches import calls for generic inputs of tessellation evaluation shader.
Value* PatchInOutImportExport::PatchTesGenericInputImport(
    Type*        pInputTy,        // [in] Type of input value
    uint32_t     location,        // Base location of the input
    Value*       pLocOffset,      // [in] Relative location offset
    Value*       pCompIdx,        // [in] Index used for vector element indexing
    Value*       pVertexIdx,      // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(pCompIdx != nullptr);

    auto pLdsOffset = CalcLdsOffsetForTesInput(pInputTy, location, pLocOffset, pCompIdx, pVertexIdx, pInsertPos);
    return ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);
}

// =====================================================================================================================
// Patches import calls for generic inputs of geometry shader.
Value* PatchInOutImportExport::PatchGsGenericInputImport(
    Type*        pInputTy,        // [in] Type of input value
    uint32_t     location,        // Location of the input
    uint32_t     compIdx,         // Index used for vector element indexing
    Value*       pVertexIdx,      // [in] Input array outermost index used for vertex indexing
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(pVertexIdx != nullptr);

    const uint32_t compCount = pInputTy->isVectorTy() ? pInputTy->getVectorNumElements() : 1;
    const uint32_t bitWidth = pInputTy->getScalarSizeInBits();

    Type* pOrigInputTy = pInputTy;

    if (bitWidth == 64)
    {
        compIdx *= 2; // For 64-bit data type, the component indexing must multiply by 2

        // Cast 64-bit data type to float vector
        pInputTy = VectorType::get(m_pContext->FloatTy(), compCount * 2);
    }
    else
    {
        LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32));
    }

    Value* pInput = LoadValueFromEsGsRing(pInputTy, location, compIdx, pVertexIdx, pInsertPos);

    if (pInputTy != pOrigInputTy)
    {
        // Cast back to oringinal input type
        LLPC_ASSERT(CanBitCast(pInputTy, pOrigInputTy));
        LLPC_ASSERT(pInputTy->isVectorTy());

        pInput = new BitCastInst(pInput, pOrigInputTy, "", pInsertPos);
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for generic inputs of fragment shader.
Value* PatchInOutImportExport::PatchFsGenericInputImport(
    Type*        pInputTy,        // [in] Type of input value
    uint32_t     location,        // Base location of the input
    Value*       pLocOffset,      // [in] Relative location offset
    Value*       pCompIdx,        // [in] Index used for vector element indexing (could be null)
    Value*       pAuxInterpValue, // [in] Auxiliary value of interpolation: for non "custom" interpolation, it is the
                                  // explicitly calculated I/J; for "custom" interpolation, it is vertex no. (could be
                                  // null)
    uint32_t     interpMode,      // Interpolation mode
    uint32_t     interpLoc,       // Interpolation location
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment);
    auto& interpInfo = pResUsage->inOutUsage.fs.interpInfo;

    const uint32_t locCount = (pInputTy->getPrimitiveSizeInBits() / 8 > SizeOfVec4) ? 2 : 1;
    while (interpInfo.size() <= location + locCount - 1)
    {
        interpInfo.push_back(InvalidFsInterpInfo);
    }
    interpInfo[location] = { location, (interpMode == InterpModeFlat), (interpMode == InterpModeCustom) };

    if (locCount > 1)
    {
        // The input occupies two consecutive locations
        LLPC_ASSERT(locCount == 2);
        interpInfo[location + 1] = { location + 1, (interpMode == InterpModeFlat), (interpMode == InterpModeCustom) };
    }

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageFragment)->entryArgIdxs.fs;
    auto  pPrimMask    = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.primMask);

    Value* pI  = nullptr;
    Value* pJ  = nullptr;

    // Not "flat" and "custom" interpolation
    if ((interpMode != InterpModeFlat) && (interpMode != InterpModeCustom))
    {
        auto pIJ = pAuxInterpValue;
        if (pIJ == nullptr)
        {
            if (interpMode == InterpModeSmooth)
            {
                if (interpLoc == InterpLocCentroid)
                {
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.centroid);
                }
                else if (interpLoc == InterpLocSample)
                {
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.sample);
                }
                else
                {
                    LLPC_ASSERT(interpLoc == InterpLocCenter);
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.center);
                }
            }
            else
            {
                LLPC_ASSERT(interpMode == InterpModeNoPersp);
                if (interpLoc == InterpLocCentroid)
                {
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.centroid);
                }
                else if (interpLoc == InterpLocSample)
                {
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.sample);
                }
                else
                {
                    LLPC_ASSERT(interpLoc == InterpLocCenter);
                    pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.center);
                }
            }
        }
        pI = ExtractElementInst::Create(pIJ, ConstantInt::get(m_pContext->Int32Ty(), 0), "", pInsertPos);
        pJ = ExtractElementInst::Create(pIJ, ConstantInt::get(m_pContext->Int32Ty(), 1), "", pInsertPos);
    }

    std::vector<Value*> args;

    std::vector<Attribute::AttrKind> attribs;
    attribs.push_back(Attribute::ReadNone);

    Type* pBasicTy = pInputTy->isVectorTy() ? pInputTy->getVectorElementType() : pInputTy;

    const uint32_t compCout = pInputTy->isVectorTy() ? pInputTy->getVectorNumElements() : 1;
    const uint32_t bitWidth = pInputTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

    const uint32_t numChannels = ((bitWidth == 64) ? 2 : 1) * compCout;

    Type* pInterpTy = nullptr;
    if (bitWidth == 16)
    {
        pInterpTy =
            (numChannels > 1) ? VectorType::get(m_pContext->Float16Ty(), numChannels) : m_pContext->Float16Ty();
    }
    else
    {
        pInterpTy = (numChannels > 1) ? VectorType::get(m_pContext->FloatTy(), numChannels) : m_pContext->FloatTy();
    }
    Value* pInterp = UndefValue::get(pInterpTy);

    uint32_t startChannel = 0;
    if (pCompIdx != nullptr)
    {
        startChannel = cast<ConstantInt>(pCompIdx)->getZExtValue();
    }

    Value* pLoc = ConstantInt::get(m_pContext->Int32Ty(), location);
    if (pLocOffset != nullptr)
    {
        pLoc = BinaryOperator::CreateAdd(pLoc, pLocOffset, "", pInsertPos);
        LLPC_ASSERT((startChannel + numChannels) <= 4);
    }

    for (uint32_t i = startChannel; i < startChannel + numChannels; ++i)
    {
        Value* pCompValue = nullptr;

        if ((interpMode != InterpModeFlat) && (interpMode != InterpModeCustom))
        {
            LLPC_ASSERT((pBasicTy->isHalfTy() || pBasicTy->isFloatTy()) && (numChannels <= 4));

            args.clear();
            args.push_back(pI);                                             // i
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), i));     // attr_chan
            args.push_back(pLoc);                                           // attr
            args.push_back(pPrimMask);                                      // m0

            pCompValue = EmitCall(m_pModule,
                                  "llvm.amdgcn.interp.p1",
                                  m_pContext->FloatTy(),
                                  args,
                                  attribs,
                                  pInsertPos);

            args.clear();
            args.push_back(pCompValue);                                     // p1
            args.push_back(pJ);                                             // j
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), i));     // attr_chan
            args.push_back(pLoc);                                           // attr
            args.push_back(pPrimMask);                                      // m0

            pCompValue = EmitCall(m_pModule,
                                  "llvm.amdgcn.interp.p2",
                                  m_pContext->FloatTy(),
                                  args,
                                  attribs,
                                  pInsertPos);

            // TODO: Support 16-bit interpolation.
            if (bitWidth == 16)
            {
                pCompValue = new BitCastInst(pCompValue, m_pContext->Int32Ty(), "", pInsertPos);
                pCompValue = new TruncInst(pCompValue, m_pContext->Int16Ty(), "", pInsertPos);
                pCompValue = new BitCastInst(pCompValue, m_pContext->Float16Ty(), "", pInsertPos);
            }
        }
        else
        {
            InterpParam interpParam = INTERP_PARAM_P0;

            if (interpMode == InterpModeCustom)
            {
                LLPC_ASSERT(isa<ConstantInt>(pAuxInterpValue));
                uint32_t vertexNo = cast<ConstantInt>(pAuxInterpValue)->getZExtValue();

                switch (vertexNo)
                {
                case 0:
                    interpParam = INTERP_PARAM_P0;
                    break;
                case 1:
                    interpParam = INTERP_PARAM_P10;
                    break;
                case 2:
                    interpParam = INTERP_PARAM_P20;
                    break;
                default:
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
            else
            {
                LLPC_ASSERT(interpMode == InterpModeFlat);
            }

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), interpParam));       // param
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), i % 4));             // attr_chan
            args.push_back((pLocOffset != nullptr) ?
                            pLoc :
                            ConstantInt::get(m_pContext->Int32Ty(), location + i / 4)); // attr
            args.push_back(pPrimMask);                                                  // m0

            pCompValue = EmitCall(m_pModule,
                                  "llvm.amdgcn.interp.mov",
                                  m_pContext->FloatTy(),
                                  args,
                                  attribs,
                                  pInsertPos);

            if (bitWidth == 16)
            {
                pCompValue = new BitCastInst(pCompValue, m_pContext->Int32Ty(), "", pInsertPos);
                pCompValue = new TruncInst(pCompValue, m_pContext->Int16Ty(), "", pInsertPos);
                pCompValue = new BitCastInst(pCompValue, m_pContext->Float16Ty(), "", pInsertPos);
            }
        }

        if (numChannels == 1)
        {
            pInterp = pCompValue;
        }
        else
        {
            pInterp = InsertElementInst::Create(pInterp,
                                                pCompValue,
                                                ConstantInt::get(m_pContext->Int32Ty(), i - startChannel),
                                                "",
                                                pInsertPos);
        }
    }

    // Store interpolation results to inputs
    if (pInterpTy == pInputTy)
    {
        pInput = pInterp;
    }
    else
    {
        LLPC_ASSERT(CanBitCast(pInterpTy, pInputTy));
        pInput = new BitCastInst(pInterp, pInputTy, "", pInsertPos);
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for generic outputs of tessellation control shader.
Value* PatchInOutImportExport::PatchTcsGenericOutputImport(
    Type*        pOutputTy,       // [in] Type of output value
    uint32_t     location,        // Base location of the output
    Value*       pLocOffset,      // [in] Relative location offset
    Value*       pCompIdx,        // [in] Index used for vector element indexing
    Value*       pVertexIdx,      // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(pCompIdx != nullptr);

    auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, location, pLocOffset, pCompIdx, pVertexIdx, pInsertPos);
    return ReadValueFromLds(true, pOutputTy, pLdsOffset, pInsertPos);
}

// =====================================================================================================================
// Patches export calls for generic outputs of vertex shader.
void PatchInOutImportExport::PatchVsGenericOutputExport(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Location of the output
    uint32_t     compIdx,        // Index used for vector element indexing
    Instruction* pInsertPos)     // [in] Where to insert the patch instruction
{
    auto pOutputTy = pOutput->getType();

    if (m_hasTs)
    {
        auto pLdsOffset = CalcLdsOffsetForVsOutput(pOutputTy, location, compIdx, pInsertPos);
        WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
    }
    else
    {
        if (m_hasGs)
        {
            LLPC_ASSERT(pOutputTy->isIntOrIntVectorTy() || pOutputTy->isFPOrFPVectorTy());

            const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
            if (bitWidth == 64)
            {
                // For 64-bit data type, the component indexing must multiply by 2
                compIdx *= 2;

                uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() * 2 : 2;

                pOutputTy = VectorType::get(m_pContext->FloatTy(), compCount);
                pOutput = new BitCastInst(pOutput, pOutputTy, "", pInsertPos);
            }
            else
            {
                LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32));
            }

            StoreValueToEsGsRing(pOutput, location, compIdx, pInsertPos);
        }
        else
        {
            AddExportInstForGenericOutput(pOutput, location, compIdx, pInsertPos);
        }
    }
}

// =====================================================================================================================
// Patches export calls for generic outputs of tessellation control shader.
void PatchInOutImportExport::PatchTcsGenericOutputExport(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Base location of the output
    Value*       pLocOffset,     // [in] Relative location offset
    Value*       pCompIdx,       // [in] Index used for vector element indexing
    Value*       pVertexIdx,     // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)     // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(pCompIdx != nullptr);

    Type* pOutputTy = pOutput->getType();
    auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, location, pLocOffset, pCompIdx, pVertexIdx, pInsertPos);
    WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
}

// =====================================================================================================================
// Patches export calls for generic outputs of tessellation evaluation shader.
void PatchInOutImportExport::PatchTesGenericOutputExport(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Location of the output
    uint32_t     compIdx,        // Index used for vector element indexing
    Instruction* pInsertPos)     // [in] Where to insert the patch instruction
{
    if (m_hasGs)
    {
        auto pOutputTy = pOutput->getType();
        LLPC_ASSERT(pOutputTy->isIntOrIntVectorTy() || pOutputTy->isFPOrFPVectorTy());

        const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
        if (bitWidth == 64)
        {
            // For 64-bit data type, the component indexing must multiply by 2
            compIdx *= 2;

            uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() * 2 : 2;
            pOutputTy = VectorType::get(m_pContext->FloatTy(), compCount);

            pOutput = new BitCastInst(pOutput, pOutputTy, "", pInsertPos);
        }
        else
        {
            LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32));
        }

        StoreValueToEsGsRing(pOutput, location, compIdx, pInsertPos);
    }
    else
    {
        AddExportInstForGenericOutput(pOutput, location, compIdx, pInsertPos);
    }
}

// =====================================================================================================================
// Patches export calls for generic outputs of geometry shader.
void PatchInOutImportExport::PatchGsGenericOutputExport(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Location of the output
    uint32_t     compIdx,        // Index used for vector element indexing
    uint32_t     streamId,       // ID of output vertex stream
    Instruction* pInsertPos)     // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(streamId == 0); // TODO: Multiple output streams are not supported.

    auto pOutputTy = pOutput->getType();

    // Cast double or double vector to float vector.
    const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
    if (bitWidth == 64)
    {
        // For 64-bit data type, the component indexing must multiply by 2
        compIdx *= 2;

        if (pOutputTy->isVectorTy())
        {
            pOutputTy = VectorType::get(m_pContext->FloatTy(), pOutputTy->getVectorNumElements() * 2);
        }
        else
        {
            pOutputTy = m_pContext->Floatx2Ty();
        }

        pOutput = new BitCastInst(pOutput, pOutputTy, "", pInsertPos);
    }
    else
    {
        LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32));
    }

    const uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() : 1;
    // NOTE: Currently, to simplify the design of load/store data from GS-VS ring, we always extend WORD to DWORD and
    // store DWORD to GS-VS ring. So for 16-bit data type, the actual byte size is based on number of DWORDs.
    const uint32_t byteSize = ((bitWidth == 16) ? 2 : 1) * (pOutputTy->getScalarSizeInBits() / 8) * compCount;

    LLPC_ASSERT(compIdx <= 4);
    auto& genericOutByteSizes =
        m_pContext->GetShaderResourceUsage(ShaderStageGeometry)->inOutUsage.gs.genericOutByteSizes;
    genericOutByteSizes[location][compIdx] = byteSize;

    if (compCount == 1)
    {
        StoreValueToGsVsRingBuffer(pOutput, location, compIdx, pInsertPos);
    }
    else
    {
        for (uint32_t i = 0; i < compCount; ++i)
        {
            auto pComp = ExtractElementInst::Create(pOutput,
                                                    ConstantInt::get(m_pContext->Int32Ty(), i),
                                                    "",
                                                    pInsertPos);
            StoreValueToGsVsRingBuffer(pComp, location + ((compIdx + i) / 4), (compIdx + i) % 4, pInsertPos);
        }
    }

}

// =====================================================================================================================
// Patches export calls for generic outputs of fragment shader.
void PatchInOutImportExport::PatchFsGenericOutputExport(
    Value*       pOutput,         // [in] Output value
    uint32_t     location,        // Location of the output
    uint32_t     compIdx,         // Index used for vector element indexing
    Instruction* pInsertPos)      // [in] Where to insert the patch instruction
{
    Type* pOutputTy = pOutput->getType();

    const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32));

    auto pCompTy = pOutputTy->isVectorTy() ? pOutputTy->getVectorElementType() : pOutputTy;
    uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() : 1;

    std::vector<Value*> outputComps;
    for (uint32_t i = 0; i < compCount; ++i)
    {
        Value* pOutputComp = nullptr;
        if (compCount == 1)
        {
            pOutputComp = pOutput;
        }
        else
        {
            pOutputComp = ExtractElementInst::Create(pOutput,
                                                     ConstantInt::get(m_pContext->Int32Ty(), i),
                                                     "",
                                                     pInsertPos);
        }

        outputComps.push_back(pOutputComp);
    }

    LLPC_ASSERT(location < MaxColorTargets);
    auto& expFragColor = m_expFragColors[location];

    while (compIdx + compCount > expFragColor.size())
    {
        expFragColor.push_back(UndefValue::get(pCompTy));
    }

    for (uint32_t i = 0; i < compCount; ++i)
    {
        expFragColor[compIdx + i] = outputComps[i];
    }
}

// =====================================================================================================================
// Patches import calls for built-in inputs of vertex shader.
Value* PatchInOutImportExport::PatchVsBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageVertex)->entryArgIdxs.vs;
    auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;

    switch (builtInId)
    {
    case BuiltInVertexIndex:
        {
            pInput = m_pVertexFetch->GetVertexIndex();
            break;
        }
    case BuiltInInstanceIndex:
        {
            pInput = m_pVertexFetch->GetInstanceIndex();
            break;
        }
    case BuiltInBaseVertex:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.baseVertex);
            break;
        }
    case BuiltInBaseInstance:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.baseInstance);
            break;
        }
    case BuiltInDrawIndex:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.drawIndex);
            break;
        }
    case BuiltInViewIndex:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);
            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.deviceIndex);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in inputs of tessellation control shader.
Value* PatchInOutImportExport::PatchTcsBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Value*       pElemIdx,      // [in] Index used for array/vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl);
    auto& inoutUsage = pResUsage->inOutUsage;
    auto& builtInInLocMap = inoutUsage.builtInInputLocMap;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsInput(pInputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
            pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInPointSize:
        {
            LLPC_ASSERT(pElemIdx == nullptr);
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsInput(pInputTy, loc, nullptr, nullptr, pVertexIdx, pInsertPos);
            pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInClipDistance:
    case BuiltInCullDistance:
        {
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_ClipDistanceIn[]/gl_CullDistanceIn[] is treated as 2 x vec4
                LLPC_ASSERT(pInputTy->isArrayTy());

                auto pElemTy = pInputTy->getArrayElementType();
                for (uint32_t i = 0; i < pInputTy->getArrayNumElements(); ++i)
                {
                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTcsInput(pElemTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    auto pElem = ReadValueFromLds(false, pElemTy, pLdsOffset, pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    pInput = InsertValueInst::Create(pInput, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTcsInput(pInputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);
            }

            break;
        }
    case BuiltInPatchVertices:
        {
            auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.patchControlPoints);
            break;
        }
    case BuiltInPrimitiveId:
        {
            pInput = inoutUsage.tcs.pPrimitiveId;
            break;
        }
    case BuiltInInvocationId:
        {
            pInput = inoutUsage.tcs.pInvocationId;
            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.deviceIndex);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in inputs of tessellation evaluation shader.
Value* PatchInOutImportExport::PatchTesBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Value*       pElemIdx,      // [in] Index used for array/vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageTessEval)->entryArgIdxs.tes;

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval);
    auto& inOutUsage = pResUsage->inOutUsage;
    auto& builtInInLocMap = inOutUsage.builtInInputLocMap;
    auto& perPatchBuiltInInLocMap = inOutUsage.perPatchBuiltInInputLocMap;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTesInput(pInputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
            pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInPointSize:
        {
            LLPC_ASSERT(pElemIdx == nullptr);
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTesInput(pInputTy, loc, nullptr, nullptr, pVertexIdx, pInsertPos);
            pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInClipDistance:
    case BuiltInCullDistance:
        {
            LLPC_ASSERT(builtInInLocMap.find(builtInId) != builtInInLocMap.end());
            const uint32_t loc = builtInInLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_ClipDistanceIn[]/gl_CullDistanceIn[] is treated as 2 x vec4
                LLPC_ASSERT(pInputTy->isArrayTy());

                auto pElemTy = pInputTy->getArrayElementType();
                for (uint32_t i = 0; i < pInputTy->getArrayNumElements(); ++i)
                {
                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTesInput(pElemTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    auto pElem = ReadValueFromLds(false, pElemTy, pLdsOffset, pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    pInput = InsertValueInst::Create(pInput, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTesInput(pInputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);
            }

            break;
        }
    case BuiltInPatchVertices:
        {
            uint32_t patchVertices = MaxTessPatchVertices;
            const bool hasTcs = ((m_pContext->GetShaderStageMask() & ShaderStageToMask(ShaderStageTessControl)) != 0);
            if (hasTcs)
            {
                const auto& tcsBuiltInUsage =
                    m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
                patchVertices = tcsBuiltInUsage.outputVertices;
            }

            pInput = ConstantInt::get(m_pContext->Int32Ty(), patchVertices);

            break;
        }
    case BuiltInPrimitiveId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.patchId);
            break;
        }
    case BuiltInTessCoord:
        {
            Value* pTessCoord = inOutUsage.tes.pTessCoord;

            if (pElemIdx != nullptr)
            {
                pInput = ExtractElementInst::Create(pTessCoord, pElemIdx, "", pInsertPos);
            }
            else
            {
                pInput = pTessCoord;
            }

            break;
        }
    case BuiltInTessLevelOuter:
    case BuiltInTessLevelInner:
        {
            LLPC_ASSERT(perPatchBuiltInInLocMap.find(builtInId) != perPatchBuiltInInLocMap.end());
            uint32_t loc = perPatchBuiltInInLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_TessLevelOuter[4] is treated as vec4
                // gl_TessLevelInner[2] is treated as vec2
                LLPC_ASSERT(pInputTy->isArrayTy());

                auto pElemTy = pInputTy->getArrayElementType();
                for (uint32_t i = 0; i < pInputTy->getArrayNumElements(); ++i)
                {
                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTesInput(pElemTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    auto pElem = ReadValueFromLds(false, pElemTy, pLdsOffset, pInsertPos);
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    pInput = InsertValueInst::Create(pInput, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTesInput(pInputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                pInput = ReadValueFromLds(false, pInputTy, pLdsOffset, pInsertPos);
            }

            break;
        }
    case BuiltInViewIndex:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);
            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.deviceIndex);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in inputs of geometry shader.
Value* PatchInOutImportExport::PatchGsBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Value*       pVertexIdx,    // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = nullptr;

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageGeometry)->entryArgIdxs.gs;
    auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry)->builtInUsage.gs;
    auto& inOutUsage   = m_pContext->GetShaderResourceUsage(ShaderStageGeometry)->inOutUsage;

    uint32_t loc = inOutUsage.builtInInputLocMap[builtInId];
    LLPC_ASSERT(loc != InvalidValue);

    switch (builtInId)
    {
    case BuiltInPosition:
    case BuiltInPointSize:
        {
            pInput = LoadValueFromEsGsRing(pInputTy, loc, 0, pVertexIdx, pInsertPos);
            break;
        }
    case BuiltInClipDistance:
        {
            pInput = UndefValue::get(pInputTy);
            for (uint32_t i = 0; i < builtInUsage.clipDistanceIn; ++i)
            {
                auto pComp = LoadValueFromEsGsRing(pInputTy->getArrayElementType(),
                                                   loc + i / 4,
                                                   i % 4,
                                                   pVertexIdx,
                                                   pInsertPos);

                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                pInput = InsertValueInst::Create(pInput, pComp, idxs, "", pInsertPos);
            }
            break;
        }
    case BuiltInCullDistance:
        {
            pInput = UndefValue::get(pInputTy);
            for (uint32_t i = 0; i < builtInUsage.cullDistanceIn; ++i)
            {
                auto pComp = LoadValueFromEsGsRing(pInputTy->getArrayElementType(),
                                                   loc + i / 4,
                                                   i % 4,
                                                   pVertexIdx,
                                                   pInsertPos);

                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                pInput = InsertValueInst::Create(pInput, pComp, idxs, "", pInsertPos);
            }
            break;
        }
    case BuiltInPrimitiveId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.primitiveId);
            break;
        }
    case BuiltInInvocationId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.invocationId);
            break;
        }
    case BuiltInViewIndex:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.viewIndex);
            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.deviceIndex);
            break;
        }
    // Handle internal-use built-ins
    case BuiltInWaveId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.waveId);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in inputs of fragment shader.
Value* PatchInOutImportExport::PatchFsBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = UndefValue::get(pInputTy);

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageFragment)->entryArgIdxs.fs;
    auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;
    auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment)->inOutUsage;

    std::vector<Value*> args;

    switch (builtInId)
    {
    case BuiltInSampleMask:
        {
            LLPC_ASSERT(pInputTy->isArrayTy());

            auto pSampleCoverage = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.sampleCoverage);
            auto pAncillary = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.ancillary);
            auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());

            // gl_SampleID = Ancillary[11:8]
            std::vector<Value*> args;
            args.push_back(pAncillary);
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 8));
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 4));
            auto pSampleId =
                EmitCall(m_pModule, "llvm.amdgcn.ubfe.i32", m_pContext->Int32Ty(), args, NoAttrib, pInsertPos);

            auto pSampleMaskIn = pSampleCoverage;
            if (pPipelineInfo->rsState.perSampleShading)
            {
                // gl_SampleMaskIn[0] = (SampleCoverage & (1 << gl_SampleID))
                pSampleMaskIn = BinaryOperator::CreateShl(ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                          pSampleId,
                                                          "",
                                                          pInsertPos);
                pSampleMaskIn = BinaryOperator::CreateAnd(pSampleCoverage, pSampleMaskIn, "", pInsertPos);
            }

            // NOTE: Only gl_SampleMaskIn[0] is valid for us.
            std::vector<uint32_t> idxs;
            idxs.push_back(0);
            pInput = InsertValueInst::Create(pInput, pSampleMaskIn, idxs, "", pInsertPos);

            break;
        }
    case BuiltInFragCoord:
        {
            // TODO: Support layout qualifiers "pixel_center_integer" and "origin_upper_left".
            Value* fragCoord[4] =
            {
                GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.x),
                GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.y),
                GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.z),
                GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.w),
            };

            std::vector<Attribute::AttrKind> attribs;
            attribs.push_back(Attribute::ReadNone);

            args.clear();
            args.push_back(fragCoord[3]);

            fragCoord[3] =
                EmitCall(m_pModule, "llvm.amdgcn.rcp.f32", m_pContext->FloatTy(), args, attribs, pInsertPos);

            for (uint32_t i = 0; i < 4; ++i)
            {
                pInput = InsertElementInst::Create(pInput,
                                                   fragCoord[i],
                                                   ConstantInt::get(m_pContext->Int32Ty(), i),
                                                   "",
                                                   pInsertPos);
            }

            break;
        }
    case BuiltInFrontFacing:
        {
            auto pFrontFacing = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.frontFacing);
            pInput = new ICmpInst(pInsertPos,
                                  ICmpInst::ICMP_NE,
                                  pFrontFacing,
                                  ConstantInt::get(m_pContext->Int32Ty(),
                                  0));
            pInput = CastInst::Create(Instruction::ZExt, pInput, m_pContext->Int8Ty(), "", pInsertPos);
            break;
        }
    case BuiltInPointCoord:
        {
            LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInPointCoord) != inOutUsage.builtInInputLocMap.end());
            const uint32_t loc = inOutUsage.builtInInputLocMap[BuiltInPointCoord];

            auto& interpInfo = inOutUsage.fs.interpInfo;
            while (interpInfo.size() <= loc)
            {
                interpInfo.push_back(InvalidFsInterpInfo);
            }
            interpInfo[loc] = { loc, false, false };

            // Emulation for "in vec2 gl_PointCoord"
            pInput = PatchFsGenericInputImport(pInputTy,
                                               loc,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               InterpModeSmooth,
                                               InterpLocCenter,
                                               pInsertPos);
            break;
        }
    case BuiltInHelperInvocation:
        {
            std::vector<Attribute::AttrKind> attribs;
            attribs.push_back(Attribute::ReadNone);

            pInput = EmitCall(m_pModule, "llvm.amdgcn.ps.live", m_pContext->BoolTy(), args, attribs, pInsertPos);
            pInput = BinaryOperator::CreateNot(pInput, "", pInsertPos);

            pInput = new ZExtInst(pInput, m_pContext->Int8Ty(), "", pInsertPos);
            break;
        }
    case BuiltInPrimitiveId:
    case BuiltInLayer:
    case BuiltInViewportIndex:
    case BuiltInViewIndex:
        {
            uint32_t loc = InvalidValue;

            if (builtInId == BuiltInPrimitiveId)
            {
                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInPrimitiveId) !=
                            inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInPrimitiveId];
            }
            else if (builtInId == BuiltInLayer)
            {
                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInLayer) != inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInLayer];
            }
            else if (builtInId == BuiltInViewIndex)
            {
                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInViewIndex) != inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInViewIndex];
            }
            else
            {
                LLPC_ASSERT(builtInId == BuiltInViewportIndex);

                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInViewportIndex) !=
                            inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInViewportIndex];
            }

            auto& interpInfo = inOutUsage.fs.interpInfo;
            while (interpInfo.size() <= loc)
            {
                interpInfo.push_back(InvalidFsInterpInfo);
            }
            interpInfo[loc] = { loc, true, false }; // Flat interpolation

            // Emulation for "in int gl_PrimitiveID" or "in int gl_Layer" or "in int gl_ViewportIndex"
            // or "in int gl_ViewIndex"
            pInput = PatchFsGenericInputImport(pInputTy,
                                               loc,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               InterpModeFlat,
                                               InterpLocCenter,
                                               pInsertPos);
            break;
        }
    case BuiltInClipDistance:
    case BuiltInCullDistance:
        {
            LLPC_ASSERT(pInputTy->isArrayTy());

            uint32_t loc = InvalidValue;
            uint32_t locCount = 0;
            uint32_t startChannel = 0;

            if (builtInId == BuiltInClipDistance)
            {
                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInClipDistance];
                locCount = (builtInUsage.clipDistance > 4) ? 2 : 1;
                startChannel = 0;
            }
            else
            {
                LLPC_ASSERT(builtInId == BuiltInCullDistance);

                LLPC_ASSERT(inOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            inOutUsage.builtInInputLocMap.end());
                loc = inOutUsage.builtInInputLocMap[BuiltInCullDistance];
                locCount = (builtInUsage.clipDistance + builtInUsage.cullDistance > 4) ? 2 : 1;
                startChannel = builtInUsage.clipDistance;
            }

            auto& interpInfo = inOutUsage.fs.interpInfo;
            while (interpInfo.size() <= loc + locCount -1)
            {
                interpInfo.push_back(InvalidFsInterpInfo);
            }

            interpInfo[loc] = { loc, false, false };
            if (locCount > 1)
            {
                interpInfo[loc + 1] = { loc + 1, false, false };
            }

            // Emulation for "in float gl_ClipDistance[]" or "in float gl_CullDistance[]"
            auto pPrimMask = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.primMask);
            auto pIJ = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.center);

            pIJ = new BitCastInst(pIJ, m_pContext->Floatx2Ty(), "", pInsertPos);
            auto pI = ExtractElementInst::Create(pIJ, ConstantInt::get(m_pContext->Int32Ty(), 0), "", pInsertPos);
            auto pJ = ExtractElementInst::Create(pIJ, ConstantInt::get(m_pContext->Int32Ty(), 1), "", pInsertPos);

            std::vector<Attribute::AttrKind> attribs;
            attribs.push_back(Attribute::ReadNone);

            const uint32_t elemCount = pInputTy->getArrayNumElements();
            LLPC_ASSERT(elemCount <= MaxClipCullDistanceCount);

            for (uint32_t i = 0; i < elemCount; ++i)
            {
                args.clear();
                args.push_back(pI);                                                                     // i
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), (startChannel + i) % 4));        // attr_chan
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), loc + (startChannel + i) / 4));  // attr
                args.push_back(pPrimMask);                                                              // m0

                auto pCompValue = EmitCall(m_pModule,
                                           "llvm.amdgcn.interp.p1",
                                           m_pContext->FloatTy(),
                                           args,
                                           attribs,
                                           pInsertPos);

                args.clear();
                args.push_back(pCompValue);                                                             // p1
                args.push_back(pJ);                                                                     // j
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), (startChannel + i) % 4));        // attr_chan
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), loc + (startChannel + i) / 4));  // attr
                args.push_back(pPrimMask);                                                              // m0

                pCompValue = EmitCall(m_pModule,
                                      "llvm.amdgcn.interp.p2",
                                      m_pContext->FloatTy(),
                                      args,
                                      attribs,
                                      pInsertPos);

                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                pInput = InsertValueInst::Create(pInput, pCompValue, idxs, "", pInsertPos);
            }

            break;
        }
    case BuiltInSampleId:
        {
            auto pAncillary = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.ancillary);

            // gl_SampleID = Ancillary[11:8]
            std::vector<Value*> args;
            args.push_back(pAncillary);
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 8));
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 4));
            pInput = EmitCall(m_pModule, "llvm.amdgcn.ubfe.i32", pInputTy, args, NoAttrib, pInsertPos);

            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->iaState.deviceIndex);
            break;
        }
    // Handle internal-use built-ins for sample position emulation
    case BuiltInNumSamples:
        {
            auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->rsState.numSamples);
            break;
        }
    case BuiltInSamplePatternIdx:
        {
            auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->rsState.samplePatternIdx);
            break;
        }
    // Handle internal-use built-ins for interpolation functions and AMD extension (AMD_shader_explicit_vertex_parameter)
    case BuiltInInterpPerspSample:
    case BuiltInBaryCoordSmoothSampleAMD:
        {
            LLPC_ASSERT(entryArgIdxs.perspInterp.sample != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.sample);
            break;
        }
    case BuiltInInterpPerspCenter:
    case BuiltInBaryCoordSmoothAMD:
        {
            LLPC_ASSERT(entryArgIdxs.perspInterp.center != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.center);
            break;
        }
    case BuiltInInterpPerspCentroid:
    case BuiltInBaryCoordSmoothCentroidAMD:
        {
            LLPC_ASSERT(entryArgIdxs.perspInterp.centroid != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.centroid);
            break;
        }
    case BuiltInInterpPullMode:
    case BuiltInBaryCoordPullModelAMD:
        {
            LLPC_ASSERT(entryArgIdxs.perspInterp.pullMode != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.perspInterp.pullMode);
            break;
        }
    case BuiltInInterpLinearSample:
    case BuiltInBaryCoordNoPerspSampleAMD:
        {
            LLPC_ASSERT(entryArgIdxs.linearInterp.sample != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.sample);
            break;
        }
    case BuiltInInterpLinearCenter:
    case BuiltInBaryCoordNoPerspAMD:
        {
            LLPC_ASSERT(entryArgIdxs.linearInterp.center != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.center);
            break;
        }
    case BuiltInInterpLinearCentroid:
    case BuiltInBaryCoordNoPerspCentroidAMD:
        {
            LLPC_ASSERT(entryArgIdxs.linearInterp.centroid != 0);
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.linearInterp.centroid);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in inputs of compute shader.
Value* PatchInOutImportExport::PatchCsBuiltInInputImport(
    Type*        pInputTy,      // [in] Type of input value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pInput = nullptr;

    auto  pIntfData    = m_pContext->GetShaderInterfaceData(ShaderStageCompute);
    auto& entryArgIdxs = pIntfData->entryArgIdxs.cs;
    auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageCompute)->builtInUsage.cs;

    switch (builtInId)
    {
    case BuiltInWorkgroupSize:
        {
            auto pWorkgroupSizeX = ConstantInt::get(m_pContext->Int32Ty(), builtInUsage.workgroupSizeX);
            auto pWorkgroupSizeY = ConstantInt::get(m_pContext->Int32Ty(), builtInUsage.workgroupSizeY);
            auto pWorkgroupSizeZ = ConstantInt::get(m_pContext->Int32Ty(), builtInUsage.workgroupSizeZ);

            std::vector<Constant*> workgroupSizes;
            workgroupSizes.push_back(pWorkgroupSizeX);
            workgroupSizes.push_back(pWorkgroupSizeY);
            workgroupSizes.push_back(pWorkgroupSizeZ);

            pInput = ConstantVector::get(workgroupSizes);
            break;
        }
    case BuiltInNumWorkgroups:
        {
            pInput = pIntfData->pNumWorkgroups;
            break;
        }
    case BuiltInWorkgroupId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.workgroupId);
            break;
        }
    case BuiltInLocalInvocationId:
        {
            pInput = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.localInvocationId);
            break;
        }
    case BuiltInDeviceIndex:
        {
            auto pPipelineInfo = reinterpret_cast<const ComputePipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            pInput = ConstantInt::get(m_pContext->Int32Ty(), pPipelineInfo->deviceIndex);
            break;
        }
    case BuiltInNumSubgroups:
        {
            // workgroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z
            const uint32_t workgroupSize = builtInUsage.workgroupSizeX *
                                           builtInUsage.workgroupSizeY *
                                           builtInUsage.workgroupSizeZ;

            // gl_NumSubgroups = (workgroupSize + gl_SubGroupSize - 1) / gl_SubgroupSize
            const uint32_t subgroupSize = m_pContext->GetGpuProperty()->waveSize;
            const uint32_t numSubgroups = (workgroupSize + subgroupSize - 1) / subgroupSize;

            pInput = ConstantInt::get(m_pContext->Int32Ty(), numSubgroups);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pInput;
}

// =====================================================================================================================
// Patches import calls for built-in outputs of tessellation control shader.
Value* PatchInOutImportExport::PatchTcsBuiltInOutputImport(
    Type*        pOutputTy,     // [in] Type of output value
    uint32_t     builtInId,     // ID of the built-in variable
    Value*       pElemIdx,      // [in] Index used for array/vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    Value* pOutput = UndefValue::get(pOutputTy);

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl);
    auto& builtInUsage = pResUsage->builtInUsage.tcs;
    auto& inoutUsage = pResUsage->inOutUsage;
    auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;
    auto& perPatchBuiltInOutLocMap = pResUsage->inOutUsage.perPatchBuiltInOutputLocMap;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            LLPC_ASSERT(builtInUsage.position);

            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
            pOutput = ReadValueFromLds(true, pOutputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInPointSize:
        {
            LLPC_ASSERT(builtInUsage.pointSize);

            LLPC_ASSERT(pElemIdx == nullptr);
            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, nullptr, pVertexIdx, pInsertPos);
            pOutput = ReadValueFromLds(true, pOutputTy, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInClipDistance:
    case BuiltInCullDistance:
        {
            if (builtInId == BuiltInClipDistance)
            {
                LLPC_ASSERT(builtInUsage.clipDistance > 0);
            }
            else
            {
                LLPC_ASSERT(builtInId == BuiltInCullDistance);
                LLPC_ASSERT(builtInUsage.cullDistance > 0);
            }

            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_ClipDistance[]/gl_CullDistance[] is treated as 2 x vec4
                LLPC_ASSERT(pOutputTy->isArrayTy());

                auto pElemTy = pOutputTy->getArrayElementType();
                for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTcsOutput(pElemTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    auto pElem = ReadValueFromLds(true, pElemTy, pLdsOffset, pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    pOutput = InsertValueInst::Create(pOutput, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                pOutput = ReadValueFromLds(true, pOutputTy, pLdsOffset, pInsertPos);
            }

            break;
        }
    case BuiltInTessLevelOuter:
    case BuiltInTessLevelInner:
        {
            if (builtInId == BuiltInTessLevelOuter)
            {
                LLPC_ASSERT(builtInUsage.tessLevelOuter);
            }
            else
            {
                LLPC_ASSERT(builtInId == BuiltInTessLevelInner);
                LLPC_ASSERT(builtInUsage.tessLevelInner);
            }

            LLPC_ASSERT(perPatchBuiltInOutLocMap.find(builtInId) != perPatchBuiltInOutLocMap.end());
            uint32_t loc = perPatchBuiltInOutLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_TessLevelOuter[4] is treated as vec4
                // gl_TessLevelInner[2] is treated as vec2
                LLPC_ASSERT(pOutputTy->isArrayTy());

                auto pElemTy = pOutputTy->getArrayElementType();
                for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTcsOutput(pElemTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    auto pElem = ReadValueFromLds(true, pElemTy, pLdsOffset, pInsertPos);

                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    pOutput = InsertValueInst::Create(pOutput, pElem, idxs, "", pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                pOutput = ReadValueFromLds(true, pOutputTy, pLdsOffset, pInsertPos);
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return pOutput;
}

// =====================================================================================================================
// Patches export calls for built-in outputs of vertex shader.
void PatchInOutImportExport::PatchVsBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    auto pOutputTy = pOutput->getType();

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageVertex);
    auto& builtInUsage = pResUsage->builtInUsage.vs;
    auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;

    std::vector<Value*> args;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            if (builtInUsage.position == false)
            {
                const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);
                return;
            }

            if (m_hasTs)
            {
                uint32_t loc = builtInOutLocMap[builtInId];
                auto pLdsOffset = CalcLdsOffsetForVsOutput(pOutputTy, loc, 0, pInsertPos);
                WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
            }
            else
            {
                if (m_hasGs)
                {
                    LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                    uint32_t loc = builtInOutLocMap[builtInId];

                    StoreValueToEsGsRing(pOutput, loc, 0, pInsertPos);
                }
                else
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
            }

            break;
        }
    case BuiltInPointSize:
        {
            if (builtInUsage.pointSize == false)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_PointSize is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.pointSize = false;
                return;
            }

            if (m_hasTs)
            {
                uint32_t loc = builtInOutLocMap[builtInId];
                auto pLdsOffset = CalcLdsOffsetForVsOutput(pOutputTy, loc, 0, pInsertPos);
                WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
            }
            else
            {
                if (m_hasGs)
                {
                    LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                    uint32_t loc = builtInOutLocMap[builtInId];

                    StoreValueToEsGsRing(pOutput, loc, 0, pInsertPos);
                }
                else
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
            }

            break;
        }
    case BuiltInClipDistance:
        {
            if (builtInUsage.clipDistance == 0)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_ClipDistance[] is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.clipDistance = 0;
                return;
            }

            if (m_hasTs)
            {
                LLPC_ASSERT(pOutputTy->isArrayTy());

                uint32_t loc = builtInOutLocMap[builtInId];
                auto pLdsOffset = CalcLdsOffsetForVsOutput(pOutputTy->getArrayElementType(), loc, 0, pInsertPos);

                for (int i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                    WriteValueToLds(pElem, pLdsOffset, pInsertPos);

                    pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset,
                                                           ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                           "",
                                                           pInsertPos);
                }
            }
            else
            {
                if (m_hasGs)
                {
                    LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                    uint32_t loc = builtInOutLocMap[builtInId];

                    pOutputTy = pOutput->getType();
                    for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);
                        auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                        StoreValueToEsGsRing(pElem, loc + i / 4, i % 4, pInsertPos);
                    }
                }
                else
                {
                    // NOTE: The export of gl_ClipDistance[] is delayed and is done before entry-point returns.
                    m_pClipDistance = pOutput;
                }
            }

            break;
        }
    case BuiltInCullDistance:
        {
            if (builtInUsage.cullDistance == 0)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_CullDistance[] is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.cullDistance = 0;
                return;
            }

            if (m_hasTs)
            {
                LLPC_ASSERT(pOutputTy->isArrayTy());

                uint32_t loc = builtInOutLocMap[builtInId];
                auto pLdsOffset = CalcLdsOffsetForVsOutput(pOutputTy->getArrayElementType(), loc, 0, pInsertPos);

                for (int i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                    WriteValueToLds(pElem, pLdsOffset, pInsertPos);

                    pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset,
                                                           ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                           "",
                                                           pInsertPos);
                }
            }
            else
            {
                if (m_hasGs)
                {
                    LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                    uint32_t loc = builtInOutLocMap[builtInId];

                    pOutputTy = pOutput->getType();
                    for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);
                        auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                        StoreValueToEsGsRing(pElem, loc + i / 4, i % 4, pInsertPos);
                    }
                }
                else
                {
                    // NOTE: The export of gl_CullDistance[] is delayed and is done before entry-point returns.
                    m_pCullDistance = pOutput;
                }
            }

            break;
        }
    case BuiltInLayer:
        {
            if (builtInUsage.layer == false)
            {
                return;
            }

            const auto enableMultiView = static_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo())->iaState.enableMultiView;

            // NOTE: Only last non-fragment shader stage has to export the value of gl_Layer.
            if ((m_hasTs == false) && (m_hasGs == false) && (enableMultiView == false))
            {
                if (m_gfxIp.major <= 8)
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
                else
                {
                    // NOTE: The export of gl_Layer is delayed and is done before entry-point returns.
                    m_pLayer = pOutput;
                }
            }

            break;
        }
    case BuiltInViewportIndex:
        {
            if (builtInUsage.viewportIndex == false)
            {
                return;
            }

            // NOTE: Only last non-fragment shader stage has to export the value of gl_ViewportIndex.
            if ((m_hasTs == false) && (m_hasGs == false))
            {
                if (m_gfxIp.major <= 8)
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
                else
                {
                    // NOTE: The export of gl_ViewportIndex is delayed and is done before entry-point returns.
                    m_pViewportIndex = pOutput;
                }
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Patches export calls for built-in outputs of tessellation control shader.
void PatchInOutImportExport::PatchTcsBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Value*       pElemIdx,      // [in] Index used for array/vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Input array outermost index used for vertex indexing (could be null)
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    auto pOutputTy = pOutput->getType();

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl);
    auto& builtInUsage = pResUsage->builtInUsage.tcs;
    auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;
    auto& perPatchBuiltInOutLocMap = pResUsage->inOutUsage.perPatchBuiltInOutputLocMap;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            if (builtInUsage.position == false)
            {
                return;
            }

            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
            WriteValueToLds(pOutput, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInPointSize:
        {
            if (builtInUsage.pointSize == false)
            {
                return;
            }

            LLPC_ASSERT(pElemIdx == nullptr);
            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, nullptr, pVertexIdx, pInsertPos);
            WriteValueToLds(pOutput, pLdsOffset, pInsertPos);

            break;
        }
    case BuiltInClipDistance:
    case BuiltInCullDistance:
        {
            if (((builtInId == BuiltInClipDistance) && (builtInUsage.clipDistance == 0)) ||
                ((builtInId == BuiltInCullDistance) && (builtInUsage.cullDistance == 0)))
            {
                return;
            }

            LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
            uint32_t loc = builtInOutLocMap[builtInId];

            if (pElemIdx == nullptr)
            {
                // gl_ClipDistance[]/gl_CullDistance[] is treated as 2 x vec4
                LLPC_ASSERT(pOutputTy->isArrayTy());

                for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);

                    auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                    auto pLdsOffset =
                        CalcLdsOffsetForTcsOutput(pElem->getType(), loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                    WriteValueToLds(pElem, pLdsOffset, pInsertPos);
                }
            }
            else
            {
                auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
            }

            break;
        }
    case BuiltInTessLevelOuter:
        {
            if (builtInUsage.tessLevelOuter)
            {
                // Extract tessellation factors
                std::vector<Value*> tessFactors;
                if (pElemIdx == nullptr)
                {
                    LLPC_ASSERT(pOutputTy->isArrayTy());

                    uint32_t tessFactorCount = 0;

                    uint32_t primitiveMode =
                        m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes.primitiveMode;

                    switch (primitiveMode)
                    {
                    case Isolines:
                        tessFactorCount = 2;
                        break;
                    case Triangles:
                        tessFactorCount = 3;
                        break;
                    case Quads:
                        tessFactorCount = 4;
                        break;
                    default:
                        LLPC_NEVER_CALLED();
                        break;
                    }

                    for (uint32_t i = 0; i < tessFactorCount; ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);

                        Value* pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                        tessFactors.push_back(pElem);
                    }

                    if (primitiveMode == Isolines)
                    {
                        LLPC_ASSERT(tessFactorCount == 2);
                        std::swap(tessFactors[0], tessFactors[1]);
                    }
                }
                else
                {
                    LLPC_ASSERT(pOutputTy->isFloatTy());
                    tessFactors.push_back(pOutput);
                }

                Value* pTessFactorOffset = CalcTessFactorOffset(true, pElemIdx, pInsertPos);
                StoreTessFactorToBuffer(tessFactors, pTessFactorOffset, pInsertPos);

                LLPC_ASSERT(perPatchBuiltInOutLocMap.find(builtInId) != perPatchBuiltInOutLocMap.end());
                uint32_t loc = perPatchBuiltInOutLocMap[builtInId];

                if (pElemIdx == nullptr)
                {
                    // gl_TessLevelOuter[4] is treated as vec4
                    LLPC_ASSERT(pOutputTy->isArrayTy());

                    for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);
                        auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);

                        auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                        auto pLdsOffset =
                            CalcLdsOffsetForTcsOutput(pElem->getType(), loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                        WriteValueToLds(pElem, pLdsOffset, pInsertPos);
                    }
                }
                else
                {
                    auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, nullptr, pInsertPos);
                    WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
                }
            }
            break;
        }
    case BuiltInTessLevelInner:
        {
            if (builtInUsage.tessLevelInner)
            {
                // Extract tessellation factors
                std::vector<Value*> tessFactors;
                if (pElemIdx == nullptr)
                {
                    uint32_t tessFactorCount = 0;

                    uint32_t primitiveMode =
                        m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes.primitiveMode;

                    switch (primitiveMode)
                    {
                    case Isolines:
                        tessFactorCount = 0;
                        break;
                    case Triangles:
                        tessFactorCount = 1;
                        break;
                    case Quads:
                        tessFactorCount = 2;
                        break;
                    default:
                        LLPC_NEVER_CALLED();
                        break;
                    }

                    for (uint32_t i = 0; i < tessFactorCount; ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);

                        Value* pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                        tessFactors.push_back(pElem);
                    }
                }
                else
                {
                    LLPC_ASSERT(pOutputTy->isFloatTy());
                    tessFactors.push_back(pOutput);
                }

                Value* pTessFactorOffset = CalcTessFactorOffset(false, pElemIdx, pInsertPos);
                StoreTessFactorToBuffer(tessFactors, pTessFactorOffset, pInsertPos);

                LLPC_ASSERT(perPatchBuiltInOutLocMap.find(builtInId) != perPatchBuiltInOutLocMap.end());
                uint32_t loc = perPatchBuiltInOutLocMap[builtInId];

                if (pElemIdx == nullptr)
                {
                    // gl_TessLevelInner[2] is treated as vec2
                    LLPC_ASSERT(pOutputTy->isArrayTy());

                    for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                    {
                        std::vector<uint32_t> idxs;
                        idxs.push_back(i);
                        auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);

                        auto pElemIdx = ConstantInt::get(m_pContext->Int32Ty(), i);
                        auto pLdsOffset =
                            CalcLdsOffsetForTcsOutput(pElem->getType(), loc, nullptr, pElemIdx, pVertexIdx, pInsertPos);
                        WriteValueToLds(pElem, pLdsOffset, pInsertPos);
                    }
                }
                else
                {
                    auto pLdsOffset = CalcLdsOffsetForTcsOutput(pOutputTy, loc, nullptr, pElemIdx, nullptr, pInsertPos);
                    WriteValueToLds(pOutput, pLdsOffset, pInsertPos);
                }

            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Patches export calls for built-in outputs of tessellation evaluation shader.
void PatchInOutImportExport::PatchTesBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    auto pOutputTy = pOutput->getType();

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval);
    auto& builtInUsage = pResUsage->builtInUsage.tes;
    auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            if (builtInUsage.position == false)
            {
                return;
            }

            if (m_hasGs)
            {
                LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                uint32_t loc = builtInOutLocMap[builtInId];

                StoreValueToEsGsRing(pOutput, loc, 0, pInsertPos);
            }
            else
            {
                AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
            }

            break;
        }
    case BuiltInPointSize:
        {
            if (builtInUsage.pointSize == false)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_PointSize is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.pointSize = false;
                return;
            }

            if (m_hasGs)
            {
                LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                uint32_t loc = builtInOutLocMap[builtInId];

                StoreValueToEsGsRing(pOutput, loc, 0, pInsertPos);
            }
            else
            {
                AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
            }

            break;
        }
    case BuiltInClipDistance:
        {
            if (builtInUsage.clipDistance == 0)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_ClipDistance[] is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.clipDistance = 0;
                return;
            }

            if (m_hasGs)
            {
                LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                uint32_t loc = builtInOutLocMap[builtInId];

                for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                    StoreValueToEsGsRing(pElem, loc + i / 4, i % 4, pInsertPos);
                }
            }
            else
            {
                // NOTE: The export of gl_ClipDistance[] is delayed and is done before entry-point returns.
                m_pClipDistance = pOutput;
            }

            break;
        }
    case BuiltInCullDistance:
        {
            if (builtInUsage.cullDistance == 0)
            {
                return;
            }

            if (isa<UndefValue>(pOutput))
            {
                // NOTE: gl_CullDistance[] is always declared as a field of gl_PerVertex. We have to check the output
                // value to determine if it is actually referenced in shader.
                builtInUsage.cullDistance = 0;
                return;
            }

            if (m_hasGs)
            {
                LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
                uint32_t loc = builtInOutLocMap[builtInId];

                for (uint32_t i = 0; i < pOutputTy->getArrayNumElements(); ++i)
                {
                    std::vector<uint32_t> idxs;
                    idxs.push_back(i);
                    auto pElem = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
                    StoreValueToEsGsRing(pElem, loc + i / 4, i % 4, pInsertPos);
                }
            }
            else
            {
                // NOTE: The export of gl_CullDistance[] is delayed and is done before entry-point returns.
                m_pCullDistance = pOutput;
            }

            break;
        }
    case BuiltInLayer:
        {
            if (builtInUsage.layer == false)
            {
                return;
            }

            const auto enableMultiView = static_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo())->iaState.enableMultiView;

            // NOTE: Only last non-fragment shader stage has to export the value of gl_Layer.
            if ((m_hasGs == false) && (enableMultiView == false))
            {
                if (m_gfxIp.major <= 8)
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
                else
                {
                    // NOTE: The export of gl_Layer is delayed and is done before entry-point returns.
                    m_pLayer = pOutput;
                }
            }

            break;
        }
    case BuiltInViewportIndex:
        {
            if (builtInUsage.viewportIndex == false)
            {
                return;
            }

            // NOTE: Only last non-fragment shader stage has to export the value of gl_ViewportIndex.
            if (m_hasGs == false)
            {
                if (m_gfxIp.major <= 8)
                {
                    AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
                }
                else
                {
                    // NOTE: The export of gl_ViewportIndex is delayed and is done before entry-point returns.
                    m_pViewportIndex = pOutput;
                }
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Patches export calls for built-in outputs of geometry shader.
void PatchInOutImportExport::PatchGsBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    uint32_t     streamId,       // ID of output vertex stream
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    LLPC_ASSERT(streamId == 0); // NOTE: Currently, all built-in outputs are bound to vertex stream 0.

    auto pOutputTy = pOutput->getType();

    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry);
    auto& builtInUsage = pResUsage->builtInUsage.gs;
    auto& builtInOutLocMap = pResUsage->inOutUsage.builtInOutputLocMap;

    std::vector<Value*> args;

    LLPC_ASSERT(builtInOutLocMap.find(builtInId) != builtInOutLocMap.end());
    uint32_t loc = builtInOutLocMap[builtInId];

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            LLPC_ASSERT(builtInUsage.position);

            for (uint32_t i = 0; i < 4; ++i)
            {
                auto pComp = ExtractElementInst::Create(pOutput,
                                                        ConstantInt::get(m_pContext->Int32Ty(), i),
                                                        "",
                                                        pInsertPos);
                StoreValueToGsVsRingBuffer(pComp, loc, i, pInsertPos);
            }
            break;
        }
    case BuiltInPointSize:
        {
            LLPC_ASSERT(builtInUsage.pointSize);

            StoreValueToGsVsRingBuffer(pOutput, loc, 0, pInsertPos);
            break;
        }
    case BuiltInClipDistance:
        {
            LLPC_ASSERT(builtInUsage.clipDistance);

            for (uint32_t i = 0; i < builtInUsage.clipDistance; ++i)
            {
                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                auto pElem = ExtractValueInst::Create(pOutput,
                                                      idxs,
                                                      "",
                                                      pInsertPos);

                StoreValueToGsVsRingBuffer(pElem, loc + i / 4, i % 4, pInsertPos);
            }

            break;
        }
    case BuiltInCullDistance:
        {
            LLPC_ASSERT(builtInUsage.cullDistance);

            for (uint32_t i = 0; i < builtInUsage.cullDistance; ++i)
            {
                std::vector<uint32_t> idxs;
                idxs.push_back(i);
                auto pElem = ExtractValueInst::Create(pOutput,
                                                      idxs,
                                                      "",
                                                      pInsertPos);
                StoreValueToGsVsRingBuffer(pElem, loc + i / 4, i % 4, pInsertPos);
            }

            break;
        }
    case BuiltInPrimitiveId:
        {
            LLPC_ASSERT(builtInUsage.primitiveId);

            StoreValueToGsVsRingBuffer(pOutput, loc, 0, pInsertPos);
            break;
        }
    case BuiltInLayer:
        {
            LLPC_ASSERT(builtInUsage.layer);

            StoreValueToGsVsRingBuffer(pOutput, loc, 0, pInsertPos);
            break;
        }
    case BuiltInViewportIndex:
        {
            LLPC_ASSERT(builtInUsage.viewportIndex);

            StoreValueToGsVsRingBuffer(pOutput, loc, 0, pInsertPos);
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Patches export calls for built-in outputs of fragment shader.
void PatchInOutImportExport::PatchFsBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    auto pOutputTy = pOutput->getType();

    const auto pUndef = UndefValue::get(m_pContext->FloatTy());

    std::vector<Value*> args;

    switch (builtInId)
    {
    case BuiltInFragDepth:
        {
            if (m_gfxIp.major == 6)
            {
                m_pFragDepth = pOutput;
            }
            else
            {
                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_Z));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x1));           // en

                // src0 ~ src3
                args.push_back(pOutput);
                args.push_back(pUndef);
                args.push_back(pUndef);
                args.push_back(pUndef);

                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

                // "Done" flag is valid for exporting MRT
                m_pLastExport = cast<CallInst>(
                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            }
            break;
        }
    case BuiltInSampleMask:
        {
            LLPC_ASSERT(pOutputTy->isArrayTy());

            // NOTE: Only gl_SampleMask[0] is valid for us.
            std::vector<uint32_t> idxs;
            idxs.push_back(0);
            Value* pSampleMask = ExtractValueInst::Create(pOutput, idxs, "", pInsertPos);
            pSampleMask = new BitCastInst(pSampleMask, m_pContext->FloatTy(), "", pInsertPos);

            if (m_gfxIp.major == 6)
            {
                m_pSampleMask = pSampleMask;
            }
            else
            {
                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_Z));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x4));           // en

                // src0 ~ src3
                args.push_back(pUndef);
                args.push_back(pUndef);
                args.push_back(pSampleMask);
                args.push_back(pUndef);

                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

                // "Done" flag is valid for exporting MRT
                m_pLastExport = cast<CallInst>(
                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            }
            break;
        }
    case BuiltInFragStencilRefEXT:
        {
            Value* pFragStencilRef = new BitCastInst(pOutput, m_pContext->FloatTy(), "", pInsertPos);
            if (m_gfxIp.major == 6)
            {
                m_pFragStencilRef = pFragStencilRef;
            }
            else
            {
                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_Z));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x2));           // en

                // src0 ~ src3
                args.push_back(pUndef);
                args.push_back(pFragStencilRef);
                args.push_back(pUndef);
                args.push_back(pUndef);

                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

                // "Done" flag is valid for exporting MRT
                m_pLastExport = cast<CallInst>(
                    EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            }
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Patches export calls for generic outputs of copy shader.
void PatchInOutImportExport::PatchCopyShaderGenericOutputExport(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Location of the output
    Instruction* pInsertPos)     // [in] Where to insert the patch instruction
{
    AddExportInstForGenericOutput(pOutput, location, 0, pInsertPos);
}

// =====================================================================================================================
// Patches export calls for built-in outputs of copy shader.
void PatchInOutImportExport::PatchCopyShaderBuiltInOutputExport(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the patch instruction
{
    switch (builtInId)
    {
    case BuiltInPosition:
    case BuiltInPointSize:
        {
            AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
            break;
        }
    case BuiltInClipDistance:
        {
            // NOTE: The export of gl_ClipDistance[] is delayed and is done before entry-point returns.
            m_pClipDistance = pOutput;
            break;
        }
    case BuiltInCullDistance:
        {
            // NOTE: The export of gl_CullDistance[] is delayed and is done before entry-point returns.
            m_pCullDistance = pOutput;
            break;
        }
     case BuiltInPrimitiveId:
        {
            // NOTE: The export of gl_PrimitiveID is delayed and is done before entry-point returns.
            m_pPrimitiveId = pOutput;
            break;
        }
    case BuiltInLayer:
        {
            const auto enableMultiView = static_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo())->iaState.enableMultiView;

            if ((m_gfxIp.major <= 8) && (enableMultiView == false))
            {
                AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
            }
            else
            {
                // NOTE: The export of gl_Layer is delayed and is done before entry-point returns.
                m_pLayer = pOutput;
            }

            break;
        }
    case BuiltInViewportIndex:
        {
            if (m_gfxIp.major <= 8)
            {
                AddExportInstForBuiltInOutput(pOutput, builtInId, pInsertPos);
            }
            else
            {
                // NOTE: The export of gl_ViewportIndex is delayed and is done before entry-point returns.
                m_pViewportIndex = pOutput;
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

// =====================================================================================================================
// Stores value to ES-GS ring (buffer or LDS).
void PatchInOutImportExport::StoreValueToEsGsRing(
    Value*       pStoreValue,   // [in] Value to store
    uint32_t     location,      // Output location
    uint32_t     compIdx,       // Output component index
    Instruction* pInsertPos)    // [in] Where to insert the store instruction
{
    auto pStoreTy = pStoreValue->getType();

    const uint64_t bitWidth = pStoreTy->getScalarSizeInBits();
    LLPC_ASSERT((pStoreTy->isFPOrFPVectorTy() || pStoreTy->isIntOrIntVectorTy()) &&
                ((bitWidth == 16) || (bitWidth == 32)));

    if (pStoreTy->isVectorTy())
    {
        const uint32_t compCount = pStoreTy->getVectorNumElements();

        for (uint32_t i = 0; i < compCount; ++i)
        {
            auto pStoreComp = ExtractElementInst::Create(pStoreValue,
                                                         ConstantInt::get(m_pContext->Int32Ty(), i),
                                                         "",
                                                         pInsertPos);

            StoreValueToEsGsRing(pStoreComp, location + (compIdx + i) / 4, (compIdx + i) % 4, pInsertPos);
        }
    }
    else
    {
        if (bitWidth == 16)
        {
            if (pStoreTy->isFloatingPointTy())
            {
                pStoreValue = new BitCastInst(pStoreValue, m_pContext->Int16Ty(), "", pInsertPos);
            }

            pStoreValue = new ZExtInst(pStoreValue, m_pContext->Int32Ty(), "", pInsertPos);
        }
        else
        {
            LLPC_ASSERT(bitWidth == 32);
            if (pStoreTy->isFloatingPointTy())
            {
                pStoreValue = new BitCastInst(pStoreValue, m_pContext->Int32Ty(), "", pInsertPos);
            }
        }

        // Call buffer store intrinsic or LDS store
        const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
        LLPC_ASSERT(inOutUsage.pEsGsRingBufDesc != nullptr);

        const auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs;
        Value* pEsGsOffset = nullptr;
        Value* pRingBufDesc = nullptr;
        if (m_shaderStage == ShaderStageVertex)
        {
            pEsGsOffset = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.vs.esGsOffset);
            pRingBufDesc = inOutUsage.pEsGsRingBufDesc;
        }
        else
        {
            LLPC_ASSERT(m_shaderStage == ShaderStageTessEval);
            pEsGsOffset = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.tes.esGsOffset);
            pRingBufDesc = inOutUsage.pEsGsRingBufDesc;
        }

        auto pRingOffset = CalcEsGsRingOffsetForOutput(location, compIdx, pEsGsOffset, pInsertPos);

        if (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9))   // ES -> GS ring is always on-chip on GFX9
        {
            std::vector<Value*> idxs;
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            idxs.push_back(pRingOffset);

            Value* pStorePtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
            new StoreInst(pStoreValue, pStorePtr, false, m_pLds->getAlignment(), pInsertPos);
        }
        else
        {
            // NOTE: Here we use tbuffer_store instruction instead of buffer_store because we have to do explicit control
            // of soffset. This is required by swizzle enabled mode when address range checking should be complied with.
            std::vector<Value*> args;
            args.push_back(pStoreValue);                                                    // vdata
            args.push_back(pRingBufDesc);                                                   // rsrc
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // vindex
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // voffset
            args.push_back(pEsGsOffset);                                                    // soffset
            args.push_back(pRingOffset);                                                    // offset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_DATA_FORMAT_32));    // dfmt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_NUM_FORMAT_UINT));   // nfmt
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // glc
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // slc
            EmitCall(m_pModule, "llvm.amdgcn.tbuffer.store.i32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        }
    }
}

// =====================================================================================================================
// Loads value from ES-GS ring (buffer or LDS).
Value* PatchInOutImportExport::LoadValueFromEsGsRing(
    Type*        pLoadTy,       // [in] Load value type
    uint32_t     location,      // Input location
    uint32_t     compIdx,       // Input component index
    Value*       pVertexIdx,    // [in] Vertex index
    Instruction* pInsertPos)    // [in] Where to insert the load instruction
{
    const uint64_t bitWidth = pLoadTy->getScalarSizeInBits();
    LLPC_ASSERT((pLoadTy->isFPOrFPVectorTy() || pLoadTy->isIntOrIntVectorTy()) &&
                ((bitWidth == 16) || (bitWidth == 32)));

    const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
    LLPC_ASSERT(inOutUsage.pEsGsRingBufDesc != nullptr);

    // Get vertex offset
    Value* pLoadValue = nullptr;

    if (pLoadTy->isVectorTy())
    {
        pLoadValue = UndefValue::get(pLoadTy);
        auto pCompTy = pLoadTy->getVectorElementType();
        const uint32_t compCount = pLoadTy->getVectorNumElements();

        for (uint32_t i = 0; i < compCount; ++i)
        {
            auto pLoadComp = LoadValueFromEsGsRing(pCompTy,
                                                   location + (compIdx + i) / 4,
                                                   (compIdx + i) % 4,
                                                   pVertexIdx,
                                                   pInsertPos);

            pLoadValue = InsertElementInst::Create(pLoadValue,
                                                   pLoadComp,
                                                   ConstantInt::get(m_pContext->Int32Ty(), i),
                                                   "",
                                                   pInsertPos);
        }
    }
    else
    {
        Value* pRingOffset = CalcEsGsRingOffsetForInput(location, compIdx, pVertexIdx, pInsertPos);
        if (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9))   // ES -> GS ring is always on-chip on GFX9
        {
            std::vector<Value*> idxs;
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            idxs.push_back(pRingOffset);

            Value* pLoadPtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
            pLoadValue = new LoadInst(pLoadPtr, "", false, m_pLds->getAlignment(), pInsertPos);

            if (bitWidth == 16)
            {
                pLoadValue = new TruncInst(pLoadValue, m_pContext->Int16Ty(), "", pInsertPos);
            }

            if (pLoadTy->isFloatingPointTy())
            {
                pLoadValue = new BitCastInst(pLoadValue, pLoadTy, "", pInsertPos);
            }
        }
        else
        {
            std::vector<Value*> args;
            args.push_back(inOutUsage.pEsGsRingBufDesc);
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            args.push_back(pRingOffset);
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));   // glc
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));   // slc
            pLoadValue = EmitCall(m_pModule,
                                  "llvm.amdgcn.buffer.load.f32",
                                  m_pContext->FloatTy(),
                                  args,
                                  NoAttrib,
                                  pInsertPos);

            if (bitWidth == 16)
            {
                pLoadValue = new BitCastInst(pLoadValue, m_pContext->Int32Ty(), "", pInsertPos);
                pLoadValue = new TruncInst(pLoadValue, m_pContext->Int16Ty(), "", pInsertPos);

                if (pLoadTy->isFloatingPointTy())
                {
                    pLoadValue = new BitCastInst(pLoadValue, pLoadTy, "", pInsertPos);
                }
            }
            else
            {
                LLPC_ASSERT(bitWidth == 32);
                if (pLoadTy->isIntegerTy())
                {
                    pLoadValue = new BitCastInst(pLoadValue, pLoadTy, "", pInsertPos);
                }
            }
        }
    }

    return pLoadValue;
}

// =====================================================================================================================
// Stores value to GS-VS ring buffer.
void PatchInOutImportExport::StoreValueToGsVsRingBuffer(
    Value*       pStoreValue,   // [in] Value to store
    uint32_t     location,      // Output location
    uint32_t     compIdx,       // Output component index
    Instruction* pInsertPos)    // [in] Where to insert the store instruction
{
    auto pStoreTy = pStoreValue->getType();

    const uint32_t bitWidth = pStoreTy->getScalarSizeInBits();
    LLPC_ASSERT((pStoreTy->isFloatingPointTy() || pStoreTy->isIntegerTy()) &&
                ((bitWidth == 16) || (bitWidth == 32)));

    if (bitWidth == 16)
    {
        // NOTE: Currently, to simplify the design of load/store data from GS-VS ring, we always extend WORD to DWORD.
        // This is because copy shader does not know the actual data type. It only generates output export calls based
        // on number of DWORDs.
        if (pStoreTy->isFloatingPointTy())
        {
            pStoreValue = new BitCastInst(pStoreValue, m_pContext->Int16Ty(), "", pInsertPos);

        }

        pStoreValue = new ZExtInst(pStoreValue, m_pContext->Int32Ty(), "", pInsertPos);
    }
    else
    {
        LLPC_ASSERT(bitWidth == 32);
        if (pStoreTy->isFloatingPointTy())
        {
            pStoreValue = new BitCastInst(pStoreValue, m_pContext->Int32Ty(), "", pInsertPos);
        }
    }

    // Call buffer store intrinsic
    const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
    LLPC_ASSERT(inOutUsage.gs.pGsVsRingBufDesc != nullptr);

    const auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs;
    Value* pGsVsOffset = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.gs.gsVsOffset);

    auto pEmitCounter = new LoadInst(inOutUsage.gs.pEmitCounterPtr, "", pInsertPos);

    auto pRingOffset = CalcGsVsRingOffsetForOutput(location, compIdx, pEmitCounter, pGsVsOffset, pInsertPos);

    if (m_pContext->IsGsOnChip())
    {
        std::vector<Value*> idxs;
        idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        idxs.push_back(pRingOffset);

        Value* pStorePtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
        new StoreInst(pStoreValue, pStorePtr, false, m_pLds->getAlignment(), pInsertPos);
    }
    else
    {
        // NOTE: Here we use tbuffer_store instruction instead of buffer_store because we have to do explicit
        // control of soffset. This is required by swizzle enabled mode when address range checking should be
        // complied with.
        std::vector<Value*> args;
        args.push_back(pStoreValue);                                                    // vdata
        args.push_back(inOutUsage.gs.pGsVsRingBufDesc);                                 // rsrc
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // vindex
        args.push_back(pRingOffset);                                                    // voffset
        args.push_back(pGsVsOffset);                                                    // soffset
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // offset
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_DATA_FORMAT_32));    // dfmt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_NUM_FORMAT_UINT));   // nfmt
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // glc
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // slc
        EmitCall(m_pModule, "llvm.amdgcn.tbuffer.store.i32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }
}

// =====================================================================================================================
// Calculates the byte offset to store the output value to ES-GS ring based on the specified output info.
Value* PatchInOutImportExport::CalcEsGsRingOffsetForOutput(
    uint32_t        location,    // Output location
    uint32_t        compIdx,     // Output component index
    Value*          pEsGsOffset, // [in] ES-GS ring offset in bytes
    Instruction*    pInsertPos)  // [in] Where to insert the instruction
{
    Value* pRingOffset = nullptr;
    if (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9))   // ES -> GS ring is always on-chip on GFX9
    {
        // ringOffset = esGsOffset + threadId * esGsRingItemSize + location * 4 + compIdx

        LLPC_ASSERT((m_pContext->GetShaderStageMask() & ShaderStageToMask(ShaderStageGeometry)) != 0);
        const auto& calcFactor = m_pContext->GetShaderResourceUsage(ShaderStageGeometry)->inOutUsage.gs.calcFactor;

        pEsGsOffset = BinaryOperator::CreateLShr(pEsGsOffset,
                                                 ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                 "",
                                                 pInsertPos);

        pRingOffset = BinaryOperator::CreateMul(m_pThreadId,
                                                ConstantInt::get(m_pContext->Int32Ty(), calcFactor.esGsRingItemSize),
                                                "",
                                                pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset, pEsGsOffset, "", pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), (location * 4 + compIdx)),
                                                "",
                                                pInsertPos);
    }
    else
    {
        // ringOffset = (location * 4 + compIdx) * 4
        pRingOffset = ConstantInt::get(m_pContext->Int32Ty(), (location * 4 + compIdx) * 4);
    }
    return pRingOffset;
}

// =====================================================================================================================
// Calculates the byte offset to load the input value from ES-GS ring based on the specified input info.
Value* PatchInOutImportExport::CalcEsGsRingOffsetForInput(
    uint32_t        location,    // Input location
    uint32_t        compIdx,     // Input Component index
    Value*          pVertexIdx,  // [in] Vertex index
    Instruction*    pInsertPos)  // [in] Where to insert the instruction
{
    Value* pRingOffset = nullptr;
    if (m_pContext->IsGsOnChip() || (m_gfxIp.major >= 9))   // ES -> GS ring is always on-chip on GFX9
    {
        const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
        LLPC_ASSERT(inOutUsage.gs.pEsGsOffsets != nullptr);

        Value* pVertexOffset = ExtractElementInst::Create(inOutUsage.gs.pEsGsOffsets,
                                                          pVertexIdx,
                                                          "",
                                                          pInsertPos);

        // ringOffset = vertexOffset[N] + (location * 4 + compIdx);
        pRingOffset =
            BinaryOperator::CreateAdd(pVertexOffset,
                                      ConstantInt::get(m_pContext->Int32Ty(), (location * 4 + compIdx)),
                                      "",
                                      pInsertPos);
    }
    else
    {
        const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;
        LLPC_ASSERT(inOutUsage.gs.pEsGsOffsets != nullptr);

        Value* pVertexOffset = ExtractElementInst::Create(inOutUsage.gs.pEsGsOffsets,
                                                          pVertexIdx,
                                                          "",
                                                          pInsertPos);

        // ringOffset = vertexOffset[N] * 4 + (location * 4 + compIdx) * 64 * 4;
        pRingOffset = BinaryOperator::CreateMul(pVertexOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                "",
                                                pInsertPos);

        pRingOffset =
            BinaryOperator::CreateAdd(pRingOffset,
                                      ConstantInt::get(m_pContext->Int32Ty(), (location * 4 + compIdx) * 64 * 4),
                                      "",
                                      pInsertPos);
    }

    return pRingOffset;
}

// =====================================================================================================================
// Calculates the offset to store the output value to GS-VS ring based on the specified output info.
Value* PatchInOutImportExport::CalcGsVsRingOffsetForOutput(
    uint32_t        location,    // Output location
    uint32_t        compIdx,     // Output component
    Value*          pVertexIdx,  // [in] Vertex index
    Value*          pGsVsOffset, // [in] ES-GS ring offset in bytes
    Instruction*    pInsertPos)  // [in] Where to insert the instruction
{
    auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageGeometry);

    Value* pRingOffset = nullptr;
    if (m_pContext->IsGsOnChip())
    {
        // ringOffset = esGsLdsSize +
        //              gsVsOffset +
        //              threadId * gsVsRingItemSize +
        //              (vertexIdx * vertexSize) + location * 4 + compIdx

        auto pEsGsLdsSize = ConstantInt::get(m_pContext->Int32Ty(), pResUsage->inOutUsage.gs.calcFactor.esGsLdsSize);

        pGsVsOffset = BinaryOperator::CreateExact(Instruction::LShr,
                                                  pGsVsOffset,
                                                  ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                  "",
                                                  pInsertPos);

        auto pRingItemOffset =
            BinaryOperator::CreateMul(m_pThreadId,
                                      ConstantInt::get(m_pContext->Int32Ty(),
                                                       pResUsage->inOutUsage.gs.calcFactor.gsVsRingItemSize),
                                      "",
                                      pInsertPos);

        uint32_t vertexSize = pResUsage->inOutUsage.outputMapLocCount * 4;
        auto pVertexItemOffset = BinaryOperator::CreateMul(pVertexIdx,
                                                           ConstantInt::get(m_pContext->Int32Ty(), vertexSize),
                                                           "",
                                                           pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pEsGsLdsSize, pGsVsOffset, "", pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset, pRingItemOffset, "", pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset, pVertexItemOffset, "", pInsertPos);

        pRingOffset = BinaryOperator::CreateAdd(pRingOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), (location * 4) + compIdx),
                                                "",
                                                pInsertPos);

    }
    else
    {
        // ringOffset = ((location * 4 + compIdx) * maxVertices + vertexIdx) * 4;

        uint32_t outputVertices = pResUsage->builtInUsage.gs.outputVertices;

        pRingOffset = BinaryOperator::CreateAdd(ConstantInt::get(m_pContext->Int32Ty(),
                                                                 (location * 4 + compIdx) * outputVertices),
                                                pVertexIdx,
                                                "",
                                                pInsertPos);

        pRingOffset = BinaryOperator::CreateMul(pRingOffset,
                                                ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                "",
                                                pInsertPos);
    }

    return pRingOffset;
}

// =====================================================================================================================
// Reads value from LDS.
Value* PatchInOutImportExport::ReadValueFromLds(
    bool         isOutput,    // is the value from output variable
    Type*        pReadTy,     // [in] Type of value read from LDS
    Value*       pLdsOffset,  // [in] Start offset to do LDS read operations
    Instruction* pInsertPos)  // [in] Where to insert read instructions
{
    LLPC_ASSERT(m_pLds != nullptr);
    LLPC_ASSERT(pReadTy->isSingleValueType());

    // Read DWORDs from LDS
    const uint32_t compCount = pReadTy->isVectorTy() ? pReadTy->getVectorNumElements() : 1;
    const uint32_t bitWidth = pReadTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));
    const uint32_t numChannels = compCount * ((bitWidth == 64) ? 2 : 1);

    std::vector<Value*> loadValues(numChannels);

    const bool isTcsOutput = (isOutput && (m_shaderStage == ShaderStageTessControl));
    const bool isTesInput = ((isOutput == false) && (m_shaderStage == ShaderStageTessEval));

    if (m_pContext->IsTessOffChip() && (isTcsOutput || isTesInput)) // Read from off-chip LDS buffer
    {
        const auto& offChipLdsBase = (m_shaderStage == ShaderStageTessEval) ?
            m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs.tes.offChipLdsBase :
            m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs.tcs.offChipLdsBase;

        const auto& pOffChipLdsDesc = (m_shaderStage == ShaderStageTessEval) ?
            m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage.tes.pOffChipLdsDesc :
            m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage.tcs.pOffChipLdsDesc;

        auto pOffChipLdsBase = GetFunctionArgument(m_pEntryPoint, offChipLdsBase);

        // Convert DWORD off-chip LDS offset to byte offset
        pLdsOffset = BinaryOperator::CreateMul(pLdsOffset,
                                               ConstantInt::get(m_pContext->Int32Ty(), 4),
                                               "",
                                               pInsertPos);

        for (uint32_t i = 0; i < numChannels; ++i)
        {
            std::vector<Value*> args;
            args.push_back(pOffChipLdsDesc);                                                // rsrc
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // vindex
            args.push_back(pLdsOffset);                                                     // voffset
            args.push_back(pOffChipLdsBase);                                                // soffset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), i * 4));                 // offset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_DATA_FORMAT_32));    // dfmt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_NUM_FORMAT_FLOAT));  // nfmt
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // glc
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                  // slc

            loadValues[i] = EmitCall(m_pModule,
                                     "llvm.amdgcn.tbuffer.load.i32",
                                     m_pContext->Int32Ty(),
                                     args,
                                     NoAttrib,
                                     pInsertPos);

            if (bitWidth == 16)
            {
                loadValues[i] = new TruncInst(loadValues[i], m_pContext->Int16Ty(), "", pInsertPos);
            }
        }
    }
    else // Read from on-chip LDS
    {
        for (uint32_t i = 0; i < numChannels; ++i)
        {
            std::vector<Value*> idxs;
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            idxs.push_back(pLdsOffset);

            Value* pLoadPtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
            loadValues[i] = new LoadInst(pLoadPtr, "", false, m_pLds->getAlignment(), pInsertPos);

            if (bitWidth == 16)
            {
                loadValues[i] = new TruncInst(loadValues[i], m_pContext->Int16Ty(), "", pInsertPos);
            }

            pLdsOffset =
                BinaryOperator::CreateAdd(pLdsOffset, ConstantInt::get(m_pContext->Int32Ty(), 1), "", pInsertPos);
        }
    }

    // Construct <n x i16> or <n x i32> vector from load values (DWORDs)
    Value* pCastValue = nullptr;
    if (numChannels > 1)
    {
        auto pCastTy = VectorType::get((bitWidth == 16) ? m_pContext->Int16Ty() : m_pContext->Int32Ty(), numChannels);
        pCastValue = UndefValue::get(pCastTy);

        for (uint32_t i = 0; i < numChannels; ++i)
        {
            pCastValue = InsertElementInst::Create(pCastValue,
                                                    loadValues[i],
                                                    ConstantInt::get(m_pContext->Int32Ty(), i),
                                                    "",
                                                    pInsertPos);
        }
    }
    else
    {
        pCastValue = loadValues[0];
    }

    // Cast <n x i16> or <n x i32> vector to read value
    return new BitCastInst(pCastValue, pReadTy, "", pInsertPos);
}

// =====================================================================================================================
// Writes value to LDS.
void PatchInOutImportExport::WriteValueToLds(
    Value*        pWriteValue,   // [in] Value Written to LDS
    Value*        pLdsOffset,    // [in] Start offset to do LDS write operations
    Instruction*  pInsertPos)    // [in] Where to insert write instructions
{
    LLPC_ASSERT(m_pLds != nullptr);

    auto pWriteTy = pWriteValue->getType();
    LLPC_ASSERT(pWriteTy->isSingleValueType());

    const uint32_t compCout = pWriteTy->isVectorTy() ? pWriteTy->getVectorNumElements() : 1;
    const uint32_t bitWidth = pWriteTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));
    const uint32_t numChannels = compCout * ((bitWidth == 64) ? 2 : 1);

    // Cast write value to <n x i32> vector
    Type* pCastTy = (numChannels > 1) ?
                        VectorType::get((bitWidth == 16) ? m_pContext->Int16Ty() : m_pContext->Int32Ty(), numChannels) :
                        ((bitWidth == 16) ? m_pContext->Int16Ty() : m_pContext->Int32Ty());
    Value* pCastValue = new BitCastInst(pWriteValue, pCastTy, "", pInsertPos);

    // Extract store values (DWORDs) from <n x i16> or <n x i32> vector
    std::vector<Value*> storeValues(numChannels);
    if (numChannels > 1)
    {
        for (uint32_t i = 0; i < numChannels; ++i)
        {
            storeValues[i] = ExtractElementInst::Create(pCastValue,
                                                        ConstantInt::get(m_pContext->Int32Ty(), i),
                                                        "",
                                                        pInsertPos);

            if (bitWidth == 16)
            {
                storeValues[i] = new ZExtInst(storeValues[i], m_pContext->Int32Ty(), "", pInsertPos);
            }
        }
    }
    else
    {
        storeValues[0] = pCastValue;

        if (bitWidth == 16)
        {
            storeValues[0] = new ZExtInst(storeValues[0], m_pContext->Int32Ty(), "", pInsertPos);
        }
    }

    if (m_pContext->IsTessOffChip() && m_shaderStage == ShaderStageTessControl)     // Write to off-chip LDS buffer
    {
        auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs.tcs;
        const auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage.tcs;

        auto pOffChipLdsBase = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.offChipLdsBase);
        // Convert DWORD off-chip LDS offset to byte offset
        pLdsOffset = BinaryOperator::CreateMul(pLdsOffset,
                                               ConstantInt::get(m_pContext->Int32Ty(), 4),
                                               "",
                                               pInsertPos);

        for (uint32_t i = 0; i < numChannels; ++i)
        {
            std::vector<Value*> args;
            args.push_back(storeValues[i]);                                                    // vdata
            args.push_back(inOutUsage.pOffChipLdsDesc);                                        // rsrc
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                        // vindex
            args.push_back(pLdsOffset);                                                        // voffset
            args.push_back(pOffChipLdsBase);                                                   // soffset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), i * 4));                    // offset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_DATA_FORMAT_32));       // dfmt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_NUM_FORMAT_FLOAT));     // nfmt
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                      // glc
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                     // slc
            EmitCall(m_pModule, "llvm.amdgcn.tbuffer.store.i32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        }
    }
    else // Write to on-chip LDS
    {
        for (uint32_t i = 0; i < numChannels; ++i)
        {
            std::vector<Value*> idxs;
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            idxs.push_back(pLdsOffset);

            Value* pStorePtr = GetElementPtrInst::Create(nullptr, m_pLds, idxs, "", pInsertPos);
            new StoreInst(storeValues[i], pStorePtr, false, m_pLds->getAlignment(), pInsertPos);

            pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, ConstantInt::get(m_pContext->Int32Ty(), 1), "", pInsertPos);
        }
    }
}

// =====================================================================================================================
// Calculates start offset of tessellation factors in the TF buffer.
Value* PatchInOutImportExport::CalcTessFactorOffset(
    bool         isOuter,     // Whether the calculation is for tessellation outer factors
    Value*       pElemIdx,    // [in] Index used for array element indexing (could be null)
    Instruction* pInsertPos)  // [in] Where to insert store instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

    // NOTE: Tessellation factors are from tessellation level arry and we have:
    //   (1) Isoline
    //      tessFactor[0] = gl_TessLevelOuter[1]
    //      tessFactor[1] = gl_TessLevelOuter[0]
    //   (2) Triangle
    //      tessFactor[0] = gl_TessLevelOuter[0]
    //      tessFactor[1] = gl_TessLevelOuter[1]
    //      tessFactor[2] = gl_TessLevelOuter[2]
    //      tessFactor[3] = gl_TessLevelInner[0]
    //   (3) Quad
    //      tessFactor[0] = gl_TessLevelOuter[0]
    //      tessFactor[1] = gl_TessLevelOuter[1]
    //      tessFactor[2] = gl_TessLevelOuter[2]
    //      tessFactor[3] = gl_TessLevelOuter[3]
    //      tessFactor[4] = gl_TessLevelInner[0]
    //      tessFactor[5] = gl_TessLevelInner[1]

    auto& calcFactor = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs.calcFactor;
    uint32_t primitiveMode = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes.primitiveMode;

    uint32_t tessFactorCount = 0;
    uint32_t tessFactorStart = 0;
    switch (primitiveMode)
    {
    case Isolines:
        tessFactorCount = isOuter ? 2 : 0;
        tessFactorStart = isOuter ? 0 : 2;
        break;
    case Triangles:
        tessFactorCount = isOuter ? 3 : 1;
        tessFactorStart = isOuter ? 0 : 3;
        break;
    case Quads:
        tessFactorCount = isOuter ? 4 : 2;
        tessFactorStart = isOuter ? 0 : 4;
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }

    Value* pTessFactorOffset = ConstantInt::get(m_pContext->Int32Ty(), tessFactorStart);
    if (pElemIdx != nullptr)
    {
        if (isa<ConstantInt>(pElemIdx))
        {
            // Constant element indexing
            uint32_t elemIdx = cast<ConstantInt>(pElemIdx)->getZExtValue();
            if (elemIdx < tessFactorCount)
            {
                if ((primitiveMode == Isolines) && isOuter)
                {
                    // NOTE: In case of the isoline,  hardware wants two tessellation factor: the first is detail
                    // TF, the second is density TF. The order is reversed, different from GLSL spec.
                    LLPC_ASSERT(tessFactorCount == 2);
                    elemIdx = 1 - elemIdx;
                }

                pTessFactorOffset = ConstantInt::get(m_pContext->Int32Ty(), tessFactorStart + elemIdx);
            }
            else
            {
                // Out of range, drop it
                pTessFactorOffset = ConstantInt::get(m_pContext->Int32Ty(), InvalidValue);
            }
        }
        else
        {
            // Dynamic element indexing
            if ((primitiveMode == Isolines) && isOuter)
            {
                // NOTE: In case of the isoline,  hardware wants two tessellation factor: the first is detail
                // TF, the second is density TF. The order is reversed, different from GLSL spec.
                LLPC_ASSERT(tessFactorCount == 2);

                // elemIdx = (elemIdx <= 1) ? 1 - elemIdx : elemIdx
                auto pCond = new ICmpInst(pInsertPos,
                                          ICmpInst::ICMP_ULE,
                                          pElemIdx,
                                          ConstantInt::get(m_pContext->Int32Ty(), 1),
                                          "");

                auto pSwapElemIdx = BinaryOperator::CreateSub(ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                              pElemIdx,
                                                              "",
                                                              pInsertPos);

                pElemIdx = SelectInst::Create(pCond, pSwapElemIdx, pElemIdx, "", pInsertPos);
            }

            // tessFactorOffset = (elemIdx < tessFactorCount) ? (tessFactorStart + elemIdx) : invalidValue
            pTessFactorOffset = BinaryOperator::CreateAdd(pTessFactorOffset,
                                                          pElemIdx,
                                                          "",
                                                          pInsertPos);

            auto pCond = new ICmpInst(pInsertPos,
                                      ICmpInst::ICMP_ULT,
                                      pElemIdx,
                                      ConstantInt::get(m_pContext->Int32Ty(), tessFactorCount),
                                      "");

            pTessFactorOffset = SelectInst::Create(pCond,
                                                   pTessFactorOffset,
                                                   ConstantInt::get(m_pContext->Int32Ty(), InvalidValue),
                                                   "",
                                                   pInsertPos);
        }
    }

    return pTessFactorOffset;
}

// =====================================================================================================================
// Stores tessellation factors (outer/inner) to corresponding tessellation factor (TF) buffer.
void PatchInOutImportExport::StoreTessFactorToBuffer(
    const std::vector<Value*>& tessFactors,         // [in] Tessellation factors to be stored
    Value*                     pTessFactorOffset,   // [in] Start offset to store the specified tessellation factors
    Instruction*               pInsertPos)          // [in] Where to insert store instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

    if (tessFactors.size() == 0)
    {
        // No tessellation factor should be stored
        return;
    }

    const auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs;
    const auto& calcFactor = inOutUsage.calcFactor;

    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageTessControl)->entryArgIdxs.tcs;
    auto pTfBufferBase = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.tfBufferBase);

    auto pTessFactorStride = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.tessFactorStride);

    if (isa<ConstantInt>(pTessFactorOffset))
    {
        uint32_t tessFactorOffset = cast<ConstantInt>(pTessFactorOffset)->getZExtValue();
        if (tessFactorOffset == InvalidValue)
        {
            // Out of range, drop it
            return;
        }

        Value* pTfBufferOffset = BinaryOperator::CreateMul(inOutUsage.pRelativeId, pTessFactorStride, "", pInsertPos);
        pTfBufferOffset = BinaryOperator::CreateMul(pTfBufferOffset,
                                                    ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                    "",
                                                    pInsertPos);

        for (uint32_t i = 0; i < tessFactors.size(); ++i)
        {
            uint32_t  tessFactorByteOffset = i * 4 + tessFactorOffset * 4;
            if (m_pContext->GetGfxIpVersion().major != 9)
            {
                // NOTE: GFX9 does not support dynamic tessellation control, so additional 4-byte offset is not required for
                // tessellation off-chip mode.
                tessFactorByteOffset += (m_pContext->IsTessOffChip() ? 4 : 0);
            }

            std::vector<Value*> args;

            args.push_back(tessFactors[i]);                                                 // vdata
            args.push_back(inOutUsage.pTessFactorBufDesc);                                  // rsrc
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));                     // vindex
            args.push_back(pTfBufferOffset);                                                // voffset
            args.push_back(pTfBufferBase);                                                  // soffset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), tessFactorByteOffset));  // offset
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_DATA_FORMAT_32));    // dfmt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), BUF_NUM_FORMAT_FLOAT));  // nfmt
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));                   // glc
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                  // slc

            EmitCall(m_pModule, "llvm.amdgcn.tbuffer.store.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        }
    }
    else
    {
        // Must be element indexing of tessellation level array
        LLPC_ASSERT(tessFactors.size() == 1);

        if (m_pModule->getFunction(LlpcName::TfBufferStore) == nullptr)
        {
            CreateTessBufferStoreFunction();
        }

        if (m_pContext->IsTessOffChip())
        {
            if (m_pContext->GetGfxIpVersion().major != 9)
            {
                pTfBufferBase = BinaryOperator::CreateAdd(pTfBufferBase,
                                                          ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                          "",
                                                          pInsertPos);
            }

        }

        std::vector<Value*> args;

        args.push_back(inOutUsage.pTessFactorBufDesc);  // tfBufferDesc
        args.push_back(pTfBufferBase);                  // tfBufferBase
        args.push_back(inOutUsage.pRelativeId);         // relPatchId
        args.push_back(pTessFactorStride);              // tfStride
        args.push_back(pTessFactorOffset);              // tfOffset
        args.push_back(tessFactors[0]);                 // tfValue

        EmitCall(m_pModule, LlpcName::TfBufferStore, m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
    }
}

// =====================================================================================================================
// Creates the LLPC intrinsic "llpc.tfbuffer.store.f32" to store tessellation factor (dynamic element indexing for
// tessellation level array).
void PatchInOutImportExport::CreateTessBufferStoreFunction()
{
    // define void @llpc.tfbuffer.store.f32(
    //     <4 x i32> %tfBufferDesc, i32 %tfBufferBase, i32 %relPatchId, i32 %tfStride, i32 %tfOffset, float %tfValue)
    // {
    //     %1 = icmp ne i32 %tfOffset, -1 (invalidValue)
    //     br i1 %1, label %.tfstore, label %.end
    //
    // .tfstore:
    //     %2 = mul i32 %tfStride, 4
    //     %3 = mul i32 %relPatchId, %2
    //     %4 = mul i32 %tfOffset, 4
    //     %5 = add i32 %3, %4
    //     %6 = add i32 %tfBufferBase, %5
    //     call void @llvm.amdgcn.buffer.store.f32(
    //         float %tfValue, <4 x i32> %tfBufferDesc, i32 0, i32 %6, i1 true, i1 false)
    //     br label %.end
    //
    // .end:
    //     ret void
    // }
    std::vector<Type*> argTys;
    argTys.push_back(m_pContext->Int32x4Ty());  // TF buffer descriptor
    argTys.push_back(m_pContext->Int32Ty());    // TF buffer base
    argTys.push_back(m_pContext->Int32Ty());    // Relative patch ID
    argTys.push_back(m_pContext->Int32Ty());    // TF stride
    argTys.push_back(m_pContext->Int32Ty());    // TF offset
    argTys.push_back(m_pContext->FloatTy());    // TF value

    auto pFuncTy = FunctionType::get(m_pContext->VoidTy(), argTys, false);
    auto pFunc = Function::Create(pFuncTy, GlobalValue::ExternalLinkage, LlpcName::TfBufferStore, m_pModule);

    pFunc->setCallingConv(CallingConv::C);
    pFunc->addFnAttr(Attribute::NoUnwind);

    auto argIt = pFunc->arg_begin();

    Value* pTfBufferDesc = argIt++;
    pTfBufferDesc->setName("tfBufferDesc");

    Value* pTfBufferBase = argIt++;
    pTfBufferBase->setName("tfBufferBase");

    Value* pRelPatchId = argIt++;
    pRelPatchId->setName("relPatchId");

    Value* pTfStride = argIt++;
    pTfStride->setName("tfStride");

    Value* pTfOffset = argIt++;
    pTfOffset->setName("tfOffset");

    Value* pTfValue = argIt++;
    pTfValue->setName("tfValue");

    // Create ".end" block
    BasicBlock* pEndBlock = BasicBlock::Create(*m_pContext, ".end", pFunc);
    ReturnInst::Create(*m_pContext, pEndBlock);

    // Create ".tfstore" block
    BasicBlock* pTfStoreBlock = BasicBlock::Create(*m_pContext, ".tfstore", pFunc, pEndBlock);

    Value *pTfByteOffset = BinaryOperator::CreateMul(pTfOffset,
                                                     ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                     "",
                                                     pTfStoreBlock);

    Value* pTfByteStride = BinaryOperator::CreateMul(pTfStride,
                                                     ConstantInt::get(m_pContext->Int32Ty(), 4),
                                                     "",
                                                     pTfStoreBlock);
    Value* pTfBufferOffset = BinaryOperator::CreateMul(pRelPatchId, pTfByteStride, "", pTfStoreBlock);

    pTfBufferOffset = BinaryOperator::CreateAdd(pTfBufferOffset, pTfByteOffset, "", pTfStoreBlock);
    pTfBufferOffset = BinaryOperator::CreateAdd(pTfBufferOffset, pTfBufferBase, "", pTfStoreBlock);

    auto pBranch = BranchInst::Create(pEndBlock, pTfStoreBlock);

    std::vector<Value*> args;
    args.push_back(pTfValue);                                       // vdata
    args.push_back(pTfBufferDesc);                                  // rsrc
    args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));     // vindex
    args.push_back(pTfBufferOffset);                                // offset
    args.push_back(ConstantInt::get(m_pContext->BoolTy(), true));   // glc
    args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // slc

    EmitCall(m_pModule, "llvm.amdgcn.buffer.store.f32", m_pContext->VoidTy(), args, NoAttrib, pBranch);

    // Create entry block
    BasicBlock* pEntryBlock = BasicBlock::Create(*m_pContext, "", pFunc, pTfStoreBlock);
    Value* pCond = new ICmpInst(*pEntryBlock,
                                ICmpInst::ICMP_NE,
                                pTfOffset,
                                ConstantInt::get(m_pContext->Int32Ty(), InvalidValue),
                                "");
    BranchInst::Create(pTfStoreBlock, pEndBlock, pCond, pEntryBlock);
}

// =====================================================================================================================
// Calculates the DWORD offset to write value to LDS based on the specified VS output info.
Value* PatchInOutImportExport::CalcLdsOffsetForVsOutput(
    Type*        pOutputTy,     // [in] Type of the output
    uint32_t     location,      // Base location of the output
    uint32_t     compIdx,       // Index used for vector element indexing
    Instruction* pInsertPos)    // [in] Where to insert calculation instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageVertex);

    // attribOffset = location * 4 + compIdx
    Value* pAttribOffset = ConstantInt::get(m_pContext->Int32Ty(), location * 4);

    const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

    if (bitWidth == 64)
    {
        // For 64-bit data type, the component indexing must multiply by 2
        compIdx *= 2;
    }

    pAttribOffset = BinaryOperator::CreateAdd(pAttribOffset,
                                              ConstantInt::get(m_pContext->Int32Ty(), compIdx),
                                              "",
                                              pInsertPos);

    const auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageVertex)->entryArgIdxs.vs;
    auto pRelVertexId = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.relVertexId);

    const auto& calcFactor = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs.calcFactor;
    auto pVertexStride = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.inVertexStride);

    // dwordOffset = relVertexId * vertexStride + attribOffset
    auto pLdsOffset = BinaryOperator::CreateMul(pRelVertexId, pVertexStride, "", pInsertPos);
    pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttribOffset, "", pInsertPos);

    return pLdsOffset;
}

// =====================================================================================================================
// Calculates the DWORD offset to read value from LDS based on the specified TCS input info.
Value* PatchInOutImportExport::CalcLdsOffsetForTcsInput(
    Type*        pInputTy,      // [in] Type of the input
    uint32_t     location,      // Base location of the input
    Value*       pLocOffset,    // [in] Relative location offset
    Value*       pCompIdx,      // [in] Index used for vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Vertex indexing
    Instruction* pInsertPos)    // [in] Where to insert calculation instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

    const auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs;
    const auto& calcFactor = inOutUsage.calcFactor;

    // attribOffset = (location + locOffset) * 4 + compIdx
    Value* pAttribOffset = ConstantInt::get(m_pContext->Int32Ty(), location);

    if (pLocOffset != nullptr)
    {
        pAttribOffset = BinaryOperator::CreateAdd(pAttribOffset, pLocOffset, "", pInsertPos);
    }

    pAttribOffset = BinaryOperator::CreateMul(pAttribOffset,
                                              ConstantInt::get(m_pContext->Int32Ty(), 4),
                                              "",
                                              pInsertPos);

    if (pCompIdx != nullptr)
    {
        const uint32_t bitWidth = pInputTy->getScalarSizeInBits();
        LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

        if (bitWidth == 64)
        {
            // For 64-bit data type, the component indexing must multiply by 2
            pCompIdx = BinaryOperator::CreateMul(pCompIdx,
                                                 ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                 "",
                                                 pInsertPos);
        }

        pAttribOffset = BinaryOperator::CreateAdd(pAttribOffset, pCompIdx, "", pInsertPos);
    }

    // dwordOffset = (relativeId * inVertexCount + vertexId) * inVertexStride + attribOffset
    auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
    auto inVertexCount = pPipelineInfo->iaState.patchControlPoints;
    auto pInVertexCount = ConstantInt::get(m_pContext->Int32Ty(), inVertexCount);

    Value* pLdsOffset = BinaryOperator::CreateMul(inOutUsage.pRelativeId, pInVertexCount, "", pInsertPos);
    pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pVertexIdx, "", pInsertPos);

    auto pInVertexStride = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.inVertexStride);
    pLdsOffset = BinaryOperator::CreateMul(pLdsOffset, pInVertexStride, "", pInsertPos);

    pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttribOffset, "", pInsertPos);

    return pLdsOffset;
}

// =====================================================================================================================
// Calculates the DWORD offset to read/write value from/to LDS based on the specified TCS output info.
Value* PatchInOutImportExport::CalcLdsOffsetForTcsOutput(
    Type*        pOutputTy,     // [in] Type of the output
    uint32_t     location,      // Base location of the output
    Value*       pLocOffset,    // [in] Relative location offset (could be null)
    Value*       pCompIdx,      // [in] Index used for vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Vertex indexing
    Instruction* pInsertPos)    // [in] Where to insert calculation instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

    const auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs;
    const auto& calcFactor = inOutUsage.calcFactor;

    auto outPatchStart = m_pContext->IsTessOffChip() ? calcFactor.offChip.outPatchStart :
        calcFactor.onChip.outPatchStart;

    auto patchConstStart = m_pContext->IsTessOffChip() ? calcFactor.offChip.patchConstStart :
        calcFactor.onChip.patchConstStart;

    // attribOffset = (location + locOffset) * 4 + compIdx * bitWidth / 32
    Value* pAttibOffset = ConstantInt::get(m_pContext->Int32Ty(), location);

    if (pLocOffset != nullptr)
    {
        pAttibOffset = BinaryOperator::CreateAdd(pAttibOffset, pLocOffset, "", pInsertPos);
    }

    pAttibOffset = BinaryOperator::CreateMul(pAttibOffset,
                                             ConstantInt::get(m_pContext->Int32Ty(), 4),
                                             "",
                                             pInsertPos);

    if (pCompIdx != nullptr)
    {
        const uint32_t bitWidth = pOutputTy->getScalarSizeInBits();
        LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

        if (bitWidth == 64)
        {
            // For 64-bit data type, the component indexing must multiply by 2
            pCompIdx = BinaryOperator::CreateMul(pCompIdx,
                                                 ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                 "",
                                                 pInsertPos);
        }

        pAttibOffset = BinaryOperator::CreateAdd(pAttibOffset, pCompIdx, "", pInsertPos);
    }

    Value* pLdsOffset = nullptr;

    const bool perPatch = (pVertexIdx == nullptr); // Vertex indexing is unavailable for per-patch output
    if (perPatch)
    {
        // dwordOffset = patchConstStart + relativeId * patchConstSize + attribOffset
        auto pPatchConstSize = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.patchConstSize);
        pLdsOffset = BinaryOperator::CreateMul(inOutUsage.pRelativeId, pPatchConstSize, "", pInsertPos);

        auto pPatchConstStart = ConstantInt::get(m_pContext->Int32Ty(), patchConstStart);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pPatchConstStart, "", pInsertPos);

        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttibOffset, "", pInsertPos);
    }
    else
    {
        // dwordOffset = outPatchStart + (relativeId * outVertexCount + vertexId) * outVertexStride + attribOffset
        //             = outPatchStart + relativeId * outPatchSize + vertexId  * outVertexStride + attribOffset
        auto pOutPatchSize = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.outPatchSize);
        pLdsOffset = BinaryOperator::CreateMul(inOutUsage.pRelativeId, pOutPatchSize, "", pInsertPos);

        auto pOutPatchStart = ConstantInt::get(m_pContext->Int32Ty(), outPatchStart);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pOutPatchStart, "", pInsertPos);

        auto pOutVertexStride = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.outVertexStride);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset,
                                               BinaryOperator::CreateMul(pVertexIdx, pOutVertexStride, "", pInsertPos),
                                               "",
                                               pInsertPos);

        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttibOffset, "", pInsertPos);
    }

    return pLdsOffset;
}

// =====================================================================================================================
// Calculates the DWORD offset to read/write value from/to LDS based on the specified TES input info.
Value* PatchInOutImportExport::CalcLdsOffsetForTesInput(
    Type*        pInputTy,      // [in] Type of the input
    uint32_t     location,      // Base location of the input
    Value*       pLocOffset,    // [in] Relative location offset
    Value*       pCompIdx,      // [in] Index used for vector element indexing (could be null)
    Value*       pVertexIdx,    // [in] Vertex indexing
    Instruction* pInsertPos)    // [in] Where to insert calculation instructions
{
    LLPC_ASSERT(m_shaderStage == ShaderStageTessEval);

    const auto& calcFactor = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs.calcFactor;

    auto outPatchStart = m_pContext->IsTessOffChip() ? calcFactor.offChip.outPatchStart :
        calcFactor.onChip.outPatchStart;

    auto patchConstStart = m_pContext->IsTessOffChip() ? calcFactor.offChip.patchConstStart :
        calcFactor.onChip.patchConstStart;

    const auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(m_shaderStage)->entryArgIdxs.tes;

    auto pRelPatchId = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.relPatchId);

    // attribOffset = (location + locOffset) * 4 + compIdx
    Value* pAttibOffset = ConstantInt::get(m_pContext->Int32Ty(), location);

    if (pLocOffset != nullptr)
    {
        pAttibOffset = BinaryOperator::CreateAdd(pAttibOffset, pLocOffset, "", pInsertPos);
    }

    pAttibOffset = BinaryOperator::CreateMul(pAttibOffset,
                                             ConstantInt::get(m_pContext->Int32Ty(), 4),
                                             "",
                                             pInsertPos);

    if (pCompIdx != nullptr)
    {
        const uint32_t bitWidth = pInputTy->getScalarSizeInBits();
        LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

        if (bitWidth == 64)
        {
            // For 64-bit data type, the component indexing must multiply by 2
            pCompIdx = BinaryOperator::CreateMul(pCompIdx,
                                                 ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                 "",
                                                 pInsertPos);
        }

        pAttibOffset = BinaryOperator::CreateAdd(pAttibOffset, pCompIdx, "", pInsertPos);
    }

    Value* pLdsOffset = nullptr;

    const bool perPatch = (pVertexIdx == nullptr); // Vertex indexing is unavailable for per-patch input
    if (perPatch)
    {
        // dwordOffset = patchConstStart + relPatchId * patchConstSize + attribOffset
        auto pPatchConstSize = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.patchConstSize);
        pLdsOffset = BinaryOperator::CreateMul(pRelPatchId, pPatchConstSize, "", pInsertPos);

        auto pPatchConstStart = ConstantInt::get(m_pContext->Int32Ty(), patchConstStart);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pPatchConstStart, "", pInsertPos);

        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttibOffset, "", pInsertPos);
    }
    else
    {
        // dwordOffset = patchStart + (relPatchId * vertexCount + vertexId) * vertexStride + attribOffset
        //             = patchStart + relPatchId * patchSize + vertexId  * vertexStride + attribOffset
        auto pPatchSize = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.outPatchSize);
        pLdsOffset = BinaryOperator::CreateMul(pRelPatchId, pPatchSize, "", pInsertPos);

        auto pPatchStart = ConstantInt::get(m_pContext->Int32Ty(), outPatchStart);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pPatchStart, "", pInsertPos);

        auto pVertexStride = ConstantInt::get(m_pContext->Int32Ty(), calcFactor.outVertexStride);
        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset,
                                               BinaryOperator::CreateMul(pVertexIdx, pVertexStride, "", pInsertPos),
                                               "",
                                               pInsertPos);

        pLdsOffset = BinaryOperator::CreateAdd(pLdsOffset, pAttibOffset, "", pInsertPos);
    }

    return pLdsOffset;
}

// =====================================================================================================================
// Calculates the patch count for per-thread group.
uint32_t PatchInOutImportExport::CalcPatchCountPerThreadGroup(
    uint32_t inVertexCount,     // Count of vertices of input patch
    uint32_t inVertexStride,    // Vertex stride of input patch in (DWORDs)
    uint32_t outVertexCount,    // Count of vertices of output patch
    uint32_t outVertexStride,   // Vertex stride of output patch in (DWORDs)
    uint32_t patchConstCount    // Count of output patch constants
    ) const
{
    const uint32_t wavefrontSize = m_pContext->GetGpuProperty()->waveSize;

    // NOTE: The limit of thread count for tessellation control shader is 4 wavefronts per thread group.
    const uint32_t maxThreadCountPerThreadGroup = (4 * wavefrontSize);
    const uint32_t maxThreadCountPerPatch = std::max(inVertexCount, outVertexCount);
    const uint32_t patchCountLimitedByThread = maxThreadCountPerThreadGroup / maxThreadCountPerPatch;

    const uint32_t inPatchSize = (inVertexCount * inVertexStride);
    const uint32_t outPatchSize = (outVertexCount * outVertexStride);
    const uint32_t patchConstSize = patchConstCount * 4;

    // Compute the required LDS size per patch, always include the space for VS vertex out
    uint32_t ldsSizePerPatch = inPatchSize;
    uint32_t patchCountLimitedByLds = (m_pContext->GetGpuProperty()->ldsSizePerThreadGroup / ldsSizePerPatch);

    uint32_t patchCountPerThreadGroup = std::min(patchCountLimitedByThread, patchCountLimitedByLds);

    // NOTE: Performance analysis shows that 16 patches per thread group is an optimal upper-bound. The value is only
    // an experimental number. For GFX9. 64 is an optimal number instead.
    const auto gfxIp = m_pContext->GetGfxIpVersion();
    const uint32_t optimalPatchCountPerThreadGroup = (gfxIp.major >= 9) ? 64 : 16;

    patchCountPerThreadGroup = std::min(patchCountPerThreadGroup, optimalPatchCountPerThreadGroup);

    if (m_pContext->IsTessOffChip())
    {
        auto outPatchLdsBufferSize = (outPatchSize + patchConstSize) * 4;
        auto tessOffChipPatchCountPerThreadGroup =
            m_pContext->GetGpuProperty()->tessOffChipLdsBufferSize / outPatchLdsBufferSize;
        patchCountPerThreadGroup = std::min(patchCountPerThreadGroup, tessOffChipPatchCountPerThreadGroup);
    }

    return patchCountPerThreadGroup;
}

// =====================================================================================================================
// Inserts "exp" instruction to export generic output.
void PatchInOutImportExport::AddExportInstForGenericOutput(
    Value*       pOutput,        // [in] Output value
    uint32_t     location,       // Location of the output
    uint32_t     compIdx,        // Index used for vector element indexing
    Instruction* pInsertPos)     // [in] Where to insert the "exp" instruction
{
    // Check if the shader stage is valid to use "exp" instruction to export output
    const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);
    const bool useExpInst = (((m_shaderStage == ShaderStageVertex) || (m_shaderStage == ShaderStageTessEval) ||
                              (m_shaderStage == ShaderStageCopyShader)) &&
                             ((nextStage == ShaderStageInvalid) || (nextStage == ShaderStageFragment)));
    LLPC_ASSERT(useExpInst);

    auto pOutputTy = pOutput->getType();

    auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;

    const uint32_t compCount = pOutputTy->isVectorTy() ? pOutputTy->getVectorNumElements() : 1;
    const uint32_t bitWidth  = pOutputTy->getScalarSizeInBits();
    LLPC_ASSERT((bitWidth == 16) || (bitWidth == 32) || (bitWidth == 64));

    // Convert the output value to floating-point export value
    Value* pExport = nullptr;
    const uint32_t numChannels = (bitWidth == 64) ? compCount * 2 : compCount;
    uint32_t startChannel = (bitWidth == 64) ? compIdx * 2 : compIdx;
    Type* pExportTy = (numChannels > 1) ? VectorType::get(m_pContext->FloatTy(), numChannels) : m_pContext->FloatTy();

    if (pOutputTy != pExportTy)
    {
        if (bitWidth == 16)
        {
            // NOTE: For 16-bit output export, we have to cast the 16-bit value to 32-bit floating-point value.
            auto pCompTy = pOutputTy->getScalarType();

            if (pOutputTy->isFPOrFPVectorTy())
            {
                pExport = new BitCastInst(pOutput,
                                          pOutputTy->isVectorTy() ?
                                              VectorType::get(m_pContext->Int16Ty(), compCount) : m_pContext->Int16Ty(),
                                          "",
                                          pInsertPos);
            }
            else
            {
                LLPC_ASSERT(pOutputTy->isIntOrIntVectorTy());
                pExport = pOutput;
            }

            pExport = new ZExtInst(pExport,
                                   pOutputTy->isVectorTy() ?
                                       VectorType::get(m_pContext->Int32Ty(), compCount) : m_pContext->Int32Ty(),
                                   "",
                                   pInsertPos);
            pExport = new BitCastInst(pExport, pExportTy, "", pInsertPos);
        }
        else
        {
            LLPC_ASSERT(CanBitCast(pOutputTy, pExportTy));
            pExport = new BitCastInst(pOutput, pExportTy, "", pInsertPos);
        }
    }
    else
    {
        pExport = pOutput;
    }

    LLPC_ASSERT(numChannels <= 8);
    Value* exportValues[8] = { nullptr };

    if (numChannels == 1)
    {
        exportValues[0] = pExport;
    }
    else
    {
        for (uint32_t i = 0; i < numChannels; ++i)
        {
            exportValues[i] = ExtractElementInst::Create(pExport,
                                                         ConstantInt::get(m_pContext->Int32Ty(), i),
                                                         "",
                                                         pInsertPos);
        }
    }

    std::vector<Value*> args;

    if (numChannels <= 4)
    {
        LLPC_ASSERT(startChannel + numChannels <= 4);
        const uint32_t channelMask = ((1 << (startChannel + numChannels)) - 1) - ((1 << startChannel) - 1);

        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + location)); // tgt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), channelMask));                   // en

        // src0 ~ src3
        for (uint32_t i = 0; i < startChannel; ++i)
        {
            // Inactive components (dummy)
            args.push_back(UndefValue::get(m_pContext->FloatTy()));
        }

        for (uint32_t i = startChannel; i < startChannel + numChannels; ++i)
        {
            args.push_back(exportValues[i - startChannel]);
        }

        for (uint32_t i = startChannel + numChannels; i < 4; ++i)
        {
            // Inactive components (dummy)
            args.push_back(UndefValue::get(m_pContext->FloatTy()));
        }

        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

        EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        ++inOutUsage.expCount;
    }
    else
    {
        // We have to do exporting twice for this output
        LLPC_ASSERT(startChannel == 0); // Other values are disallowed according to GLSL spec
        LLPC_ASSERT((numChannels == 6) || (numChannels == 8));

        // Do the first exporting
        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + location)); // tgt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                           // en

        // src0 ~ src3
        for (uint32_t i = 0; i < 4; ++i)
        {
            args.push_back(exportValues[i]);
        }

        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

        EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        ++inOutUsage.expCount;

        // Do the second exporting
        const uint32_t channelMask = ((1 << (numChannels - 4)) - 1);

        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + location + 1)); // tgt
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), channelMask));                       // en

        // src0 ~ src3
        for (uint32_t i = 4; i < numChannels; ++i)
        {
            args.push_back(exportValues[i]);
        }

        for (uint32_t i = numChannels; i < 8; ++i)
        {
            // Inactive components (dummy)
            args.push_back(UndefValue::get(m_pContext->FloatTy()));
        }

        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false)); // done
        args.push_back(ConstantInt::get(m_pContext->BoolTy(), false)); // vm

        EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
        ++inOutUsage.expCount;
    }
}

// =====================================================================================================================
// Inserts "exp" instruction to export built-in output.
void PatchInOutImportExport::AddExportInstForBuiltInOutput(
    Value*       pOutput,       // [in] Output value
    uint32_t     builtInId,     // ID of the built-in variable
    Instruction* pInsertPos)    // [in] Where to insert the "exp" instruction
{
    // Check if the shader stage is valid to use "exp" instruction to export output
    const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);
    const bool useExpInst = (((m_shaderStage == ShaderStageVertex) || (m_shaderStage == ShaderStageTessEval) ||
                              (m_shaderStage == ShaderStageCopyShader)) &&
                             ((nextStage == ShaderStageInvalid) || (nextStage == ShaderStageFragment)));
    LLPC_ASSERT(useExpInst);

    auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;

    const auto pUndef = UndefValue::get(m_pContext->FloatTy());

    std::vector<Value*> args;

    switch (builtInId)
    {
    case BuiltInPosition:
        {
            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_0)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));              // en

            // src0 ~ src3
            for (uint32_t i = 0; i < 4; ++i)
            {
                auto pCompValue = ExtractElementInst::Create(pOutput,
                                                             ConstantInt::get(m_pContext->Int32Ty(), i),
                                                             "",
                                                             pInsertPos);
                args.push_back(pCompValue);
            }

            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));  // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            break;
        }
    case BuiltInPointSize:
        {
            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_1)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x1));              // en
            args.push_back(pOutput);                                                   // src0
            args.push_back(pUndef);                                                    // src1
            args.push_back(pUndef);                                                    // src2
            args.push_back(pUndef);                                                    // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));
            break;
        }
    case BuiltInLayer:
        {
            LLPC_ASSERT(m_gfxIp.major <= 8); // For GFX9, gl_ViewportIndex and gl_Layer are packed

            const auto enableMultiView = static_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo())->iaState.enableMultiView;

            Value* pLayer = new BitCastInst(pOutput, m_pContext->FloatTy(), "", pInsertPos);

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_1)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x4));              // en
            args.push_back(pUndef);                                                    // src0
            args.push_back(pUndef);                                                    // src1
            args.push_back(pLayer);                                                    // src2
            args.push_back(pUndef);                                                    // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));

            // NOTE: We have to export gl_Layer via generic outputs as well.
            bool hasLayerExport = true;
            if (nextStage == ShaderStageFragment)
            {
                const auto& nextBuiltInUsage =
                    m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

                hasLayerExport = nextBuiltInUsage.layer || nextBuiltInUsage.viewIndex;
            }

            if (hasLayerExport)
            {
                uint32_t loc = InvalidValue;
                if (m_shaderStage == ShaderStageCopyShader)
                {
                    LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInLayer) !=
                                inOutUsage.gs.builtInOutLocs.end() ||
                                inOutUsage.gs.builtInOutLocs.find(BuiltInViewIndex) !=
                                inOutUsage.gs.builtInOutLocs.end());
                    loc = enableMultiView ? inOutUsage.gs.builtInOutLocs[BuiltInViewIndex]:
                        inOutUsage.gs.builtInOutLocs[BuiltInLayer];
                }
                else
                {
                    LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInLayer) !=
                                inOutUsage.builtInOutputLocMap.end() ||
                                inOutUsage.builtInOutputLocMap.find(BuiltInViewIndex) !=
                                inOutUsage.builtInOutputLocMap.end());

                    loc = enableMultiView ? inOutUsage.builtInOutputLocMap[BuiltInViewIndex] :
                        inOutUsage.builtInOutputLocMap[BuiltInLayer];
                }

                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                       // en
                args.push_back(pLayer);                                                             // src0
                args.push_back(pUndef);                                                             // src1
                args.push_back(pUndef);                                                             // src2
                args.push_back(pUndef);                                                             // src3
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                ++inOutUsage.expCount;
            }

            break;
        }
    case BuiltInViewportIndex:
        {
            LLPC_ASSERT(m_gfxIp.major <= 8); // For GFX9, gl_ViewportIndex and gl_Layer are packed
            Value* pViewportIndex = new BitCastInst(pOutput, m_pContext->FloatTy(), "", pInsertPos);

            args.clear();
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_POS_1)); // tgt
            args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0x8));              // en
            args.push_back(pUndef);                                                    // src0
            args.push_back(pUndef);                                                    // src1
            args.push_back(pUndef);                                                    // src2
            args.push_back(pViewportIndex);                                            // src3
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // done
            args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));             // vm

            // "Done" flag is valid for exporting position 0 ~ 3
            m_pLastExport = cast<CallInst>(
                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos));

            // NOTE: We have to export gl_ViewportIndex via generic outputs as well.
            bool hasViewportIndexExport = true;
            if (nextStage == ShaderStageFragment)
            {
                const auto& nextBuiltInUsage =
                    m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

                hasViewportIndexExport = nextBuiltInUsage.viewportIndex;
            }

            if (hasViewportIndexExport)
            {
                uint32_t loc = InvalidValue;
                if (m_shaderStage == ShaderStageCopyShader)
                {
                    LLPC_ASSERT(inOutUsage.gs.builtInOutLocs.find(BuiltInViewportIndex) !=
                                inOutUsage.gs.builtInOutLocs.end());
                    loc = inOutUsage.gs.builtInOutLocs[BuiltInViewportIndex];
                }
                else
                {
                    LLPC_ASSERT(inOutUsage.builtInOutputLocMap.find(BuiltInViewportIndex) !=
                                inOutUsage.builtInOutputLocMap.end());
                    loc = inOutUsage.builtInOutputLocMap[BuiltInViewportIndex];
                }

                args.clear();
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), EXP_TARGET_PARAM_0 + loc));  // tgt
                args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0xF));                       // en
                args.push_back(pViewportIndex);                                                     // src0
                args.push_back(pUndef);                                                             // src1
                args.push_back(pUndef);                                                             // src2
                args.push_back(pUndef);                                                             // src3
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // done
                args.push_back(ConstantInt::get(m_pContext->BoolTy(), false));                      // vm

                EmitCall(m_pModule, "llvm.amdgcn.exp.f32", m_pContext->VoidTy(), args, NoAttrib, pInsertPos);
                ++inOutUsage.expCount;
            }

            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patching opertions for input import and output export.
INITIALIZE_PASS(PatchInOutImportExport, "Patch-in-out-import-export",
                "Patch LLVM for input import and output export operations", false, false)
