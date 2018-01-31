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
 * @file  llpcPatchDescriptorLoad.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchDescriptorLoad.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-descriptor-load"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llpcContext.h"
#include "llpcPatchDescriptorLoad.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PatchDescriptorLoad::ID = 0;

// =====================================================================================================================
PatchDescriptorLoad::PatchDescriptorLoad()
    :
    Patch(ID)
{
    initializePatchDescriptorLoadPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM patching pass on the specified LLVM module.
bool PatchDescriptorLoad::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Patch-Descriptor-Load\n");

    Patch::Init(&module);

    // Invoke handling of "call" instruction
    visit(*m_pModule);

    // Remove unnecessary descriptor load calls
    for (auto pCallInst : m_descLoadCalls)
    {
        pCallInst->dropAllReferences();
        pCallInst->eraseFromParent();
    }

    // Remove unnecessary descriptor load functions
    for (auto pFunc : m_descLoadFuncs)
    {
        if (pFunc->user_empty())
        {
            pFunc->dropAllReferences();
            pFunc->eraseFromParent();
        }
    }

    DEBUG(dbgs() << "After the pass Patch-Descriptor-Load:" << module);

    std::string errMsg;
    raw_string_ostream errStream(errMsg);
    if (verifyModule(module, &errStream))
    {
        LLPC_ERRS("Fails to verify module (" DEBUG_TYPE "): " << errStream.str() << "\n");
    }

    return true;
}

