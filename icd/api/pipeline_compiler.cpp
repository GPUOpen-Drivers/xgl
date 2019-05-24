/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  pipeline_compiler.cpp
 * @brief Contains implementation of Vulkan pipeline compiler
 ***********************************************************************************************************************
 */

#include "include/pipeline_compiler.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_graphics_pipeline.h"
#include <vector>

#include "palFile.h"
#include "palHashSetImpl.h"

#include "palPipelineAbiProcessorImpl.h"

#include <inttypes.h>

namespace vk
{

extern bool IsSrcAlphaUsedInBlend(VkBlendFactor blend);

// =====================================================================================================================
PipelineCompiler::PipelineCompiler(
    PhysicalDevice* pPhysicalDevice)
    :
    m_pPhysicalDevice(pPhysicalDevice)
    , m_compilerSolutionLlpc(pPhysicalDevice)
{

}

// =====================================================================================================================
PipelineCompiler::~PipelineCompiler()
{
}

// =====================================================================================================================
// Initializes pipeline compiler.
VkResult PipelineCompiler::Initialize()
{
    Pal::IDevice* pPalDevice        = m_pPhysicalDevice->PalDevice();
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    // Initialize GfxIp informations per PAL device properties
    Pal::DeviceProperties info;
    pPalDevice->GetProperties(&info);

    switch (info.gfxLevel)
    {
    case Pal::GfxIpLevel::GfxIp6:
        m_gfxIp.major = 6;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp7:
        m_gfxIp.major = 7;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp8:
        m_gfxIp.major = 8;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp8_1:
        m_gfxIp.major = 8;
        m_gfxIp.minor = 1;
        break;
    case Pal::GfxIpLevel::GfxIp9:
        m_gfxIp.major = 9;
        m_gfxIp.minor = 0;
        break;

    default:
        VK_NEVER_CALLED();
        break;
    }

    m_gfxIp.stepping = info.gfxStepping;

    // Create compiler objects
    VkResult result = VK_SUCCESS;

    if (result == VK_SUCCESS)
    {
        result = m_compilerSolutionLlpc.Initialize();
    }

    return result;
}

// =====================================================================================================================
// Destroys all compiler instance.
void PipelineCompiler::Destroy()
{
    m_compilerSolutionLlpc.Destroy();

}

// =====================================================================================================================
// Creates shader cache object.
VkResult PipelineCompiler::CreateShaderCache(
    const void*   pInitialData,
    size_t        initialDataSize,
    void*         pShaderCacheMem,
    ShaderCache*  pShaderCache)
{
    VkResult                     result         = VK_SUCCESS;
    PipelineCompilerType         cacheType      = GetShaderCacheType();

    {
        result = m_compilerSolutionLlpc.CreateShaderCache(pInitialData,
            initialDataSize, pShaderCacheMem, false, pShaderCache);
    }

    return result;
}

// =====================================================================================================================
// Gets the size of shader cache object.
size_t PipelineCompiler::GetShaderCacheSize(
    PipelineCompilerType cacheType)
{
    size_t shaderCacheSize = 0;
    return shaderCacheSize;
}

// =====================================================================================================================
// Gets shader cache type.
PipelineCompilerType PipelineCompiler::GetShaderCacheType()
{
    PipelineCompilerType cacheType;
    cacheType = PipelineCompilerTypeLlpc;
    return cacheType;
}

// =====================================================================================================================
// Loads shader binary  from replace shader folder with specified shader hash code.
bool PipelineCompiler::LoadReplaceShaderBinary(
    uint64_t shaderHash,
    size_t*  pCodeSize,
    void**   ppCode)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    const RuntimeSettings* pSettings = &m_pPhysicalDevice->GetRuntimeSettings();
    bool findShader = false;

    char replaceFileName[4096] = {};
    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/Shader_0x%016llX_replace.spv",
        pSettings->shaderReplaceDir, shaderHash);

    Util::File replaceFile;
    if (replaceFile.Open(replaceFileName, Util::FileAccessRead | Util::FileAccessBinary) == Util::Result::Success)
    {
        size_t replaceCodeSize = replaceFile.GetFileSize(replaceFileName);
        auto pReplaceCode = pInstance->AllocMem(replaceCodeSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        size_t readBytes = 0;
        replaceFile.Read(pReplaceCode, replaceCodeSize, &readBytes);
        VK_ASSERT(readBytes == replaceCodeSize);

        *ppCode = pReplaceCode;
        *pCodeSize = replaceCodeSize;
        findShader = true;
    }

    return findShader;
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult PipelineCompiler::BuildShaderModule(
    const Device*       pDevice,
    size_t              codeSize,
    const void*         pCode,
    ShaderModuleHandle* pShaderModule)
{
    const RuntimeSettings* pSettings = &m_pPhysicalDevice->GetRuntimeSettings();
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    VkResult result = VK_SUCCESS;
    uint32_t compilerMask = GetCompilerCollectionMask();
    Util::MetroHash::Hash hash = {};
    Util::MetroHash64::Hash(reinterpret_cast<const uint8_t*>(pCode), codeSize, hash.bytes);
    bool findReplaceShader = false;

    if (pSettings->shaderReplaceMode == ShaderReplaceShaderHash)
    {
        size_t replaceCodeSize = 0;
        void* pReplaceCode = nullptr;
        uint64_t hash64 = Util::MetroHash::Compact64(&hash);
        findReplaceShader = LoadReplaceShaderBinary(hash64, &replaceCodeSize, &pReplaceCode);
        if (findReplaceShader)
        {
            pCode = pReplaceCode;
            codeSize = replaceCodeSize;
        }
    }

    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        result = m_compilerSolutionLlpc.BuildShaderModule(pDevice, codeSize, pCode, pShaderModule, hash);
    }

    if (findReplaceShader)
    {
        pInstance->FreeMem(const_cast<void*>(pCode));
    }
    return result;
}

