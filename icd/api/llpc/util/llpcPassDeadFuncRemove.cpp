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
 * @file  llpcPassDeadFuncRemove.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PassDeadFuncRemove.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-pass-dead-func-remove"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llpcPassDeadFuncRemove.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PassDeadFuncRemove::ID = 0;

// =====================================================================================================================
PassDeadFuncRemove::PassDeadFuncRemove()
    :
    llvm::ModulePass(ID)
{
    initializePassDeadFuncRemovePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM pass on the specified LLVM module.
bool PassDeadFuncRemove::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Pass-Dead-Func-Remove\n");

    uint32_t iterCount = 0;
    bool changed = false; // Whether changes are made

    do
    {
        changed = false; // Reset this flag at the beginning of each iteration

        for (auto pFunc = module.begin(), pEnd = module.end(); pFunc != pEnd; )
        {
            auto pCurrFunc = pFunc++;
            auto callConv = pCurrFunc->getCallingConv();

            // Ignore entry points
            if (pCurrFunc->getDLLStorageClass() == GlobalValue::DLLExportStorageClass)
            {
                continue;
            }

            // Remove dead functions
            if (pCurrFunc->use_empty())
            {
                DEBUG(dbgs() << "Remove ";
                      pCurrFunc->printAsOperand(dbgs());
                      dbgs() << '\n');
                pCurrFunc->dropAllReferences();
                pCurrFunc->eraseFromParent();
                changed = true;
            }
        }

        ++iterCount;
    } while (changed && (iterCount < MaxIterCountOfDetection));

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return true;
}

} // Llpc

// =====================================================================================================================
// Initializes the LLVM pass for dead function removal.
INITIALIZE_PASS(PassDeadFuncRemove, "Pass-dead-func-remove",
                "LLVM pass for dead function removal", false, false)
