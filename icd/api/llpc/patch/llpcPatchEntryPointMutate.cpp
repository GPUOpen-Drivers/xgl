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
 * @file  llpcPatchEntryPointMutate.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchEntryPointMutate.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-entry-point-mutate"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcGfx6Chip.h"
#include "llpcGfx9Chip.h"
#include "llpcIntrinsDefs.h"
#include "llpcPatchEntryPointMutate.h"

using namespace llvm;
using namespace Llpc;

namespace llvm
{

namespace cl
{

// -vgpr-limit: maximum VGPR limit for this shader
static opt<uint32_t> VgprLimit("vgpr-limit", desc("Maximum VGPR limit for this shader"), init(0));

// -sgpr-limit: maximum SGPR limit for this shader
static opt<uint32_t> SgprLimit("sgpr-limit", desc("Maximum SGPR limit for this shader"), init(0));

// -waves-per-eu: the range of waves per EU for this shader
static opt<std::string> WavesPerEu("waves-per-eu",
                                   desc("The range of waves per EU for this shader"),
                                   value_desc("minVal,maxVal"),
                                   init(""));

// -inreg-esgs-lds-size: Add a dummy "inreg" argument for ES-GS LDS size, this is to keep consistent with PAL's
// GS on-chip behavior. In the future, if PAL allows hardcoded ES-GS LDS size, this option could be deprecated.
opt<bool> InRegEsGsLdsSize("inreg-esgs-lds-size",
                          desc("For GS on-chip, add esGsLdsSize in user data"),
                          init(true));

extern opt<bool> EnableShadowDescriptorTable;

extern opt<uint32_t> ShadowDescTablePtrHigh;

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchEntryPointMutate::ID = 0;

// =====================================================================================================================
PatchEntryPointMutate::PatchEntryPointMutate()
    :
    Patch(ID),
    m_hasTs(false),
    m_hasGs(false)
{
    initializePatchEntryPointMutatePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM patching pass on the specified LLVM module.
bool PatchEntryPointMutate::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Entry-Point-Mutate\n");

    Patch::Init(&module);

    const uint32_t stageMask = m_pContext->GetShaderStageMask();
    m_hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                             ShaderStageToMask(ShaderStageTessEval))) != 0);
    m_hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    const auto& dataLayout = m_pModule->getDataLayout();

    // Create new entry-point from the original one (mutate it)
    // TODO: We should mutate entry-point arguments instead of clone a new entry-point.
    uint64_t inRegMask = 0;
    FunctionType* pEntryPointTy = GenerateEntryPointType(&inRegMask);

    Function* pOrigEntryPoint = GetEntryPoint(m_pModule);

    Function* pEntryPoint = Function::Create(pEntryPointTy,
                                             GlobalValue::ExternalLinkage,
                                             "main",
                                             m_pModule);
    pEntryPoint->setCallingConv(pOrigEntryPoint->getCallingConv());
    pEntryPoint->addFnAttr(Attribute::NoUnwind);

    ValueToValueMapTy valueMap;
    SmallVector<ReturnInst*, 8> retInsts;
    CloneFunctionInto(pEntryPoint, pOrigEntryPoint, valueMap, false, retInsts);

    // Set Attributes on cloned function here as some are overwritten during CloneFunctionInto otherwise
    AttrBuilder builder;
    if (m_shaderStage == ShaderStageFragment)
    {
        auto& builtInUsage = m_pContext->GetShaderResourceUsage(ShaderStageFragment)->builtInUsage.fs;

        SpiPsInputAddr spiPsInputAddr = {};

        spiPsInputAddr.PERSP_SAMPLE_ENA     = ((builtInUsage.smooth && builtInUsage.sample) ||
                                               builtInUsage.baryCoordSmoothSample);
        spiPsInputAddr.PERSP_CENTER_ENA     = ((builtInUsage.smooth && builtInUsage.center) ||
                                               builtInUsage.baryCoordSmooth);
        spiPsInputAddr.PERSP_CENTROID_ENA   = ((builtInUsage.smooth && builtInUsage.centroid) ||
                                               builtInUsage.baryCoordSmoothCentroid);
        spiPsInputAddr.PERSP_PULL_MODEL_ENA = ((builtInUsage.smooth && builtInUsage.pullMode) ||
                                               builtInUsage.baryCoordPullModel);
        spiPsInputAddr.LINEAR_SAMPLE_ENA    = ((builtInUsage.noperspective && builtInUsage.sample) ||
                                               builtInUsage.baryCoordNoPerspSample);
        spiPsInputAddr.LINEAR_CENTER_ENA    = ((builtInUsage.noperspective && builtInUsage.center) ||
                                               builtInUsage.baryCoordNoPersp);
        spiPsInputAddr.LINEAR_CENTROID_ENA  = ((builtInUsage.noperspective && builtInUsage.centroid) ||
                                               builtInUsage.baryCoordNoPerspCentroid);
        spiPsInputAddr.POS_X_FLOAT_ENA      = builtInUsage.fragCoord;
        spiPsInputAddr.POS_Y_FLOAT_ENA      = builtInUsage.fragCoord;
        spiPsInputAddr.POS_Z_FLOAT_ENA      = builtInUsage.fragCoord;
        spiPsInputAddr.POS_W_FLOAT_ENA      = builtInUsage.fragCoord;
        spiPsInputAddr.FRONT_FACE_ENA       = builtInUsage.frontFacing;
        spiPsInputAddr.ANCILLARY_ENA        = builtInUsage.sampleId;
        spiPsInputAddr.SAMPLE_COVERAGE_ENA  = builtInUsage.sampleMaskIn;

        builder.addAttribute("InitialPSInputAddr", std::to_string(spiPsInputAddr.u32All));
    }

    // Set vgpr,sgpr and wave limits
    uint32_t vgprLimit = 0;
    uint32_t sgprLimit = 0;
    std::string wavesPerEu;
    auto pShaderOptions = &(m_pContext->GetPipelineShaderInfo(m_shaderStage)->options);
    auto pResourceUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    pResourceUsage->numSgprsAvailable = m_pContext->GetGpuProperty()->maxSgprsAvailable;
    pResourceUsage->numVgprsAvailable = m_pContext->GetGpuProperty()->maxVgprsAvailable;

    if ((pShaderOptions->vgprLimit != 0) && (pShaderOptions->vgprLimit != UINT32_MAX))
    {
        vgprLimit = pShaderOptions->vgprLimit;
    }
    else if (cl::VgprLimit != 0)
    {
        vgprLimit = cl::VgprLimit;
    }

    if (vgprLimit != 0)
    {
        builder.addAttribute("amdgpu-num-vgpr", std::to_string(vgprLimit));
        pResourceUsage->numVgprsAvailable = std::min(vgprLimit, pResourceUsage->numVgprsAvailable);
    }

    if ((pShaderOptions->sgprLimit != 0) && (pShaderOptions->sgprLimit != UINT32_MAX))
    {
        sgprLimit = pShaderOptions->sgprLimit;
    }
    else if (cl::SgprLimit != 0)
    {
        sgprLimit = cl::SgprLimit;
    }

    if (sgprLimit != 0)
    {
        builder.addAttribute("amdgpu-num-sgpr", std::to_string(sgprLimit));
        pResourceUsage->numSgprsAvailable = std::min(sgprLimit, pResourceUsage->numSgprsAvailable);
    }

    if (pShaderOptions->maxThreadGroupsPerComputeUnit != 0)
    {
        wavesPerEu = std::string("0,") + std::to_string(pShaderOptions->maxThreadGroupsPerComputeUnit);
    }
    else if (cl::WavesPerEu.empty() == false)
    {
        wavesPerEu = cl::WavesPerEu;
    }

    if (wavesPerEu.empty() == false)
    {
        builder.addAttribute("amdgpu-waves-per-eu", wavesPerEu);
    }

    AttributeList::AttrIndex attribIdx = AttributeList::AttrIndex(AttributeList::FunctionIndex);
    pEntryPoint->addAttributes(attribIdx, builder);

    // NOTE: Remove "readnone" attribute for entry-point. If GS is emtry, this attribute will allow
    // LLVM optimization to remove sendmsg(GS_DONE). It is unexpected.
    if (pEntryPoint->hasFnAttribute(Attribute::ReadNone))
    {
        pEntryPoint->removeFnAttr(Attribute::ReadNone);
    }

    // Update attributes of new entry-point
    for (auto pArg = pEntryPoint->arg_begin(), pEnd = pEntryPoint->arg_end(); pArg != pEnd; ++pArg)
    {
        auto argIdx = pArg->getArgNo();
        if (inRegMask & (1ull << argIdx))
        {
            pArg->addAttr(Attribute::InReg);
        }
    }

    // Update Shader interface data according to new entry-point
    auto pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);
    auto pIntfData   = m_pContext->GetShaderInterfaceData(m_shaderStage);

    auto pInsertPos = pEntryPoint->begin()->getFirstInsertionPt();

    // Global internal table
    auto pInternalTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                            dataLayout.getAllocaAddrSpace(),
                                            "",
                                            &*pInsertPos);
    auto pInternalTablePtrLow = GetFunctionArgument(pEntryPoint, 0);

    auto pDescTablePtrTy = PointerType::get(ArrayType::get(m_pContext->Int8Ty(), UINT32_MAX), ADDR_SPACE_CONST);

    // Use s_getpc if descriptor table ptr high isn't available
    std::vector<Value*> args; // Empty arguments
    Value* pPc = EmitCall(m_pModule, "llvm.amdgcn.s.getpc", m_pContext->Int64Ty(), args, NoAttrib, &*pInsertPos);
    pPc = new BitCastInst(pPc, m_pContext->Int32x2Ty(), "", &*pInsertPos);
    Value* pDescTablePtrHigh = ExtractElementInst::Create(pPc,
                                                          ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                          "",
                                                          &*pInsertPos);

    pIntfData->pInternalTablePtr = InitPointerWithValue(pInternalTablePtr,
                                                        pInternalTablePtrLow,
                                                        pDescTablePtrHigh,
                                                        pDescTablePtrTy,
                                                        &*pInsertPos);

    if (m_pContext->GetShaderResourceUsage(m_shaderStage)->perShaderTable)
    {
        auto pInternalPerShaderTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                            dataLayout.getAllocaAddrSpace(),
                                            "",
                                            &*pInsertPos);

        // Per shader table is always the second function argument
        auto pInternalTablePtrLow = GetFunctionArgument(pEntryPoint, 1);

        pIntfData->pInternalPerShaderTablePtr = InitPointerWithValue(pInternalPerShaderTablePtr,
                                                                     pInternalTablePtrLow,
                                                                     pDescTablePtrHigh,
                                                                     pDescTablePtrTy,
                                                                     &*pInsertPos);
    }

    // Initialize spill table pointer
    if (pIntfData->entryArgIdxs.spillTable != InvalidValue)
    {
        // Initialize the base pointer
        auto pSpillTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                             dataLayout.getAllocaAddrSpace(),
                                             "",
                                             &*pInsertPos);
        auto pSpillTablePtrLow = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.spillTable);
        auto pSpillTablePtrTy = PointerType::get(ArrayType::get(m_pContext->Int8Ty(), InterfaceData::MaxSpillTableSize),
                                                 ADDR_SPACE_CONST);
        pIntfData->spillTable.pTablePtr = InitPointerWithValue(pSpillTablePtr,
                                                               pSpillTablePtrLow,
                                                               pDescTablePtrHigh,
                                                               pSpillTablePtrTy,
                                                               &*pInsertPos);

        // Initialize the pointer for push constant
        if (pIntfData->pushConst.resNodeIdx != InvalidValue)
        {
            auto pPushConstNode = &pShaderInfo->pUserDataNodes[pIntfData->pushConst.resNodeIdx];
            if (pPushConstNode->offsetInDwords >= pIntfData->spillTable.offsetInDwords)
            {
                auto pPushConstTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                                         dataLayout.getAllocaAddrSpace(),
                                                         "",
                                                         &*pInsertPos);
                uint32_t pustConstOffset = pPushConstNode->offsetInDwords * sizeof(uint32_t);
                auto pPushConstOffset = ConstantInt::get(m_pContext->Int32Ty(), pustConstOffset);
                auto pPushConstTablePtrLow = BinaryOperator::CreateAdd(pSpillTablePtrLow, pPushConstOffset, "", &*pInsertPos);
                pIntfData->pushConst.pTablePtr = InitPointerWithValue(pPushConstTablePtr,
                                                                      pPushConstTablePtrLow,
                                                                      pDescTablePtrHigh,
                                                                      pSpillTablePtrTy,
                                                                      &*pInsertPos);
            }
        }
    }

    uint32_t dynDescIdx = 0;
    // Descriptor sets and vertex buffer
    for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
    {
        auto pNode = &pShaderInfo->pUserDataNodes[i];

        Value* pResNodeValue = nullptr;
        if (pNode->type == ResourceMappingNodeType::IndirectUserDataVaPtr)
        {
            // Do nothing
        }
        else if (IsResourceMappingNodeActive(pNode) == false)
        {
            if  ((pNode->type == ResourceMappingNodeType::DescriptorResource) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorSampler) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorTexelBuffer) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorFmask) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorBuffer) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorBufferCompact))
            {
                ++dynDescIdx;
            }
            // Do nothing for inactive node
            continue;
        }
        else if ((i < InterfaceData::MaxDescTableCount) && (pIntfData->entryArgIdxs.resNodeValues[i] > 0))
        {
            // Resource node isn't spilled, load its value from function argument
            pResNodeValue = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.resNodeValues[i]);
        }
        else if (pNode->type != ResourceMappingNodeType::PushConst)
        {
            // Resource node is spilled, load its value from spill table
            uint32_t byteOffset = pNode->offsetInDwords * sizeof(uint32_t);

            std::vector<Value*> idxs;
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), byteOffset));

            auto pElemPtr = GetElementPtrInst::CreateInBounds(pIntfData->spillTable.pTablePtr, idxs, "", &*pInsertPos);

            Type* pResNodePtrTy = nullptr;

            if  ((pNode->type == ResourceMappingNodeType::DescriptorResource) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorSampler) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorTexelBuffer) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorFmask) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorBuffer) ||
                 (pNode->type == ResourceMappingNodeType::DescriptorBufferCompact))
            {
                pResNodePtrTy = VectorType::get(m_pContext->Int32Ty(), pNode->sizeInDwords)->getPointerTo(ADDR_SPACE_CONST);
            }
            else
            {
                pResNodePtrTy = m_pContext->Int32Ty()->getPointerTo(ADDR_SPACE_CONST);
            }

            auto pResNodePtr = BitCastInst::CreatePointerCast(pElemPtr, pResNodePtrTy, "", &*pInsertPos);
            pResNodePtr->setMetadata(m_pContext->MetaIdUniform(), m_pContext->GetEmptyMetadataNode());

            pResNodeValue = new LoadInst(pResNodePtr, "", &*pInsertPos);
        }

        switch (pNode->type)
        {
        case ResourceMappingNodeType::DescriptorTableVaPtr:
            {
                auto pDescTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                                    dataLayout.getAllocaAddrSpace(),
                                                    "",
                                                    &*pInsertPos);
                auto pDescTablePtrLow = pResNodeValue;
                auto descSet = pNode->tablePtr.pNext->srdRange.set;
                pIntfData->descTablePtrs[descSet] = InitPointerWithValue(pDescTablePtr,
                                                                         pDescTablePtrLow,
                                                                         pDescTablePtrHigh,
                                                                         pDescTablePtrTy,
                                                                         &*pInsertPos);
                if (cl::EnableShadowDescriptorTable)
                {
                    Value* pShadowDescTablePtrHigh =
                        ConstantInt::get(m_pContext->Int32Ty(), cl::ShadowDescTablePtrHigh);
                    auto pShadowDescTablePtr = new AllocaInst(m_pContext->Int32x2Ty(),
                                                              dataLayout.getAllocaAddrSpace(),
                                                              "",
                                                              &*pInsertPos);

                    auto descSet = pNode->tablePtr.pNext->srdRange.set;
                    pIntfData->shadowDescTablePtrs[descSet] = InitPointerWithValue(pShadowDescTablePtr,
                                                                                   pDescTablePtrLow,
                                                                                   pShadowDescTablePtrHigh,
                                                                                   pDescTablePtrTy,
                                                                                   &*pInsertPos);
                }
                break;
            }
        case ResourceMappingNodeType::IndirectUserDataVaPtr:
            {
                auto pVbTablePtr =
                    new AllocaInst(m_pContext->Int32x2Ty(),
                                   dataLayout.getAllocaAddrSpace(),
                                   "",
                                   &*pInsertPos);
                auto pVbTablePtrLow = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.vs.vbTablePtr);
                auto pVbTablePtrTy = PointerType::get(ArrayType::get(m_pContext->Int32x4Ty(), 16), ADDR_SPACE_CONST);
                pIntfData->vbTable.pTablePtr = InitPointerWithValue(pVbTablePtr,
                                                                    pVbTablePtrLow,
                                                                    pDescTablePtrHigh,
                                                                    pVbTablePtrTy,
                                                                    &*pInsertPos);
                break;
            }
        case ResourceMappingNodeType::DescriptorResource:
        case ResourceMappingNodeType::DescriptorSampler:
        case ResourceMappingNodeType::DescriptorTexelBuffer:
        case ResourceMappingNodeType::DescriptorFmask:
        case ResourceMappingNodeType::DescriptorBuffer:
        case ResourceMappingNodeType::DescriptorBufferCompact:
            {
                pIntfData->dynDescs[dynDescIdx] = pResNodeValue;
                ++dynDescIdx;
                break;
            }
        case ResourceMappingNodeType::PushConst:
            {
                // NOTE: Node type "push constant" is processed by LLVM patch operation "PatchPushConstantOp".
                break;
            }
        case ResourceMappingNodeType::DescriptorCombinedTexture:
        default:
            {
                LLPC_NEVER_CALLED();
                break;
            }
        }
    }

    if (m_shaderStage == ShaderStageCompute)
    {
        auto pResUsage = m_pContext->GetShaderResourceUsage(ShaderStageCompute);
        if (pResUsage->builtInUsage.cs.numWorkgroups)
        {
            auto pNumWorkgroupPtr = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.cs.numWorkgroupsPtr);
            auto pNumWorkgroups = new LoadInst(pNumWorkgroupPtr, "", &*pInsertPos);
            pNumWorkgroups->setMetadata(m_pContext->MetaIdInvariantLoad(), m_pContext->GetEmptyMetadataNode());
            pIntfData->pNumWorkgroups = pNumWorkgroups;
        }
    }
    else if (m_shaderStage == ShaderStageTessControl)
    {
        auto& inoutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs;

        // Extract value of primitive ID
        inoutUsage.pPrimitiveId = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.tcs.patchId);

        Value* pRelPatchId = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.tcs.relPatchId);

        // Extract the value for the built-in gl_InvocationID
        std::vector<Attribute::AttrKind> attribs;
        attribs.push_back(Attribute::ReadNone);
        std::vector<Value*> args;
        args.push_back(pRelPatchId);
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 8));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 5));

        inoutUsage.pInvocationId =
            EmitCall(m_pModule, "llvm.amdgcn.ubfe.i32", m_pContext->Int32Ty(), args, attribs, &*pInsertPos);

        // Extract the value for relative patch ID
        inoutUsage.pRelativeId = BinaryOperator::CreateAnd(pRelPatchId,
                                                           ConstantInt::get(m_pContext->Int32Ty(), 0xFF),
                                                           "",
                                                           &*pInsertPos);

        // Get the descriptor for tessellation factor (TF) buffer
        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), InternalResourceTable));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_TF_BUFFER_OFFS));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));

        inoutUsage.pTessFactorBufDesc = EmitCall(m_pModule,
                                                 LlpcName::DescriptorLoadBuffer,
                                                 m_pContext->Int32x4Ty(),
                                                 args,
                                                 NoAttrib,
                                                 &*pInsertPos);

        // Get the descriptor for the off-chip LDS buffer
        args.clear();
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), InternalResourceTable));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_HS_BUFFER0_OFFS));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));

        inoutUsage.pOffChipLdsDesc = EmitCall(m_pModule,
                                              LlpcName::DescriptorLoadBuffer,
                                              m_pContext->Int32x4Ty(),
                                              args,
                                              NoAttrib,
                                              &*pInsertPos);

    }
    else if (m_shaderStage == ShaderStageTessEval)
    {
        auto& inOutUsage = m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->inOutUsage.tes;

        Value* pTessCoordX = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.tes.tessCoordX);
        Value* pTessCoordY = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.tes.tessCoordY);
        Value* pTessCoordZ = BinaryOperator::CreateFAdd(pTessCoordX, pTessCoordY, "", &*pInsertPos);

        pTessCoordZ = BinaryOperator::CreateFSub(ConstantFP::get(m_pContext->FloatTy(), 1.0f),
                                                 pTessCoordZ,
                                                 "",
                                                 &*pInsertPos);

        uint32_t primitiveMode =
            m_pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes.primitiveMode;
        pTessCoordZ = (primitiveMode == Triangles) ? pTessCoordZ : ConstantFP::get(m_pContext->FloatTy(), 0.0f);

        Value* pTessCoord = UndefValue::get(m_pContext->Floatx3Ty());
        pTessCoord = InsertElementInst::Create(pTessCoord,
                                               pTessCoordX,
                                               ConstantInt::get(m_pContext->Int32Ty(), 0),
                                               "",
                                               &*pInsertPos);
        pTessCoord = InsertElementInst::Create(pTessCoord,
                                               pTessCoordY,
                                               ConstantInt::get(m_pContext->Int32Ty(), 1),
                                               "",
                                               &*pInsertPos);
        pTessCoord = InsertElementInst::Create(pTessCoord,
                                               pTessCoordZ,
                                               ConstantInt::get(m_pContext->Int32Ty(), 2),
                                               "",
                                               &*pInsertPos);
        inOutUsage.pTessCoord = pTessCoord;

        // Get the descriptor for the off-chip LDS buffer
        std::vector<Value*> args;
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), InternalResourceTable));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_HS_BUFFER0_OFFS));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));

        inOutUsage.pOffChipLdsDesc = EmitCall(m_pModule,
                                                 LlpcName::DescriptorLoadBuffer,
                                                 m_pContext->Int32x4Ty(),
                                                 args,
                                                 NoAttrib,
                                                 &*pInsertPos);
    }
    else if (m_shaderStage == ShaderStageGeometry)
    {
        const auto pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

        // TODO: we should only insert those offsets required by the specified input primitive.

        // Setup ES-GS ring buffer vertex offsets
        Value* pEsGsOffsets = UndefValue::get(m_pContext->Int32x6Ty());
        for (uint32_t i = 0; i < InterfaceData::MaxEsGsOffsetCount; ++i)
        {
            auto pEsGsOffset = GetFunctionArgument(pEntryPoint, pIntfData->entryArgIdxs.gs.esGsOffsets[i]);
            pEsGsOffsets = InsertElementInst::Create(pEsGsOffsets,
                                                     pEsGsOffset,
                                                     ConstantInt::get(m_pContext->Int32Ty(), i),
                                                     "",
                                                     &*pInsertPos);
        }

        pResUsage->inOutUsage.gs.pEsGsOffsets = pEsGsOffsets;

        // Setup ES-GS ring buffer descriptor for GS input
        std::vector<Value*> args;
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), InternalResourceTable));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_GS_RING_IN_OFFS));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        auto pEsGsRingBufDesc = EmitCall(m_pModule,
                                         LlpcName::DescriptorLoadBuffer,
                                         m_pContext->Int32x4Ty(),
                                         args,
                                         NoAttrib,
                                         &*pInsertPos);

        pResUsage->inOutUsage.pEsGsRingBufDesc = pEsGsRingBufDesc;

        // Setup GS-VS ring buffer descriptor for GS ouput
        args[1] = ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_GS_RING_OUT0_OFFS);
        auto pGsVsRingBufDesc = EmitCall(m_pModule,
                                         LlpcName::DescriptorLoadBuffer,
                                         m_pContext->Int32x4Ty(),
                                         args,
                                         NoAttrib,
                                         &*pInsertPos);

        // Patch GS-VS ring buffer descriptor stride for GS output
        Value* pGsVsRingBufDescElem1 = ExtractElementInst::Create(pGsVsRingBufDesc,
                                                               ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                               "",
                                                               &*pInsertPos);

        // Clear stride in SRD DWORD1
        SqBufRsrcWord1 strideClearMask = {};
        strideClearMask.u32All         = UINT32_MAX;
        strideClearMask.bits.STRIDE    = 0;
        pGsVsRingBufDescElem1 = BinaryOperator::CreateAnd(pGsVsRingBufDescElem1,
                                                          ConstantInt::get(m_pContext->Int32Ty(), strideClearMask.u32All),
                                                          "",
                                                          &*pInsertPos);

        // Calculate and set stride in SRD dword1
        uint32_t gsVsStride = pResUsage->builtInUsage.gs.outputVertices *
                              pResUsage->inOutUsage.outputMapLocCount *
                              sizeof(uint32_t) * 4;

        SqBufRsrcWord1 strideSetValue = {};
        strideSetValue.bits.STRIDE = gsVsStride;
        pGsVsRingBufDescElem1 = BinaryOperator::CreateOr(pGsVsRingBufDescElem1,
                                                         ConstantInt::get(m_pContext->Int32Ty(), strideSetValue.u32All),
                                                         "",
                                                         &*pInsertPos);

        pGsVsRingBufDesc = InsertElementInst::Create(pGsVsRingBufDesc,
                                                     pGsVsRingBufDescElem1,
                                                     ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                     "",
                                                     &*pInsertPos);

        if (m_pContext->GetGfxIpVersion().major >= 8)
        {
            // NOTE: For GFX8+, we have to explicitly set DATA_FORMAT for GS-VS ring buffer descriptor.
            pGsVsRingBufDesc = SetRingBufferDataFormat(pGsVsRingBufDesc, BUF_DATA_FORMAT_32, &*pInsertPos);
        }

        pResUsage->inOutUsage.gs.pGsVsRingBufDesc = pGsVsRingBufDesc;

        // Setup GS emit vertex counter
        // TODO: Multiple output streams are not supported (only stream 0 is valid)
        auto pEmitCounterPtr = new AllocaInst(m_pContext->Int32Ty(),
                                              dataLayout.getAllocaAddrSpace(),
                                              "",
                                              &*pInsertPos);

        new StoreInst(ConstantInt::get(m_pContext->Int32Ty(), 0),
                      pEmitCounterPtr,
                      &*pInsertPos);

        pResUsage->inOutUsage.gs.pEmitCounterPtr = pEmitCounterPtr;
    }

	// Setup ES-GS ring buffer descriptor
    if (((m_shaderStage == ShaderStageVertex) && m_hasGs && (m_hasTs == false)) ||
        ((m_shaderStage == ShaderStageTessEval) && m_hasGs))
    {
        // Setup ES-GS ring buffer descriptor for VS or TES output
        const auto pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

        std::vector<Value*> args;
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), InternalResourceTable));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), SI_DRV_TABLE_ES_RING_OUT_OFFS));
        args.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        auto pEsGsRingBufDesc = EmitCall(m_pModule,
                                         LlpcName::DescriptorLoadBuffer,
                                         m_pContext->Int32x4Ty(),
                                         args,
                                         NoAttrib,
                                         &*pInsertPos);

        if (m_pContext->GetGfxIpVersion().major >= 8)
        {
            // NOTE: For GFX8+, we have to explicitly set DATA_FORMAT for GS-VS ring buffer descriptor.
            pEsGsRingBufDesc = SetRingBufferDataFormat(pEsGsRingBufDesc, BUF_DATA_FORMAT_32, &*pInsertPos);
        }

        pResUsage->inOutUsage.pEsGsRingBufDesc = pEsGsRingBufDesc;
    }

    // Remove original entry-point
    pOrigEntryPoint->dropAllReferences();
    pOrigEntryPoint->eraseFromParent();

    // Set calling convention for appropriate hardware shader stage. Also remove the dllexport that
    // we were using to identify the entrypoint.
    auto callingConv = CallingConv::AMDGPU_CS;
    switch (m_shaderStage)
    {
    case ShaderStageVertex:
        callingConv = m_hasTs ? CallingConv::AMDGPU_LS :
                                (m_hasGs ? CallingConv::AMDGPU_ES : CallingConv::AMDGPU_VS);
        break;
    case ShaderStageTessControl:
        callingConv = CallingConv::AMDGPU_HS;
        break;
    case ShaderStageTessEval:
        callingConv = m_hasGs ? CallingConv::AMDGPU_ES : CallingConv::AMDGPU_VS;
        break;
    case ShaderStageGeometry:
        callingConv = CallingConv::AMDGPU_GS;
        break;
    case ShaderStageFragment:
        callingConv = CallingConv::AMDGPU_PS;
        break;
    default:
        break;
    }
    pEntryPoint->setCallingConv(callingConv);
    pEntryPoint->setDLLStorageClass(GlobalValue::DefaultStorageClass);

    // Set the entry name required by PAL ABI
    auto entryStage = Util::Abi::PipelineSymbolType::CsMainEntry;
    switch (callingConv)
    {
    case CallingConv::AMDGPU_PS:
        entryStage = Util::Abi::PipelineSymbolType::PsMainEntry;
        break;
    case CallingConv::AMDGPU_VS:
        entryStage = Util::Abi::PipelineSymbolType::VsMainEntry;
        break;
    case CallingConv::AMDGPU_GS:
        entryStage = Util::Abi::PipelineSymbolType::GsMainEntry;
        break;
    case CallingConv::AMDGPU_ES:
        entryStage = Util::Abi::PipelineSymbolType::EsMainEntry;
        break;
    case CallingConv::AMDGPU_HS:
        entryStage = Util::Abi::PipelineSymbolType::HsMainEntry;
        break;
    case CallingConv::AMDGPU_LS:
        entryStage = Util::Abi::PipelineSymbolType::LsMainEntry;
        break;
    default:
        break;
    }

    const char* pEntryName = Util::Abi::PipelineAbiSymbolNameStrings[static_cast<uint32_t>(entryStage)];
    pEntryPoint->setName(pEntryName);
    pEntryPoint->setDLLStorageClass(GlobalValue::DLLExportStorageClass);

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Checks whether the specified resource mapping node is active.
bool PatchEntryPointMutate::IsResourceMappingNodeActive(
    const ResourceMappingNode* pNode        // [in] Resource mapping node
    ) const
{
    bool active = false;

    const ResourceUsage* pResUsage1 = m_pContext->GetShaderResourceUsage(m_shaderStage);
    const ResourceUsage* pResUsage2 = nullptr;

    const auto gfxIp = m_pContext->GetGfxIpVersion();
    if (gfxIp.major >= 9)
    {
        // NOTE: For LS-HS/ES-GS merged shader, resource mapping nodes of the two shader stages are merged as a whole.
        // So we have to check activeness of both shader stages at the same time. Here, we determine the second shader
       // stage and get its resource usage accordingly.
        uint32_t stageMask = m_pContext->GetShaderStageMask();
        const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                            ShaderStageToMask(ShaderStageTessEval))) != 0);
        const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

        if (hasTs || hasGs)
        {
            const auto shaderStage1 = m_shaderStage;
            auto shaderStage2 = ShaderStageInvalid;

            if (shaderStage1 == ShaderStageVertex)
            {
                shaderStage2 = hasTs ? ShaderStageTessControl :
                                       (hasGs ? ShaderStageGeometry : ShaderStageInvalid);
            }
            else if (shaderStage1 == ShaderStageTessControl)
            {
                shaderStage2 = ShaderStageVertex;
            }
            else if (shaderStage1 == ShaderStageTessEval)
            {
                shaderStage2 = hasGs ? ShaderStageGeometry : ShaderStageInvalid;
            }
            else if (shaderStage1 == ShaderStageGeometry)
            {
                shaderStage2 = hasTs ? ShaderStageTessEval : ShaderStageVertex;
            }

            if (shaderStage2 != ShaderStageInvalid)
            {
                pResUsage2 = m_pContext->GetShaderResourceUsage(shaderStage2);
            }
        }
    }

    if (pNode->type == ResourceMappingNodeType::PushConst)
    {
        active = (pResUsage1->pushConstSizeInBytes > 0);
        if ((active == false) && (pResUsage2 != nullptr))
        {
            active = (pResUsage2->pushConstSizeInBytes > 0);
        }
    }
    else if (pNode->type == ResourceMappingNodeType::DescriptorTableVaPtr)
    {
        // Check if any contained descriptor node is active
        for (uint32_t i = 0; i < pNode->tablePtr.nodeCount; ++i)
        {
            if (IsResourceMappingNodeActive(pNode->tablePtr.pNext + i))
            {
                active = true;
                break;
            }
        }
    }
    else if (pNode->type == ResourceMappingNodeType::IndirectUserDataVaPtr)
    {
        // NOTE: We assume indirect user data is always active.
        active = true;
    }
    else
    {
        LLPC_ASSERT((pNode->type != ResourceMappingNodeType::PushConst) &&
                    (pNode->type != ResourceMappingNodeType::DescriptorTableVaPtr) &&
                    (pNode->type != ResourceMappingNodeType::IndirectUserDataVaPtr));

        DescriptorPair descPair = {};
        descPair.descSet = pNode->srdRange.set;
        descPair.binding = pNode->srdRange.binding;

        active = (pResUsage1->descPairs.find(descPair.u64All) != pResUsage1->descPairs.end());
        if ((active == false) && (pResUsage2 != nullptr))
        {
            active = (pResUsage2->descPairs.find(descPair.u64All) != pResUsage2->descPairs.end());
        }
    }

    return active;
}