// =====================================================================================================================
// Frees shader module memory
void PipelineCompiler::FreeShaderModule(
    ShaderModuleHandle* pShaderModule)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    m_compilerSolutionLlpc.FreeShaderModule(pShaderModule);
}

// =====================================================================================================================
// Replaces pipeline binary from external replacment file (<pipeline_name>_repalce.elf)
template<class PipelineBuildInfo>
bool PipelineCompiler::ReplacePipelineBinary(
        const PipelineBuildInfo* pPipelineBuildInfo,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary)
{
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    char fileName[128];
    Llpc::IPipelineDumper::GetPipelineName(pPipelineBuildInfo, fileName, 128);

    char replaceFileName[256];
    int32_t length = Util::Snprintf(replaceFileName, 256, "%s/%s_replace.elf", settings.shaderReplaceDir, fileName);
    VK_ASSERT(length > 0 && (static_cast<uint32_t>(length) < sizeof(replaceFileName)));

    Util::Result result = Util::File::Exists(replaceFileName) ? Util::Result::Success : Util::Result::ErrorUnavailable;
    if (result == Util::Result::Success)
    {
        Util::File elfFile;
        result = elfFile.Open(replaceFileName, Util::FileAccessRead | Util::FileAccessBinary);
        if (result == Util::Result::Success)
        {
            size_t binSize = Util::File::GetFileSize(replaceFileName);
            void *pAllocBuf = pInstance->AllocMem(
                binSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            elfFile.Read(pAllocBuf, binSize, nullptr);

            *pPipelineBinarySize = binSize;
            *ppPipelineBinary = pAllocBuf;
            return true;
        }
    }
    return false;
}

// =====================================================================================================================
// Replaces shader module data in the input PipelineShaderInfo
bool PipelineCompiler::ReplacePipelineShaderModule(
    const Device*             pDevice,
    PipelineCompilerType      compilerType,
    Llpc::PipelineShaderInfo* pShaderInfo,
    ShaderModuleHandle*       pShaderModule)
{
    bool replaced = false;
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pShaderInfo->pModuleData != nullptr)
    {
        uint64_t hash64 = Llpc::IPipelineDumper::GetShaderHash(pShaderInfo->pModuleData);
        size_t codeSize = 0;
        void* pCode = nullptr;

        if (LoadReplaceShaderBinary(hash64, &codeSize, &pCode))
        {
            VkResult result = BuildShaderModule(pDevice, codeSize, pCode, pShaderModule);
            if (result == VK_SUCCESS)
            {
                pShaderInfo->pModuleData = ShaderModule::GetShaderData(compilerType, pShaderModule);
                replaced = true;
            }

            pInstance->FreeMem(pCode);
        }
    }

    return replaced;
}

// =====================================================================================================================
// Drop pipeline binary instruction.
void PipelineCompiler::DropPipelineBinaryInst(
    Device*                pDevice,
    const RuntimeSettings& settings,
    const void*            pPipelineBinary,
    size_t                 pipelineBinarySize)
{
    if (settings.enableDropPipelineBinaryInst == true)
    {
        const void* pPipelineCode = nullptr;
        size_t pipelineCodeSize = 0;

        Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());
        Pal::Result palResult = abiProcessor.LoadFromBuffer(pPipelineBinary, pipelineBinarySize);
        if (palResult == Pal::Result::Success)
        {
            abiProcessor.GetPipelineCode(&pPipelineCode, &pipelineCodeSize);
        }

        VK_ASSERT(pPipelineCode != nullptr);
        VK_ASSERT(pipelineCodeSize > 0);

        uint32_t firstInstruction = *(uint32_t*)pPipelineCode;
        uint32_t* pFirstInstruction = nullptr;

        for (uint32_t i = 0; i <= pipelineBinarySize - sizeof(uint32_t); i++)
        {
            uint32_t* p = reinterpret_cast<uint32_t*>(((uint8_t*)pPipelineBinary) + i);

            if (*p == firstInstruction)
            {
                pFirstInstruction = p;
                break;
            }
        }

        VK_ASSERT(pFirstInstruction != nullptr);
        VK_ASSERT(settings.dropPipelineBinaryInstSize > 0);

        uint32_t refValue = settings.dropPipelineBinaryInstToken & settings.dropPipelineBinaryInstMask;
        static constexpr uint32_t Nop = 0xBF800000;   // ISA code for NOP instruction

        for (uint32_t i = 0; i <= pipelineCodeSize/sizeof(uint32_t) - settings.dropPipelineBinaryInstSize; i++)
        {
            if ((pFirstInstruction[i] & settings.dropPipelineBinaryInstMask) == refValue)
            {
                for (uint32_t j = 0; j < settings.dropPipelineBinaryInstSize; j++)
                {
                    pFirstInstruction[i+j] = Nop;
                }

                i += (settings.dropPipelineBinaryInstSize - 1);
            }
        }
    }
}

