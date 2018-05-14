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
 * @file  llpcPatch.cpp
 * @brief LLPC source file: contains implementation of class Llpc::Patch.
 ***********************************************************************************************************************
 */
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils.h"
#include "llpcContext.h"
#include "llpcInternal.h"
#include "llpcPassDeadFuncRemove.h"
#include "llpcPassExternalLibLink.h"
#include "llpcPatch.h"
#include "llpcPatchAddrSpaceMutate.h"
#include "llpcPatchBufferOp.h"
#include "llpcPatchDescriptorLoad.h"
#include "llpcPatchEntryPointMutate.h"
#include "llpcPatchImageOp.h"
#include "llpcPatchInOutImportExport.h"
#include "llpcPatchPushConstOp.h"
#include "llpcPatchResourceCollect.h"

#define DEBUG_TYPE "llpc-patch"

using namespace llvm;

namespace llvm
{

namespace cl
{

// -auto-layout-desc: automatically create descriptor layout based on resource usages
opt<bool> AutoLayoutDesc("auto-layout-desc",
                         desc("Automatically create descriptor layout based on resource usages"));

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
// Executes LLVM preliminary patching opertions for LLVM module.
Result Patch::PreRun(
    Module* pModule)    // [in,out] LLVM module to be run on
{
    Result result = Result::Success;

    auto pContext = static_cast<Context*>(&pModule->getContext());
    ShaderStage shaderStage = GetShaderStageFromModule(pModule);

    if (cl::AutoLayoutDesc)
    {
        // Automatically layout descriptor
        pContext->AutoLayoutDescriptor(shaderStage);
    }

    // Do patching opertions
    if (result == Result::Success)
    {
        legacy::PassManager passMgr;

        // Patch resource collecting, remove inactive resources (should be the first preliminary pass)
        passMgr.add(PatchResourceCollect::Create());

        if (passMgr.run(*pModule) == false)
        {
            result = Result::ErrorInvalidShader;
        }
    }

    return result;
}

// =====================================================================================================================
// Executes LLVM patching opertions for LLVM module and links it with external LLVM libraries.
Result Patch::Run(
    Module* pModule)    // [in,out] LLVM module to be run on
{
    Result result = Result::Success;
    Context* pContext = static_cast<Context*>(&pModule->getContext());
    // Do patching opertions
    legacy::PassManager passMgr;

    // Lower SPIRAS address spaces to AMDGPU address spaces.
    passMgr.add(PatchAddrSpaceMutate::Create());

    // Patch entry-point mutation (should be done before external library link)
    passMgr.add(PatchEntryPointMutate::Create());

    // Patch image operations (should be done before external library link)
    passMgr.add(PatchImageOp::Create());

    // Patch push constant loading (should be done before external library link)
    passMgr.add(PatchPushConstOp::Create());

    // Patch buffer operations (should be done before external library link)
    passMgr.add(PatchBufferOp::Create());

    // Link external libraries and remove dead functions after it
    passMgr.add(PassExternalLibLink::Create(pContext->GetGlslEmuLibrary()));
    passMgr.add(PassDeadFuncRemove::Create());

    // Function inlining and remove dead functions after it
    passMgr.add(createFunctionInliningPass(InlineThreshold));
    passMgr.add(PassDeadFuncRemove::Create());

    // Patch input import and output export operations
    passMgr.add(PatchInOutImportExport::Create());

    // Patch descriptor load opertions
    passMgr.add(PatchDescriptorLoad::Create());

    // Prior to general optimization, do funcion inlining and dead function removal once again
    passMgr.add(createFunctionInliningPass(InlineThreshold));
    passMgr.add(PassDeadFuncRemove::Create());

    // Add some optimization passes
    passMgr.add(createPromoteMemoryToRegisterPass());
    passMgr.add(createSROAPass());
    passMgr.add(createLICMPass());
    passMgr.add(createAggressiveDCEPass());
    passMgr.add(createCFGSimplificationPass());
    passMgr.add(createInstructionCombiningPass());

    if (passMgr.run(*pModule) == false)
    {
        result = Result::ErrorInvalidShader;
    }

    if (result == Result::Success)
    {
        std::string errMsg;
        raw_string_ostream errStream(errMsg);
        if (verifyModule(*pModule, &errStream))
        {
            LLPC_ERRS("Fails to verify module (" DEBUG_TYPE "): " << errStream.str() << "\n");
            result = Result::ErrorInvalidShader;
        }
    }

    return result;
}

// =====================================================================================================================
// Initializes the pass according to the specified module.
//
// NOTE: This function should be called at the beginning of "runOnModule()".
void Patch::Init(
    Module* pModule) // [in] LLVM module
{
    m_pModule  = pModule;
    m_pContext = static_cast<Context*>(&m_pModule->getContext());
    m_shaderStage = GetShaderStageFromModule(m_pModule);
    m_pEntryPoint = GetEntryPoint(m_pModule);
}

} // Llpc