// =====================================================================================================================
// Explicitly set the DATA_FORMAT of ring buffer descriptor.
Value* PatchEntryPointMutate::SetRingBufferDataFormat(
    Value*          pBufDesc,       // [in] Buffer Descriptor
    uint32_t        dataFormat,     // Data format
    Instruction*    pInsertPos      // [in] Where to insert instructions
    ) const
{
    Value* pElem3 = ExtractElementInst::Create(pBufDesc,
                                               ConstantInt::get(m_pContext->Int32Ty(), 3),
                                               "",
                                               pInsertPos);

    SqBufRsrcWord3 dataFormatClearMask;
    dataFormatClearMask.u32All = UINT32_MAX;
    dataFormatClearMask.bits.DATA_FORMAT = 0;
    pElem3 = BinaryOperator::CreateAnd(pElem3,
                                       ConstantInt::get(m_pContext->Int32Ty(), dataFormatClearMask.u32All),
                                       "",
                                       pInsertPos);

    SqBufRsrcWord3 dataFormatSetValue = {};
    dataFormatSetValue.bits.DATA_FORMAT = dataFormat;
    pElem3 = BinaryOperator::CreateOr(pElem3,
                                      ConstantInt::get(m_pContext->Int32Ty(), dataFormatSetValue.u32All),
                                      "",
                                      pInsertPos);

    pBufDesc = InsertElementInst::Create(pBufDesc, pElem3, ConstantInt::get(m_pContext->Int32Ty(), 3), "", pInsertPos);

    return pBufDesc;
}