// =====================================================================================================================
// Replace pipeline binary instruction.
void PipelineCompiler::ReplacePipelineIsaCode(
    Device *     pDevice,
    uint64_t     pipelineHash,
    const void*  pPipelineBinary,
    size_t pipelineBinarySize)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    char replaceFileName[256];
    int32_t length = Util::Snprintf(replaceFileName, 256, "%s/" "0x%016" PRIX64 "_replace.txt",
        settings.shaderReplaceDir, pipelineHash);

    Util::File isaCodeFile;
    if (isaCodeFile.Open(replaceFileName, Util::FileAccessRead) != Util::Result::Success)
    {
        // skip replacement if fail to replace file
        return;
    }

    const void* pPipelineCode = nullptr;
    size_t pipelineCodeSize = 0;

    Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());
    Pal::Result palResult = abiProcessor.LoadFromBuffer(pPipelineBinary, pipelineBinarySize);
    if (palResult == Pal::Result::Success)
    {
        // the elf .text section ISA shader code
        abiProcessor.GetPipelineCode(&pPipelineCode, &pipelineCodeSize);
    }
    else
    {
        return;
    }

    uint32_t firstInstruction = *(uint32_t*)pPipelineCode;
    uint8_t* pFirstInstruction = nullptr;

    // Move the pipeline elf pointer to the .text section "shader isa"
    for (uint32_t i = 0; i <= pipelineBinarySize - sizeof(uint32_t); i++)
    {
        uint8_t* p = ((uint8_t*)pPipelineBinary) + i;

        if (*reinterpret_cast<uint32_t*>(p) == firstInstruction)
        {
            pFirstInstruction = p;
            break;
        }
    }

    std::vector<Util::Abi::PipelineSymbolEntry> shaderStageSymbols;
    Util::Abi::PipelineSymbolType stageSymbolTypes[] =
    {
        Util::Abi::PipelineSymbolType::LsMainEntry,
        Util::Abi::PipelineSymbolType::HsMainEntry,
        Util::Abi::PipelineSymbolType::EsMainEntry,
        Util::Abi::PipelineSymbolType::GsMainEntry,
        Util::Abi::PipelineSymbolType::VsMainEntry,
        Util::Abi::PipelineSymbolType::PsMainEntry,
        Util::Abi::PipelineSymbolType::CsMainEntry
    };
    Util::Abi::PipelineSymbolEntry symbol;
    for (const auto& simbolTypeEntry : stageSymbolTypes)
    {
        if (abiProcessor.HasPipelineSymbolEntry(simbolTypeEntry, &symbol))
        {
            shaderStageSymbols.push_back(symbol);
        }
    }
    /* modified code in the 0xAAA_replace.txt looks like
        848:0x7E120303
        1480:0x7E1E0303
        2592:0x7E0E030E
    */
    char codeLine[256] = {};
    char codeOffset[256] = {};
    while ((isaCodeFile.ReadLine(codeLine, sizeof(char) * 256, nullptr) == Util::Result::Success))
    {
        const char* offsetEnd = strchr(codeLine, ':');
        strncpy(codeOffset, codeLine, offsetEnd - codeLine);
        codeOffset[offsetEnd - codeLine] = '\0';
        uint32_t offset = strtoul(codeOffset, nullptr, 10);
        bool inRange = false;
        for (const auto& simboEntry : shaderStageSymbols)
        {
            if ((offset >= simboEntry.value) && (offset < simboEntry.value + simboEntry.size))
            {
                inRange = true;
                break;
            }
        }
        VK_ASSERT(inRange);
        offsetEnd++;
        uint8_t* pCode = pFirstInstruction + offset;
        uint32_t replaceCode = strtoul(offsetEnd, nullptr, 16);
        *reinterpret_cast<uint32_t*>(pCode) = replaceCode;
    }

}

