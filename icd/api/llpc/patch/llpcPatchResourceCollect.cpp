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
 * @file  llpcPatchResourceCollect.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchResourceCollect.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-resource-collect"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcIntrinsDefs.h"
#include "llpcPatchResourceCollect.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchResourceCollect::ID = 0;

// =====================================================================================================================
PatchResourceCollect::PatchResourceCollect()
    :
    Patch(ID),
    m_hasPushConstOp(false),
    m_hasDynIndexedInput(false),
    m_hasDynIndexedOutput(false),
    m_pResUsage(nullptr)
{
    initializePatchResourceCollectPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM patching pass on the specified LLVM module.
bool PatchResourceCollect::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Resource-Collect\n");

    Patch::Init(&module);

    m_pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    // Invoke handling of "call" instruction
    visit(m_pModule);

    // Disable push constant if not used
    if (m_hasPushConstOp == false)
    {
        m_pResUsage->pushConstSizeInBytes = 0;
    }

    ClearInactiveInput();

    if (m_pContext->IsGraphics())
    {
        MatchGenericInOut();
        MapBuiltInToGenericInOut();
    }

    if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
    {
        ReviseTessExecutionMode();
    }
    else if (m_shaderStage == ShaderStageFragment)
    {
        if (m_pResUsage->builtInUsage.fs.fragCoord)
        {
            const GraphicsPipelineBuildInfo* pPipelineInfo =
                reinterpret_cast<const GraphicsPipelineBuildInfo*>(m_pContext->GetPipelineBuildInfo());
            if (pPipelineInfo->rsState.perSampleShading)
            {
                m_pResUsage->builtInUsage.fs.runAtSampleRate = true;
            }
        }
    }

    // Remove dead calls
    for (auto pCall : m_deadCalls)
    {
        LLPC_ASSERT(pCall->user_empty());
        pCall->dropAllReferences();
        pCall->eraseFromParent();
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "call" instruction.
void PatchResourceCollect::visitCallInst(
    CallInst& callInst) // [in] "Call" instruction
{
    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    bool isDeadCall = callInst.user_empty();

    auto mangledName = pCallee->getName();

    if (mangledName.startswith(LlpcName::PushConstLoad))
    {
        // Push constant operations
        if (isDeadCall)
        {
            m_deadCalls.insert(&callInst);
        }
        else
        {
            m_hasPushConstOp = true;
        }
    }
    else if (mangledName.startswith(LlpcName::BufferCallPrefix))
    {
        // Buffer operations
        if (isDeadCall &&
            (mangledName.startswith(LlpcName::BufferAtomic) == false) &&
            (mangledName.startswith(LlpcName::BufferStore) == false))
        {
            m_deadCalls.insert(&callInst);
        }
        else
        {
            uint32_t descSet = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
            uint32_t binding = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
            DescriptorPair descPair = { descSet, binding };
            m_pResUsage->descPairs.insert(descPair.u64All);
        }
    }
    else if (mangledName.startswith(LlpcName::ImageCallPrefix))
    {
        // Image operations
        auto opName = mangledName.substr(strlen(LlpcName::ImageCallPrefix));

        ShaderImageCallMetadata imageCallMeta = {};
        LLPC_ASSERT(callInst.getNumArgOperands() >= 2);
        uint32_t metaOperandIndex = callInst.getNumArgOperands() - 1;
        imageCallMeta.U32All =  cast<ConstantInt>(callInst.getArgOperand(metaOperandIndex))->getZExtValue();

        SPIRVImageOpKind imageOp = imageCallMeta.OpKind;

        // NOTE: All "readonly" image operations are expected to be less than the numeric value of "ImageOpWrite".
        if (isDeadCall && isImageOpReadOnly(imageOp))
        {
            m_deadCalls.insert(&callInst);
        }

        uint32_t descSet = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
        uint32_t binding = cast<ConstantInt>(callInst.getOperand(1))->getZExtValue();
        DescriptorPair descPair = { descSet, binding };
        m_pResUsage->descPairs.insert(descPair.u64All);

        std::string imageSampleName;
        std::string imageGatherName;
        std::string imageQueryLodName;
        SPIRV::SPIRVImageOpKindNameMap::find(ImageOpSample, &imageSampleName);
        SPIRV::SPIRVImageOpKindNameMap::find(ImageOpGather, &imageGatherName);
        SPIRV::SPIRVImageOpKindNameMap::find(ImageOpQueryLod, &imageQueryLodName);

        // NOTE: For image sampling operations, we have to add both resource descriptor and sampler descriptor info
        // to descriptor usages, operand 0 and 1 are sampler descriptor, 3 and 4 are resource descriptor
        if (opName.startswith(imageSampleName) ||
            opName.startswith(imageGatherName) ||
            opName.startswith(imageQueryLodName))
        {
            uint32_t descSet = cast<ConstantInt>(callInst.getOperand(3))->getZExtValue();
            uint32_t binding = cast<ConstantInt>(callInst.getOperand(4))->getZExtValue();
            DescriptorPair descPair = { descSet, binding };
            m_pResUsage->descPairs.insert(descPair.u64All);
        }
    }
    else if (mangledName.startswith(LlpcName::InputImportGeneric))
    {
        // Generic input import
        if (isDeadCall)
        {
            m_deadCalls.insert(&callInst);
        }
        else
        {
            auto pInputTy = callInst.getType();
            LLPC_ASSERT(pInputTy->isSingleValueType());

            auto loc = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();

            if ((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval))
            {
                auto pLocOffset = callInst.getOperand(1);
                auto pCompIdx = callInst.getOperand(2);

                if (isa<ConstantInt>(pLocOffset))
                {
                    // Location offset is constant
                    auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
                    loc += locOffset;

                    auto bitWidth = pInputTy->getScalarSizeInBits();
                    if (bitWidth == 64)
                    {
                        if (isa<ConstantInt>(pCompIdx))
                        {
                            auto compIdx = cast<ConstantInt>(pCompIdx)->getZExtValue();

                            m_activeInputLocs.insert(loc);
                            if (compIdx >= 2)
                            {
                                // NOTE: For the addressing of .z/.w component of 64-bit vector/scalar, the count of
                                // occupied locations are two.
                                m_activeInputLocs.insert(loc + 1);
                            }
                        }
                        else
                        {
                            // NOTE: If vector component index is not constant, we treat this as dynamic indexing.
                            m_hasDynIndexedInput = true;
                        }
                    }
                    else
                    {
                        // NOTE: For 32-bit vector/scalar, one location is sufficient regardless of vector component
                        // addressing.
                        LLPC_ASSERT(bitWidth == 32);
                        m_activeInputLocs.insert(loc);
                    }
                }
                else
                {
                    // NOTE: If location offset is not constant, we treat this as dynamic indexing.
                    m_hasDynIndexedInput = true;
                }
            }
            else
            {
                m_activeInputLocs.insert(loc);
                if (pInputTy->getPrimitiveSizeInBits() > (8 * SizeOfVec4))
                {
                    LLPC_ASSERT(pInputTy->getPrimitiveSizeInBits() <= (8 * 2 * SizeOfVec4));
                    m_activeInputLocs.insert(loc + 1);
                }
            }
        }
    }
    else if (mangledName.startswith(LlpcName::InputImportInterpolant))
    {
        // Interpolant input import
        LLPC_ASSERT(m_shaderStage == ShaderStageFragment);

        if (isDeadCall)
        {
            m_deadCalls.insert(&callInst);
        }
        else
        {
            auto pInputTy = callInst.getType();
            LLPC_ASSERT(pInputTy->isSingleValueType());

            auto pLocOffset = callInst.getOperand(1);
            if (isa<ConstantInt>(pLocOffset))
            {
                // Location offset is constant
                auto loc = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
                auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
                loc += locOffset;

                LLPC_ASSERT(pInputTy->getPrimitiveSizeInBits() <= (8 * SizeOfVec4));
                m_activeInputLocs.insert(loc);
            }
            else
            {
                // NOTE: If location offset is not constant, we consider dynamic indexing occurs.
                m_hasDynIndexedInput = true;
            }
        }
    }
    else if (mangledName.startswith(LlpcName::InputImportBuiltIn))
    {
        // Built-in input import
        if (isDeadCall)
        {
            m_deadCalls.insert(&callInst);
        }
        else
        {
            uint32_t builtInId = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
            m_activeInputBuiltIns.insert(builtInId);
        }
    }
    else if (mangledName.startswith(LlpcName::OutputImportGeneric))
    {
        // Generic output import
        LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

        auto pOutputTy = callInst.getType();
        LLPC_ASSERT(pOutputTy->isSingleValueType());

        auto loc = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
        auto pLocOffset = callInst.getOperand(1);
        auto pCompIdx = callInst.getOperand(2);

        if (isa<ConstantInt>(pLocOffset))
        {
            // Location offset is constant
            auto locOffset = cast<ConstantInt>(pLocOffset)->getZExtValue();
            loc += locOffset;

            auto bitWidth = pOutputTy->getScalarSizeInBits();
            if (bitWidth == 64)
            {
                if (isa<ConstantInt>(pCompIdx))
                {
                    auto compIdx = cast<ConstantInt>(pCompIdx)->getZExtValue();

                    m_importedOutputLocs.insert(loc);
                    if (compIdx >= 2)
                    {
                        // NOTE: For the addressing of .z/.w component of 64-bit vector/scalar, the count of
                        // occupied locations are two.
                        m_importedOutputLocs.insert(loc + 1);
                    }
                }
                else
                {
                    // NOTE: If vector component index is not constant, we treat this as dynamic indexing.
                    m_hasDynIndexedOutput = true;
                }
            }
            else
            {
                // NOTE: For 32-bit vector/scalar, one location is sufficient regardless of vector component
                // addressing.
                LLPC_ASSERT(bitWidth == 32);
                m_importedOutputLocs.insert(loc);
            }
        }
        else
        {
            // NOTE: If location offset is not constant, we treat this as dynamic indexing.
            m_hasDynIndexedOutput = true;
        }
    }
    else if (mangledName.startswith(LlpcName::OutputImportBuiltIn))
    {
        // Built-in output import
        LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);

        uint32_t builtInId = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
        m_importedOutputBuiltIns.insert(builtInId);
    }
    else if (mangledName.startswith(LlpcName::OutputExportGeneric))
    {
        // Generic output export
        if (m_shaderStage == ShaderStageTessControl)
        {
            auto pOutput = callInst.getOperand(callInst.getNumArgOperands() - 1);
            auto pOutputTy = pOutput->getType();
            LLPC_ASSERT(pOutputTy->isSingleValueType());

            auto loc = cast<ConstantInt>(callInst.getOperand(0))->getZExtValue();
            auto pLocOffset = callInst.getOperand(1);
            auto pCompIdx = callInst.getOperand(2);

            if (isa<ConstantInt>(pLocOffset))
            {
                // Location offset is constant
                auto bitWidth = pOutputTy->getScalarSizeInBits();
                LLPC_ASSERT((bitWidth == 32) || (bitWidth == 64));

                if ((bitWidth == 64) && (isa<ConstantInt>(pCompIdx) == false))
                {
                    // NOTE: If vector component index is not constant and it is vector component addressing for
                    // 64-bit vector, we treat this as dynamic indexing.
                    m_hasDynIndexedOutput = true;
                }
            }
            else
            {
                // NOTE: If location offset is not constant, we consider dynamic indexing occurs.
                m_hasDynIndexedOutput = true;
            }
        }
    }
}

// =====================================================================================================================
// Clears inactive (those actually unused) inputs.
void PatchResourceCollect::ClearInactiveInput()
{
    // Clear those inactive generic inputs, remove them from location mappings
    if (m_pContext->IsGraphics() && (m_hasDynIndexedInput == false) && (m_shaderStage != ShaderStageTessEval))
    {
        // TODO: Here, we keep all generic inputs of tessellation evaluation shader. This is because corresponding
        // generic outputs of tessellation control shader might involve in output import and dynamic indexing, which
        // is easy to cause incorrectness of location mapping.

        // Clear normal inputs
        std::unordered_set<uint32_t> unusedLocs;
        for (auto locMap : m_pResUsage->inOutUsage.inputLocMap)
        {
            uint32_t loc = locMap.first;
            if (m_activeInputLocs.find(loc) == m_activeInputLocs.end())
            {
                 unusedLocs.insert(loc);
            }
        }

        for (auto loc : unusedLocs)
        {
            m_pResUsage->inOutUsage.inputLocMap.erase(loc);
        }

        // Clear per-patch inputs
        if (m_shaderStage == ShaderStageTessEval)
        {
            unusedLocs.clear();
            for (auto locMap : m_pResUsage->inOutUsage.perPatchInputLocMap)
            {
                uint32_t loc = locMap.first;
                if (m_activeInputLocs.find(loc) == m_activeInputLocs.end())
                {
                     unusedLocs.insert(loc);
                }
            }

            for (auto loc : unusedLocs)
            {
                m_pResUsage->inOutUsage.perPatchInputLocMap.erase(loc);
            }
        }
        else
        {
            // For other stages, must be empty
            LLPC_ASSERT(m_pResUsage->inOutUsage.perPatchInputLocMap.empty());
        }
    }

    // Clear those inactive built-in inputs (some are not checked, whose usage flags do not rely on their
    // actual uses)
    if (m_activeInputBuiltIns.empty() == false)
    {
        auto& builtInUsage = m_pResUsage->builtInUsage;

        // Check per-stage built-in usage
        if (m_shaderStage == ShaderStageVertex)
        {
            if (builtInUsage.vs.drawIndex &&
                (m_activeInputBuiltIns.find(BuiltInDrawIndex) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.vs.drawIndex = false;
            }
        }
        else if (m_shaderStage == ShaderStageTessControl)
        {
            if (builtInUsage.tcs.pointSizeIn &&
                (m_activeInputBuiltIns.find(BuiltInPointSize) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.pointSizeIn = false;
            }

            if (builtInUsage.tcs.positionIn &&
                (m_activeInputBuiltIns.find(BuiltInPosition) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.positionIn = false;
            }

            if ((builtInUsage.tcs.clipDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInClipDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.clipDistanceIn = 0;
            }

            if ((builtInUsage.tcs.cullDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInCullDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.cullDistanceIn = 0;
            }

            if (builtInUsage.tcs.patchVertices &&
                (m_activeInputBuiltIns.find(BuiltInPatchVertices) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.patchVertices = false;
            }

            if (builtInUsage.tcs.primitiveId &&
                (m_activeInputBuiltIns.find(BuiltInPrimitiveId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.primitiveId = false;
            }

            if (builtInUsage.tcs.invocationId &&
                (m_activeInputBuiltIns.find(BuiltInInvocationId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tcs.invocationId = false;
            }
        }
        else if (m_shaderStage == ShaderStageTessEval)
        {
            if (builtInUsage.tes.pointSizeIn &&
                (m_activeInputBuiltIns.find(BuiltInPointSize) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.pointSizeIn = false;
            }

            if (builtInUsage.tes.positionIn &&
                (m_activeInputBuiltIns.find(BuiltInPosition) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.positionIn = false;
            }

            if ((builtInUsage.tes.clipDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInClipDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.clipDistanceIn = 0;
            }

            if ((builtInUsage.tes.cullDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInCullDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.cullDistanceIn = 0;
            }

            if (builtInUsage.tes.patchVertices &&
                (m_activeInputBuiltIns.find(BuiltInPatchVertices) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.patchVertices = false;
            }

            if (builtInUsage.tes.primitiveId &&
                (m_activeInputBuiltIns.find(BuiltInPrimitiveId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.primitiveId = false;
            }

            if (builtInUsage.tes.tessCoord &&
                (m_activeInputBuiltIns.find(BuiltInTessCoord) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.tessCoord = false;
            }

            if (builtInUsage.tes.tessLevelOuter &&
                (m_activeInputBuiltIns.find(BuiltInTessLevelOuter) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.tessLevelOuter = false;
            }

            if (builtInUsage.tes.tessLevelInner &&
                (m_activeInputBuiltIns.find(BuiltInTessLevelInner) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.tes.tessLevelInner = false;
            }
        }
        else if (m_shaderStage == ShaderStageGeometry)
        {
            if (builtInUsage.gs.pointSizeIn &&
                (m_activeInputBuiltIns.find(BuiltInPointSize) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.pointSizeIn = false;
            }

            if (builtInUsage.gs.positionIn &&
                (m_activeInputBuiltIns.find(BuiltInPosition) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.positionIn = false;
            }

            if ((builtInUsage.gs.clipDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInClipDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.clipDistanceIn = 0;
            }

            if ((builtInUsage.gs.cullDistanceIn > 0) &&
                (m_activeInputBuiltIns.find(BuiltInCullDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.cullDistanceIn = 0;
            }

            if (builtInUsage.gs.primitiveIdIn &&
                (m_activeInputBuiltIns.find(BuiltInPrimitiveId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.primitiveIdIn = false;
            }

            if (builtInUsage.gs.invocationId &&
                (m_activeInputBuiltIns.find(BuiltInInvocationId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.gs.invocationId = false;
            }
        }
        else if (m_shaderStage == ShaderStageFragment)
        {
            if (builtInUsage.fs.fragCoord &&
                (m_activeInputBuiltIns.find(BuiltInFragCoord) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.fragCoord = false;
            }

            if (builtInUsage.fs.frontFacing &&
                (m_activeInputBuiltIns.find(BuiltInFrontFacing) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.frontFacing = false;
            }

            if (builtInUsage.fs.fragCoord &&
                (m_activeInputBuiltIns.find(BuiltInFragCoord) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.fragCoord = false;
            }

            if ((builtInUsage.fs.clipDistance > 0) &&
                (m_activeInputBuiltIns.find(BuiltInClipDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.clipDistance = 0;
            }

            if ((builtInUsage.fs.cullDistance > 0) &&
                (m_activeInputBuiltIns.find(BuiltInCullDistance) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.cullDistance = 0;
            }

            if (builtInUsage.fs.pointCoord &&
                (m_activeInputBuiltIns.find(BuiltInPointCoord) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.pointCoord = false;
            }

            if (builtInUsage.fs.primitiveId &&
                (m_activeInputBuiltIns.find(BuiltInPrimitiveId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.primitiveId = false;
            }

            if (builtInUsage.fs.sampleId &&
                (m_activeInputBuiltIns.find(BuiltInSampleId) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.sampleId = false;
            }

            if (builtInUsage.fs.samplePosition &&
                (m_activeInputBuiltIns.find(BuiltInSamplePosition) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.samplePosition = false;
            }

            if (builtInUsage.fs.sampleMaskIn &&
                (m_activeInputBuiltIns.find(BuiltInSampleMask) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.sampleMaskIn = false;
            }

            if (builtInUsage.fs.layer &&
                (m_activeInputBuiltIns.find(BuiltInLayer) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.layer = false;
            }

            if (builtInUsage.fs.viewIndex &&
                (m_activeInputBuiltIns.find(BuiltInViewIndex) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.viewIndex = false;
            }

            if (builtInUsage.fs.viewportIndex &&
                (m_activeInputBuiltIns.find(BuiltInViewportIndex) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.viewportIndex = false;
            }

            if (builtInUsage.fs.helperInvocation &&
                (m_activeInputBuiltIns.find(BuiltInHelperInvocation) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.fs.helperInvocation = false;
            }
        }
        else if (m_shaderStage == ShaderStageCompute)
        {
            if (builtInUsage.cs.numWorkgroups &&
                (m_activeInputBuiltIns.find(BuiltInNumWorkgroups) == m_activeInputBuiltIns.end()))
            {
                builtInUsage.cs.numWorkgroups = false;
            }

            if (builtInUsage.cs.localInvocationId &&
                ((m_activeInputBuiltIns.find(BuiltInLocalInvocationId) == m_activeInputBuiltIns.end()) &&
                 (m_activeInputBuiltIns.find(BuiltInGlobalInvocationId) == m_activeInputBuiltIns.end()) &&
                 (m_activeInputBuiltIns.find(BuiltInLocalInvocationIndex) == m_activeInputBuiltIns.end())))
            {
                builtInUsage.cs.localInvocationId = false;
            }

            if (builtInUsage.cs.workgroupId &&
                ((m_activeInputBuiltIns.find(BuiltInWorkgroupId) == m_activeInputBuiltIns.end()) &&
                 (m_activeInputBuiltIns.find(BuiltInGlobalInvocationId) == m_activeInputBuiltIns.end()) &&
                 (m_activeInputBuiltIns.find(BuiltInLocalInvocationIndex) == m_activeInputBuiltIns.end())))
            {
                builtInUsage.cs.workgroupId = false;
            }
        }

        // Check common built-in usage
        if (builtInUsage.common.subgroupSize &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupSize) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupSize = false;
        }

        if (builtInUsage.common.subgroupLocalInvocationId &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupLocalInvocationId) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupLocalInvocationId = false;
        }

        if (builtInUsage.common.subgroupEqMask &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupEqMaskKHR) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupEqMask = false;
        }

        if (builtInUsage.common.subgroupGeMask &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupGeMaskKHR) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupGeMask = false;
        }

        if (builtInUsage.common.subgroupGtMask &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupGtMaskKHR) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupGtMask = false;
        }

        if (builtInUsage.common.subgroupLeMask &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupLeMaskKHR) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupLeMask = false;
        }

        if (builtInUsage.common.subgroupLtMask &&
            (m_activeInputBuiltIns.find(BuiltInSubgroupLtMaskKHR) == m_activeInputBuiltIns.end()))
        {
            builtInUsage.common.subgroupLtMask = false;
        }
    }
}

// =====================================================================================================================
// Does generic input/output matching and does location mapping afterwards.
//
// NOTE: This function should be called after the cleanup work of inactive inputs is done.
void PatchResourceCollect::MatchGenericInOut()
{
    LLPC_ASSERT(m_pContext->IsGraphics());
    auto& inOutUsage = m_pContext->GetShaderResourceUsage(m_shaderStage)->inOutUsage;

    auto& inLocMap  = inOutUsage.inputLocMap;
    auto& outLocMap = inOutUsage.outputLocMap;

    auto& perPatchInLocMap  = inOutUsage.perPatchInputLocMap;
    auto& perPatchOutLocMap = inOutUsage.perPatchOutputLocMap;

    const uint32_t stageMask = m_pContext->GetShaderStageMask();

    // Do input/output matching
    if (m_shaderStage != ShaderStageFragment)
    {
        const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);

        // Do normal input/output matching
        if (nextStage != ShaderStageInvalid)
        {
            const auto pNextResUsage = m_pContext->GetShaderResourceUsage(nextStage);
            const auto& nextInLocMap = pNextResUsage->inOutUsage.inputLocMap;

            uint32_t availInMapLoc = pNextResUsage->inOutUsage.inputMapLocCount;

            // Collect locations of those outputs that are not used by next shader stage
            std::vector<uint32_t> unusedLocs;
            for (auto& locMap : outLocMap)
            {
                const uint32_t loc = locMap.first;
                if (nextInLocMap.find(loc) == nextInLocMap.end())
                {
                    if (m_hasDynIndexedOutput || (m_importedOutputLocs.find(loc) != m_importedOutputLocs.end()))
                    {
                        // NOTE: If either dynamic indexing of generic outputs exists or the generic output involve in
                        // output import, we have to mark it as active. The assigned location must not overlap with
                        // those used by inputs of next shader stage.
                        LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
                        locMap.second = availInMapLoc++;
                    }
                    else
                    {
                        unusedLocs.push_back(loc);
                    }
                }
            }

            // Remove those collected locations
            for (auto loc : unusedLocs)
            {
                outLocMap.erase(loc);
            }
        }

        // Do per-patch input/output matching
        if (m_shaderStage == ShaderStageTessControl)
        {
            if (nextStage != ShaderStageInvalid)
            {
                const auto pNextResUsage = m_pContext->GetShaderResourceUsage(nextStage);
                const auto& nextPerPatchInLocMap = pNextResUsage->inOutUsage.perPatchInputLocMap;

                uint32_t availPerPatchInMapLoc = pNextResUsage->inOutUsage.perPatchInputMapLocCount;

                // Collect locations of those outputs that are not used by next shader stage
                std::vector<uint32_t> unusedLocs;
                for (auto& locMap : perPatchOutLocMap)
                {
                    const uint32_t loc = locMap.first;
                    if (nextPerPatchInLocMap.find(loc) == nextPerPatchInLocMap.end())
                    {
                        // NOTE: If either dynamic indexing of generic outputs exists or the generic output involve in
                        // output import, we have to mark it as active. The assigned location must not overlap with
                        // those used by inputs of next shader stage.
                        if (m_hasDynIndexedOutput || (m_importedOutputLocs.find(loc) != m_importedOutputLocs.end()))
                        {
                            LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
                            locMap.second = availPerPatchInMapLoc++;
                        }
                        else
                        {
                            unusedLocs.push_back(loc);
                        }
                    }
                }

                // Remove those collected locations
                for (auto loc : unusedLocs)
                {
                    perPatchOutLocMap.erase(loc);
                }
            }
        }
        else
        {
            // For other stages, must be empty
            LLPC_ASSERT(perPatchOutLocMap.empty());
        }
    }

    // Do location mapping
    LLPC_OUTS("===============================================================================\n");
    LLPC_OUTS("// LLPC location input/output mapping results (" << GetShaderStageName(m_shaderStage)
              << " shader)\n\n");
    uint32_t nextMapLoc = 0;
    if (inLocMap.empty() == false)
    {
        LLPC_ASSERT(inOutUsage.inputMapLocCount == 0);
        for (auto& locMap : inLocMap)
        {
            LLPC_ASSERT(locMap.second == InvalidValue);
            // NOTE: For vertex shader, the input location mapping is actually trivial.
            locMap.second = (m_shaderStage == ShaderStageVertex) ? locMap.first : nextMapLoc++;
            inOutUsage.inputMapLocCount = std::max(inOutUsage.inputMapLocCount, locMap.second + 1);
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input:  loc = "
                          << locMap.first << "  =>  Mapped = " << locMap.second << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (outLocMap.empty() == false)
    {
        nextMapLoc = 0;
        LLPC_ASSERT(inOutUsage.outputMapLocCount == 0);
        for (auto& locMap : outLocMap)
        {
            if (locMap.second == InvalidValue)
            {
                // Only do location mapping if the output has not been mapped
                locMap.second = nextMapLoc++;
            }
            else
            {
                LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
            }
            inOutUsage.outputMapLocCount = std::max(inOutUsage.outputMapLocCount, locMap.second + 1);
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output: loc = "
                          << locMap.first << "  =>  Mapped = " << locMap.second << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (perPatchInLocMap.empty() == false)
    {
        nextMapLoc = 0;
        LLPC_ASSERT(inOutUsage.perPatchInputMapLocCount == 0);
        for (auto& locMap : perPatchInLocMap)
        {
            LLPC_ASSERT(locMap.second == InvalidValue);
            locMap.second = nextMapLoc++;
            inOutUsage.perPatchInputMapLocCount = std::max(inOutUsage.perPatchInputMapLocCount, locMap.second + 1);
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input (per-patch):  loc = "
                          << locMap.first << "  =>  Mapped = " << locMap.second << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (perPatchOutLocMap.empty() == false)
    {
        nextMapLoc = 0;
        LLPC_ASSERT(inOutUsage.perPatchOutputMapLocCount == 0);
        for (auto& locMap : perPatchOutLocMap)
        {
            if (locMap.second == InvalidValue)
            {
                // Only do location mapping if the per-patch output has not been mapped
                locMap.second = nextMapLoc++;
            }
            else
            {
                LLPC_ASSERT(m_shaderStage == ShaderStageTessControl);
            }
            inOutUsage.perPatchOutputMapLocCount = std::max(inOutUsage.perPatchOutputMapLocCount, locMap.second + 1);
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output (per-patch): loc = "
                          << locMap.first << "  =>  Mapped = " << locMap.second << "\n");
        }
        LLPC_OUTS("\n");
    }

    LLPC_OUTS("// LLPC location count results (after input/output matching) \n\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input:  loc count = "
                  << inOutUsage.inputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output: loc count = "
                  << inOutUsage.outputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input (per-patch):  loc count = "
                  << inOutUsage.perPatchInputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output (per-patch): loc count = "
                  << inOutUsage.perPatchOutputMapLocCount << "\n");
    LLPC_OUTS("\n");
}

// =====================================================================================================================
// Maps special built-in input/output to generic ones.
//
// NOTE: This function should be called after generic input/output matching is done.
void PatchResourceCollect::MapBuiltInToGenericInOut()
{
    LLPC_ASSERT(m_pContext->IsGraphics());

    const auto pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    auto& builtInUsage = pResUsage->builtInUsage;
    auto& inOutUsage = pResUsage->inOutUsage;

    const auto nextStage = m_pContext->GetNextShaderStage(m_shaderStage);
    auto pNextResUsage =
        (nextStage != ShaderStageInvalid) ? m_pContext->GetShaderResourceUsage(nextStage) : nullptr;

    LLPC_ASSERT(inOutUsage.builtInInputLocMap.empty()); // Should be empty
    LLPC_ASSERT(inOutUsage.builtInOutputLocMap.empty());

    // NOTE: The rules of mapping built-ins to generic inputs/outputs are as follow:
    //       (1) For built-in outputs, if next shader stager is valid and has corresponding built-in input used,
    //           get the mapped location from next shader stage inout usage and use it. If next shader stage
    //           is absent or it does not have such input used, we allocate the mapped location.
    //       (2) For built-on inputs, we always allocate the mapped location based its actual usage.
    if (m_shaderStage == ShaderStageVertex)
    {
        // VS  ==>  XXX
        uint32_t availOutMapLoc = inOutUsage.outputMapLocCount;

        // Map built-in outputs to generic ones
        if (nextStage == ShaderStageFragment)
        {
            // VS  ==>  FS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.fs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.clipDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
            }

            if (nextBuiltInUsage.cullDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
            }

            if (nextBuiltInUsage.primitiveId)
            {
                // NOTE: The usage flag of gl_PrimitiveID must be set if fragment shader uses it.
                builtInUsage.vs.primitiveId = true;

                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPrimitiveId) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPrimitiveId];
                inOutUsage.builtInOutputLocMap[BuiltInPrimitiveId] = mapLoc;
            }

            if (nextBuiltInUsage.layer)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInLayer) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInLayer];
                inOutUsage.builtInOutputLocMap[BuiltInLayer] = mapLoc;
            }

            if (nextBuiltInUsage.viewIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewIndex) !=
                    nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewIndex];
                inOutUsage.builtInOutputLocMap[BuiltInViewIndex] = mapLoc;
            }

            if (nextBuiltInUsage.viewportIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewportIndex) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewportIndex];
                inOutUsage.builtInOutputLocMap[BuiltInViewportIndex] = mapLoc;
            }
        }
        else if (nextStage == ShaderStageTessControl)
        {
            // VS  ==>  TCS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.tcs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.positionIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPosition) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPosition];
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.vs.position = false;
            }

            if (nextBuiltInUsage.pointSizeIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPointSize) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPointSize];
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.vs.pointSize = false;
            }

            if (nextBuiltInUsage.clipDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.clipDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.vs.clipDistance = 0;
            }

            if (nextBuiltInUsage.cullDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.cullDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.vs.cullDistance = 0;
            }

            builtInUsage.vs.layer = false;
            builtInUsage.vs.viewportIndex = false;
        }
        else if (nextStage == ShaderStageGeometry)
        {
            // VS  ==>  GS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.gs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.positionIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPosition) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPosition];
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.vs.position = false;
            }

            if (nextBuiltInUsage.pointSizeIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPointSize) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPointSize];
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.vs.pointSize = false;
            }

            if (nextBuiltInUsage.clipDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.clipDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.vs.clipDistance = 0;
            }

            if (nextBuiltInUsage.cullDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.cullDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.vs.cullDistance = 0;
            }

            builtInUsage.vs.layer = false;
            builtInUsage.vs.viewportIndex = false;
        }
        else if (nextStage == ShaderStageInvalid)
        {
            // VS only
            if ((builtInUsage.vs.clipDistance > 0) || (builtInUsage.vs.cullDistance > 0))
            {
                uint32_t mapLoc = availOutMapLoc++;
                if (builtInUsage.vs.clipDistance + builtInUsage.vs.cullDistance > 4)
                {
                    LLPC_ASSERT(builtInUsage.vs.clipDistance +
                                builtInUsage.vs.cullDistance <= MaxClipCullDistanceCount);
                    ++availOutMapLoc; // Occupy two locations
                }

                if (builtInUsage.vs.clipDistance > 0)
                {
                    inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                }

                if (builtInUsage.vs.cullDistance > 0)
                {
                    if (builtInUsage.vs.clipDistance >= 4)
                    {
                        ++mapLoc;
                    }
                    inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                }
            }

            if (builtInUsage.vs.viewportIndex)
            {
                inOutUsage.builtInOutputLocMap[BuiltInViewportIndex] = availOutMapLoc++;
            }

            if (builtInUsage.vs.layer)
            {
                inOutUsage.builtInOutputLocMap[BuiltInLayer] = availOutMapLoc++;
            }

            if (builtInUsage.vs.viewIndex)
            {
                inOutUsage.builtInOutputLocMap[BuiltInViewIndex] = availOutMapLoc++;
            }
        }

        inOutUsage.outputMapLocCount = std::max(inOutUsage.outputMapLocCount, availOutMapLoc);
    }
    else if (m_shaderStage == ShaderStageTessControl)
    {
        // TCS  ==>  XXX
        uint32_t availInMapLoc = inOutUsage.inputMapLocCount;
        uint32_t availOutMapLoc = inOutUsage.outputMapLocCount;

        uint32_t availPerPatchOutMapLoc = inOutUsage.perPatchOutputMapLocCount;

        // Map built-in inputs to generic ones
        if (builtInUsage.tcs.positionIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPosition] = availInMapLoc++;
        }

        if (builtInUsage.tcs.pointSizeIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPointSize] = availInMapLoc++;
        }

        if (builtInUsage.tcs.clipDistanceIn > 0)
        {
            inOutUsage.builtInInputLocMap[BuiltInClipDistance] = availInMapLoc++;
            if (builtInUsage.tcs.clipDistanceIn > 4)
            {
                ++availInMapLoc;
            }
        }

        if (builtInUsage.tcs.cullDistanceIn > 0)
        {
            inOutUsage.builtInInputLocMap[BuiltInCullDistance] = availInMapLoc++;
            if (builtInUsage.tcs.cullDistanceIn > 4)
            {
                ++availInMapLoc;
            }
        }

        // Map built-in outputs to generic ones
        if (nextStage == ShaderStageTessEval)
        {
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.tes;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            // NOTE: For tessellation control shadder, those built-in outputs that involve in output import have to
            // be mapped to generic ones even if they do not have corresponding built-in inputs used in next shader
            // stage.
            if (nextBuiltInUsage.positionIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPosition) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPosition];
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                if (m_importedOutputBuiltIns.find(BuiltInPosition) != m_importedOutputBuiltIns.end())
                {
                    inOutUsage.builtInOutputLocMap[BuiltInPosition] = InvalidValue;
                }
                else
                {
                    builtInUsage.tcs.position = false;
                }
            }

            if (nextBuiltInUsage.pointSizeIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPointSize) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPointSize];
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                if (m_importedOutputBuiltIns.find(BuiltInPointSize) != m_importedOutputBuiltIns.end())
                {
                    inOutUsage.builtInOutputLocMap[BuiltInPointSize] = InvalidValue;
                }
                else
                {
                    builtInUsage.tcs.pointSize = false;
                }
            }

            if (nextBuiltInUsage.clipDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.clipDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                if (m_importedOutputBuiltIns.find(BuiltInClipDistance) != m_importedOutputBuiltIns.end())
                {
                    inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = InvalidValue;
                }
                else
                {
                    builtInUsage.tcs.clipDistance = 0;
                }
            }

            if (nextBuiltInUsage.cullDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.cullDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                if (m_importedOutputBuiltIns.find(BuiltInCullDistance) != m_importedOutputBuiltIns.end())
                {
                    inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = InvalidValue;
                }
                else
                {
                    builtInUsage.tcs.cullDistance = 0;
                }
            }

            if (nextBuiltInUsage.tessLevelOuter)
            {
                LLPC_ASSERT(nextInOutUsage.perPatchBuiltInInputLocMap.find(BuiltInTessLevelOuter) !=
                            nextInOutUsage.perPatchBuiltInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.perPatchBuiltInInputLocMap[BuiltInTessLevelOuter];
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelOuter] = mapLoc;
                availPerPatchOutMapLoc = std::max(availPerPatchOutMapLoc, mapLoc + 1);
            }
            else
            {
                // NOTE: We have to map gl_TessLevelOuter to generic per-patch output as long as it is used.
                if (builtInUsage.tcs.tessLevelOuter)
                {
                    inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelOuter] = InvalidValue;
                }
            }

            if (nextBuiltInUsage.tessLevelInner)
            {
                LLPC_ASSERT(nextInOutUsage.perPatchBuiltInInputLocMap.find(BuiltInTessLevelInner) !=
                            nextInOutUsage.perPatchBuiltInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.perPatchBuiltInInputLocMap[BuiltInTessLevelInner];
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelInner] = mapLoc;
                availPerPatchOutMapLoc = std::max(availPerPatchOutMapLoc, mapLoc + 1);
            }
            else
            {
                // NOTE: We have to map gl_TessLevelInner to generic per-patch output as long as it is used.
                if (builtInUsage.tcs.tessLevelInner)
                {
                    inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelInner] = InvalidValue;
                }
            }

            // Revisit built-in outputs and map those unmapped to generic ones
            if ((inOutUsage.builtInOutputLocMap.find(BuiltInPosition) != inOutUsage.builtInOutputLocMap.end()) &&
                (inOutUsage.builtInOutputLocMap[BuiltInPosition] == InvalidValue))
            {
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = availOutMapLoc++;
            }

            if ((inOutUsage.builtInOutputLocMap.find(BuiltInPointSize) != inOutUsage.builtInOutputLocMap.end()) &&
                (inOutUsage.builtInOutputLocMap[BuiltInPointSize] == InvalidValue))
            {
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = availOutMapLoc++;
            }

            if ((inOutUsage.builtInOutputLocMap.find(BuiltInClipDistance) != inOutUsage.builtInOutputLocMap.end()) &&
                (inOutUsage.builtInOutputLocMap[BuiltInClipDistance] == InvalidValue))
            {
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = availOutMapLoc++;
            }

            if ((inOutUsage.builtInOutputLocMap.find(BuiltInCullDistance) != inOutUsage.builtInOutputLocMap.end()) &&
                (inOutUsage.builtInOutputLocMap[BuiltInCullDistance] == InvalidValue))
            {
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = availOutMapLoc++;
            }

            if ((inOutUsage.perPatchBuiltInOutputLocMap.find(BuiltInTessLevelOuter) !=
                 inOutUsage.perPatchBuiltInOutputLocMap.end()) &&
                (inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelOuter] == InvalidValue))
            {
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelOuter] = availPerPatchOutMapLoc++;
            }

            if ((inOutUsage.perPatchBuiltInOutputLocMap.find(BuiltInTessLevelInner) !=
                 inOutUsage.perPatchBuiltInOutputLocMap.end()) &&
                (inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelInner] == InvalidValue))
            {
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelInner] = availPerPatchOutMapLoc++;
            }
        }
        else if (nextStage == ShaderStageInvalid)
        {
            // TCS only
            if (builtInUsage.tcs.position)
            {
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = availOutMapLoc++;
            }

            if (builtInUsage.tcs.pointSize)
            {
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = availOutMapLoc++;
            }

            if (builtInUsage.tcs.clipDistance > 0)
            {
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = availOutMapLoc++;
                if (builtInUsage.tcs.clipDistance > 4)
                {
                    ++availOutMapLoc;
                }
            }

            if (builtInUsage.tcs.cullDistance > 0)
            {
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = availOutMapLoc++;
                if (builtInUsage.tcs.cullDistance > 4)
                {
                    ++availOutMapLoc;
                }
            }

            if (builtInUsage.tcs.tessLevelOuter)
            {
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelOuter] = availPerPatchOutMapLoc++;
            }

            if (builtInUsage.tcs.tessLevelInner)
            {
                inOutUsage.perPatchBuiltInOutputLocMap[BuiltInTessLevelInner] = availPerPatchOutMapLoc++;
            }
        }

        inOutUsage.inputMapLocCount = std::max(inOutUsage.inputMapLocCount, availInMapLoc);
        inOutUsage.outputMapLocCount = std::max(inOutUsage.outputMapLocCount, availOutMapLoc);
        inOutUsage.perPatchOutputMapLocCount = std::max(inOutUsage.perPatchOutputMapLocCount, availPerPatchOutMapLoc);
    }
    else if (m_shaderStage == ShaderStageTessEval)
    {
        // TES  ==>  XXX
        uint32_t availInMapLoc = inOutUsage.inputMapLocCount;
        uint32_t availOutMapLoc = inOutUsage.outputMapLocCount;

        uint32_t availPerPatchInMapLoc = inOutUsage.perPatchInputMapLocCount;

        // Map built-in inputs to generic ones
        if (builtInUsage.tes.positionIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPosition] = availInMapLoc++;
        }

        if (builtInUsage.tes.pointSizeIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPointSize] = availInMapLoc++;
        }

        if (builtInUsage.tes.clipDistanceIn > 0)
        {
            uint32_t clipDistanceCount = builtInUsage.tes.clipDistanceIn;

            // NOTE: If gl_in[].gl_ClipDistance is used, we have to check the usage of gl_out[].gl_ClipDistance in
            // tessellation control shader. The clip distance is the maximum of the two. We do this to avoid
            // incorrectness of location assignment during builtin-to-generic mapping.
            const auto prevStage = m_pContext->GetPrevShaderStage(m_shaderStage);
            if (prevStage == ShaderStageTessControl)
            {
                const auto& prevBuiltInUsage = m_pContext->GetShaderResourceUsage(prevStage)->builtInUsage.tcs;
                clipDistanceCount = std::max(clipDistanceCount, prevBuiltInUsage.clipDistance);
            }

            inOutUsage.builtInInputLocMap[BuiltInClipDistance] = availInMapLoc++;
            if (clipDistanceCount > 4)
            {
                ++availInMapLoc;
            }
        }

        if (builtInUsage.tes.cullDistanceIn > 0)
        {
            uint32_t cullDistanceCount = builtInUsage.tes.cullDistanceIn;

            const auto prevStage = m_pContext->GetPrevShaderStage(m_shaderStage);
            if (prevStage == ShaderStageTessControl)
            {
                const auto& prevBuiltInUsage = m_pContext->GetShaderResourceUsage(prevStage)->builtInUsage.tcs;
                cullDistanceCount = std::max(cullDistanceCount, prevBuiltInUsage.clipDistance);
            }

            inOutUsage.builtInInputLocMap[BuiltInCullDistance] = availInMapLoc++;
            if (cullDistanceCount > 4)
            {
                ++availInMapLoc;
            }
        }

        if (builtInUsage.tes.tessLevelOuter)
        {
            inOutUsage.perPatchBuiltInInputLocMap[BuiltInTessLevelOuter] = availPerPatchInMapLoc++;
        }

        if (builtInUsage.tes.tessLevelInner)
        {
            inOutUsage.perPatchBuiltInInputLocMap[BuiltInTessLevelInner] = availPerPatchInMapLoc++;
        }

        // Map built-in outputs to generic ones
        if (nextStage == ShaderStageFragment)
        {
            // TES  ==>  FS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.fs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.clipDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
            }

            if (nextBuiltInUsage.cullDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
            }

            if (nextBuiltInUsage.primitiveId)
            {
                // NOTE: The usage flag of gl_PrimitiveID must be set if fragment shader uses it.
                builtInUsage.tes.primitiveId = true;

                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPrimitiveId) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPrimitiveId];
                inOutUsage.builtInOutputLocMap[BuiltInPrimitiveId] = mapLoc;
            }

            if (nextBuiltInUsage.layer)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInLayer) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInLayer];
                inOutUsage.builtInOutputLocMap[BuiltInLayer] = mapLoc;
            }

            if (nextBuiltInUsage.viewIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewIndex) !=
                    nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewIndex];
                inOutUsage.builtInOutputLocMap[BuiltInViewIndex] = mapLoc;
            }

            if (nextBuiltInUsage.viewportIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewportIndex) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewportIndex];
                inOutUsage.builtInOutputLocMap[BuiltInViewportIndex] = mapLoc;
            }
        }
        else if (nextStage == ShaderStageGeometry)
        {
            // TES  ==>  GS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.gs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.positionIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPosition) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPosition];
                inOutUsage.builtInOutputLocMap[BuiltInPosition] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.tes.position = false;
            }

            if (nextBuiltInUsage.pointSizeIn)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPointSize) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPointSize];
                inOutUsage.builtInOutputLocMap[BuiltInPointSize] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + 1);
            }
            else
            {
                builtInUsage.tes.pointSize = false;
            }

            if (nextBuiltInUsage.clipDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.clipDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.tes.clipDistance = 0;
            }

            if (nextBuiltInUsage.cullDistanceIn > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                availOutMapLoc = std::max(availOutMapLoc, mapLoc + ((nextBuiltInUsage.cullDistanceIn > 4) ? 2u : 1u));
            }
            else
            {
                builtInUsage.tes.cullDistance = 0;
            }

            builtInUsage.tes.layer = false;
            builtInUsage.tes.viewportIndex = false;
        }
        else if (nextStage == ShaderStageInvalid)
        {
            // TES only
            if ((builtInUsage.tes.clipDistance > 0) || (builtInUsage.tes.cullDistance > 0))
            {
                uint32_t mapLoc = availOutMapLoc++;
                if (builtInUsage.tes.clipDistance + builtInUsage.tes.cullDistance > 4)
                {
                    LLPC_ASSERT(builtInUsage.tes.clipDistance +
                                builtInUsage.tes.cullDistance <= MaxClipCullDistanceCount);
                    ++availOutMapLoc; // Occupy two locations
                }

                if (builtInUsage.tes.clipDistance > 0)
                {
                    inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = mapLoc;
                }

                if (builtInUsage.tes.cullDistance > 0)
                {
                    if (builtInUsage.tes.clipDistance >= 4)
                    {
                        ++mapLoc;
                    }
                    inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = mapLoc;
                }
            }

            if (builtInUsage.tes.viewportIndex)
            {
                inOutUsage.builtInOutputLocMap[BuiltInViewportIndex] = availOutMapLoc++;
            }

            if (builtInUsage.tes.layer)
            {
                inOutUsage.builtInOutputLocMap[BuiltInLayer] = availOutMapLoc++;
            }

            if (builtInUsage.tes.viewIndex)
            {
                inOutUsage.builtInOutputLocMap[BuiltInViewIndex] = availOutMapLoc++;
            }
        }

        inOutUsage.inputMapLocCount = std::max(inOutUsage.inputMapLocCount, availInMapLoc);
        inOutUsage.outputMapLocCount = std::max(inOutUsage.outputMapLocCount, availOutMapLoc);

        inOutUsage.perPatchInputMapLocCount = std::max(inOutUsage.perPatchInputMapLocCount, availPerPatchInMapLoc);
    }
    else if (m_shaderStage == ShaderStageGeometry)
    {
        // GS  ==>  XXX
        uint32_t availInMapLoc  = inOutUsage.inputMapLocCount;
        uint32_t availOutMapLoc = inOutUsage.outputMapLocCount;

        // Map built-in inputs to generic ones
        if (builtInUsage.gs.positionIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPosition] = availInMapLoc++;
        }

        if (builtInUsage.gs.pointSizeIn)
        {
            inOutUsage.builtInInputLocMap[BuiltInPointSize] = availInMapLoc++;
        }

        if (builtInUsage.gs.clipDistanceIn > 0)
        {
            inOutUsage.builtInInputLocMap[BuiltInClipDistance] = availInMapLoc++;
            if (builtInUsage.gs.clipDistanceIn > 4)
            {
                ++availInMapLoc;
            }
        }

        if (builtInUsage.gs.cullDistanceIn > 0)
        {
            inOutUsage.builtInInputLocMap[BuiltInCullDistance] = availInMapLoc++;
            if (builtInUsage.gs.cullDistanceIn > 4)
            {
                ++availInMapLoc;
            }
        }

        // Map built-in outputs to generic ones (for GS)
        if (builtInUsage.gs.position)
        {
            inOutUsage.builtInOutputLocMap[BuiltInPosition] = availOutMapLoc++;
        }

        if (builtInUsage.gs.pointSize)
        {
            inOutUsage.builtInOutputLocMap[BuiltInPointSize] = availOutMapLoc++;
        }

        if (builtInUsage.gs.clipDistance > 0)
        {
            inOutUsage.builtInOutputLocMap[BuiltInClipDistance] = availOutMapLoc++;
            if (builtInUsage.gs.clipDistance > 4)
            {
                ++availOutMapLoc;
            }
        }

        if (builtInUsage.gs.cullDistance > 0)
        {
            inOutUsage.builtInOutputLocMap[BuiltInCullDistance] = availOutMapLoc++;
            if (builtInUsage.gs.cullDistance > 4)
            {
                ++availOutMapLoc;
            }
        }

        if (builtInUsage.gs.primitiveId)
        {
            inOutUsage.builtInOutputLocMap[BuiltInPrimitiveId] = availOutMapLoc++;
        }

        if (builtInUsage.gs.layer)
        {
            inOutUsage.builtInOutputLocMap[BuiltInLayer] = availOutMapLoc++;
        }

        if (builtInUsage.gs.viewIndex)
        {
            inOutUsage.builtInOutputLocMap[BuiltInViewIndex] = availOutMapLoc++;
        }

        if (builtInUsage.gs.viewportIndex)
        {
            inOutUsage.builtInOutputLocMap[BuiltInViewportIndex] = availOutMapLoc++;
        }

        // Map built-in outputs to generic ones (for copy shader)
        auto& builtInOutLocs = inOutUsage.gs.builtInOutLocs;

        if (nextStage == ShaderStageFragment)
        {
            // GS  ==>  FS
            const auto& nextBuiltInUsage = pNextResUsage->builtInUsage.fs;
            auto& nextInOutUsage = pNextResUsage->inOutUsage;

            if (nextBuiltInUsage.clipDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInClipDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInClipDistance];
                builtInOutLocs[BuiltInClipDistance] = mapLoc;
            }

            if (nextBuiltInUsage.cullDistance > 0)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInCullDistance) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInCullDistance];
                builtInOutLocs[BuiltInCullDistance] = mapLoc;
            }

            if (nextBuiltInUsage.primitiveId)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInPrimitiveId) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInPrimitiveId];
                builtInOutLocs[BuiltInPrimitiveId] = mapLoc;
            }

            if (nextBuiltInUsage.layer)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInLayer) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInLayer];
                builtInOutLocs[BuiltInLayer] = mapLoc;
            }

            if (nextBuiltInUsage.viewIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewIndex) !=
                    nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewIndex];
                builtInOutLocs[BuiltInViewIndex] = mapLoc;
            }

            if (nextBuiltInUsage.viewportIndex)
            {
                LLPC_ASSERT(nextInOutUsage.builtInInputLocMap.find(BuiltInViewportIndex) !=
                            nextInOutUsage.builtInInputLocMap.end());
                const uint32_t mapLoc = nextInOutUsage.builtInInputLocMap[BuiltInViewportIndex];
                builtInOutLocs[BuiltInViewportIndex] = mapLoc;
            }
        }
        else if (nextStage == ShaderStageInvalid)
        {
            // GS only
            uint32_t availOutMapLoc = inOutUsage.outputLocMap.size(); // Reset available location

            if ((builtInUsage.gs.clipDistance > 0) || (builtInUsage.gs.cullDistance > 0))
            {
                uint32_t mapLoc = availOutMapLoc++;
                if (builtInUsage.gs.clipDistance + builtInUsage.gs.cullDistance > 4)
                {
                    LLPC_ASSERT(builtInUsage.gs.clipDistance +
                                builtInUsage.gs.cullDistance <= MaxClipCullDistanceCount);
                    ++availOutMapLoc; // Occupy two locations
                }

                if (builtInUsage.gs.clipDistance > 0)
                {
                    builtInOutLocs[BuiltInClipDistance] = mapLoc;
                }

                if (builtInUsage.gs.cullDistance > 0)
                {
                    if (builtInUsage.gs.clipDistance >= 4)
                    {
                        ++mapLoc;
                    }
                    builtInOutLocs[BuiltInCullDistance] = mapLoc;
                }
            }

            if (builtInUsage.gs.primitiveId)
            {
                builtInOutLocs[BuiltInPrimitiveId] = availOutMapLoc++;
            }

            if (builtInUsage.gs.viewportIndex)
            {
                builtInOutLocs[BuiltInViewportIndex] = availOutMapLoc++;
            }

            if (builtInUsage.gs.layer)
            {
                builtInOutLocs[BuiltInLayer] = availOutMapLoc++;
            }

            if (builtInUsage.gs.viewIndex)
            {
                builtInOutLocs[BuiltInViewIndex] = availOutMapLoc++;
            }
        }

        inOutUsage.inputMapLocCount = std::max(inOutUsage.inputMapLocCount, availInMapLoc);
        inOutUsage.outputMapLocCount = std::max(inOutUsage.outputMapLocCount, availOutMapLoc);
    }
    else if (m_shaderStage == ShaderStageFragment)
    {
        // FS
        uint32_t availInMapLoc = inOutUsage.inputMapLocCount;

        if (builtInUsage.fs.pointCoord)
        {
            inOutUsage.builtInInputLocMap[BuiltInPointCoord] = availInMapLoc++;
        }

        if (builtInUsage.fs.primitiveId)
        {
            inOutUsage.builtInInputLocMap[BuiltInPrimitiveId] = availInMapLoc++;
        }

        if (builtInUsage.fs.layer)
        {
            inOutUsage.builtInInputLocMap[BuiltInLayer] = availInMapLoc++;
        }

        if (builtInUsage.fs.viewIndex)
        {
            inOutUsage.builtInInputLocMap[BuiltInViewIndex] = availInMapLoc++;
        }

        if (builtInUsage.fs.viewportIndex)
        {
            inOutUsage.builtInInputLocMap[BuiltInViewportIndex] = availInMapLoc++;
        }

        if ((builtInUsage.fs.clipDistance > 0) || (builtInUsage.fs.cullDistance > 0))
        {
            uint32_t mapLoc = availInMapLoc++;
            if (builtInUsage.fs.clipDistance + builtInUsage.fs.cullDistance > 4)
            {
                LLPC_ASSERT(builtInUsage.fs.clipDistance +
                            builtInUsage.fs.cullDistance <= MaxClipCullDistanceCount);
                ++availInMapLoc; // Occupy two locations
            }

            if (builtInUsage.fs.clipDistance > 0)
            {
                inOutUsage.builtInInputLocMap[BuiltInClipDistance] = mapLoc;
            }

            if (builtInUsage.fs.cullDistance > 0)
            {
                if (builtInUsage.fs.clipDistance >= 4)
                {
                    ++mapLoc;
                }
                inOutUsage.builtInInputLocMap[BuiltInCullDistance] = mapLoc;
            }
        }

        inOutUsage.inputMapLocCount = std::max(inOutUsage.inputMapLocCount, availInMapLoc);
    }

    // Do builtin-to-generic mapping
    LLPC_OUTS("===============================================================================\n");
    LLPC_OUTS("// LLPC builtin-to-generic mapping results (" << GetShaderStageName(m_shaderStage)
              << " shader)\n\n");
    uint32_t nextMapLoc = 0;
    if (inOutUsage.builtInInputLocMap.empty() == false)
    {
        for (const auto& builtInMap : inOutUsage.builtInInputLocMap)
        {
            const BuiltIn builtInId = static_cast<BuiltIn>(builtInMap.first);
            const uint32_t loc = builtInMap.second;
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input:  builtin = "
                          << getNameMap(builtInId).map(builtInId).substr(strlen("BuiltIn"))
                          << "  =>  Mapped = " << loc << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (inOutUsage.builtInOutputLocMap.empty() == false)
    {
        for (const auto& builtInMap : inOutUsage.builtInOutputLocMap)
        {
            const BuiltIn builtInId = static_cast<BuiltIn>(builtInMap.first);
            const uint32_t loc = builtInMap.second;
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output: builtin = "
                          << getNameMap(builtInId).map(builtInId).substr(strlen("BuiltIn"))
                          << "  =>  Mapped = " << loc << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (inOutUsage.perPatchBuiltInInputLocMap.empty() == false)
    {
        for (const auto& builtInMap : inOutUsage.perPatchBuiltInInputLocMap)
        {
            const BuiltIn builtInId = static_cast<BuiltIn>(builtInMap.first);
            const uint32_t loc = builtInMap.second;
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input (per-patch):  builtin = "
                          << getNameMap(builtInId).map(builtInId).substr(strlen("BuiltIn"))
                          << "  =>  Mapped = " << loc << "\n");
        }
        LLPC_OUTS("\n");
    }

    if (inOutUsage.perPatchBuiltInOutputLocMap.empty() == false)
    {
        for (const auto& builtInMap : inOutUsage.perPatchBuiltInOutputLocMap)
        {
            const BuiltIn builtInId = static_cast<BuiltIn>(builtInMap.first);
            const uint32_t loc = builtInMap.second;
            LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output (per-patch): builtin = "
                          << getNameMap(builtInId).map(builtInId).substr(strlen("BuiltIn"))
                          << "  =>  Mapped = " << loc << "\n");
        }
        LLPC_OUTS("\n");
    }

    LLPC_OUTS("// LLPC location count results (after builtin-to-generic mapping)\n\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input:  loc count = "
                  << inOutUsage.inputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output: loc count = "
                  << inOutUsage.outputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Input (per-patch):  loc count = "
                  << inOutUsage.perPatchInputMapLocCount << "\n");
    LLPC_OUTS("(" << GetShaderStageAbbreviation(m_shaderStage, true) << ") Output (per-patch): loc count = "
                  << inOutUsage.perPatchOutputMapLocCount << "\n");
    LLPC_OUTS("\n");
}

// =====================================================================================================================
// Revises the usage of execution modes for tessellation shader.
void PatchResourceCollect::ReviseTessExecutionMode()
{
    LLPC_ASSERT((m_shaderStage == ShaderStageTessControl) || (m_shaderStage == ShaderStageTessEval));

    // NOTE: Usually, "output vertices" is specified on tessellation control shader and "vertex spacing", "vertex
    // order", "point mode", "primitive mode" are all specified on tessellation evaluation shader according to GLSL
    // spec. However, SPIR-V spec allows those execution modes to be specified on any of tessellation shader. So we
    // have to revise the execution modes and make them follow GLSL spec.
    auto& tcsBuiltInUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
    auto& tesBuiltInUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;

    if (tcsBuiltInUsage.outputVertices == 0)
    {
        if (tesBuiltInUsage.outputVertices != 0)
        {
            tcsBuiltInUsage.outputVertices = tesBuiltInUsage.outputVertices;
            tesBuiltInUsage.outputVertices = 0;
        }
        else
        {
            tcsBuiltInUsage.outputVertices = MaxTessPatchVertices;
        }
    }

    if (tesBuiltInUsage.vertexSpacing == SpacingUnknown)
    {
        if (tcsBuiltInUsage.vertexSpacing != SpacingUnknown)
        {
            tesBuiltInUsage.vertexSpacing = tcsBuiltInUsage.vertexSpacing;
            tcsBuiltInUsage.vertexSpacing = SpacingUnknown;
        }
        else
        {
            tesBuiltInUsage.vertexSpacing = SpacingEqual;
        }
    }

    if (tesBuiltInUsage.vertexOrder == VertexOrderUnknown)
    {
        if (tcsBuiltInUsage.vertexOrder != VertexOrderUnknown)
        {
            tesBuiltInUsage.vertexOrder = tcsBuiltInUsage.vertexOrder;
            tcsBuiltInUsage.vertexOrder = VertexOrderUnknown;
        }
        else
        {
            tesBuiltInUsage.vertexOrder = VertexOrderCcw;
        }
    }

    if (tesBuiltInUsage.pointMode == false)
    {
        if (tcsBuiltInUsage.pointMode)
        {
            tesBuiltInUsage.pointMode = tcsBuiltInUsage.pointMode;
            tcsBuiltInUsage.pointMode = false;
        }
    }

    if (tesBuiltInUsage.primitiveMode == Unknown)
    {
        if (tcsBuiltInUsage.primitiveMode != Unknown)
        {
            tesBuiltInUsage.primitiveMode = tcsBuiltInUsage.primitiveMode;
            tcsBuiltInUsage.primitiveMode = Unknown;
        }
        else
        {
            tesBuiltInUsage.primitiveMode = Triangles;
        }
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patch operations for resource collecting.
INITIALIZE_PASS(PatchResourceCollect, "Patch-resource-collect",
                "Patch LLVM for resource collecting", false, false)
