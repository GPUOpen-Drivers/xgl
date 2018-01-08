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
 * @file  llpcSpirvLowerDynIndex.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerDynIndex.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-dyn-index"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcSpirvLowerDynIndex.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerDynIndex::ID = 0;

// =====================================================================================================================
SpirvLowerDynIndex::SpirvLowerDynIndex()
    :
    SpirvLower(ID)
{
    initializeSpirvLowerDynIndexPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerDynIndex::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Dyn-Index\n");

    SpirvLower::Init(&module);

    visit(m_pModule);

    // Remove those instructions that are replaced by this lower pass
    for (auto pInst : m_loadInsts)
    {
        LLPC_ASSERT(pInst->user_empty());
        pInst->dropAllReferences();
        pInst->eraseFromParent();
    }

    for (auto pInst : m_getElemPtrInsts)
    {
        LLPC_ASSERT(pInst->user_empty());
        pInst->dropAllReferences();
        pInst->eraseFromParent();
    }

    DEBUG(dbgs() << "After the pass Spirv-Lower-Dyn-Index: " << module);

    std::string errMsg;
    raw_string_ostream errStream(errMsg);
    if (verifyModule(module, &errStream))
    {
        LLPC_ERRS("Fails to verify module (" DEBUG_TYPE "): " << errStream.str() << "\n");
    }

    return true;
}

// =====================================================================================================================
// Visits "getelementptr" instruction.
void SpirvLowerDynIndex::visitGetElementPtrInst(
    GetElementPtrInst& getElemPtrInst) // "GetElementPtr" instruction
{
    uint32_t operandIndex = InvalidValue;
    uint32_t dynIndexBound = 0;

    if (NeedExpandDynamicIndex(&getElemPtrInst, &operandIndex, &dynIndexBound))
    {
        SmallVector<GetElementPtrInst*, 1> getElemPtrs;
        auto pDynIndex = getElemPtrInst.getOperand(operandIndex);
        bool isType64 = (pDynIndex->getType()->getPrimitiveSizeInBits() == 64);

        // Create "getelementptr" instructions with constant indices
        for (uint32_t i = 0; i < dynIndexBound; ++i)
        {
            auto pGetElemPtr = cast<GetElementPtrInst>(getElemPtrInst.clone());
            auto pConstIndex = isType64 ? ConstantInt::get(m_pContext->Int64Ty(), i) :
                                          ConstantInt::get(m_pContext->Int32Ty(), i);
            pGetElemPtr->setOperand(operandIndex, pConstIndex);
            getElemPtrs.push_back(pGetElemPtr);
            pGetElemPtr->insertBefore(&getElemPtrInst);
        }

        // Copy users, ExpandStoreInst/ExpandLoadInst change getElemPtrInst's user
        std::vector<User*> users;
        for (auto pUser : getElemPtrInst.users())
        {
            users.push_back(pUser);
        }

        // Replace the original "getelementptr" instructions with a group of newly-created "getelementptr" instructions
        for (auto pUser : users)
        {
            auto pLoadInst = dyn_cast<LoadInst>(pUser);
            auto pStoreInst = dyn_cast<StoreInst>(pUser);

            if (pLoadInst != nullptr)
            {
                ExpandLoadInst(pLoadInst, getElemPtrs, pDynIndex);
            }
            else if (pStoreInst != nullptr)
            {
                ExpandStoreInst(pStoreInst, getElemPtrs, pDynIndex);
            }
            else
            {
                LLPC_NEVER_CALLED();
            }
        }

        // Collect replaced instructions that will be removed
        m_getElemPtrInsts.insert(&getElemPtrInst);
    }
}

// =====================================================================================================================
// Checks whether the specified "getelementptr" instruction contains dynamic index and is therefore able to be expanded.
bool SpirvLowerDynIndex::NeedExpandDynamicIndex(
    GetElementPtrInst* pGetElemPtr,       // [in] "GetElementPtr" instruction
    uint32_t*          pOperandIndex,     // [out] Index of the operand that represents a dynamic index
    uint32_t*          pDynIndexBound     // [out] Upper bound of dynamic index
    ) const
{
    static const uint32_t MaxDynIndexBound = 8;

    std::vector<Value*> idxs;
    uint32_t operandIndex = InvalidValue;
    bool     needExpand   = false;
    bool     allowExpand  = true;
    auto     pPtrVal      = pGetElemPtr->getPointerOperand();

    // NOTE: We only handle local variables.
    if (pPtrVal->getType()->getPointerAddressSpace() != SPIRAS_Private)
    {
        allowExpand = false;
    }

    for (uint32_t i = 1, operandCount = pGetElemPtr->getNumOperands(); allowExpand && (i < operandCount); ++i)
    {
        auto pIndex = pGetElemPtr->getOperand(i);
        if (isa<Constant>(pIndex) == false)
        {
            // Find the operand that represents a dynamic index
            if (operandIndex == InvalidValue)
            {
                // This is the first operand that represents a dynamic index
                operandIndex = i;
                needExpand = true;

                auto pIndexedTy = pGetElemPtr->getIndexedType(pPtrVal->getType()->getPointerElementType(), idxs);
                if (pIndexedTy != nullptr)
                {
                    // Check the upper bound of dynamic index
                    if (isa<ArrayType>(pIndexedTy))
                    {
                        auto pArrayTy = dyn_cast<ArrayType>(pIndexedTy);
                        if (pArrayTy->getArrayNumElements() > MaxDynIndexBound)
                        {
                            // Skip expand if array size greater than threshold
                            allowExpand = false;
                        }
                        else
                        {
                            *pDynIndexBound = pArrayTy->getArrayNumElements();
                        }
                    }
                    else if (isa<VectorType>(pIndexedTy))
                    {
                        // Always expand for vector
                        auto pVectorTy = dyn_cast<VectorType>(pIndexedTy);
                        *pDynIndexBound = pVectorTy->getVectorNumElements();
                    }
                    else
                    {
                        LLPC_NEVER_CALLED();
                        allowExpand = false;
                    }
                }
                else
                {
                    LLPC_NEVER_CALLED();
                    allowExpand = false;
                }
            }
            else
            {
                // Skip expand if there are more than one dynamic indices
                allowExpand = false;
            }
        }
        else
        {
            idxs.push_back(pIndex);
        }
    }

    if (needExpand && allowExpand)
    {
        // Skip expand if the user of "getelementptr" is neither "load" nor "store"
        for (auto pUser : pGetElemPtr->users())
        {
            if ((isa<LoadInst>(pUser) == false) && (isa<StoreInst>(pUser) == false))
            {
                allowExpand = false;
                break;
            }
        }
    }

    *pOperandIndex = operandIndex;
    return needExpand && allowExpand;
}