// =====================================================================================================================
// Creates graphics pipeline binary.
VkResult PipelineCompiler::CreateGraphicsPipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    GraphicsPipelineCreateInfo*         pCreateInfo,
    size_t*                             pPipelineBinarySize,
    const void**                        ppPipelineBinary,
    uint32_t                            rasterizationStream,
    Util::MetroHash::Hash*              pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    bool                   shouldCompile = true;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance     = m_pPhysicalDevice->Manager()->VkInstance();

    int64_t compileTime = 0;
    uint64_t pipelineHash = Llpc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* moduleDataBaks[ShaderGfxStageCount];
    ShaderModuleHandle shaderModuleReplaceHandles[ShaderGfxStageCount];
    bool shaderModuleReplaced = false;

    Llpc::PipelineShaderInfo* shaderInfos[ShaderGfxStageCount] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    if (settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash)
    {
        if (ReplacePipelineBinary(&pCreateInfo->pipelineInfo, pPipelineBinarySize, ppPipelineBinary))
        {
            shouldCompile = false;
        }
    }
    else if (settings.shaderReplaceMode == ShaderReplaceShaderPipelineHash)
    {
        char pipelineHashString[64];
        Util::Snprintf(pipelineHashString, 64, "0x%016" PRIX64, pipelineHash);

        if (strstr(settings.shaderReplacePipelineHashes, pipelineHashString) != nullptr)
        {
            memset(shaderModuleReplaceHandles, 0, sizeof(shaderModuleReplaceHandles));
            for (uint32_t i = 0; i < ShaderGfxStageCount; ++i)
            {
                moduleDataBaks[i] = shaderInfos[i]->pModuleData;
                shaderModuleReplaced |= ReplacePipelineShaderModule(pDevice,
                    pCreateInfo->compilerType, shaderInfos[i], &shaderModuleReplaceHandles[i]);
            }

            if (shaderModuleReplaced)
            {
                pipelineHash = Llpc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
            }
        }
    }

    if (settings.enablePipelineDump)
    {
        Llpc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir                 = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines    = settings.dumpDuplicatePipelines;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
        Llpc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Llpc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo);
#else
        pPipelineDumpHandle = Llpc::IPipelineDumper::BeginPipelineDump(&dumpOptions, nullptr, &pCreateInfo->pipelineInfo);
#endif
    }

    if ((pCreateInfo->compilerType == PipelineCompilerTypeLlpc) && shouldCompile)
    {
        result = m_compilerSolutionLlpc.CreateGraphicsPipelineBinary(pDevice, deviceIdx, pPipelineCache, pCreateInfo,
            pPipelineBinarySize, ppPipelineBinary, rasterizationStream, shaderInfos, pPipelineDumpHandle,
            pipelineHash, &compileTime);
    }

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, *ppPipelineBinary, *pPipelineBinarySize);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Llpc::BinaryData pipelineBinary = {};
            pipelineBinary.codeSize = *pPipelineBinarySize;
            pipelineBinary.pCode = *ppPipelineBinary;
            Llpc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineBinary);
        }
        Llpc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }

    if (shaderModuleReplaced)
    {
        for (uint32_t i = 0; i < ShaderGfxStageCount; ++i)
        {
            shaderInfos[i]->pModuleData = moduleDataBaks[i];
            FreeShaderModule(&shaderModuleReplaceHandles[i]);
        }
    }

    DropPipelineBinaryInst(pDevice, settings, *ppPipelineBinary, *pPipelineBinarySize);

    return result;
}

// =====================================================================================================================
// Creates compute pipeline binary.
VkResult PipelineCompiler::CreateComputePipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    ComputePipelineCreateInfo*          pCreateInfo,
    size_t*                             pPipelineBinarySize,
    const void**                        ppPipelineBinary,
    Util::MetroHash::Hash*              pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance     = m_pPhysicalDevice->Manager()->VkInstance();
    bool                   shouldCompile = true;

    pCreateInfo->pipelineInfo.deviceIndex = deviceIdx;

    int64_t compileTime = 0;
    uint64_t pipelineHash = Llpc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* pModuleDataBak = nullptr;
    ShaderModuleHandle shaderModuleReplaceHandle = {};
    bool shaderModuleReplaced = false;

    if (settings.enablePipelineDump)
    {
        Llpc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir                 = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines    = settings.dumpDuplicatePipelines;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
        Llpc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pComputeInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Llpc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo);
#else
        pPipelineDumpHandle = Llpc::IPipelineDumper::BeginPipelineDump(&dumpOptions, &pCreateInfo->pipelineInfo, nullptr);