// =====================================================================================================================
// Visits "call" instruction.
void PatchDescriptorLoad::visitCallInst(
    CallInst& callInst)   // [in] "Call" instruction
{
    auto pCallee = callInst.getCalledFunction();
    if (pCallee == nullptr)
    {
        return;
    }

    auto mangledName = pCallee->getName();

    std::string descLoadPrefix = LlpcName::DescriptorLoadPrefix;
    bool isDescLoad = (strncmp(mangledName.data(),descLoadPrefix.c_str(), descLoadPrefix.size()) == 0);

    if (isDescLoad == false)
    {
        return; // Not descriptor load call
    }

    // Descriptor loading should be inlined and stay in shader entry-point
    LLPC_ASSERT(callInst.getParent()->getParent() == m_pEntryPoint);

    Type* pDescPtrTy = nullptr;
    ResourceMappingNodeType nodeType = ResourceMappingNodeType::Unknown;

    bool loadSpillTable = false;

    // TODO: The address space ID 2 is a magic number. We have to replace it with defined LLPC address space ID.
    if (mangledName == LlpcName::DescriptorLoadResource)
    {
        pDescPtrTy = m_pContext->Int32x8Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::DescriptorResource;
    }
    else if (mangledName == LlpcName::DescriptorLoadSampler)
    {
        pDescPtrTy = m_pContext->Int32x4Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::DescriptorSampler;
    }
    else if (mangledName == LlpcName::DescriptorLoadFmask)
    {
        pDescPtrTy = m_pContext->Int32x8Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::DescriptorFmask;
    }
    else if (mangledName == LlpcName::DescriptorLoadBuffer)
    {
        pDescPtrTy = m_pContext->Int32x4Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::DescriptorBuffer;
    }
    else if (mangledName == LlpcName::DescriptorLoadInlineBuffer)
    {
        pDescPtrTy = m_pContext->Int32x4Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::PushConst;
    }
    else if (mangledName == LlpcName::DescriptorLoadAddress)
    {
        nodeType = ResourceMappingNodeType::PushConst;
    }
    else if (mangledName == LlpcName::DescriptorLoadTexelBuffer)
    {
        pDescPtrTy = m_pContext->Int32x4Ty()->getPointerTo(ADDR_SPACE_CONST);
        nodeType = ResourceMappingNodeType::DescriptorTexelBuffer;
    }
    else if (mangledName == LlpcName::DescriptorLoadSpillTable)
    {
        loadSpillTable = true;
    }
    else
    {
        LLPC_NEVER_CALLED();
    }

    if (loadSpillTable)
    {
        auto pIntfData   = m_pContext->GetShaderInterfaceData(m_shaderStage);
        callInst.replaceAllUsesWith(pIntfData->pushConst.pTablePtr);
        m_descLoadCalls.push_back(&callInst);
        m_descLoadFuncs.insert(pCallee);
    }
    else
    {
        LLPC_ASSERT(nodeType != ResourceMappingNodeType::Unknown);

        // Calculate descriptor offset (in bytes)
        auto pDescSet = cast<ConstantInt>(callInst.getOperand(0));
        auto pBinding = cast<ConstantInt>(callInst.getOperand(1));
        auto pArrayOffset = callInst.getOperand(2); // Offset for arrayed resource (index)

        uint32_t descOffset = 0;
        uint32_t descSize   = 0;
        uint32_t dynDescIdx = InvalidValue;
        Value*   pDesc = nullptr;
        auto  pDescRangeValue = GetDescriptorRangeValue(nodeType, pDescSet->getZExtValue(), pBinding->getZExtValue());

        if (pDescRangeValue != nullptr)
        {
            // Descriptor range value (immutable sampler in Vulkan)
            LLPC_ASSERT(nodeType == ResourceMappingNodeType::DescriptorSampler);

            uint32_t descSizeInDword = pDescPtrTy->getPointerElementType()->getVectorNumElements();

            if ((pDescRangeValue->arraySize == 1) || isa<ConstantInt>(pArrayOffset))
            {
                // Array size is 1 or array offset is constant
                uint32_t arrayOffset = 0;
                if (isa<ConstantInt>(pArrayOffset))
                {
                    arrayOffset = cast<ConstantInt>(pArrayOffset)->getZExtValue();
                }

                const uint32_t* pDescValue = pDescRangeValue->pValue + arrayOffset * descSizeInDword;

                std::vector<Constant*> descElems;
                for (uint32_t i = 0; i < descSizeInDword; ++i)
                {
                    descElems.push_back(ConstantInt::get(m_pContext->Int32Ty(), pDescValue[i]));
                }
                pDesc = ConstantVector::get(descElems);
            }
            else
            {
                // Array size is greater than 1 and array offset is non-constant
                GlobalVariable* pDescs = nullptr;

                if (m_descs.find(pDescRangeValue) == m_descs.end())
                {
                    std::vector<Constant*> descs;
                    for (uint32_t i = 0; i < pDescRangeValue->arraySize; ++i)
                    {
                        const uint32_t* pDescValue = pDescRangeValue->pValue + i * descSizeInDword;

                        std::vector<Constant*> descElems;
                        for (uint32_t j = 0; j < descSizeInDword; ++j)
                        {
                            descElems.push_back(ConstantInt::get(m_pContext->Int32Ty(), pDescValue[j]));
                        }

                        descs.push_back(ConstantVector::get(descElems));
                    }

                    auto pDescsTy = ArrayType::get(VectorType::get(m_pContext->Int32Ty(), descSizeInDword),
                                                   pDescRangeValue->arraySize);

                    pDescs = new GlobalVariable(*m_pModule,
                                                pDescsTy,
                                                true, // isConstant
                                                GlobalValue::InternalLinkage,
                                                ConstantArray::get(pDescsTy, descs),
                                                "",
                                                nullptr,
                                                GlobalValue::NotThreadLocal,
                                                ADDR_SPACE_CONST);

                    m_descs[pDescRangeValue] = pDescs;
                }
                else
                {
                    pDescs = m_descs[pDescRangeValue];
                }

                std::vector<Value*> idxs;
                idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
                idxs.push_back(pArrayOffset);

                auto pDescPtr = GetElementPtrInst::Create(nullptr, pDescs, idxs, "", &callInst);
                pDesc = new LoadInst(pDescPtr, "", &callInst);
            }
        }

        if (pDesc == nullptr)
        {
            CalcDescriptorOffsetAndSize(nodeType,
                                        pDescSet->getZExtValue(),
                                        pBinding->getZExtValue(),
                                        &descOffset,
                                        &descSize,
                                        &dynDescIdx);

            uint32_t descSizeInDword = descSize / sizeof(uint32_t);
            if (dynDescIdx != InvalidValue)
            {
                // Dynamic descriptors
                auto pIntfData = m_pContext->GetShaderInterfaceData(m_shaderStage);
                if ((dynDescIdx < InterfaceData::MaxDynDescCount) && (pIntfData->dynDescs[dynDescIdx] != nullptr))
                {
                    auto pDescTy = VectorType::get(m_pContext->Int32Ty(), descSizeInDword);
                    pDesc = pIntfData->dynDescs[dynDescIdx];
                    if (pDesc->getType() != pDescTy)
                    {
                        // Array dynamic descriptor
                        Value* pDynDesc = UndefValue::get(pDescTy);
                        auto pDescStride = ConstantInt::get(m_pContext->Int32Ty(), descSizeInDword);
                        auto pIndex = BinaryOperator::CreateMul(pArrayOffset, pDescStride, "", &callInst);
                        for (uint32_t i = 0; i < descSizeInDword; ++i)
                        {
                            auto pDescElem = ExtractElementInst::Create(pDesc, pIndex, "", &callInst);
                            pDynDesc = InsertElementInst::Create(pDynDesc,
                                                                 pDescElem,
                                                                 ConstantInt::get(m_pContext->Int32Ty(), i),
                                                                 "",
                                                                 &callInst);
                            pIndex = BinaryOperator::CreateAdd(pIndex,
                                                               ConstantInt::get(m_pContext->Int32Ty(), 1),
                                                               "",
                                                               &callInst);
                        }
                        pDesc = pDynDesc;
                    }

                    // Extract compact buffer descriptor
                    if (descSizeInDword == DescriptorSizeBufferCompact / sizeof(uint32_t))
                    {
                        SqBufRsrcWord1 sqBufRsrcWord1 = {};
                        SqBufRsrcWord2 sqBufRsrcWord2 = {};
                        SqBufRsrcWord3 sqBufRsrcWord3 = {};

                        sqBufRsrcWord1.bits.BASE_ADDRESS_HI = UINT16_MAX;
                        sqBufRsrcWord2.bits.NUM_RECORDS = (m_pContext->GetGfxIpVersion().major == 6) ?
                                                          UINT16_MAX :
                                                          UINT32_MAX;

                        sqBufRsrcWord3.bits.DST_SEL_X = BUF_DST_SEL_X;
                        sqBufRsrcWord3.bits.DST_SEL_Y = BUF_DST_SEL_Y;
                        sqBufRsrcWord3.bits.DST_SEL_Z = BUF_DST_SEL_Z;
                        sqBufRsrcWord3.bits.DST_SEL_W = BUF_DST_SEL_W;
                        sqBufRsrcWord3.bits.NUM_FORMAT = BUF_NUM_FORMAT_UINT;
                        sqBufRsrcWord3.bits.DATA_FORMAT = BUF_DATA_FORMAT_32;
                        LLPC_ASSERT(sqBufRsrcWord3.u32All == 0x24FAC);

                        // Extract compact buffer descriptor
                        Value* pDescElem0 = ExtractElementInst::Create(pDesc,
                            ConstantInt::get(m_pContext->Int32Ty(), 0),
                            "",
                            &callInst);

                        Value* pDescElem1 = ExtractElementInst::Create(pDesc,
                            ConstantInt::get(m_pContext->Int32Ty(), 1),
                            "",
                            &callInst);

                        // Build normal buffer descriptor
                        auto pBufDescTy = m_pContext->Int32x4Ty();
                        Value* pBufDesc = UndefValue::get(pBufDescTy);

                        // DWORD0
                        pBufDesc = InsertElementInst::Create(pBufDesc,
                            pDescElem0,
                            ConstantInt::get(m_pContext->Int32Ty(), 0),
                            "",
                            &callInst);

                        // DWORD1
                        pDescElem1 = BinaryOperator::CreateAnd(pDescElem1,
                            ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord1.u32All),
                            "",
                            &callInst);
                        pBufDesc = InsertElementInst::Create(pBufDesc,
                            pDescElem1,
                            ConstantInt::get(m_pContext->Int32Ty(), 1),
                            "",
                            &callInst);

                        // DWORD2
                        pBufDesc = InsertElementInst::Create(pBufDesc,
                            ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord2.u32All),
                            ConstantInt::get(m_pContext->Int32Ty(), 2),
                            "",
                            &callInst);

                        // DWORD3
                        pBufDesc = InsertElementInst::Create(pBufDesc,
                            ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord3.u32All),
                            ConstantInt::get(m_pContext->Int32Ty(), 3),
                            "",
                            &callInst);

                        pDesc = pBufDesc;
                    }

                }
                else
                {
                    LLPC_NEVER_CALLED();
                }
            }
            else if (nodeType == ResourceMappingNodeType::PushConst)
            {
                auto pDescTablePtr =
                    m_pContext->GetShaderInterfaceData(m_shaderStage)->descTablePtrs[pDescSet->getZExtValue()];

                Value* pDescTableAddr = new PtrToIntInst(pDescTablePtr,
                                                         m_pContext->Int64Ty(),
                                                         "",
                                                         &callInst);

                pDescTableAddr = new BitCastInst(pDescTableAddr, m_pContext->Int32x2Ty(), "", &callInst);

                // Extract descriptor table address
                Value* pDescElem0 = ExtractElementInst::Create(pDescTableAddr,
                    ConstantInt::get(m_pContext->Int32Ty(), 0),
                    "",
                    &callInst);

                auto pDescOffset = ConstantInt::get(m_pContext->Int32Ty(), descOffset);

                pDescElem0 = BinaryOperator::CreateAdd(pDescElem0, pDescOffset, "", &callInst);

                if (pDescPtrTy == nullptr)
                {
                    // Load the address of inline constant buffer
                    pDesc = InsertElementInst::Create(pDescTableAddr,
                        pDescElem0,
                        ConstantInt::get(m_pContext->Int32Ty(), 0),
                        "",
                        &callInst);
                }
                else
                {
                    // Build buffer descriptor from inline constant buffer address
                    SqBufRsrcWord1 sqBufRsrcWord1 = {};
                    SqBufRsrcWord2 sqBufRsrcWord2 = {};
                    SqBufRsrcWord3 sqBufRsrcWord3 = {};

                    sqBufRsrcWord1.bits.BASE_ADDRESS_HI = UINT16_MAX;
                    sqBufRsrcWord2.bits.NUM_RECORDS = UINT32_MAX;

                    sqBufRsrcWord3.bits.DST_SEL_X = BUF_DST_SEL_X;
                    sqBufRsrcWord3.bits.DST_SEL_Y = BUF_DST_SEL_Y;
                    sqBufRsrcWord3.bits.DST_SEL_Z = BUF_DST_SEL_Z;
                    sqBufRsrcWord3.bits.DST_SEL_W = BUF_DST_SEL_W;
                    sqBufRsrcWord3.bits.NUM_FORMAT = BUF_NUM_FORMAT_UINT;
                    sqBufRsrcWord3.bits.DATA_FORMAT = BUF_DATA_FORMAT_32;
                    LLPC_ASSERT(sqBufRsrcWord3.u32All == 0x24FAC);

                    Value* pDescElem1 = ExtractElementInst::Create(pDescTableAddr,
                        ConstantInt::get(m_pContext->Int32Ty(), 1),
                        "",
                        &callInst);

                    auto pBufDescTy = m_pContext->Int32x4Ty();
                    pDesc = UndefValue::get(pBufDescTy);

                    // DWORD0
                    pDesc = InsertElementInst::Create(pDesc,
                        pDescElem0,
                        ConstantInt::get(m_pContext->Int32Ty(), 0),
                        "",
                        &callInst);

                    // DWORD1
                    pDescElem1 = BinaryOperator::CreateAnd(pDescElem1,
                        ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord1.u32All),
                        "",
                        &callInst);
                    pDesc = InsertElementInst::Create(pDesc,
                        pDescElem1,
                        ConstantInt::get(m_pContext->Int32Ty(), 1),
                        "",
                        &callInst);

                    // DWORD2
                    pDesc = InsertElementInst::Create(pDesc,
                        ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord2.u32All),
                        ConstantInt::get(m_pContext->Int32Ty(), 2),
                        "",
                        &callInst);

                    // DWORD3
                    pDesc = InsertElementInst::Create(pDesc,
                        ConstantInt::get(m_pContext->Int32Ty(), sqBufRsrcWord3.u32All),
                        ConstantInt::get(m_pContext->Int32Ty(), 3),
                        "",
                        &callInst);
                }
            }
            else
            {
                auto pDescOffset = ConstantInt::get(m_pContext->Int64Ty(), descOffset);
                auto pDescSize   = ConstantInt::get(m_pContext->Int64Ty(), descSize, 0);

                pArrayOffset = CastInst::CreateZExtOrBitCast(pArrayOffset, m_pContext->Int64Ty(), "", &callInst);
                auto pOffset = BinaryOperator::CreateMul(pArrayOffset, pDescSize, "", &callInst);
                pOffset = BinaryOperator::CreateAdd(pOffset, pDescOffset, "", &callInst);

                // Get descriptor address
                std::vector<Value*> idxs;
                idxs.push_back(ConstantInt::get(m_pContext->Int64Ty(), 0, false));
                idxs.push_back(pOffset);

                Value* pDescTablePtr = nullptr;
                uint32_t descSet = pDescSet->getZExtValue();

                if (descSet == InternalResourceTable)
                {
                    pDescTablePtr = m_pContext->GetShaderInterfaceData(m_shaderStage)->pInternalTablePtr;
                }
                else if (descSet == InternalPerShaderTable)
                {
                    pDescTablePtr = m_pContext->GetShaderInterfaceData(m_shaderStage)->pInternalPerShaderTablePtr;
                }
                else
                {
                    pDescTablePtr = m_pContext->GetShaderInterfaceData(m_shaderStage)->descTablePtrs[descSet];
                }
                auto pDescPtr = GetElementPtrInst::Create(nullptr, pDescTablePtr, idxs, "", &callInst);
                auto pCastedDescPtr = CastInst::Create(Instruction::BitCast, pDescPtr, pDescPtrTy, "", &callInst);

                // Load descriptor
                pCastedDescPtr->setMetadata(m_pContext->MetaIdUniform(), m_pContext->GetEmptyMetadataNode());
                pDesc = new LoadInst(pCastedDescPtr, "", &callInst);
                cast<LoadInst>(pDesc)->setAlignment(16);
            }
        }

        if (pDesc != nullptr)
        {
            callInst.replaceAllUsesWith(pDesc);
            m_descLoadCalls.push_back(&callInst);
            m_descLoadFuncs.insert(pCallee);
        }
    }
}

