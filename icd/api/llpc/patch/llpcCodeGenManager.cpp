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
 * @file  llpcCodeGenManager.cpp
 * @brief LLPC source file: contains implementation of class Llpc::CodeGenManager.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-code-gen-manager"

#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "spirv.hpp"
#include "llpcCodeGenManager.h"
#include "llpcGfx6ConfigBuilder.h"
#ifdef LLPC_BUILD_GFX9
#include "llpcGfx9ConfigBuilder.h"
#endif
#include "llpcContext.h"
#include "llpcElf.h"
#include "llpcGfx6Chip.h"
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
// Generates GPU ISA codes.
Result CodeGenManager::GenerateCode(
    Module*            pModule,   // [in] LLVM module
    raw_pwrite_stream& outStream, // [out] Output stream (ELF)
    std::string&       errMsg)    // [out] Error message reported in code generation
{
    Result result = Result::Success;

    Context* pContext = static_cast<Context*>(&pModule->getContext());

    std::string triple("amdgcn--amdpal");
    auto Target = TargetRegistry::lookupTarget(triple, errMsg);

    TargetOptions targetOpts;
    auto relocModel = Optional<Reloc::Model>();
    std::string features = "+vgpr-spilling";

    if (cl::EnablePipelineDump || EnableOuts())
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

    std::unique_ptr<TargetMachine> targetMachine(Target->createTargetMachine(triple,
                                                 pContext->GetGpuNameString(),
                                                 features,
                                                 targetOpts,
                                                 relocModel));
    pModule->setTargetTriple(triple);
    pModule->setDataLayout(targetMachine->createDataLayout());

    pContext->setDiagnosticHandler(llvm::make_unique<LlpcDiagnosticHandler>());
    legacy::PassManager passMgr;
    if (result == Result::Success)
    {
        bool success = true;
#if LLPC_ENABLE_EXCEPTION
        try
#endif
        {
            if (targetMachine->addPassesToEmitFile(passMgr, outStream, TargetMachine::CGFT_ObjectFile))
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
// Merges ELF data entries and creates a new section from them.
void CodeGenManager::CreateSectionFromDataEntry(
    const char*           pSectionName,        // [in] Section name
    uint32_t              sectionSize,         // Size of the section
    uint32_t              dataEntryCount,      // Count of ELF data entries
    const ElfDataEntry*   pDataEntries,        // [in] List of ELF data entries
    ElfWriter<Elf64>&     writer)              // [out] Elf writer
{
    if (sectionSize > 0)
    {
        // Create a new section
        uint8_t*    pBuffer      = nullptr;
        const void* pSectionData = nullptr;
        uint32_t    sectionIndex = 0;
        if (dataEntryCount > 1)
        {
            pBuffer = new uint8_t[sectionSize];
            pSectionData = pBuffer;
            for (uint32_t i = 0; i < dataEntryCount; ++i)
            {
                if (pDataEntries[i].size > 0)
                {
                    memcpy(pBuffer + pDataEntries[i].offset, pDataEntries[i].pData, pDataEntries[i].size);
                    if (pDataEntries[i].padSize > 0)
                    {
                        memset(pBuffer + pDataEntries[i].offset + pDataEntries[i].size,
                               0,
                               pDataEntries[i].padSize);
                    }
                }
            }
        }
        else
        {
            pSectionData = pDataEntries[0].pData;
        }

        writer.AddBinarySection(pSectionName, pSectionData, sectionSize, &sectionIndex);
        if (pBuffer != nullptr)
        {
            delete[] pBuffer;
        }

        // Add symbol from data entries
        for (uint32_t i = 0; i < dataEntryCount; ++i)
        {
            if (pDataEntries[i].pSymName != nullptr)
            {
                ElfSymbol sym = {};
                sym.secIdx    = sectionIndex;
                sym.pSymName  = pDataEntries[i].pSymName;
                sym.value     = pDataEntries[i].offset;
                sym.size      = pDataEntries[i].size;
                writer.AddSymbol(&sym);
            }
        }
    }
}

// =====================================================================================================================
// Finalizes ELF package for the pipeline, merging those input packages of all shader stages from code generation to
// a single output one.
Result CodeGenManager::FinalizeElf(
    Context*          pContext,     // [in] LLPC context
    const ElfPackage* pElfIns,      // [in] Input ELF packages
    uint32_t          elfInCount,   // Count of input ELF packages
    ElfPackage*       pElfOut)      // [out] Finalized output ELF package
{
    Result result = Result::Success;

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();
    uint32_t stageMask = pContext->GetShaderStageMask();

#ifdef LLPC_BUILD_GFX9
    if (gfxIp.major >= 9)
    {
        // NOTE: For GFX9+, some shader stages are merged together, we have to adjust the stage mask accordingly.
        const bool hasVs  = ((stageMask & ShaderStageToMask(ShaderStageVertex)) != 0);
        const bool hasTcs = ((stageMask & ShaderStageToMask(ShaderStageTessControl)) != 0);

        const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                          ShaderStageToMask(ShaderStageTessEval))) != 0);
        const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

        if (hasTs && (hasVs || hasTcs))
        {
            // When LS-HS merged shader is present, it is treated as tessellation control shader
            stageMask &= ~ShaderStageToMask(ShaderStageVertex);
            stageMask |= ShaderStageToMask(ShaderStageTessControl);
        }

        if (hasGs)
        {
            // When ES-GS merged shader is present, it is treated as geometry shader
            stageMask &= ~ShaderStageToMask(hasTs ? ShaderStageTessEval : ShaderStageVertex);
            stageMask |= ShaderStageToMask(ShaderStageGeometry);
        }
    }
#endif

    ElfWriter<Elf64> writer;

    std::string sectionName;
    const void* pData = nullptr;
    size_t dataLen = 0;

    // Add GPU info
    Util::Abi::AbiAmdGpuVersionNote ntHsaIsa = {};
    ntHsaIsa.vendorNameSize = sizeof(Util::Abi::AmdGpuVendorName);
    ntHsaIsa.archNameSize   = sizeof(Util::Abi::AmdGpuArchName);
    strcpy(ntHsaIsa.vendorName, Util::Abi::AmdGpuVendorName);
    strcpy(ntHsaIsa.archName, Util::Abi::AmdGpuArchName);
    ntHsaIsa.gfxipMajorVer = gfxIp.major;
    ntHsaIsa.gfxipMinorVer = gfxIp.minor;
    ntHsaIsa.gfxipStepping = gfxIp.stepping;
    writer.AddNote(Util::Abi::PipelineAbiNoteType::HsaIsa, sizeof(ntHsaIsa), &ntHsaIsa);

    // Add PAL version info
    Util::Abi::AbiMinorVersionNote ntAbiMinorVersion = {};
    ntAbiMinorVersion.minorVersion = Util::Abi::ElfAbiMinorVersion;
    writer.AddNote(Util::Abi::PipelineAbiNoteType::AbiMinorVersion, sizeof(ntAbiMinorVersion), &ntAbiMinorVersion);

    if (pContext->IsGraphics())
    {
        std::vector<ElfDataEntry> textData(elfInCount);
        std::vector<ElfDataEntry> disasmData(elfInCount);
        std::vector<ElfDataEntry> csdataData(elfInCount);
        std::vector<ElfDataEntry> configData(elfInCount);

        uint32_t textSectionSize   = 0;
        uint32_t disasmSectionSize = 0;
        uint32_t csdataSectionSize = 0;

        uint32_t elfIdx = 0;

        // NOTE: Here, we have to count copy shader in.
        for (uint32_t stage = 0; (stage < ShaderStageCountInternal) && (result == Result::Success); ++stage)
        {
            if ((stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage))) == 0)
            {
                continue;
            }

            const ElfPackage* pElfIn = &pElfIns[elfIdx];
            LLPC_ASSERT(pElfIn->size() > 0);

            ElfReader<Elf64> reader(gfxIp);

            size_t readSize = 0;
            result = reader.ReadFromBuffer(pElfIn->data(), &readSize);

            // ELF dump
            if (result == Result::Success)
            {
                LLPC_OUTS("===============================================================================\n");
                LLPC_OUTS("// LLPC backend results (" <<
                          GetShaderStageName(static_cast<ShaderStage>(stage)) << " shader)\n");
                LLPC_OUTS(reader);
            }

            // Propagate e_flags from input ELF to output ELF. All input ELFs have the same e_flags value,
            // so it does not matter that we set the e_flags for output ELF multiple times.
            writer.SetFlags(reader.GetFlags());

            // Write ".text" section
            if (result == Result::Success)
            {
                sectionName = TextName;
                pData = nullptr;
                dataLen = 0;

                result = reader.GetSectionData(sectionName.c_str(), &pData, &dataLen);
                if ((result == Result::Success) && (dataLen > 0))
                {
                    textData[elfIdx].pData = pData;
                    textData[elfIdx].size = dataLen;
                    textData[elfIdx].padSize = (dataLen % 256) ? 256 - (dataLen % 256) : 0;
                    textData[elfIdx].offset = textSectionSize;
                    textData[elfIdx].pSymName =
                        GetSymbolNameForTextSection(static_cast<ShaderStage>(stage), stageMask);
                    textSectionSize += (textData[elfIdx].size + textData[elfIdx].padSize);
                }
            }

            // Write ".AMDGPU.disasm" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuDisasmName;
                pData = nullptr;
                dataLen = 0;

                // NOTE: This section is optional and we do not care if it fails.
                if ((reader.GetSectionData(sectionName.c_str(), &pData, &dataLen) == Result::Success) &&
                    (dataLen > 0))
                {
                    disasmData[elfIdx].pData = pData;
                    disasmData[elfIdx].size = dataLen;
                    disasmData[elfIdx].padSize = 0;
                    disasmData[elfIdx].offset = disasmSectionSize;
                    disasmData[elfIdx].pSymName =
                        GetSymbolNameForDisasmSection(static_cast<ShaderStage>(stage), stageMask);
                    disasmSectionSize += dataLen;
                }
            }

            // Write ".AMDGPU.csdata" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuCsdataName;
                pData = nullptr;
                dataLen = 0;

                // NOTE: This section is optional and we do not care if it fails.
                if ((reader.GetSectionData(sectionName.c_str(), &pData, &dataLen) == Result::Success) &&
                    (dataLen > 0))
                {
                    csdataData[elfIdx].pData = pData;
                    csdataData[elfIdx].size = dataLen;
                    csdataData[elfIdx].padSize = 0;
                    csdataData[elfIdx].offset = csdataSectionSize;
                    csdataData[elfIdx].pSymName =
                        GetSymbolNameForCsdataSection(static_cast<ShaderStage>(stage), stageMask);
                    csdataSectionSize += dataLen;
                }
            }

            // Write ".AMDGPU.config" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuConfigName;
                pData = nullptr;
                dataLen = 0;

                result = reader.GetSectionData(sectionName.c_str(), &pData, &dataLen);
                if ((result == Result::Success) && (dataLen > 0))
                {
                    configData[elfIdx].pData = pData;
                    configData[elfIdx].size = dataLen;
                }
            }

            ++elfIdx;
        }

        if (result == Result::Success)
        {
            CreateSectionFromDataEntry(TextName, textSectionSize, elfInCount, textData.data(), writer);
            CreateSectionFromDataEntry(AmdGpuDisasmName, disasmSectionSize, elfInCount, disasmData.data(), writer);
            CreateSectionFromDataEntry(AmdGpuCsdataName, csdataSectionSize, elfInCount, csdataData.data(), writer);

            void* pConfig = nullptr;
            size_t configSize = 0;
            result = BuildGraphicsPipelineRegConfig(pContext, configData.data(), &pConfig, &configSize);
            writer.AddNote(Util::Abi::PipelineAbiNoteType::PalMetadata, configSize, pConfig);
            delete pConfig;
        }
    }
    else
    {
        // Compute pipeline
        LLPC_ASSERT(elfInCount == 1);
        const ElfPackage* pElfIn = &pElfIns[0];

        if (pElfIn->size() > 0)
        {
            ElfReader<Elf64> reader(gfxIp);

            ElfDataEntry   textData   = {};
            ElfDataEntry   disasmData = {};
            ElfDataEntry   csdataData = {};
            ElfDataEntry   configData = {};
            size_t         readSize   = 0;

            result = reader.ReadFromBuffer(pElfIn->data(), &readSize);

            // ELF dump
            if (result == Result::Success)
            {
                LLPC_OUTS("===============================================================================\n");
                LLPC_OUTS("// LLPC backend results (" <<
                          GetShaderStageName(ShaderStageCompute) << " shader)\n");
                LLPC_OUTS(reader);
            }

            // Write ".text" section
            if (result == Result::Success)
            {
                sectionName = TextName;
                pData = nullptr;
                dataLen = 0;

                result = reader.GetSectionData(sectionName.c_str(), &pData, &dataLen);
                if ((result == Result::Success) && (dataLen > 0))
                {
                    textData.pData = pData;
                    textData.size = dataLen;
                    textData.pSymName = GetSymbolNameForTextSection(ShaderStageCompute, stageMask);
                }
            }

            // Write ".AMDGPU.disasm" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuDisasmName;
                pData = nullptr;
                dataLen = 0;

                // NOTE: This section is optional and we do not care if it fails.
                if ((reader.GetSectionData(sectionName.c_str(), &pData, &dataLen) == Result::Success) &&
                    (dataLen > 0))
                {
                    disasmData.pData = pData;
                    disasmData.size = dataLen;
                    disasmData.pSymName = GetSymbolNameForDisasmSection(ShaderStageCompute, stageMask);
                }
            }

            // Write ".AMDGPU.csdata" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuCsdataName;
                pData = nullptr;
                dataLen = 0;

                // NOTE: This section is optional and we do not care if it fails.
                if ((reader.GetSectionData(sectionName.c_str(), &pData, &dataLen) == Result::Success) &&
                    (dataLen > 0))
                {
                    csdataData.pData = pData;
                    csdataData.size = dataLen;
                    csdataData.pSymName = GetSymbolNameForCsdataSection(ShaderStageCompute, stageMask);
                }
            }

            // Write ".AMDGPU.config" section
            if (result == Result::Success)
            {
                sectionName = AmdGpuConfigName;
                pData = nullptr;
                dataLen = 0;

                result = reader.GetSectionData(sectionName.c_str(), &pData, &dataLen);
                if ((result == Result::Success) && (dataLen > 0))
                {
                    configData.pData = pData;
                    configData.size = dataLen;
                }
            }

            if (result == Result::Success)
            {
                CreateSectionFromDataEntry(TextName, textData.size, 1, &textData, writer);
                CreateSectionFromDataEntry(AmdGpuDisasmName, disasmData.size, 1, &disasmData, writer);
                CreateSectionFromDataEntry(AmdGpuCsdataName, csdataData.size, 1, &csdataData, writer);

                void* pConfig = nullptr;
                size_t configSize = 0;
                result = BuildComputePipelineRegConfig(pContext, &configData, &pConfig, &configSize);
                writer.AddNote(Util::Abi::PipelineAbiNoteType::PalMetadata, configSize, pConfig);
                delete pConfig;
            }
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }

    const size_t reqSize = writer.GetRequiredBufferSizeBytes();
    pElfOut->resize(reqSize);
    writer.WriteToBuffer(pElfOut->data(), reqSize);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline.