#endif
    }

    if (settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash)
    {
        if (ReplacePipelineBinary(&pCreateInfo->pipelineInfo, pPipelineBinarySize, ppPipelineBinary))
        {
            shouldCompile = false;
        }
    }
    else if (settings.shaderReplaceMode == ShaderReplaceShaderPipelineHash)
    {
        char pipelineHashString[64];
        Util::Snprintf(pipelineHashString, 64, "0x%016" PRIX64, pipelineHash);

        if (strstr(settings.shaderReplacePipelineHashes, pipelineHashString) != nullptr)
        {
            pModuleDataBak = pCreateInfo->pipelineInfo.cs.pModuleData;
            shaderModuleReplaced = ReplacePipelineShaderModule(
                pDevice, pCreateInfo->compilerType, &pCreateInfo->pipelineInfo.cs, &shaderModuleReplaceHandle);

            if (shaderModuleReplaced)
            {
                pipelineHash = Llpc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
            }
        }
    }

    if ((pCreateInfo->compilerType == PipelineCompilerTypeLlpc) && shouldCompile)
    {
        result = m_compilerSolutionLlpc.CreateComputePipelineBinary(pDevice, deviceIdx, pPipelineCache, pCreateInfo,
            pPipelineBinarySize, ppPipelineBinary, pPipelineDumpHandle, pipelineHash, &compileTime);
    }

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, *ppPipelineBinary, *pPipelineBinarySize);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Llpc::BinaryData pipelineBinary = {};
            pipelineBinary.codeSize = *pPipelineBinarySize;
            pipelineBinary.pCode = *ppPipelineBinary;
            Llpc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineBinary);
        }
        Llpc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }

    if (shaderModuleReplaced)
    {
        pCreateInfo->pipelineInfo.cs.pModuleData = pModuleDataBak;
        FreeShaderModule(&shaderModuleReplaceHandle);
    }

    DropPipelineBinaryInst(pDevice, settings, *ppPipelineBinary, *pPipelineBinarySize);

    return result;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    Device*                             pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    GraphicsPipelineCreateInfo*         pCreateInfo,
    VbBindingInfo*                      pVbInfo)
{
    VkResult               result    = VK_SUCCESS;
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    EXTRACT_VK_STRUCTURES_0(
        gfxPipeline,
        GraphicsPipelineCreateInfo,
        pIn,
        GRAPHICS_PIPELINE_CREATE_INFO)

    // Fill in necessary non-zero defaults in case some information is missing
    const RenderPass* pRenderPass = nullptr;
    const PipelineLayout* pLayout = nullptr;
    const VkPipelineShaderStageCreateInfo* pStageInfos[ShaderGfxStageCount] = {};

    if (pGraphicsPipelineCreateInfo != nullptr)
    {
        for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
        {
            ShaderStage stage = ShaderFlagBitToStage(pGraphicsPipelineCreateInfo->pStages[i].stage);
            VK_ASSERT(stage < ShaderGfxStageCount);
            pStageInfos[stage] = &pGraphicsPipelineCreateInfo->pStages[i];
        }

        VK_IGNORE(pGraphicsPipelineCreateInfo->flags & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT);

        pRenderPass = RenderPass::ObjectFromHandle(pGraphicsPipelineCreateInfo->renderPass);

        if (pGraphicsPipelineCreateInfo->layout != VK_NULL_HANDLE)
        {
            pLayout = PipelineLayout::ObjectFromHandle(pGraphicsPipelineCreateInfo->layout);
        }

        pCreateInfo->pipelineInfo.pVertexInput = pGraphicsPipelineCreateInfo->pVertexInputState;

        const VkPipelineInputAssemblyStateCreateInfo* pIa = pGraphicsPipelineCreateInfo->pInputAssemblyState;
        // According to the spec this should never be null
        VK_ASSERT(pIa != nullptr);

        pCreateInfo->pipelineInfo.iaState.enableMultiView = pRenderPass->IsMultiviewEnabled();
        pCreateInfo->pipelineInfo.iaState.topology           = pIa->topology;
        pCreateInfo->pipelineInfo.iaState.disableVertexReuse = false;

        EXTRACT_VK_STRUCTURES_1(
            Tess,
            PipelineTessellationStateCreateInfo,
            PipelineTessellationDomainOriginStateCreateInfo,
            pGraphicsPipelineCreateInfo->pTessellationState,
            PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO)

        if (pPipelineTessellationStateCreateInfo != nullptr)
        {
            pCreateInfo->pipelineInfo.iaState.patchControlPoints = pPipelineTessellationStateCreateInfo->patchControlPoints;
        }

        if (pPipelineTessellationDomainOriginStateCreateInfo)
        {
            // Vulkan 1.0 incorrectly specified the tessellation u,v coordinate origin as lower left even though
            // framebuffer and image coordinate origins are in the upper left.  This has since been fixed, but
            // an extension exists to use the previous behavior.  Doing so with flat shading would likely appear
            // incorrect, but Vulkan specifies that the provoking vertex is undefined when tessellation is active.
            if (pPipelineTessellationDomainOriginStateCreateInfo->domainOrigin == VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT)
            {
                pCreateInfo->pipelineInfo.iaState.switchWinding = true;
            }
        }

        const VkPipelineRasterizationStateCreateInfo* pRs = pGraphicsPipelineCreateInfo->pRasterizationState;
        // By default rasterization is disabled, unless rasterization creation info is present
        pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = true;
        if (pRs != nullptr)
        {
            pCreateInfo->pipelineInfo.vpState.depthClipEnable         = (pRs->depthClampEnable == VK_FALSE);
            pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = (pRs->rasterizerDiscardEnable != VK_FALSE);
            pCreateInfo->pipelineInfo.rsState.polygonMode             = pRs->polygonMode;
            pCreateInfo->pipelineInfo.rsState.cullMode                = pRs->cullMode;
            pCreateInfo->pipelineInfo.rsState.frontFace               = pRs->frontFace;
            pCreateInfo->pipelineInfo.rsState.depthBiasEnable         = pRs->depthBiasEnable;

            union
            {
                const VkStructHeader*                                        pInfo;
                const VkPipelineRasterizationDepthClipStateCreateInfoEXT*    pRsDepthClip;
            };

            pInfo = static_cast<const VkStructHeader*>(pRs->pNext);

            while (pInfo != nullptr)
            {
                switch (static_cast<uint32_t>(pInfo->sType))
                {
                case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
                    pCreateInfo->pipelineInfo.vpState.depthClipEnable = pRsDepthClip->depthClipEnable;

                    break;
                default:
                    break;
                }

                pInfo = pInfo->pNext;
            }
        }

        const VkPipelineMultisampleStateCreateInfo* pMs = pGraphicsPipelineCreateInfo->pMultisampleState;

        pCreateInfo->pipelineInfo.rsState.numSamples = 1;

        if (pMs != nullptr)
        {
            bool multisampleEnable = (pMs->rasterizationSamples != 1);

            if (multisampleEnable)
            {
                VK_ASSERT(pRenderPass != nullptr);

                uint32_t rasterizationSampleCount   = pMs->rasterizationSamples;
                uint32_t subpassCoverageSampleCount = pRenderPass->GetSubpassMaxSampleCount(pGraphicsPipelineCreateInfo->subpass);
                uint32_t subpassColorSampleCount    = pRenderPass->GetSubpassColorSampleCount(pGraphicsPipelineCreateInfo->subpass);

                // subpassCoverageSampleCount would be equal to zero if there are zero attachments.
                subpassCoverageSampleCount = subpassCoverageSampleCount == 0 ? rasterizationSampleCount : subpassCoverageSampleCount;

                subpassColorSampleCount = subpassColorSampleCount == 0 ? subpassCoverageSampleCount : subpassColorSampleCount;

                if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
                {
                    pCreateInfo->pipelineInfo.rsState.perSampleShading = ((subpassColorSampleCount * pMs->minSampleShading) > 1.0f);
                }
                else
                {
                    pCreateInfo->pipelineInfo.rsState.perSampleShading = false;
                }

                pCreateInfo->pipelineInfo.rsState.numSamples = rasterizationSampleCount;

                // NOTE: The sample pattern index here is actually the offset of sample position pair. This is
                // different from the field of creation info of image view. For image view, the sample pattern
                // index is really table index of the sample pattern.
                pCreateInfo->pipelineInfo.rsState.samplePatternIdx =
                    Device::GetDefaultSamplePatternIndex(subpassCoverageSampleCount) * Pal::MaxMsaaRasterizerSamples;
            }
            pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = (pMs->alphaToCoverageEnable == VK_TRUE);
        }

        const VkPipelineColorBlendStateCreateInfo* pCb = pGraphicsPipelineCreateInfo->pColorBlendState;
        bool dualSourceBlend = false;

        if (pCb != nullptr)
        {
            const uint32_t numColorTargets = Util::Min(pCb->attachmentCount, Pal::MaxColorTargets);

            for (uint32_t i = 0; i < numColorTargets; ++i)
            {
                const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];
                auto pLlpcCbDst = &pCreateInfo->pipelineInfo.cbState.target[i];
                if (pRenderPass)
                {
                    auto cbFormat = pRenderPass->GetColorAttachmentFormat(pGraphicsPipelineCreateInfo->subpass, i);
                    // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
                    // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
                    // disable shader writes through that target.
                    if (cbFormat != VK_FORMAT_UNDEFINED)
                    {
                        pLlpcCbDst->format               = cbFormat;
                        pLlpcCbDst->blendEnable          = (src.blendEnable == VK_TRUE);
                        pLlpcCbDst->blendSrcAlphaToColor = IsSrcAlphaUsedInBlend(src.srcAlphaBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.dstAlphaBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.srcColorBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.dstColorBlendFactor);
                        pLlpcCbDst->channelWriteMask     = src.colorWriteMask;
                    }
                }

                dualSourceBlend |= GetDualSourceBlendEnableState(src);
            }
        }
        pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable = dualSourceBlend;

        VkFormat dbFormat = { };
        if (pRenderPass != nullptr)
        {
            dbFormat = pRenderPass->GetDepthStencilAttachmentFormat(pGraphicsPipelineCreateInfo->subpass);
            pCreateInfo->dbFormat = dbFormat;
        }
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO))
    {
        pCreateInfo->pipelineInfo.options.includeDisassembly = true;
        pCreateInfo->pipelineInfo.options.includeIr = true;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 25
        pCreateInfo->pipelineInfo.options.includeIrBinary = true;
#endif
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT))
    {
        pCreateInfo->pipelineInfo.options.scalarBlockLayout = true;
    }
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 23
    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pCreateInfo->pipelineInfo.options.robustBufferAccess = true;
    }
