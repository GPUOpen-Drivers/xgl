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
 * @file  llpcPatchImageOp.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchImageOp.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-image-op"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcPatchImageOp.h"

using namespace llvm;
using namespace Llpc;

namespace llvm
{

namespace cl
{

extern opt<bool> EnableShadowDescriptorTable;

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchImageOp::ID = 0;

// =====================================================================================================================
PatchImageOp::PatchImageOp()
    :
    Patch(ID)
{
    initializePatchImageOpPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool PatchImageOp::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Image-Op\n");

    Patch::Init(&module);

    // Invoke handling of "call" instruction
    visit(m_pModule);

    for (auto pCallInst: m_imageCalls)
    {
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "call" instruction.
void PatchImageOp::visitCallInst(
    CallInst& callInst) // [in] "Call" instruction
{
    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    auto mangledName = pCallee->getName();
    if (mangledName.startswith(LlpcName::ImageCallPrefix))
    {
        ShaderImageCallMetadata imageCallMeta = {};
        LLPC_ASSERT(callInst.getNumArgOperands() >= 2);
        uint32_t metaOperandIndex = callInst.getNumArgOperands() - 1; // Image call metadata is last argument
        imageCallMeta.U32All =  cast<ConstantInt>(callInst.getArgOperand(metaOperandIndex))->getZExtValue();

        std::string callName = mangledName.str();
        std::vector<Value*> args;

        if (imageCallMeta.Multisampled || (imageCallMeta.Dim == DimSubpassData))
        {
            if (imageCallMeta.Multisampled)
            {
                // Add name modifier for F-mask based fetch or F-mask only fetch
                const PipelineShaderInfo* pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);

                ConstantInt* pDescSet = cast<ConstantInt>(callInst.getArgOperand(0));
                ConstantInt* pBinding = cast<ConstantInt>(callInst.getArgOperand(1));
                uint32_t descSet = pDescSet->getZExtValue();
                uint32_t binding = pBinding->getZExtValue();

                const ResourceMappingNode* pResourceNode  = nullptr;
                const ResourceMappingNode* pFmaskNode     = nullptr;
                for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
                {
                    const ResourceMappingNode* pSetNode = &pShaderInfo->pUserDataNodes[i];
                    if (pSetNode->type == ResourceMappingNodeType::DescriptorTableVaPtr)
                    {
                        for (uint32_t i = 0; i < pSetNode->tablePtr.nodeCount; ++i)
                        {
                            const ResourceMappingNode* pNode = &pSetNode->tablePtr.pNext[i];
                            if ((pNode->srdRange.set == descSet) && (pNode->srdRange.binding == binding))
                            {
                                if ((pNode->type == ResourceMappingNodeType::DescriptorResource) ||
                                    (pNode->type == ResourceMappingNodeType::DescriptorCombinedTexture))
                                {
                                    pResourceNode = pNode;
                                    // NOTE: When shadow descriptor table is enable, we need get F-mask descriptor
                                    // node from associated multi-sampled texture resource node.
                                    if (cl::EnableShadowDescriptorTable)
                                    {
                                        // Fmask based fetch only can work for texel fetch or load subpass data
                                        if((imageCallMeta.OpKind == ImageOpFetch) ||
                                           ((imageCallMeta.OpKind == ImageOpRead) &&
                                            (imageCallMeta.Dim == DimSubpassData)))
                                        {
                                            pFmaskNode = pNode;
                                        }
                                    }
                                }
                                else if ((pNode->type == ResourceMappingNodeType::DescriptorFmask) &&
                                         (pFmaskNode == nullptr))
                                {
                                    pFmaskNode = pNode;
                                }
                            }
                        }
                    }
                }

                // For multi-sampled image, F-mask is only taken into account for texel fetch (not for query)
                if (imageCallMeta.OpKind != ImageOpQueryNonLod)
                {
                    auto fmaskPatchPos = callName.find(gSPIRVName::ImageCallModPatchFmaskUsage);
                    if (fmaskPatchPos != std::string::npos)
                    {
                        callName = callName.substr(0, fmaskPatchPos);
                        if ((pResourceNode != nullptr) && (pFmaskNode != nullptr))
                        {
                            // Fmask based fetch only can work for texel fetch or load subpass data
                            if((imageCallMeta.OpKind == ImageOpFetch) ||
                                ((imageCallMeta.OpKind == ImageOpRead) &&
                                (imageCallMeta.Dim == DimSubpassData)))
                            {
                                callName += gSPIRVName::ImageCallModFmaskBased;
                            }
                        }
                        else if (pFmaskNode != nullptr)
                        {
                            callName += gSPIRVName::ImageCallModFmaskId;
                        }
                    }
                }
            }

            const auto enableMultiView = (reinterpret_cast<const GraphicsPipelineBuildInfo*>(
                m_pContext->GetPipelineBuildInfo()))->iaState.enableMultiView;

            for (uint32_t i = 0; i < callInst.getNumArgOperands(); ++i)
            {
                Value* pArg = callInst.getArgOperand(i);
                if ((imageCallMeta.Dim == DimSubpassData) && (i == 3))
                {
                    LLPC_ASSERT(m_shaderStage == ShaderStageFragment);
                    auto& entryArgIdxs = m_pContext->GetShaderInterfaceData(ShaderStageFragment)->entryArgIdxs.fs;

                    // NOTE: For subpass data, if multiview enabled , gl_FragCoord and gl_ViewIndex
                    // are added as actual coordinate.

                    LLPC_ASSERT(pArg->getType()->isVectorTy() &&
                                (pArg->getType()->getVectorNumElements() == 2) &&
                                (pArg->getType()->getVectorElementType()->isIntegerTy()));

                    Value* pFragCoordX = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.x);
                    Value* pFragCoordY = GetFunctionArgument(m_pEntryPoint, entryArgIdxs.fragCoord.y);

                    Type* pFragCoordTy = enableMultiView ? m_pContext->Floatx3Ty(): m_pContext->Floatx2Ty();
                    Value* pFragCoord = UndefValue::get(pFragCoordTy);
                    pFragCoord = InsertElementInst::Create(pFragCoord,
                                                           pFragCoordX,
                                                           ConstantInt::get(m_pContext->Int32Ty(), 0, true),
                                                           "",
                                                           &callInst);
                    pFragCoord = InsertElementInst::Create(pFragCoord,
                                                           pFragCoordY,
                                                           ConstantInt::get(m_pContext->Int32Ty(), 1, true),
                                                           "",
                                                           &callInst);
                    if (enableMultiView)
                    {
                        Value* pViewIndex = ExtractElementInst::Create(pArg,
                                                                       ConstantInt::get(m_pContext->Int32Ty(), 0, true),
                                                                       "",
                                                                       &callInst);
                        pViewIndex = new SIToFPInst(pViewIndex, m_pContext->FloatTy(), "", &callInst);
                        pFragCoord = InsertElementInst::Create(pFragCoord,
                                                               pViewIndex,
                                                               ConstantInt::get(m_pContext->Int32Ty(), 2),
                                                               "",
                                                               &callInst);

                        pFragCoord = new FPToSIInst(pFragCoord,
                                                    m_pContext->Int32x3Ty(),
                                                    "",
                                                    &callInst);

                        // Replace dimension SubpassData with SubpassDataArray
                        std::string dimSubpassData = SPIRVDimNameMap::map(DimSubpassData);
                        callName.replace(callName.find(dimSubpassData), dimSubpassData.length(), dimSubpassData + "Array");
                    }
                    else
                    {
                        pFragCoord = CastInst::Create(Instruction::FPToSI,
                                                      pFragCoord,
                                                      m_pContext->Int32x2Ty(),
                                                      "",
                                                      &callInst);
                        pFragCoord = BinaryOperator::Create(Instruction::Add, pFragCoord, pArg, "", &callInst);
                    }
                    args.push_back(pFragCoord);
                }
                else
                {
                    args.push_back(pArg);
                }
            }

            CallInst* pImageCall = cast<CallInst>(EmitCall(m_pModule,
                                                           callName,
                                                           callInst.getType(),
                                                           args,
                                                           NoAttrib,
                                                           &callInst));

            callInst.replaceAllUsesWith(pImageCall);

            m_imageCalls.insert(&callInst);
        }
        else if ((imageCallMeta.OpKind == ImageOpQueryNonLod) && (imageCallMeta.Dim == DimBuffer))
        {
            // NOTE: For image buffer, the implementation of query size is different (between GFX6/7 and GFX8).
            const GfxIpVersion gfxIp = m_pContext->GetGfxIpVersion();
            if (gfxIp.major <= 8)
            {
                for (uint32_t i = 0; i < callInst.getNumArgOperands(); ++i)
                {
                    Value* pArg = callInst.getArgOperand(i);
                    args.push_back(pArg);
                }

                callName += (gfxIp.major == 8) ? ".gfx8" : ".gfx6";
                CallInst* pImageCall = cast<CallInst>(EmitCall(m_pModule,
                                                               callName,
                                                               callInst.getType(),
                                                               args,
                                                               NoAttrib,
                                                               &callInst));

                callInst.replaceAllUsesWith(pImageCall);

                m_imageCalls.insert(&callInst);
            }
        }
        else if ((imageCallMeta.Dim == DimBuffer) &&
                 ((imageCallMeta.OpKind == ImageOpFetch) ||
                  (imageCallMeta.OpKind == ImageOpRead) ||
                  (imageCallMeta.OpKind == ImageOpWrite) ||
                  (imageCallMeta.OpKind == ImageOpAtomicExchange) ||
                  (imageCallMeta.OpKind == ImageOpAtomicCompareExchange) ||
                  (imageCallMeta.OpKind == ImageOpAtomicIIncrement) ||
                  (imageCallMeta.OpKind == ImageOpAtomicIDecrement) ||
                  (imageCallMeta.OpKind == ImageOpAtomicIAdd) ||
                  (imageCallMeta.OpKind == ImageOpAtomicISub) ||
                  (imageCallMeta.OpKind == ImageOpAtomicSMin) ||
                  (imageCallMeta.OpKind == ImageOpAtomicUMin) ||
                  (imageCallMeta.OpKind == ImageOpAtomicSMax) ||
                  (imageCallMeta.OpKind == ImageOpAtomicUMax) ||
                  (imageCallMeta.OpKind == ImageOpAtomicAnd) ||
                  (imageCallMeta.OpKind == ImageOpAtomicOr) ||
                  (imageCallMeta.OpKind == ImageOpAtomicXor)))
        {
            // TODO: This is a workaround and should be removed after backend compiler fixes it. The issue is: for
            // GFX9, when texel offset is constant zero, backend will unset "idxen" flag and provide no VGPR as
            // the address. This only works on pre-GFX9.
            const GfxIpVersion gfxIp = m_pContext->GetGfxIpVersion();
            if (gfxIp.major == 9)
            {
                // Get offset from operands
                Value* pTexelOffset = callInst.getArgOperand(3);
                if (isa<ConstantInt>(*pTexelOffset))
                {
                    const uint32_t texelOffset = cast<ConstantInt>(*pTexelOffset).getZExtValue();
                    if (texelOffset == 0)
                    {
                        Value* pPc = EmitCall(m_pModule, "llvm.amdgcn.s.getpc", m_pContext->Int64Ty(), args, NoAttrib, &callInst);
                        pPc = new BitCastInst(pPc, m_pContext->Int32x2Ty(), "", &callInst);
                        Value* pPcHigh = ExtractElementInst::Create(pPc,
                                                                    ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                                    "",
                                                                    &callInst);
                        // NOTE: Here, we construct a non-constant zero value to disable the mistaken optimization in
                        // backend compiler. The most significant 8 bits of PC is always equal to zero. It is safe to
                        // do this.
                        Value* pTexelOffset = BinaryOperator::CreateLShr(pPcHigh,
                                                                        ConstantInt::get(m_pContext->Int32Ty(), 24),
                                                                        "",
                                                                        &callInst);

                        callInst.setArgOperand(3, pTexelOffset);
                    }
                }
            }
        }

        if ((imageCallMeta.OpKind == ImageOpSample) ||
            (imageCallMeta.OpKind == ImageOpGather) ||
            (imageCallMeta.OpKind == ImageOpFetch))
        {
            // Call optimized version if LOD is provided with constant 0 value
            if (mangledName.find(gSPIRVName::ImageCallModLod) != std::string::npos)
            {
                uint32_t argCount = callInst.getNumArgOperands();
                bool hasConstOffset = (mangledName.find(gSPIRVName::ImageCallModConstOffset) != std::string::npos);
                // For all supported image operations, LOD argument is the second to last operand or third to last
                // operand if constant offset is persent.
                uint32_t lodArgIdx = hasConstOffset ? (argCount - 3) : (argCount - 2);

                // If LOD argument is constant 0, call zero-LOD version of image operation implementation
                auto pLod = callInst.getArgOperand(lodArgIdx);
                if (isa<Constant>(*pLod) && cast<Constant>(pLod)->isZeroValue())
                {
                    for (uint32_t i = 0; i < callInst.getNumArgOperands(); ++i)
                    {
                        Value* pArg = callInst.getArgOperand(i);
                        args.push_back(pArg);
                    }

                    std::string callNameLodz = callName.replace(callName.find(gSPIRVName::ImageCallModLod),
                                                                strlen(gSPIRVName::ImageCallModLod),
                                                                gSPIRVName::ImageCallModLodz);
                    CallInst* pImageCall = cast<CallInst>(EmitCall(m_pModule,
                                                                   callNameLodz,
                                                                   callInst.getType(),
                                                                   args,
                                                                   NoAttrib,
                                                                   &callInst));

                    callInst.replaceAllUsesWith(pImageCall);

                    m_imageCalls.insert(&callInst);
                }
            }
        }
    }
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patch operations for image operations.
INITIALIZE_PASS(PatchImageOp, "patch-image-op",
                "Patch LLVM for for image operations (F-mask support)", false, false)
