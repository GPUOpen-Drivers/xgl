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
 * @file  llpcSpirvLowerConstImmediateStore.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerConstImmediateStore.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-const-immediate-store"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"

#include <vector>
#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcIntrinsDefs.h"
#include "llpcSpirvLowerConstImmediateStore.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerConstImmediateStore::ID = 0;

// =====================================================================================================================
SpirvLowerConstImmediateStore::SpirvLowerConstImmediateStore()
    :
    SpirvLower(ID)
{
    initializeSpirvLowerConstImmediateStorePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerConstImmediateStore::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Const-Immediate-Store\n");

    SpirvLower::Init(&module);

    // Process "alloca" instructions to see if they can be optimized to a read-only global
    // variable.
    for (auto funcIt = module.begin(), funcItEnd = module.end(); funcIt != funcItEnd; ++funcIt)
    {
        if (auto pFunc = dyn_cast<Function>(&*funcIt))
        {
            if (pFunc->empty() == false)
            {
                ProcessAllocaInstructions(pFunc);
            }
        }
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Processes "alloca" instructions at the beginning of the given non-empty function to see if they
// can be optimized to a read-only global variable.
void SpirvLowerConstImmediateStore::ProcessAllocaInstructions(
    Function* pFunc)  // [in] Function to process
{
    // NOTE: We only visit the entry block on the basis that SPIR-V translator puts all "alloca"
    // instructions there.
    auto pEntryBlock = &pFunc->front();
    for (auto instIt = pEntryBlock->begin(), instItEnd = pEntryBlock->end(); instIt != instItEnd; ++instIt)
    {
        auto pInst = &*instIt;
        if (auto pAlloca = dyn_cast<AllocaInst>(pInst))
        {
            if (pAlloca->getType()->getElementType()->isAggregateType())
            {
                // Got an "alloca" instruction of aggregate type.
                auto pStoreInst = FindSingleStore(pAlloca);
                if ((pStoreInst != nullptr) && isa<Constant>(pStoreInst->getValueOperand()))
                {
                    // Got an aggregate "alloca" with a single store to the whole type.
                    // Do the optimization.
                    ConvertAllocaToReadOnlyGlobal(pStoreInst);
                }
            }
        }
    }
}

// =====================================================================================================================
// Finds the single "store" instruction storing to this pointer.
//
// Returns nullptr if no "store", multiple "store", or partial "store" instructions (store only part
// of the memory) are found.
//
// NOTE: This is conservative in that it returns nullptr if the pointer escapes by being used in anything
// other than "store" (as the pointer), "load" or "getelementptr" instruction.
StoreInst* SpirvLowerConstImmediateStore::FindSingleStore(
    AllocaInst* pAlloca)  // [in] The "alloca" instruction to process
{
    std::vector<Instruction*> pointers;
    bool isOuterPointer = true;
    StoreInst* pStoreInstFound = nullptr;
    Instruction* pPointer = pAlloca;
    while (true)
    {
        for (auto useIt = pPointer->use_begin(), useItEnd = pPointer->use_end(); useIt != useItEnd; ++useIt)
        {
            auto pUser = cast<Instruction>(useIt->getUser());
            if (auto pStoreInst = dyn_cast<StoreInst>(pUser))
            {
                if ((pPointer == pStoreInst->getValueOperand()) || (pStoreInstFound != nullptr)
                      || isOuterPointer == false)
                {
                    // Pointer escapes by being stored, or we have already found a "store"
                    // instruction, or this is a partial "store" instruction.
                    return nullptr;
                }
                pStoreInstFound = pStoreInst;
            }
            else if (auto pGetElemPtrInst = dyn_cast<GetElementPtrInst>(pUser))
            {
                pointers.push_back(pGetElemPtrInst);
            }
            else if (isa<LoadInst>(pUser) == false)
            {
                // Pointer escapes by being used in some way other than "load/store/getelementptr".
                return nullptr;
            }
        }

        if (pointers.empty())
        {
            break;
        }

        pPointer = pointers.back();
        pointers.pop_back();
        isOuterPointer = false;
    }

    return pStoreInstFound;
}

// =====================================================================================================================
// Converts an "alloca" instruction with a single constant store into a read-only global variable.
//
// NOTE: This erases the "store" instruction (so it will not be lowered by a later lowering pass
// any more) but not the "alloca" or replaced "getelementptr" instruction (they will be removed
// later by DCE pass).
void SpirvLowerConstImmediateStore::ConvertAllocaToReadOnlyGlobal(
    StoreInst* pStoreInst)  // [in] The single constant store into the "alloca"
{
    auto pAlloca = cast<AllocaInst>(pStoreInst->getPointerOperand());
    auto pGlobal = new GlobalVariable(*m_pModule,
                                      pAlloca->getType()->getElementType(),
                                      true, // isConstant
                                      GlobalValue::InternalLinkage,
                                      cast<Constant>(pStoreInst->getValueOperand()),
                                      "",
                                      nullptr,
                                      GlobalValue::NotThreadLocal,
                                      SPIRAS_Constant);
    pGlobal->takeName(pAlloca);
    // Change all uses of pAlloca to use pGlobal. We need to do it manually, as there is a change
    // of address space, and we also need to recreate "getelementptr"s.
    std::vector<std::pair<Instruction*, Value*>> allocaToGlobalMap;
    allocaToGlobalMap.push_back(std::pair<Instruction*, Value*>(pAlloca, pGlobal));
    do
    {
        auto pAlloca = allocaToGlobalMap.back().first;
        auto pGlobal = allocaToGlobalMap.back().second;
        allocaToGlobalMap.pop_back();
        while (pAlloca->use_empty() == false)
        {
            auto useIt = pAlloca->use_begin();
            if (auto pOrigGetElemPtrInst = dyn_cast<GetElementPtrInst>(useIt->getUser()))
            {
                // This use is a "getelementptr" instruction. Create a replacement one with the new address space.
                SmallVector<Value*, 4> indices;
                for (auto idxIt = pOrigGetElemPtrInst->idx_begin(),
                        idxItEnd = pOrigGetElemPtrInst->idx_end(); idxIt != idxItEnd; ++idxIt)
                {
                    indices.push_back(*idxIt);
                }
                auto pNewGetElemPtrInst = GetElementPtrInst::Create(nullptr,
                                                                    pGlobal,
                                                                    indices,
                                                                    "",
                                                                    pOrigGetElemPtrInst);
                pNewGetElemPtrInst->takeName(pOrigGetElemPtrInst);
                pNewGetElemPtrInst->setIsInBounds(pOrigGetElemPtrInst->isInBounds());
                pNewGetElemPtrInst->copyMetadata(*pOrigGetElemPtrInst);
                // Remember that we need to replace the uses of the original "getelementptr" with the new one.
                allocaToGlobalMap.push_back(std::pair<Instruction*, Value*>(pOrigGetElemPtrInst, pNewGetElemPtrInst));
                // Remove the use from the original "getelementptr".
                *useIt = UndefValue::get(pAlloca->getType());
            }
            else
            {
                *useIt = pGlobal;
            }
        }
        // Visit next map pair.
    } while (allocaToGlobalMap.empty() == false);
    pStoreInst->eraseFromParent();
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering operations for constant immediate store.
INITIALIZE_PASS(SpirvLowerConstImmediateStore, "spirv-lower-const-immediate-store",
                "Lower SPIR-V constant immediate store", false, false)