#endif

    if (pLayout != nullptr)
    {
        pCreateInfo->tempBufferStageSize = pLayout->GetPipelineInfo()->tempStageSize;
        size_t tempBufferSize = pLayout->GetPipelineInfo()->tempBufferSize;

        // Allocate the temp buffer
        if (tempBufferSize > 0)
        {
            pCreateInfo->pMappingBuffer = pInstance->AllocMem(
                tempBufferSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pCreateInfo->pMappingBuffer == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
                // fields of resource mapping nodes are unused for certain node types. We must initialize
                // them to zeroes.
                memset(pCreateInfo->pMappingBuffer, 0, tempBufferSize);
            }
        }
    }

    // Build the LLPC pipeline
    Llpc::PipelineShaderInfo* shaderInfos[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs
    };

    // Apply patches
    pCreateInfo->pipelineInfo.pInstance      = pInstance;
    pCreateInfo->pipelineInfo.pfnOutputAlloc = AllocateShaderOutput;

    ShaderStage lastVertexStage = ShaderStageVertex;
    for (int32_t i = ShaderGfxStageCount-1; i >= 0; --i)
    {
        ShaderStage stage = static_cast<ShaderStage>(i);
        if (pStageInfos[stage] == nullptr) continue;

        if (stage != ShaderStageFragment)
        {
            lastVertexStage = stage;
            break;
        }
    }

    for (uint32_t stage = 0; stage < ShaderGfxStageCount; ++stage)
    {
        auto pStage = pStageInfos[stage];
        auto pShaderInfo = shaderInfos[stage];

        if (pStage == nullptr)
            continue;

        auto pShaderModule = ShaderModule::ObjectFromHandle(pStage->module);
        pShaderInfo->pModuleData           = pShaderModule->GetFirstValidShaderData();
        pShaderInfo->pSpecializationInfo   = pStage->pSpecializationInfo;
        pShaderInfo->pEntryTarget          = pStage->pName;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
        pShaderInfo->entryStage = static_cast<Llpc::ShaderStage>(stage);
#endif
        // Build the resource mapping description for LLPC.  This data contains things about how shader
        // inputs like descriptor set bindings are communicated to this pipeline in a form that LLPC can
        // understand.
        if (pLayout != nullptr)
        {
            const bool vertexShader = (stage == ShaderStageVertex);
            result = pLayout->BuildLlpcPipelineMapping(
                static_cast<ShaderStage>(stage),
                pCreateInfo->pMappingBuffer,
                vertexShader ? pCreateInfo->pipelineInfo.pVertexInput : nullptr,
                pShaderInfo,
                vertexShader ? pVbInfo : nullptr,
                lastVertexStage == static_cast<ShaderStage>(stage));
        }

        ApplyDefaultShaderOptions(static_cast<ShaderStage>(stage),
                                  &pShaderInfo->options
                                  );

        ApplyProfileOptions(pDevice,
                            static_cast<ShaderStage>(stage),
                            pShaderModule,
                            pShaderInfo,
                            &pCreateInfo->pipelineProfileKey
                            );
    }

    pCreateInfo->compilerType = CheckCompilerType(&pCreateInfo->pipelineInfo);
    for (uint32_t stage = 0; stage < ShaderGfxStageCount; ++stage)
    {
        auto pStage = pStageInfos[stage];
        auto pShaderInfo = shaderInfos[stage];

        if (pStage == nullptr)
            continue;

        auto pShaderModule = ShaderModule::ObjectFromHandle(pStage->module);
        pShaderInfo->pModuleData = pShaderModule->GetShaderData(pCreateInfo->compilerType);
    }

    return result;
}

