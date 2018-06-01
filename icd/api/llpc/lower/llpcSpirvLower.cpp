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
 * @file  llpcSpirvLower.cpp
 * @brief LLPC source file: contains implementation of class Llpc::SpirvLower.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-spirv-lower"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/IPO.h"

#include "llpcContext.h"
#include "llpcPassDeadFuncRemove.h"
#include "llpcPassExternalLibLink.h"
#include "llpcSpirvLower.h"
#include "llpcSpirvLowerAccessChain.h"
#include "llpcSpirvLowerAggregateLoadStore.h"
#include "llpcSpirvLowerAlgebraTransform.h"
#include "llpcSpirvLowerBufferOp.h"
#include "llpcSpirvLowerConstImmediateStore.h"
#include "llpcSpirvLowerDynIndex.h"
#include "llpcSpirvLowerGlobal.h"
#include "llpcSpirvLowerImageOp.h"
#include "llpcSpirvLowerOpt.h"
#include "llpcSpirvLowerResourceCollect.h"

using namespace llvm;

namespace llvm
{

namespace cl
{

// -lower-dyn-index: lower SPIR-V dynamic (non-constant) index in access chain
static opt<bool> LowerDynIndex("lower-dyn-index", desc("Lower SPIR-V dynamic (non-constant) index in access chain"));

// -disable-lower-opt: disable optimization for SPIR-V lowering
static opt<bool> DisableLowerOpt("disable-lower-opt", desc("Disable optimization for SPIR-V lowering"));

extern opt<bool> EnableDumpCfg;

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
// Executes various passes that do SPIR-V lowering opertions for LLVM module.
Result SpirvLower::Run(
    Module* pModule)    // [in,out] LLVM module to be run on
{
    Result result = Result::Success;
    Context* pContext = static_cast<Context*>(&pModule->getContext());

    if (cl::EnableDumpCfg)
    {
        DumpCfg("Original", pModule);
    }

    legacy::PassManager passMgr;

    // Lower SPIR-V resource collecting
    passMgr.add(SpirvLowerResourceCollect::Create());

    // Lower SPIR-V access chain
    passMgr.add(SpirvLowerAccessChain::Create());

    // Link external native library for constant folding
    passMgr.add(PassExternalLibLink::Create(pContext->GetNativeGlslEmuLibrary()));
    passMgr.add(PassDeadFuncRemove::Create());

    // Function inlining
    passMgr.add(createFunctionInliningPass(InlineThreshold));
    passMgr.add(PassDeadFuncRemove::Create());

    // Lower SPIR-V buffer operations (load and store)
    passMgr.add(SpirvLowerBufferOp::Create());

    // Lower SPIR-V global variables, inputs, and outputs
    passMgr.add(SpirvLowerGlobal::Create());

    // Lower SPIR-V constant immediate store.

    // TODO: Only enable it for GFX8+ because backend compiler has an issue.
    auto gfxIp = static_cast<Context*>(&pModule->getContext())->GetGfxIpVersion();
    if (gfxIp.major >= 8)
    {
        passMgr.add(SpirvLowerConstImmediateStore::Create());
    }

    // Lower SPIR-V dynamic index in access chain
    if (cl::LowerDynIndex)
    {
        passMgr.add(SpirvLowerDynIndex::Create());
    }

    // General optimization in lower phase
    if (cl::DisableLowerOpt == false)
    {
        passMgr.add(SpirvLowerOpt::Create());
    }

    // Lower SPIR-V algebraic transforms
    passMgr.add(SpirvLowerAlgebraTransform::Create());

    // Lower SPIR-V load/store operations on aggregate type
    passMgr.add(SpirvLowerAggregateLoadStore::Create());

    // Lower SPIR-V image operations (sample, fetch, gather, read/write),
    // NOTE: It is dependent on optimization result, should be after optimization pass.
    passMgr.add(SpirvLowerImageOp::Create());

    if (passMgr.run(*pModule) == false)
    {
        result = Result::ErrorInvalidShader;
    }

    if (result == Result::Success)
    {
        if (cl::EnableDumpCfg)
        {
            DumpCfg("Lowered", pModule);
        }

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
void SpirvLower::Init(
    Module* pModule) // [in] LLVM module
{
    m_pModule  = pModule;
    m_pContext = static_cast<Context*>(&m_pModule->getContext());
    m_shaderStage = GetShaderStageFromModule(m_pModule);
    m_pEntryPoint = GetEntryPoint(m_pModule);
}

} // Llpc