// =====================================================================================================================
// Gets the descriptor value of the specified descriptor.
const DescriptorRangeValue* PatchDescriptorLoad::GetDescriptorRangeValue(
    ResourceMappingNodeType   nodeType,   // Type of the resource mapping node
    uint32_t                  descSet,    // ID of descriptor set
    uint32_t                  binding     // ID of descriptor binding
    ) const
{
    auto pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);
    const DescriptorRangeValue* pDescRangValue = nullptr;
    for (uint32_t i = 0; i < pShaderInfo->descriptorRangeValueCount; ++i)
    {
        auto pRangeValue = &pShaderInfo->pDescriptorRangeValues[i];
        if ((pRangeValue->type == nodeType) &&
            (pRangeValue->set == descSet) &&
            (pRangeValue->binding == binding))
        {
            pDescRangValue = pRangeValue;
            break;
        }
    }
    return pDescRangValue;

}
// =====================================================================================================================
// Calculates the offset and size for the specified descriptor.
void PatchDescriptorLoad::CalcDescriptorOffsetAndSize(
    ResourceMappingNodeType   nodeType,   // Type of the resource mapping node
    uint32_t                  descSet,    // ID of descriptor set
    uint32_t                  binding,    // ID of descriptor binding
    uint32_t*                 pOffset,    // [out] Calculated offset of the descriptor
    uint32_t*                 pSize,      // [out] Calculated size of the descriptor
    uint32_t*                 pDynDescIdx // [out] Calculated index of dynamic descriptor
    ) const
{
    auto pShaderInfo = m_pContext->GetPipelineShaderInfo(m_shaderStage);
    bool exist = false;

    *pDynDescIdx = InvalidValue;
    *pOffset = 0;
    *pSize = 0;

    uint32_t dynDescIdx = 0;

    // Load descriptor from internal tables
    if ((descSet == InternalResourceTable) || (descSet == InternalPerShaderTable))
    {
        *pOffset = binding * DescriptorSizeBuffer;
        *pSize = DescriptorSizeBuffer;
        exist = true;
    }

    for (uint32_t i = 0; (i < pShaderInfo->userDataNodeCount) && (exist == false); ++i)
    {
        auto pSetNode = &pShaderInfo->pUserDataNodes[i];
        if  ((pSetNode->type == ResourceMappingNodeType::DescriptorResource) ||
             (pSetNode->type == ResourceMappingNodeType::DescriptorSampler) ||
             (pSetNode->type == ResourceMappingNodeType::DescriptorTexelBuffer) ||
             (pSetNode->type == ResourceMappingNodeType::DescriptorFmask) ||
             (pSetNode->type == ResourceMappingNodeType::DescriptorBuffer) ||
             (pSetNode->type == ResourceMappingNodeType::DescriptorBufferCompact))
        {
            if ((descSet == pSetNode->srdRange.set) &&
                (binding == pSetNode->srdRange.binding) &&
                ((nodeType == pSetNode->type) ||
                 ((nodeType == ResourceMappingNodeType::DescriptorBuffer) &&
                 (pSetNode->type == ResourceMappingNodeType::DescriptorBufferCompact))))
            {
                *pOffset = pSetNode->offsetInDwords;
                if ((pSetNode->type == ResourceMappingNodeType::DescriptorResource) ||
                    (pSetNode->type == ResourceMappingNodeType::DescriptorFmask))
                {
                    *pSize = DescriptorSizeResource;
                }
                else if (pSetNode->type == ResourceMappingNodeType::DescriptorSampler)
                {
                    *pSize = DescriptorSizeSampler;
                }
                else if ((pSetNode->type == ResourceMappingNodeType::DescriptorBuffer) ||
                         (pSetNode->type == ResourceMappingNodeType::DescriptorTexelBuffer))
                {
                    *pSize = DescriptorSizeBuffer;
                }
                else
                {
                    LLPC_ASSERT(pSetNode->type == ResourceMappingNodeType::DescriptorBufferCompact);
                    *pSize = DescriptorSizeBufferCompact;
                }

                *pDynDescIdx = dynDescIdx;
                exist = true;
            }
            ++dynDescIdx;
        }
        else if (pSetNode->type == ResourceMappingNodeType::DescriptorTableVaPtr)
        {
            for (uint32_t j = 0; (j < pSetNode->tablePtr.nodeCount) && (exist == false); ++j)
            {
                auto pNode = &pSetNode->tablePtr.pNext[j];
                switch (pNode->type)
                {
                case ResourceMappingNodeType::DescriptorResource:
                case ResourceMappingNodeType::DescriptorSampler:
                case ResourceMappingNodeType::DescriptorFmask:
                case ResourceMappingNodeType::DescriptorTexelBuffer:
                case ResourceMappingNodeType::DescriptorBuffer:
                case ResourceMappingNodeType::PushConst:
                    {
                        if ((pNode->srdRange.set == descSet) &&
                            (pNode->srdRange.binding == binding) &&
                            (nodeType == pNode->type))
                        {
                            exist = true;

                            if (pNode->type == ResourceMappingNodeType::DescriptorResource)
                            {
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize = DescriptorSizeResource;
                            }
                            else if (pNode->type == ResourceMappingNodeType::DescriptorSampler)
                            {
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize = DescriptorSizeSampler;
                            }
                            else if (pNode->type == ResourceMappingNodeType::DescriptorFmask)
                            {
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize = DescriptorSizeResource;
                            }
                            else if (pNode->type == ResourceMappingNodeType::PushConst)
                            {
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize = pNode->sizeInDwords * sizeof(uint32_t);
                            }
                            else
                            {
                                LLPC_ASSERT((pNode->type == ResourceMappingNodeType::DescriptorBuffer) ||
                                             (pNode->type == ResourceMappingNodeType::DescriptorTexelBuffer));
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize = DescriptorSizeBuffer;
                            }
                        }

                        break;
                    }
                case ResourceMappingNodeType::DescriptorCombinedTexture:
                    {
                        // TODO: Check descriptor binding in Vulkan API call to make sure sampler and texture are
                        // bound in this way.
                        if ((pNode->srdRange.set == descSet) &&
                            (pNode->srdRange.binding == binding) &&
                            ((nodeType == ResourceMappingNodeType::DescriptorResource) ||
                            (nodeType == ResourceMappingNodeType::DescriptorSampler)))
                        {
                            exist = true;

                            if (nodeType == ResourceMappingNodeType::DescriptorResource)
                            {
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t);
                                *pSize   = DescriptorSizeResource + DescriptorSizeSampler;
                            }
                            else
                            {
                                LLPC_ASSERT(nodeType == ResourceMappingNodeType::DescriptorSampler);
                                *pOffset = pNode->offsetInDwords * sizeof(uint32_t) + DescriptorSizeResource;
                                *pSize   = DescriptorSizeResource + DescriptorSizeSampler;
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
        }
    }

    // TODO: We haven't removed the dead code, so we might load inactive descriptors sometimes.
    // Currently, disable this assert.
    //LLPC_ASSERT(exist);
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patching opertions for descriptor load.
INITIALIZE_PASS(PatchDescriptorLoad, "Patch-descriptor-load",
                "Patch LLVM for descriptor load operations", false, false)