// =====================================================================================================================
// Generates the type for the new entry-point based on already-collected info in LLPC context.
FunctionType* PatchEntryPointMutate::GenerateEntryPointType(
    uint64_t* pInRegMask  // [out] "Inreg" bit mask for the arguments
    ) const
{
    uint32_t argIdx = 0;
    uint32_t userDataIdx = 0;
    std::vector<Type*> argTys;

    auto pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);
    auto pIntfData = m_pContext->GetShaderInterfaceData(m_shaderStage);
    auto pResUsage = m_pContext->GetShaderResourceUsage(m_shaderStage);

    // Global internal table
    argTys.push_back(m_pContext->Int32Ty());
    *pInRegMask |= (1ull << (argIdx++));
    ++userDataIdx;

    // TODO: We need add per shader table per real usage after switch to PAL new interface.
    //if (pResUsage->perShaderTable)
    {
        argTys.push_back(m_pContext->Int32Ty());
        *pInRegMask |= (1ull << (argIdx++));
        ++userDataIdx;
    }

    auto& builtInUsage = pResUsage->builtInUsage;
    auto& entryArgIdxs = pIntfData->entryArgIdxs;

    // Estimated available user data count
    uint32_t maxUserDataCount = m_pContext->GetGpuProperty()->maxUserDataCount;
    uint32_t availUserDataCount = maxUserDataCount - userDataIdx;
    uint32_t requiredUserDataCount = 0; // Maximum required user data
    bool useFixedLayout = (m_shaderStage == ShaderStageCompute);

    if (pShaderInfo->userDataNodeCount > 0)
    {
        for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
        {
            auto pNode = &pShaderInfo->pUserDataNodes[i];
             // NOTE: Per PAL request, the value of IndirectTableEntry is the node offset + 1.
             // and indirect user data should not be counted in possible spilled user data.
            if (pNode->type == ResourceMappingNodeType::IndirectUserDataVaPtr)
            {
                pIntfData->vbTable.resNodeIdx = pNode->offsetInDwords + 1;
                continue;
            }

            if (IsResourceMappingNodeActive(pNode) == false)
            {
                continue;
            }

            if (pNode->type == ResourceMappingNodeType::PushConst)
            {
                pIntfData->pushConst.resNodeIdx = i;
            }

            if (useFixedLayout)
            {
                requiredUserDataCount = std::max(requiredUserDataCount, pNode->offsetInDwords + pNode->sizeInDwords);
            }
            else
            {
                requiredUserDataCount += pNode->sizeInDwords;
            }
        }
    }
    const auto enableMultiView = (static_cast<const GraphicsPipelineBuildInfo*>(
                            m_pContext->GetPipelineBuildInfo()))->iaState.enableMultiView;

    switch (m_shaderStage)
    {
    case ShaderStageVertex:
        {
            if (enableMultiView)
            {
                availUserDataCount -= 1;
            }

            // Reserve register for "IndirectUserDataVaPtr"
            if (pIntfData->vbTable.resNodeIdx != InvalidValue)
            {
                availUserDataCount -= 1;
            }

            if (builtInUsage.vs.baseVertex || builtInUsage.vs.baseInstance)
            {
                availUserDataCount -= 2;
            }

            if (builtInUsage.vs.drawIndex)
            {
                availUserDataCount -= 1;
            }

            break;
        }
    case ShaderStageTessEval:
        {
            if (enableMultiView)
            {
                availUserDataCount -= 1;
            }

            break;
        }
    case ShaderStageGeometry:
        {
            if (enableMultiView)
            {
                availUserDataCount -= 1;
            }

            if (m_pContext->IsGsOnChip() && cl::InRegEsGsLdsSize)
            {
                // NOTE: Add a dummy "inreg" argument for ES-GS LDS size, this is to keep consistent
                // with PAL's GS on-chip behavior.
                availUserDataCount -= 1;
            }

            break;
        }
    case ShaderStageTessControl:
    case ShaderStageFragment:
        {
            // Do nothing
            break;
        }
    case ShaderStageCompute:
        {
            // Emulate gl_NumWorkGroups via user data registers
            if (builtInUsage.cs.numWorkgroups)
            {
                availUserDataCount -= 2;
            }
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    // NOTE: We have to spill user data to memory when available user data is less than required.
    bool needSpill = false;
    if (useFixedLayout)
    {
        LLPC_ASSERT(m_shaderStage == ShaderStageCompute);
        needSpill = (requiredUserDataCount > InterfaceData::MaxCsUserDataCount);
        availUserDataCount = InterfaceData::MaxCsUserDataCount;
    }
    else
    {
        needSpill = (requiredUserDataCount > availUserDataCount);
        pIntfData->spillTable.offsetInDwords = InvalidValue;
        if (needSpill)
        {
            // Spill table need an addtional user data
            --availUserDataCount;
        }
    }

    // Descriptor table and vertex buffer table
    uint32_t actualAvailUserDataCount = 0;
    for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
    {
        auto pNode = &pShaderInfo->pUserDataNodes[i];

        // "IndirectUserDataVaPtr" can't be spilled, it is treated as internal user data
        if (pNode->type == ResourceMappingNodeType::IndirectUserDataVaPtr)
        {
            continue;
        }

        if (IsResourceMappingNodeActive(pNode) == false)
        {
            continue;
        }

        if (useFixedLayout)
        {
            // NOTE: For fixed user data layout (for compute shader), we could not pack those user data and dummy
            // entry-point arguments are added once DWORD offsets of user data are not continuous.
           LLPC_ASSERT(m_shaderStage == ShaderStageCompute);

            while ((userDataIdx < (pNode->offsetInDwords + InterfaceData::CsStartUserData)) &&
                   (userDataIdx < (availUserDataCount + InterfaceData::CsStartUserData)))
            {
                argTys.push_back(m_pContext->Int32Ty());
                *pInRegMask |= (1ull << argIdx++);
                ++userDataIdx;
                ++actualAvailUserDataCount;
            }
        }

        if (actualAvailUserDataCount + pNode->sizeInDwords <= availUserDataCount)
        {
            // User data isn't spilled
            pIntfData->entryArgIdxs.resNodeValues[i] = argIdx;
            *pInRegMask |= (1ull << (argIdx++));
            actualAvailUserDataCount += pNode->sizeInDwords;
            switch (pNode->type)
            {
            case ResourceMappingNodeType::DescriptorTableVaPtr:
                {
                    argTys.push_back(m_pContext->Int32Ty());

                    LLPC_ASSERT(pNode->sizeInDwords == 1);

                    pIntfData->userDataMap[userDataIdx] = pNode->offsetInDwords;
                    ++userDataIdx;
                    break;
                }

            case ResourceMappingNodeType::DescriptorResource:
            case ResourceMappingNodeType::DescriptorSampler:
            case ResourceMappingNodeType::DescriptorTexelBuffer:
            case ResourceMappingNodeType::DescriptorFmask:
            case ResourceMappingNodeType::DescriptorBuffer:
            case ResourceMappingNodeType::PushConst:
            case ResourceMappingNodeType::DescriptorBufferCompact:
                {
                    argTys.push_back(VectorType::get(m_pContext->Int32Ty(), pNode->sizeInDwords));
                    for (uint32_t j = 0; j < pNode->sizeInDwords; ++j)
                    {
                        pIntfData->userDataMap[userDataIdx + j] = pNode->offsetInDwords + j;
                    }
                    userDataIdx += pNode->sizeInDwords;
                    break;
                }
            default:
                {
                    LLPC_NEVER_CALLED();
                    break;
                }
            }
        }
        else if (needSpill && (pIntfData->spillTable.offsetInDwords == InvalidValue))
        {
            pIntfData->spillTable.offsetInDwords = pNode->offsetInDwords;
        }
    }

    // Internal user data
    if (needSpill && useFixedLayout)
    {
        // Add spill table
        LLPC_ASSERT(pIntfData->spillTable.offsetInDwords != InvalidValue);
        LLPC_ASSERT(userDataIdx <= (InterfaceData::MaxCsUserDataCount + InterfaceData::CsStartUserData));
        while (userDataIdx <= (InterfaceData::MaxCsUserDataCount + InterfaceData::CsStartUserData))
        {
            argTys.push_back(m_pContext->Int32Ty());
            *pInRegMask |= (1ull << argIdx++);
            ++userDataIdx;
        }
        pIntfData->userDataUsage.spillTable = userDataIdx - 1;
        pIntfData->entryArgIdxs.spillTable = argIdx - 1;

        pIntfData->spillTable.sizeInDwords = requiredUserDataCount - pIntfData->spillTable.offsetInDwords;
    }

    switch (m_shaderStage)
    {
    case ShaderStageVertex:
        {
            // NOTE: The user data to emulate gl_ViewIndex is somewhat common. To make it consistent for GFX9
            // merged shader, we place it prior to any other special user data.
            if (enableMultiView)
            {
                argTys.push_back(m_pContext->Int32Ty()); // View Index
                entryArgIdxs.vs.viewIndex = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.vs.viewIndex = userDataIdx;
                ++userDataIdx;
            }

            for (uint32_t i = 0; i < pShaderInfo->userDataNodeCount; ++i)
            {
                auto pNode = &pShaderInfo->pUserDataNodes[i];
                if (pNode->type == ResourceMappingNodeType::IndirectUserDataVaPtr)
                {
                    argTys.push_back(m_pContext->Int32Ty());
                    LLPC_ASSERT(pNode->sizeInDwords == 1);
                    pIntfData->userDataUsage.vs.vbTablePtr = userDataIdx;
                    pIntfData->entryArgIdxs.vs.vbTablePtr = argIdx;
                    pIntfData->userDataMap[userDataIdx] = pNode->offsetInDwords;
                    *pInRegMask |= (1ull << (argIdx++));
                    ++userDataIdx;
                    break;
                }
            }

            if (builtInUsage.vs.baseVertex || builtInUsage.vs.baseInstance)
            {
                argTys.push_back(m_pContext->Int32Ty()); // Base vertex
                entryArgIdxs.vs.baseVertex = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.vs.baseVertex = userDataIdx;
                ++userDataIdx;

                argTys.push_back(m_pContext->Int32Ty()); // Base instance
                entryArgIdxs.vs.baseInstance = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.vs.baseInstance = userDataIdx;
                ++userDataIdx;
            }

            if (builtInUsage.vs.drawIndex)
            {
                argTys.push_back(m_pContext->Int32Ty()); // Draw index
                entryArgIdxs.vs.drawIndex = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.vs.drawIndex = userDataIdx;
                ++userDataIdx;
            }

            break;
        }
    case ShaderStageTessEval:
        {
            // NOTE: The user data to emulate gl_ViewIndex is somewhat common. To make it consistent for GFX9
            // merged shader, we place it prior to any other special user data.
            if (enableMultiView)
            {
                argTys.push_back(m_pContext->Int32Ty()); // View Index
                entryArgIdxs.tes.viewIndex = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.tes.viewIndex = userDataIdx;
                ++userDataIdx;
            }
            break;
        }
    case ShaderStageGeometry:
        {
            // NOTE: The user data to emulate gl_ViewIndex is somewhat common. To make it consistent for GFX9
            // merged shader, we place it prior to any other special user data.
            if (enableMultiView)
            {
                argTys.push_back(m_pContext->Int32Ty()); // View Index
                entryArgIdxs.gs.viewIndex = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
                pIntfData->userDataUsage.gs.viewIndex = userDataIdx;
                ++userDataIdx;
            }

            const auto gfxIp = m_pContext->GetGfxIpVersion();
            if (gfxIp.major <= 8)
            {
                // NOTE: Add a dummy "inreg" argument for ES-GS LDS size, this is to keep consistent
                // with PAL's GS on-chip behavior.
                if (m_pContext->IsGsOnChip() && cl::InRegEsGsLdsSize)
                {
                    argTys.push_back(m_pContext->Int32Ty());
                    *pInRegMask |= (1ull << (argIdx++));
                    pIntfData->userDataUsage.gs.esGsLdsSize = userDataIdx;
                    ++userDataIdx;
                }
            }

            break;
        }
    case ShaderStageCompute:
        {
            // Emulate gl_NumWorkGroups via user data registers
            if (builtInUsage.cs.numWorkgroups)
            {
                // NOTE: Pointer must be placed in even index according to LLVM backend compiler.
                if ((userDataIdx % 2) != 0)
                {
                    argTys.push_back(m_pContext->Int32Ty());
                    entryArgIdxs.cs.workgroupId = argIdx;
                    *pInRegMask |= (1ull << (argIdx++));
                    userDataIdx += 1;
                }

                auto pNumWorkgroupsPtrTy = PointerType::get(m_pContext->Int32x3Ty(), ADDR_SPACE_CONST);
                argTys.push_back(pNumWorkgroupsPtrTy); // NumWorkgroupsPtr
                entryArgIdxs.cs.numWorkgroupsPtr = argIdx;
                pIntfData->userDataUsage.cs.numWorkgroupsPtr = userDataIdx;
                *pInRegMask |= (1ull << (argIdx++));
                userDataIdx += 2;
            }
            break;
        }
    case ShaderStageTessControl:
    case ShaderStageFragment:
        {
            // Do nothing
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    if (needSpill && (useFixedLayout == false))
    {
        argTys.push_back(m_pContext->Int32Ty());
        *pInRegMask |= (1ull << argIdx);

        pIntfData->userDataUsage.spillTable = userDataIdx++;
        pIntfData->entryArgIdxs.spillTable = argIdx++;

        pIntfData->spillTable.sizeInDwords = requiredUserDataCount - pIntfData->spillTable.offsetInDwords;
    }
    pIntfData->userDataCount = userDataIdx;

    // NOTE: Here, we start to add system values, they should be behind user data.
    switch (m_shaderStage)
    {
    case ShaderStageVertex:
        {
            if (m_hasGs && (m_hasTs == false))
            {
                argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset
                entryArgIdxs.vs.esGsOffset = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
            }

            // NOTE: Order of these arguments could not be changed. The rule is very similar to function default
            // parameters: vertex ID [, relative vertex ID, primitive ID [, instance ID]]
            auto nextShaderStage = m_pContext->GetNextShaderStage(ShaderStageVertex);
            // NOTE: For tessellation control shader, we always need relative vertex ID.
            if (builtInUsage.vs.vertexIndex || builtInUsage.vs.primitiveId || builtInUsage.vs.instanceIndex ||
                (nextShaderStage == ShaderStageTessControl))
            {
                argTys.push_back(m_pContext->Int32Ty()); // Vertex ID
                entryArgIdxs.vs.vertexId = argIdx++;
            }

            if (builtInUsage.vs.primitiveId || builtInUsage.vs.instanceIndex ||
                (nextShaderStage == ShaderStageTessControl))
            {
                // NOTE: For tessellation control shader, we always need relative vertex ID.
                argTys.push_back(m_pContext->Int32Ty()); // Relative vertex ID (auto index)
                entryArgIdxs.vs.relVertexId = argIdx++;

                argTys.push_back(m_pContext->Int32Ty()); // Primitive ID
                entryArgIdxs.vs.primitiveId = argIdx++;
            }

            if (builtInUsage.vs.instanceIndex)
            {
                argTys.push_back(m_pContext->Int32Ty()); // Instance ID
                entryArgIdxs.vs.instanceId = argIdx++;
            }

            break;
        }
    case ShaderStageTessControl:
        {
            if (m_pContext->IsTessOffChip())
            {
                argTys.push_back(m_pContext->Int32Ty()); // Off-chip LDS buffer base
                entryArgIdxs.tcs.offChipLdsBase = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
            }

            argTys.push_back(m_pContext->Int32Ty()); // TF buffer base
            entryArgIdxs.tcs.tfBufferBase = argIdx;
            *pInRegMask |= (1ull << (argIdx++));

            argTys.push_back(m_pContext->Int32Ty()); // Patch ID
            entryArgIdxs.tcs.patchId = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Relative patch ID (control point ID included)
            entryArgIdxs.tcs.relPatchId = argIdx++;

            break;
        }
    case ShaderStageTessEval:
        {
            if (m_pContext->IsTessOffChip()) // Off-chip LDS buffer base
            {
                // NOTE: Off-chip LDS buffer base occupies two SGPRs. When TES acts as hardware VS, use second SGPR.
                // When TES acts as hardware ES, use first SGPR.
                entryArgIdxs.tes.offChipLdsBase = m_hasGs ? argIdx : argIdx + 1;

                argTys.push_back(m_pContext->Int32Ty());
                *pInRegMask |= (1ull << (argIdx++));

                argTys.push_back(m_pContext->Int32Ty());
                *pInRegMask |= (1ull << (argIdx++));
            }

            if (m_hasGs)
            {
                argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset
                entryArgIdxs.tes.esGsOffset = argIdx;
                *pInRegMask |= (1ull << (argIdx++));
            }

            argTys.push_back(m_pContext->FloatTy()); // X of TessCoord (U)
            entryArgIdxs.tes.tessCoordX = argIdx++;

            argTys.push_back(m_pContext->FloatTy()); // Y of TessCoord (V)
            entryArgIdxs.tes.tessCoordY = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Relative patch ID
            entryArgIdxs.tes.relPatchId = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Patch ID
            entryArgIdxs.tes.patchId = argIdx++;

            break;
        }
    case ShaderStageGeometry:
        {
            argTys.push_back(m_pContext->Int32Ty()); // GS to VS offset
            entryArgIdxs.gs.gsVsOffset = argIdx;
            *pInRegMask |= (1ull << (argIdx++));

            argTys.push_back(m_pContext->Int32Ty()); // GS wave ID
            entryArgIdxs.gs.waveId = argIdx;
            *pInRegMask |= (1ull << (argIdx++));

            // TODO: We should make the arguments according to real usage.
            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 0)
            entryArgIdxs.gs.esGsOffsets[0] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 1)
            entryArgIdxs.gs.esGsOffsets[1] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Primitive ID
            entryArgIdxs.gs.primitiveId = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 2)
            entryArgIdxs.gs.esGsOffsets[2] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 3)
            entryArgIdxs.gs.esGsOffsets[3] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 4)
            entryArgIdxs.gs.esGsOffsets[4] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // ES to GS offset (vertex 5)
            entryArgIdxs.gs.esGsOffsets[5] = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Invocation ID
            entryArgIdxs.gs.invocationId = argIdx++;
            break;
        }
    case ShaderStageFragment:
        {
            argTys.push_back(m_pContext->Int32Ty()); // Primitive mask
            entryArgIdxs.fs.primMask = argIdx;
            *pInRegMask |= (1ull << (argIdx++));

            argTys.push_back(m_pContext->Floatx2Ty()); // Perspective sample
            entryArgIdxs.fs.perspInterp.sample = argIdx++;

            argTys.push_back(m_pContext->Floatx2Ty()); // Perspective center
            entryArgIdxs.fs.perspInterp.center = argIdx++;

            argTys.push_back(m_pContext->Floatx2Ty()); // Perspective centroid
            entryArgIdxs.fs.perspInterp.centroid = argIdx++;

            argTys.push_back(m_pContext->Floatx3Ty()); // Perspective pull-mode
            entryArgIdxs.fs.perspInterp.pullMode = argIdx++;

            argTys.push_back(m_pContext->Floatx2Ty()); // Linear sample
            entryArgIdxs.fs.linearInterp.sample = argIdx++;

            argTys.push_back(m_pContext->Floatx2Ty()); // Linear center
            entryArgIdxs.fs.linearInterp.center = argIdx++;

            argTys.push_back(m_pContext->Floatx2Ty()); // Linear centroid
            entryArgIdxs.fs.linearInterp.centroid = argIdx++;

            argTys.push_back(m_pContext->FloatTy()); // Line stipple
            ++argIdx;

            argTys.push_back(m_pContext->FloatTy()); // X of FragCoord
            entryArgIdxs.fs.fragCoord.x = argIdx++;

            argTys.push_back(m_pContext->FloatTy()); // Y of FragCoord
            entryArgIdxs.fs.fragCoord.y = argIdx++;

            argTys.push_back(m_pContext->FloatTy()); // Z of FragCoord
            entryArgIdxs.fs.fragCoord.z = argIdx++;

            argTys.push_back(m_pContext->FloatTy()); // W of FragCoord
            entryArgIdxs.fs.fragCoord.w = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Front facing
            entryArgIdxs.fs.frontFacing = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Ancillary
            entryArgIdxs.fs.ancillary = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Sample coverage
            entryArgIdxs.fs.sampleCoverage = argIdx++;

            argTys.push_back(m_pContext->Int32Ty()); // Fixed X/Y
            ++argIdx;

            break;
        }
    case ShaderStageCompute:
        {
            // Add system values in SGPR
            argTys.push_back(m_pContext->Int32x3Ty()); // WorkgroupId
            entryArgIdxs.cs.workgroupId = argIdx;
            *pInRegMask |= (1ull << (argIdx++));

            argTys.push_back(m_pContext->Int32Ty());  // Multiple dispatch info, include TG_SIZE and etc.
            *pInRegMask |= (1ull << (argIdx++));

            // Add system value in VGPR
            argTys.push_back(m_pContext->Int32x3Ty()); // LocalInvociationId
            entryArgIdxs.cs.localInvocationId = argIdx++;
            break;
        }
    default:
        {
            LLPC_NEVER_CALLED();
            break;
        }
    }

    return FunctionType::get(m_pContext->VoidTy(), argTys, false);
}

