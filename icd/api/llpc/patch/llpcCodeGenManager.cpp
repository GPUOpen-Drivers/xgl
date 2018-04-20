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
 * @file  llpcCodeGenManager.cpp
 * @brief LLPC source file: contains implementation of class Llpc::CodeGenManager.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-code-gen-manager"

#include "llvm/Bitcode/BitcodeWriter.h"
#if XGL_LLVM_UPSTREAM == 1
#include "llvm/CodeGen/CommandFlags.inc"
#else
#include "llvm/CodeGen/CommandFlags.def"
#endif
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "spirv.hpp"
#include "llpcCodeGenManager.h"
#include "llpcContext.h"
#include "llpcElf.h"
#include "llpcGfx6ConfigBuilder.h"
#include "llpcGfx9ConfigBuilder.h"
#include "llpcInternal.h"

namespace llvm
{

namespace cl
{

// -enable-pipeline-dump: enable pipeline info dump
extern opt<bool> EnablePipelineDump;

// -enable-si-scheduler: enable target option si-scheduler
static opt<bool> EnableSiScheduler("enable-si-scheduler",
                                   desc("Enable target option si-scheduler"),
                                   init(false));

// -disable-fp32-denormals: disable target option fp32-denormals
static opt<bool> DisableFp32Denormals("disable-fp32-denormals",
                                      desc("Disable target option fp32-denormals"),
                                      init(false));

// -emit-llvm: emit LLVM bitcode instead of ISA
static opt<bool> EmitLlvm("emit-llvm",
                          desc("Emit LLVM bitcode instead of AMD GPU ISA"),
                          init(false));

} // cl

} // llvm

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// Handler for diagnosis in code generation, derived from the standard one.
class LlpcDiagnosticHandler: public llvm::DiagnosticHandler
{
    bool handleDiagnostics(const DiagnosticInfo &diagInfo) override
    {
        if (EnableOuts() || EnableErrs())
        {
            if ((diagInfo.getSeverity() == DS_Error) || (diagInfo.getSeverity() == DS_Warning))
            {
                DiagnosticPrinterRawOStream printStream(outs());
                printStream << "ERROR: LLVM DIAGNOSIS INFO: ";
                diagInfo.print(printStream);
                printStream << "\n";
                outs().flush();
            }
            else if (EnableOuts())
            {
                DiagnosticPrinterRawOStream printStream(outs());
                printStream << "\n=====  LLVM DIAGNOSIS START  =====\n\n";
                diagInfo.print(printStream);
                printStream << "\n\n=====  LLVM DIAGNOSIS END  =====\n\n";
                outs().flush();
            }
        }
        LLPC_ASSERT(diagInfo.getSeverity() != DS_Error);
        return true;
    }
};

// =====================================================================================================================
// Creates the TargetMachine if not already created, and stores it in the context. It then persists as long as
// the context.
Result CodeGenManager::CreateTargetMachine(
    Context*           pContext)  // [in/out] Pipeline context
{
    auto pPipelineOptions = pContext->GetPipelineContext()->GetPipelineOptions();
    if ((pContext->GetTargetMachine() != nullptr) &&
        (pPipelineOptions->includeDisassembly == pContext->GetTargetMachinePipelineOptions()->includeDisassembly))
    {
        return Result::Success;
    }

    Result result = Result::ErrorInvalidShader;

    std::string triple("amdgcn--amdpal");

    std::string errMsg;
    auto pTarget = TargetRegistry::lookupTarget(triple, errMsg);
    if (pTarget != nullptr)
    {
        TargetOptions targetOpts;
        auto relocModel = Optional<Reloc::Model>();
        std::string features = "+vgpr-spilling";

        if (cl::EnablePipelineDump ||
            EnableOuts() ||
            pPipelineOptions->includeDisassembly)
        {
            features += ",+DumpCode";
        }

        if (cl::EnableSiScheduler)
        {
            features += ",+si-scheduler";
        }

        if (cl::DisableFp32Denormals)
        {
            features += ",-fp32-denormals";
        }

        // Allow no signed zeros - this enables omod modifiers (div:2, mul:2)
        targetOpts.NoSignedZerosFPMath = true;

        auto pTargetMachine = pTarget->createTargetMachine(triple,
                                                           pContext->GetGpuNameString(),
                                                           features,
                                                           targetOpts,
                                                           relocModel);
        if (pTargetMachine != nullptr)
        {
            pContext->SetTargetMachine(pTargetMachine, pPipelineOptions);
            result = Result::Success;
        }
    }
    if (result != Result::Success)
    {
        LLPC_ERRS("Fails to create AMDGPU target machine: " << errMsg << "\n");
    }

    return result;
}

