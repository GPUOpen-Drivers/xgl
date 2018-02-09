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
 * @file  llpcSpirvLowerAccessChain.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLowerAccessChain.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower-access-chain"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <stack>
#include "SPIRVInternal.h"
#include "llpcSpirvLowerAccessChain.h"

using namespace llvm;
using namespace SPIRV;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char SpirvLowerAccessChain::ID = 0;

// =====================================================================================================================
SpirvLowerAccessChain::SpirvLowerAccessChain()
    :
    SpirvLower(ID)
{
    initializeSpirvLowerAccessChainPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this SPIR-V lowering pass on the specified LLVM module.
bool SpirvLowerAccessChain::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Spirv-Lower-Access-Chain\n");

    SpirvLower::Init(&module);

    // Invoke handling of "getelementptr" instruction
    visit(m_pModule);

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

// =====================================================================================================================
// Visits "getelementptr" instruction.
void SpirvLowerAccessChain::visitGetElementPtrInst(
    GetElementPtrInst& getElemPtrInst) // [in] "Getelementptr" instruction
{
    auto pGetElemPtrInst = &getElemPtrInst;

    // NOTE: Here, we try to coalesce chained "getelementptr" instructions (created from multi-level access chain).
    // Because the metadata is always decorated on top-level pointer value (actually a global variable).
    const uint32_t addrSpace = pGetElemPtrInst->getType()->getPointerAddressSpace();
    if ((addrSpace == SPIRAS_Private) ||
        (addrSpace == SPIRAS_Input) || (addrSpace == SPIRAS_Output) ||
        (addrSpace == SPIRAS_Uniform))
    {
        TryToCoalesceChain(&getElemPtrInst, addrSpace);
    }
}

// =====================================================================================================================
// Tries to coalesce chained "getelementptr" instructions (created from multi-level access chain) from bottom to top
// in the type hierarchy.
//
// e.g.
//      %x = getelementptr %blockType, %blockType addrspace(N)* @block, i32 0, i32 L, i32 M
//      %y = getelementptr %fieldType, %fieldType addrspace(N)* %x, i32 0, i32 N
//
//      =>
//
//      %y = getelementptr %blockType, %blockType addrspace(N)* @block, i32 0, i32 L, i32 M, i32 N
//
llvm::GetElementPtrInst* SpirvLowerAccessChain::TryToCoalesceChain(
    GetElementPtrInst* pGetElemPtr, // [in] "getelementptr" instruction in the bottom to do coalescing
    uint32_t           addrSpace)   // Address space of the pointer value of "getelementptr"
{
    GetElementPtrInst* pCoalescedGetElemPtr = pGetElemPtr;

    std::stack<GetElementPtrInst*> chainedInsts; // Order: from top to bottom
    std::stack<GetElementPtrInst*> removedInsts; // Order: from botton to top
    GetElementPtrInst* pPtrVal = pGetElemPtr;

    // Collect chained "getelementptr" instructions from bottom to top
    do
    {
        chainedInsts.push(pPtrVal);
        pPtrVal = dyn_cast<GetElementPtrInst>(pPtrVal->getPointerOperand());
    } while ((pPtrVal != nullptr) && (pPtrVal->getType()->getPointerAddressSpace() == addrSpace));

    // If there are more than one "getelementptr" instructions, do coalescing
    if (chainedInsts.size() > 1)
    {
        std::vector<Value*> idxs;

        GetElementPtrInst* pInst = chainedInsts.top();
        auto pBlockPtr = pInst->getPointerOperand();
        idxs.push_back(pInst->getOperand(1)); // Pointer to the block

        while (chainedInsts.empty() == false)
        {
            pInst = chainedInsts.top();
            for (uint32_t i = 2, operandCount = pInst->getNumOperands(); i < operandCount; ++i)
            {
                // NOTE: We skip the first two operands here. The first one is the pointer value from which
                // the element pointer is constructed. And the second one is always 0 to dereference the
                // pointer value.
                idxs.push_back(pInst->getOperand(i));
            }
            chainedInsts.pop();
            removedInsts.push(pInst);
        }

        // Create the coalesced "getelementptr" instruction (do combining)
        pCoalescedGetElemPtr = GetElementPtrInst::Create(nullptr, pBlockPtr, idxs, "", pGetElemPtr);
        pGetElemPtr->replaceAllUsesWith(pCoalescedGetElemPtr);

        // Try to drop references and see if we could remove some dead "getelementptr" instructions
        while (removedInsts.empty() == false)
        {
            GetElementPtrInst* pInst = removedInsts.top();
            if (pInst->user_empty())
            {
                pInst->dropAllReferences();
                pInst->eraseFromParent();
            }
            removedInsts.pop();
        }
    }

    return pCoalescedGetElemPtr;
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of SPIR-V lowering opertions for access chain.
INITIALIZE_PASS(SpirvLowerAccessChain, "spirv-lower-access-chain",
                "Lower SPIR-V access chain", false, false)