// =====================================================================================================================
// Initializes the specified pointer (64-bit) with specified initial values and casts the resulting pointer to expected
// type.
Value* PatchEntryPointMutate::InitPointerWithValue(
    Value*       pPtr,          // [in] 64-bit Pointer to be initialized
    Value*       pLowValue,     // [in] Low part of initial value
    Value*       pHighValue,    // [in] High part of initial value
    Type*        pCastedPtrTy,  // [in] Casted type of the returned pointer
    Instruction* pInsertPos     // [in] Where to insert instructions
    ) const
{
    // Initialize low part of the pointer: i32 x 2[0]
    std::vector<Value*> idxs;
    idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
    idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));

    auto pPtrLow = GetElementPtrInst::CreateInBounds(pPtr, idxs, "", pInsertPos);
    new StoreInst(pLowValue, pPtrLow, pInsertPos);

    // Initialize high part of the pointer: i32 x 2[1]
    idxs.clear();
    idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
    idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 1));

    auto pPtrHigh = GetElementPtrInst::CreateInBounds(pPtr, idxs, "", pInsertPos);
    new StoreInst(pHighValue, pPtrHigh, pInsertPos);

    // Cast i32 x 2 to i64 ptr
    auto pIntValue = new LoadInst(pPtr, "", pInsertPos);
    auto pInt64Value = new BitCastInst(pIntValue, m_pContext->Int64Ty(), "", pInsertPos);

    return CastInst::Create(Instruction::IntToPtr, pInt64Value, pCastedPtrTy, "", pInsertPos);
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patching opertions for entry-point mutation.
INITIALIZE_PASS(PatchEntryPointMutate, "Patch-entry-point-mutate",
                "Patch LLVM for entry-point mutation", false, false)
