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
 * @file  llpcSpirvLowerAggregateLoadStore.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerAggregateLoadStore.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-aggregate-load-store"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <stack>
#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcSpirvLowerAggregateLoadStore.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerAggregateLoadStore::ID = 0;

// =====================================================================================================================
SpirvLowerAggregateLoadStore::SpirvLowerAggregateLoadStore()
    :
    SpirvLower(ID)
{
    initializeSpirvLowerAggregateLoadStorePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerAggregateLoadStore::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Aggregate-Load-Store\n");

    SpirvLower::Init(&module);

    // Invoke handling of "load" and "store" instruction
    visit(m_pModule);

    // Remove unnecessary "load" instructions
    for (auto pLoadInst : m_loadInsts)
    {
        pLoadInst->dropAllReferences();
        pLoadInst->eraseFromParent();
    }

    // Remove unnecessary "store" instructions
    for (auto pStoreInst : m_storeInsts)
    {
        pStoreInst->dropAllReferences();
        pStoreInst->eraseFromParent();
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "load" instruction.
void SpirvLowerAggregateLoadStore::visitLoadInst(
    LoadInst& loadInst) // [in] "Load" instruction
{
    auto pLoadSrc = loadInst.getOperand(0);
    auto pLoadTy = loadInst.getType();
    if (pLoadSrc->getType()->getPointerAddressSpace() == SPIRAS_Private)
    {
        if (pLoadTy->isArrayTy() || pLoadTy->isStructTy())
        {
            std::vector<uint32_t> idxs;
            Value* pLoadValue = UndefValue::get(pLoadTy);
            pLoadValue = ExpandLoadInst(pLoadValue, pLoadSrc, pLoadTy, idxs, &loadInst);
            m_loadInsts.insert(&loadInst);
            loadInst.replaceAllUsesWith(pLoadValue);
        }
    }
}

// =====================================================================================================================
// Visits "store" instruction.
void SpirvLowerAggregateLoadStore::visitStoreInst(
    StoreInst& storeInst) // [in] "Store" instruction
{
    Value* pStoreValue  = storeInst.getOperand(0);
    Value* pStoreDest = storeInst.getOperand(1);

    if (pStoreDest->getType()->getPointerAddressSpace() == SPIRAS_Private)
    {
        auto pStoreTy = pStoreDest->getType()->getPointerElementType();
        if (pStoreTy->isArrayTy() || pStoreTy->isStructTy())
        {
            std::vector<uint32_t> idxs;
            ExpandStoreInst(pStoreValue, pStoreDest, pStoreTy, idxs, &storeInst);
            m_storeInsts.insert(&storeInst);
        }
    }
}

// =====================================================================================================================
// Expands the "store" instruction operating on aggregate type to several basic "store" instructions operating on
// vector or scalar type.
void SpirvLowerAggregateLoadStore::ExpandStoreInst(
    Value*                 pStoreValue,   // [in] Value to store
    Value*                 pStorePtr,     // [in] Store destination (a pointer)
    Type*                  pStoreTy,      // [in] Current store type
    std::vector<uint32_t>& idxStack,      // [in] Stack to store indices (type hierarchy access)
    Instruction*           pInsertPos)    // [in] Where to insert instructions
{
    if (pStoreTy->isArrayTy())
    {
        auto pElemTy  = pStoreTy->getArrayElementType();
        for (uint32_t i = 0, elemCount = pStoreTy->getArrayNumElements(); i < elemCount; ++i)
        {
            idxStack.push_back(i);
            ExpandStoreInst(pStoreValue, pStorePtr, pElemTy , idxStack, pInsertPos);
            idxStack.pop_back();
        }
    }
    else if (pStoreTy->isStructTy())
    {
        for (uint32_t i = 0, elemCount = pStoreTy->getStructNumElements(); i < elemCount; ++i)
        {
            auto pMemberTy = pStoreTy->getStructElementType(i);
            idxStack.push_back(i);
            ExpandStoreInst(pStoreValue, pStorePtr, pMemberTy, idxStack, pInsertPos);
            idxStack.pop_back();
        }
    }
    else
    {
        Value* pElemValue = nullptr;
        Value* pElemPtr = nullptr;

        pElemValue = ExtractValueInst::Create(pStoreValue, idxStack, "", pInsertPos);
        std::vector<Value*> idxs;
        idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        for (uint32_t i = 0, idxCount = idxStack.size(); i < idxCount; ++i)
        {
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), idxStack[i]));
        }

        pElemPtr = GetElementPtrInst::CreateInBounds(pStorePtr, idxs,"", pInsertPos);

        LLPC_ASSERT(pElemPtr->getType()->getPointerElementType() != pElemValue->getType());

        new StoreInst(pElemValue, pElemPtr, pInsertPos);
    }
}

// =====================================================================================================================
// Expands the "load" instruction operating on aggregate type to several basic "load" instructions operating on
// vector or scalar type.
Value* SpirvLowerAggregateLoadStore::ExpandLoadInst(
    Value*                 pLoadValue,    // [in] Value from load
    Value*                 pLoadPtr,      // [in] Load source (a pointer)
    Type*                  pLoadTy,       // [in] Current store type
    std::vector<uint32_t>& idxStack,      // [in] Stack to store indices (type hierarchy access)
    Instruction*           pInsertPos)    // [in] Where to insert instructions
{
    if (pLoadTy->isArrayTy())
    {
        auto pElemTy  = pLoadTy->getArrayElementType();
        for (uint32_t i = 0, elemCount = pLoadTy->getArrayNumElements(); i < elemCount; ++i)
        {
            idxStack.push_back(i);
            pLoadValue = ExpandLoadInst(pLoadValue, pLoadPtr, pElemTy , idxStack, pInsertPos);
            idxStack.pop_back();
        }
    }
    else if (pLoadTy->isStructTy())
    {
        for (uint32_t i = 0, elemCount = pLoadTy->getStructNumElements(); i < elemCount; ++i)
        {
            auto pMemberTy = pLoadTy->getStructElementType(i);
            idxStack.push_back(i);
            pLoadValue = ExpandLoadInst(pLoadValue, pLoadPtr, pMemberTy, idxStack, pInsertPos);
            idxStack.pop_back();
        }
    }
    else
    {
        std::vector<Value*> idxs;
        idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), 0));
        for (uint32_t i = 0, idxCount = idxStack.size(); i < idxCount; ++i)
        {
            idxs.push_back(ConstantInt::get(m_pContext->Int32Ty(), idxStack[i]));
        }
        auto pElemPtr = GetElementPtrInst::CreateInBounds(pLoadPtr, idxs, "", pInsertPos);
        auto pElemValue = new LoadInst(pElemPtr, "", pInsertPos);
        pLoadValue = InsertValueInst::Create(pLoadValue, pElemValue, idxStack, "", pInsertPos);
    }

    return pLoadValue;
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for load and store operations on aggregate type.
INITIALIZE_PASS(SpirvLowerAggregateLoadStore, "spirv-lower-aggregate-load-store",
                "Lower SPIR-V load and store operations on aggregate type", false, false)
