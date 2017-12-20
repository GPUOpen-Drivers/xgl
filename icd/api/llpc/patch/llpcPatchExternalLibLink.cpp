/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  PatchExternalLibLink.cpp
 * @brief LLPC source file: contains implementation of class Llpc::PatchExternalLibLink.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-patch-external-lib-link"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llpcContext.h"
#include "llpcPatchExternalLibLink.h"

using namespace llvm;
using namespace Llpc;

namespace Llpc
{

extern TimeProfileResult g_timeProfileResult;

// =====================================================================================================================
// Initializes static members.
char PatchExternalLibLink::ID = 0;
// =====================================================================================================================
PatchExternalLibLink::PatchExternalLibLink()
    :
    Patch(ID)
{
    initializePatchExternalLibLinkPass(*PassRegistry::getPassRegistry());
}

// =====================================================================================================================
// Executes this LLVM patching pass on the specified LLVM module.
bool PatchExternalLibLink::runOnModule(
    Module& module)  // [in,out] LLVM module to be run on
{
    TimeProfiler timeProfiler(&g_timeProfileResult.patchLinkTime);

    DEBUG(dbgs() << "Run the pass Patch-External-Lib-Link\n");

    Patch::Init(&module);

    Result result = Result::Success;
    auto pGlslEmuLib = m_pContext->GetGlslEmuLibrary();
    ValueToValueMapTy valueMap;

    // Add declarations of those missing functions to module based on what they are in external library
    for (const Function& libFunc : *pGlslEmuLib)
    {
        if (libFunc.isDeclaration())
        {
            auto pModuleFunc = m_pModule->getFunction(libFunc.getName());
            if (pModuleFunc == nullptr)
            {
                pModuleFunc = Function::Create(cast<FunctionType>(libFunc.getValueType()),
                                               libFunc.getLinkage(),
                                               libFunc.getName(),
                                               m_pModule);
                pModuleFunc->copyAttributesFrom(&libFunc);
            }

            valueMap[&libFunc] = pModuleFunc;
        }
    }

    // Add definitions of those missing functions to module based on what they are in external library
    for (auto& moduleFunc : *m_pModule)
    {
        if (moduleFunc.isDeclaration())
        {
            auto pLibFunc = pGlslEmuLib->getFunction(moduleFunc.getName());
            if ((pLibFunc != nullptr) && (pLibFunc->isDeclaration() == false))
            {
                Function::arg_iterator moduleFuncArgIter = moduleFunc.arg_begin();
                for (Function::const_arg_iterator libFuncArgIter = pLibFunc->arg_begin();
                         libFuncArgIter != pLibFunc->arg_end();
                         ++libFuncArgIter)
                {
                    moduleFuncArgIter->setName(libFuncArgIter->getName());
                    valueMap[&*libFuncArgIter] = &*moduleFuncArgIter++;
                }

                SmallVector<ReturnInst*, 8> retInsts;
                CloneFunctionInto(&moduleFunc, pLibFunc, valueMap, false, retInsts);
            }
        }
    }

    DEBUG(dbgs() << "After the pass Patch-External-Lib-Link: " << module);

    std::string errMsg;
    raw_string_ostream errStream(errMsg);
    if (verifyModule(module, &errStream))
    {
        LLPC_ERRS("Fails to verify module (" DEBUG_TYPE "): " << errStream.str() << "\n");
    }

    return true;
}

} // Llpc

// =====================================================================================================================
// Initializes the pass of LLVM patching opertions for linking external libraries.
INITIALIZE_PASS(PatchExternalLibLink, "Patch-external-lib-link",
                "Patch LLVM for linking external libraries", false, false)