// =====================================================================================================================
// Checks which compiler is used
template<class PipelineBuildInfo>
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const PipelineBuildInfo* pPipelineBuildInfo)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    uint32_t availCompilerMask = 0;
    uint32_t compilerMask = 0;
    availCompilerMask |= (1 << PipelineCompilerTypeLlpc);

    compilerMask = availCompilerMask;

    if (compilerMask == 0)
    {
        compilerMask = availCompilerMask;
    }

    PipelineCompilerType compilerType = PipelineCompilerTypeLlpc;

    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        compilerType = PipelineCompilerTypeLlpc;
    }

    return compilerType;
}

// =====================================================================================================================
// Checks which compiler is available in pipeline build
uint32_t PipelineCompiler::GetCompilerCollectionMask()
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    uint32_t availCompilerMask = 0;
    availCompilerMask |= (1 << PipelineCompilerTypeLlpc);

    return availCompilerMask;
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertComputePipelineInfo(
    Device*                             pDevice,
    const VkComputePipelineCreateInfo*  pIn,
    ComputePipelineCreateInfo*          pCreateInfo)
{
    VkResult result    = VK_SUCCESS;
    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    auto&    settings  = m_pPhysicalDevice->GetRuntimeSettings();

    PipelineLayout* pLayout = nullptr;
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }
    pCreateInfo->flags  = pIn->flags;

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO))
    {
        pCreateInfo->pipelineInfo.options.includeDisassembly = true;
        pCreateInfo->pipelineInfo.options.includeIr = true;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 25
        pCreateInfo->pipelineInfo.options.includeIrBinary = true;
#endif
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT))
    {
        pCreateInfo->pipelineInfo.options.scalarBlockLayout = true;
    }
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 23
    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pCreateInfo->pipelineInfo.options.robustBufferAccess = true;
    }