// =====================================================================================================================
// Generates GPU ISA codes (ELF binary, ISA assembly text, or LLVM bitcode, depending on "-filetype" and
// "-emit-llvm" options)
Result CodeGenManager::GenerateCode(
    Module*            pModule,   // [in] LLVM module
    raw_pwrite_stream& outStream, // [out] Output stream
    std::string&       errMsg)    // [out] Error message reported in code generation
{
    Result result = Result::Success;

    Context* pContext = static_cast<Context*>(&pModule->getContext());

    result = AddAbiMetadata(pContext, pModule);

    if (cl::EmitLlvm)
    {
        WriteBitcodeToFile(*pModule, outStream);
        return result;
    }

    auto pTargetMachine = pContext->GetTargetMachine();

    pContext->setDiagnosticHandler(llvm::make_unique<LlpcDiagnosticHandler>());
    legacy::PassManager passMgr;
    if (result == Result::Success)
    {
        bool success = true;
#if LLPC_ENABLE_EXCEPTION
        try
#endif
        {
            if (pTargetMachine->addPassesToEmitFile(passMgr, outStream, FileType))
            {
                success = false;
            }
        }
#if LLPC_ENABLE_EXCEPTION
        catch (const char*)
        {
            success = false;
        }
#endif
        if (success == false)
        {
            LLPC_ERRS("Target machine cannot emit a file of this type\n");
            result = Result::ErrorInvalidValue;
        }
    }

    if (result == Result::Success)
    {
        DEBUG(dbgs() << "Start code generation: \n"<< *pModule);

        bool success = false;
#if LLPC_ENABLE_EXCEPTION
        try
#endif
        {
            success = passMgr.run(*pModule);
        }
#if LLPC_ENABLE_EXCEPTION
        catch (const char*)
        {
            success = false;
        }
#endif

        if (success == false)
        {
            LLPC_ERRS("LLVM back-end fail to generate codes\n");
            result = Result::ErrorInvalidShader;
        }
    }

    pContext->setDiagnosticHandlerCallBack(nullptr);
    return result;
}

// =====================================================================================================================
// Adds metadata (not from code generation) required by PAL ABI.
Result CodeGenManager::AddAbiMetadata(
    Context*  pContext, // [in] LLPC context
    Module*   pModule)  // [in] LLVM module
{
    uint8_t* pConfig = nullptr;
    size_t configSize = 0;

    Result result = Result::Success;
    if (pContext->IsGraphics())
    {
        result = BuildGraphicsPipelineRegConfig(pContext, &pConfig, &configSize);
    }
    else
    {
        result = BuildComputePipelineRegConfig(pContext, &pConfig, &configSize);
    }
    if (result == Result::Success)
    {
        std::vector<Metadata*> abiMeta;
        for (size_t i = 0; i < configSize / sizeof(uint32_t); ++i)
        {
            abiMeta.push_back(ConstantAsMetadata::get(ConstantInt::get(
                    pContext->Int32Ty(), (reinterpret_cast<uint32_t*>(pConfig))[i], false)));
        }
        auto pAbiMetaTuple = MDTuple::get(*pContext, abiMeta);
        auto pAbiMetaNode = pModule->getOrInsertNamedMetadata("amdgpu.pal.metadata");
        pAbiMetaNode->addOperand(pAbiMetaTuple);
        delete[] pConfig;
    }
    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline.
//
// NOTE: This function will create pipeline register configuration. The caller has the responsibility of destroying it.
Result CodeGenManager::BuildGraphicsPipelineRegConfig(
    Context*            pContext,       // [in] LLPC context
    uint8_t**           ppConfig,       // [out] Register configuration for VS-FS pipeline
    size_t*             pConfigSize)    // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    if ((hasTs == false) && (hasGs == false))
    {
        // VS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
        else
        {
            result = Gfx9::ConfigBuilder::BuildPipelineVsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
    }
    else if (hasTs && (hasGs == false))
    {
        // VS-TS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsTsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
        else
        {
            result = Gfx9::ConfigBuilder::BuildPipelineVsTsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
    }
    else if ((hasTs == false) && hasGs)
    {
        // VS-GS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsGsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
        else
        {
            result = Gfx9::ConfigBuilder::BuildPipelineVsGsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
    }
    else
    {
        // VS-TS-GS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
        else
        {
            result = Gfx9::ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(pContext, ppConfig, pConfigSize);
        }
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for computer pipeline.
//
// NOTE: This function will create pipeline register configuration. The caller has the responsibility of destroying it.
Result CodeGenManager::BuildComputePipelineRegConfig(
    Context*            pContext,     // [in] LLPC context
    uint8_t**           ppConfig,     // [out] Register configuration for compute pipeline
    size_t*             pConfigSize)  // [out] Size of register configuration
{
    Result result = Result::Success;

    GfxIpVersion gfxIp =pContext->GetGfxIpVersion();
    if (gfxIp.major <= 8)
    {
        result = Gfx6::ConfigBuilder::BuildPipelineCsRegConfig(pContext, ppConfig, pConfigSize);
    }
    else
    {
        result = Gfx9::ConfigBuilder::BuildPipelineCsRegConfig(pContext, ppConfig, pConfigSize);
    }

    return result;
}

} // Llpc
