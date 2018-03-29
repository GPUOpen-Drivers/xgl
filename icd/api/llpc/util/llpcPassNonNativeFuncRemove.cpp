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
 * @file  llpcPassNonNativeFuncRemove.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PassNonNativeFuncRemove.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-pass-non-native-func-remove"

#include <unordered_set>
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llpcContext.h"
#include "llpcPassNonNativeFuncRemove.h"

using namespace llvm;
using namespace Llpc;

namespace llvm
{

namespace cl
{

// -disable-llvm-patch: disable the patch for LLVM back-end issues.
static opt<bool> DisableLLVMPatch("disable-llvm-patch",
                                  desc("Disable the patch for LLVM back-end issues"),
                                  init(false));

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
// Initializes static members.
char PassNonNativeFuncRemove::ID = 0;

// =====================================================================================================================
PassNonNativeFuncRemove::PassNonNativeFuncRemove()
    :
    ModulePass(ID)
{
    initializePassNonNativeFuncRemovePass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM pass on the specified LLVM module.
bool PassNonNativeFuncRemove::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    DEBUG(dbgs() << "Run the pass Pass-Non-Native-Func-Remove\n");

    bool changed = false;
    std::unordered_set<Function*> nonNativeFuncs;
    std::unordered_set<Function*> nonNativeFuncDecls;

    // Collect non-native functions
    for (auto pFunc = module.begin(), pEnd = module.end(); pFunc != pEnd; ++pFunc)
    {
        if (pFunc->isDeclaration())
        {
            auto funcName = pFunc->getName();

            // TODO: "llvm.fabs." is to pass CTS dEQP-VK.ssbo.layout.single_basic_type.std430/std140.row_major_lowp_
            // mat4. we should remove it once the bug in LLVM back-end is fixed.
            if (funcName.startswith("llpc.") ||
                funcName.startswith("llvm.amdgcn.") ||
                ((cl::DisableLLVMPatch == false) && funcName.startswith("llvm.fabs.")))
            {
                for (auto pUser : pFunc->users())
                {
                    auto pInst = cast<Instruction>(pUser);
                    auto pNonNativeFunc = pInst->getParent()->getParent();
                    nonNativeFuncs.insert(pNonNativeFunc);
                }
                nonNativeFuncDecls.insert(&*pFunc);
            }
        }

        if (cl::DisableLLVMPatch == false)
        {
            if (pFunc->getName().startswith("_Z14unpackHalf2x16i"))
            {
                nonNativeFuncs.insert(&*pFunc);
            }
        }
    }

    // Remove functions which reference non-native function
    for (auto pFunc : nonNativeFuncs)
    {
        LLPC_ASSERT(pFunc->use_empty());
        pFunc->dropAllReferences();
        pFunc->eraseFromParent();
        changed = true;
    }

    // Remove non-native function declarations
    for (auto pFuncDecl : nonNativeFuncDecls)
    {
        LLPC_ASSERT(pFuncDecl->use_empty());
        pFuncDecl->dropAllReferences();
        pFuncDecl->eraseFromParent();
        changed = true;
    }

    LLPC_VERIFY_MODULE_FOR_PASS(module);

    return changed;
}

} // Llpc

// =====================================================================================================================
// Initializes the LLVM pass for non-native function removal.
INITIALIZE_PASS(PassNonNativeFuncRemove, "Pass-non-native-func-remove",
                "LLVM pass for non-native function removal", false, false)