//
// NOTE: This function will create pipeline register configuration. The caller has the responsibility of destroying it.
Result CodeGenManager::BuildGraphicsPipelineRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntries,   // [in] ELF data entries
    void**              ppConfig,       // [out] Register configuration for VS-FS pipeline
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
            result = Gfx6::ConfigBuilder::BuildPipelineVsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
        }
        else
        {
#ifdef LLPC_BUILD_GFX9
            result = Gfx9::ConfigBuilder::BuildPipelineVsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
#else
            result = Result::Unsupported;
#endif
        }
    }
    else if (hasTs && (hasGs == false))
    {
        // VS-TS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsTsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
        }
        else
        {
#ifdef LLPC_BUILD_GFX9
            result = Gfx9::ConfigBuilder::BuildPipelineVsTsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
#else
            result = Result::Unsupported;
#endif
        }
    }
    else if ((hasTs == false) && hasGs)
    {
        // VS-GS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsGsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
        }
        else
        {
#ifdef LLPC_BUILD_GFX9
            result = Gfx9::ConfigBuilder::BuildPipelineVsGsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
#else
            result = Result::Unsupported;
#endif
        }
    }
    else
    {
        // VS-TS-GS-FS pipeline
        if (gfxIp.major <= 8)
        {
            result = Gfx6::ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
        }
        else
        {
#ifdef LLPC_BUILD_GFX9
            result = Gfx9::ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(pContext, pDataEntries, ppConfig, pConfigSize);
#else
            result = Result::Unsupported;
#endif
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
    const ElfDataEntry* pDataEntry,   // [in] ELF data entry
    void**              ppConfig,     // [out] Register configuration for compute pipeline
    size_t*             pConfigSize)  // [out] Size of register configuration
{
    Result result = Result::Success;

    GfxIpVersion gfxIp =pContext->GetGfxIpVersion();
    if (gfxIp.major <= 8)
    {
        result = Gfx6::ConfigBuilder::BuildPipelineCsRegConfig(pContext, pDataEntry, ppConfig, pConfigSize);
    }
    else
    {
#ifdef LLPC_BUILD_GFX9
        result = Gfx9::ConfigBuilder::BuildPipelineCsRegConfig(pContext, pDataEntry, ppConfig, pConfigSize);
#else
        result = Result::Unsupported;
#endif
    }

    return result;
}

} // Llpc