// =====================================================================================================================
// Expands "load" instruction with constant-index "getelementptr" instructions.
void SpirvLowerDynIndex::ExpandLoadInst(
    LoadInst*                    pLoadInst,       // [in] "Load" instruction
    ArrayRef<GetElementPtrInst*> getElemPtrs,     // [in] A group of "getelementptr" with constant indices
    Value*                       pDynIndex)       // [in] Dynamic index
{
    // Expand is something like this:
    //
    //   firstValue  = load getElemPtrs[0]
    //
    //   secondValue = load getElemPtrs[1]
    //   firstValue  = (dynIndex == 1) ?  secondValue : firstValue
    //
    //   secondValue = load getElemPtrs[2]
    //   firstValue  = (dynIndex == 2) ?  secondValue : firstValue
    //   ...
    //   secondValue = load getElemPtrs[upperBound -2]
    //   firstValue  = (dynIndex == upperBound - 2) ? secondValue : firstValue
    //   secondValue = load getElemPtrs[upperBound - 1]
    //   firstValue  = (dynIndex == upperBound - 1) ? secondValue : firstValue
    //
    //   loadValue   = firstValue

    bool isType64 = (pDynIndex->getType()->getPrimitiveSizeInBits() == 64);
    Instruction* pFirstLoadValue = new LoadInst(getElemPtrs[0], "", false, pLoadInst);

    for (uint32_t i = 1, getElemPtrCount = getElemPtrs.size(); i < getElemPtrCount; ++i)
    {
        auto pConstIndex = isType64 ? ConstantInt::get(m_pContext->Int64Ty(), i) :
                                      ConstantInt::get(m_pContext->Int32Ty(), i);

        auto pSecondLoadValue = new LoadInst(getElemPtrs[i], "", false, pLoadInst);
        auto pCond = new ICmpInst(pLoadInst, ICmpInst::ICMP_EQ, pDynIndex, pConstIndex);
        pFirstLoadValue = SelectInst::Create(pCond, pSecondLoadValue, pFirstLoadValue, "", pLoadInst);
    }

    pLoadInst->replaceAllUsesWith(pFirstLoadValue);
    m_loadInsts.insert(pLoadInst);
}

// =====================================================================================================================
// Expands "store" instruction with fixed indexed "getelementptr" instructions.
void SpirvLowerDynIndex::ExpandStoreInst(
    StoreInst*                   pStoreInst,     // [in] "Store" instruction
    ArrayRef<GetElementPtrInst*> getElemPtrs,    // [in] A group of "getelementptr" with constant indices
    Value*                       pDynIndex)      // [in] Dynamic index
{
    // Expand is something like this:
    //
    //   firstPtr  = getElemPtrs[0]
    //
    //   secondPtr = getElemPtrs[1]
    //   firstPtr  = (dynIndex == 1) ? secondPtr : firstPtr
    //
    //   secondPtr = getElemPtrs[2]
    //   firstPtr  = (dynIndex == 2) ? secondPtr : firstPtr
    //   ...
    //   secondPtr = getElemPtrs[upperBound - 2]
    //   firstPtr  = (dynIndex == upperBound - 2) ? secondPtr : firstPtr
    //
    //   secondPtr = getElemPtrs[upperBound - 1]
    //   firstPtr  = (dynIndex == upperBound - 1) ? secondPtr : firstPtr
    //
    //   store storeValue, firstPtr

    bool isType64 = (pDynIndex->getType()->getPrimitiveSizeInBits() == 64);
    Instruction* pFirstStoreDest = getElemPtrs[0];

    for (uint32_t i = 1, getElemPtrCount = getElemPtrs.size(); i < getElemPtrCount; ++i)
    {
        auto pSecondStoreDest = getElemPtrs[i];
        auto pConstIndex = isType64 ? ConstantInt::get(m_pContext->Int64Ty(), i) :
                                      ConstantInt::get(m_pContext->Int32Ty(), i);
        auto pCond = new ICmpInst(pStoreInst, ICmpInst::ICMP_EQ, pDynIndex, pConstIndex);
        pFirstStoreDest = SelectInst::Create(pCond, pSecondStoreDest, pFirstStoreDest, "", pStoreInst);
    }

    pStoreInst->setOperand(1, pFirstStoreDest);
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for dynamic index in access chain.
INITIALIZE_PASS(SpirvLowerDynIndex, "spirv-lower-dyn-index",
                "Lower SPIR-V dynamic index in access chain", false, false)