#endif

    if (pLayout != nullptr)
    {
        pCreateInfo->tempBufferStageSize = pLayout->GetPipelineInfo()->tempStageSize;
        size_t tempBufferSize = pLayout->GetPipelineInfo()->tempBufferSize;

        // Allocate the temp buffer
        if (tempBufferSize > 0)
        {
            pCreateInfo->pMappingBuffer = pInstance->AllocMem(
                tempBufferSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pCreateInfo->pMappingBuffer == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
                // fields of resource mapping nodes are unused for certain node types. We must initialize
                // them to zeroes.
                memset(pCreateInfo->pMappingBuffer, 0, tempBufferSize);
            }
        }
    }

     ShaderModule* pShaderModule = ShaderModule::ObjectFromHandle(pIn->stage.module);
     pCreateInfo->pipelineInfo.cs.pModuleData         = pShaderModule->GetFirstValidShaderData();
     pCreateInfo->pipelineInfo.cs.pSpecializationInfo = pIn->stage.pSpecializationInfo;
     pCreateInfo->pipelineInfo.cs.pEntryTarget        = pIn->stage.pName;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
     pCreateInfo->pipelineInfo.cs.entryStage          = Llpc::ShaderStageCompute;
#endif

    // Build the resource mapping description for LLPC.  This data contains things about how shader
    // inputs like descriptor set bindings interact with this pipeline in a form that LLPC can
    // understand.
    if (pLayout != nullptr)
    {
        result = pLayout->BuildLlpcPipelineMapping(
            ShaderStageCompute,
            pCreateInfo->pMappingBuffer,
            nullptr,
            &pCreateInfo->pipelineInfo.cs,
            nullptr,
            false);
    }

    pCreateInfo->compilerType = CheckCompilerType(&pCreateInfo->pipelineInfo);
    pCreateInfo->pipelineInfo.cs.pModuleData = pShaderModule->GetShaderData(pCreateInfo->compilerType);

    ApplyDefaultShaderOptions(ShaderStageCompute,
                              &pCreateInfo->pipelineInfo.cs.options
                              );

    ApplyProfileOptions(pDevice,
                        ShaderStageCompute,
                        pShaderModule,
                        &pCreateInfo->pipelineInfo.cs,
                        &pCreateInfo->pipelineProfileKey
                        );

    return result;
}

// =====================================================================================================================
// Set any non-zero shader option defaults
void PipelineCompiler::ApplyDefaultShaderOptions(
    ShaderStage                  stage,
    Llpc::PipelineShaderOptions* pShaderOptions
    ) const
{

}

// =====================================================================================================================
// Builds app profile key and applies profile options.
void PipelineCompiler::ApplyProfileOptions(
    Device*                      pDevice,
    ShaderStage                  stage,
    ShaderModule*                pShaderModule,
    Llpc::PipelineShaderInfo*    pShaderInfo,
    PipelineOptimizerKey*        pProfileKey
    )
{
    auto&    settings  = m_pPhysicalDevice->GetRuntimeSettings();
    PipelineShaderOptionsPtr options = {};
    options.pOptions     = &pShaderInfo->options;

    auto& shaderKey = pProfileKey->shaders[stage];
    if (settings.pipelineUseShaderHashAsProfileHash)
    {
        const void* pModuleData = pShaderInfo->pModuleData;
        shaderKey.codeHash.lower = Llpc::IPipelineDumper::GetShaderHash(pModuleData);
        shaderKey.codeHash.upper = 0;
    }
    else
    {
        // Populate the pipeline profile key.  The hash used by the profile is different from the default
        // internal hash in that it only depends on the SPIRV code + entry point.  This is to reduce the
        // chance that internal changes to our hash calculation logic drop us off pipeline profiles.
        shaderKey.codeHash = pShaderModule->GetCodeHash(pShaderInfo->pEntryTarget);
    }
    shaderKey.codeSize = pShaderModule->GetCodeSize();

    // Override the compile parameters based on any app profile
    auto* pShaderOptimizer = pDevice->GetShaderOptimizer();
    pShaderOptimizer->OverrideShaderCreateInfo(*pProfileKey, stage, options);
}

// =====================================================================================================================
// Free compute pipeline binary
void PipelineCompiler::FreeComputePipelineBinary(
    ComputePipelineCreateInfo* pCreateInfo,
    const void*                pPipelineBinary,
    size_t                     binarySize)
{
    {
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            m_compilerSolutionLlpc.FreeComputePipelineBinary(pPipelineBinary, binarySize);
        }

    }
}

// =====================================================================================================================
// Free graphics pipeline binary
void PipelineCompiler::FreeGraphicsPipelineBinary(
    GraphicsPipelineCreateInfo* pCreateInfo,
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    {
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            m_compilerSolutionLlpc.FreeGraphicsPipelineBinary(pPipelineBinary, binarySize);
        }

    }
}

// =====================================================================================================================
// Free the temp memories in compute pipeline create info
void PipelineCompiler::FreeComputePipelineCreateInfo(
    ComputePipelineCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pMappingBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pMappingBuffer);
        pCreateInfo->pMappingBuffer = nullptr;
    }
}

// =====================================================================================================================
// Free the temp memories in graphics pipeline create info
void PipelineCompiler::FreeGraphicsPipelineCreateInfo(
    GraphicsPipelineCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pMappingBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pMappingBuffer);
        pCreateInfo->pMappingBuffer = nullptr;
    }
}

}
