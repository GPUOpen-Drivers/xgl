/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 39
#define Vkgc Llpc
#endif

#include "include/log.h"
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

#include "include/pipeline_binary_cache.h"

#include "palElfReader.h"
#include "palPipelineAbiReader.h"

#if  LLPC_CLIENT_INTERFACE_MAJOR_VERSION>= 39
#include "llpc.h"
#endif

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
    , m_pBinaryCache(nullptr)
    , m_cacheAttempts(0)
    , m_cacheHits(0)
    , m_totalBinaries(0)
    , m_totalTimeSpent(0)
{

}

// =====================================================================================================================
// Dump pipeline elf cache metrics to a string
void PipelineCompiler::GetElfCacheMetricString(
    char*   pOutStr,
    size_t  outStrSize)
{
    const int64_t freq = Util::GetPerfFrequency();

    const int64_t avgUs = ((m_totalTimeSpent / m_totalBinaries) * 1000000) / freq;
    const double  avgMs = avgUs / 1000.0;

    const int64_t totalUs = (m_totalTimeSpent * 1000000) / freq;
    const double  totalMs = totalUs / 1000.0;

    const double  hitRate = m_cacheAttempts > 0 ?
        (static_cast<double>(m_cacheHits) / static_cast<double>(m_cacheAttempts)) :
        0.0;

    static constexpr char metricFmtString[] =
        "Cache hit rate - %0.1f%%\n"
        "Total request count - %d\n"
        "Total time spent - %0.1f ms\n"
        "Average time spent per request - %0.3f ms\n";

    Util::Snprintf(pOutStr, outStrSize, metricFmtString, hitRate * 100, m_totalBinaries, totalMs, avgMs);
}

// =====================================================================================================================
PipelineCompiler::~PipelineCompiler()
{
    VK_ASSERT(m_pBinaryCache == nullptr);
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
    case Pal::GfxIpLevel::GfxIp10_1:
        m_gfxIp.major = 10;
        m_gfxIp.minor = 1;
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
        result = m_compilerSolutionLlpc.Initialize(m_gfxIp, info.gfxLevel);
    }

    if ((result == VK_SUCCESS) &&
        ((settings.usePalPipelineCaching) ||
         (m_pPhysicalDevice->VkInstance()->GetDevModeMgr() != nullptr)))
    {
        m_pBinaryCache = PipelineBinaryCache::Create(
                m_pPhysicalDevice->VkInstance(), 0, nullptr, true, m_gfxIp, m_pPhysicalDevice);

        // This isn't a terminal failure, the device can continue without the pipeline cache if need be.
        VK_ALERT(m_pBinaryCache == nullptr);
    }

    return result;
}

// =====================================================================================================================
// Destroys all compiler instance.
void PipelineCompiler::Destroy()
{
    m_compilerSolutionLlpc.Destroy();

    if (m_pBinaryCache)
    {
        m_pBinaryCache->Destroy();
        m_pPhysicalDevice->VkInstance()->FreeMem(m_pBinaryCache);
        m_pBinaryCache = nullptr;
    }

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
    const Device*               pDevice,
    VkShaderModuleCreateFlags   flags,
    size_t                      codeSize,
    const void*                 pCode,
    ShaderModuleHandle*         pShaderModule)
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
        result = m_compilerSolutionLlpc.BuildShaderModule(pDevice, flags, codeSize, pCode, pShaderModule, hash);
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
    Vkgc::IPipelineDumper::GetPipelineName(pPipelineBuildInfo, fileName, 128);

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
    Vkgc::PipelineShaderInfo* pShaderInfo,
    ShaderModuleHandle*       pShaderModule)
{
    bool replaced = false;
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pShaderInfo->pModuleData != nullptr)
    {
        uint64_t hash64 = Vkgc::IPipelineDumper::GetShaderHash(pShaderInfo->pModuleData);
        size_t codeSize = 0;
        void* pCode = nullptr;

        if (LoadReplaceShaderBinary(hash64, &codeSize, &pCode))
        {
            VkResult result = BuildShaderModule(pDevice, 0, codeSize, pCode, pShaderModule);
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
        Util::ElfReader::Reader elfReader(pPipelineBinary);
        Util::ElfReader::SectionId codeSectionId = elfReader.FindSection(".text");
        VK_ASSERT(codeSectionId != 0);

        const Util::Elf::SectionHeader& codeSection = elfReader.GetSection(codeSectionId);

        size_t pipelineCodeSize = static_cast<size_t>(codeSection.sh_size);
        void* pPipelineCode = const_cast<void*>(Util::VoidPtrInc(pPipelineBinary,
            static_cast<size_t>(codeSection.sh_offset)));
        uint32_t* pFirstInstruction = static_cast<uint32_t*>(pPipelineCode);

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
    size_t       pipelineBinarySize)
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

    Util::Abi::PipelineAbiReader abiReader(pDevice->VkInstance()->Allocator(), pPipelineBinary);
    Pal::Result palResult = abiReader.Init();
    if (palResult != Pal::Result::Success)
    {
        return;
    }

    Util::ElfReader::SectionId codeSectionId = abiReader.GetElfReader().FindSection(".text");
    VK_ASSERT(codeSectionId != 0);

    const Util::Elf::SectionHeader& codeSection = abiReader.GetElfReader().GetSection(codeSectionId);

    size_t pipelineCodeSize = static_cast<size_t>(codeSection.sh_size);
    void* pPipelineCode = const_cast<void*>(Util::VoidPtrInc(pPipelineBinary,
        static_cast<size_t>(codeSection.sh_offset)));
    uint8_t* pFirstInstruction = static_cast<uint8_t*>(pPipelineCode);

    std::vector<const Util::Elf::SymbolTableEntry*> shaderStageSymbols;
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
    for (const auto& simbolTypeEntry : stageSymbolTypes)
    {
        const Util::Elf::SymbolTableEntry* pEntry = abiReader.GetPipelineSymbol(simbolTypeEntry);
        if (pEntry != nullptr)
        {
            shaderStageSymbols.push_back(pEntry);
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
        const char* pOffsetEnd = strchr(codeLine, ':');
        if (pOffsetEnd != nullptr)
        {
            strncpy(codeOffset, codeLine, pOffsetEnd - codeLine);
            codeOffset[pOffsetEnd - codeLine] = '\0';
            uint32_t offset = strtoul(codeOffset, nullptr, 10);
            bool inRange = false;
            for (const auto symbolEntry : shaderStageSymbols)
            {
                if ((offset >= symbolEntry->st_value) && (offset < symbolEntry->st_value + symbolEntry->st_size))
                {
                    inRange = true;
                    break;
                }
            }
            VK_ASSERT(inRange);
            pOffsetEnd++;
            uint32_t replaceCode = strtoul(pOffsetEnd, nullptr, 16);
            memcpy(pFirstInstruction + offset, &replaceCode, sizeof(replaceCode));
        }
    }

}

// =====================================================================================================================
// Checks PAL Pipeline cache for existing pipeline binary.
Util::Result PipelineCompiler::GetCachedPipelineBinary(
    const Util::MetroHash::Hash* pCacheId,
    const PipelineBinaryCache*   pPipelineBinaryCache,
    size_t*                      pPipelineBinarySize,
    const void**                 ppPipelineBinary,
    bool*                        pIsUserCacheHit,
    bool*                        pIsInternalCacheHit,
    bool*                        pElfWasCached,
    PipelineCreationFeedback*    pPipelineFeedback)
{
    Util::Result cacheResult = Util::Result::Success;

    if (pPipelineBinaryCache != nullptr)
    {
        cacheResult = pPipelineBinaryCache->LoadPipelineBinary(pCacheId, pPipelineBinarySize, ppPipelineBinary);
        if (cacheResult == Util::Result::Success)
        {
            *pIsUserCacheHit = true;
            pPipelineFeedback->hitApplicationCache = true;
        }
    }
    m_cacheAttempts++;

    if (m_pBinaryCache != nullptr)
    {
        // If user cache is already hit, we just need query if it is in internal cache,
        // don't need heavy loading work.
        if (*pIsUserCacheHit)
        {
            Util::QueryResult query = {};
            cacheResult = m_pBinaryCache->QueryPipelineBinary(pCacheId, &query);
        }
        else
        {
            cacheResult = m_pBinaryCache->LoadPipelineBinary(pCacheId, pPipelineBinarySize, ppPipelineBinary);
        }
        if (cacheResult == Util::Result::Success)
        {
            *pIsInternalCacheHit = true;
        }
    }
    if (*pIsUserCacheHit || *pIsInternalCacheHit)
    {
        *pElfWasCached = true;
        cacheResult = Util::Result::Success;
        m_cacheHits++;
    }

    return cacheResult;
}

// =====================================================================================================================
// Creates partial pipeline binary.
VkResult PipelineCompiler::CreatePartialPipelineBinary(
    uint32_t                            deviceIdx,
    void*                               pShaderModuleData,
    Vkgc::ShaderModuleEntryData*        pShaderModuleEntryData,
    const Vkgc::ResourceMappingNode*    pResourceMappingNode,
    uint32_t                            mappingNodeCount,
    Vkgc::ColorTarget*                  pColorTarget)
{
    uint32_t compilerMask = GetCompilerCollectionMask();
    VkResult result = VK_SUCCESS;
    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        result = m_compilerSolutionLlpc.CreatePartialPipelineBinary(deviceIdx, pShaderModuleData, pShaderModuleEntryData,
                pResourceMappingNode, mappingNodeCount, pColorTarget);
    }

    return result;
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
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* moduleDataBaks[ShaderGfxStageCount];
    ShaderModuleHandle shaderModuleReplaceHandles[ShaderGfxStageCount];
    bool shaderModuleReplaced = false;

    Vkgc::PipelineShaderInfo* shaderInfos[ShaderGfxStageCount] =
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
                pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
            }
        }
    }

    if (settings.enablePipelineDump)
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir                 = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines    = settings.dumpDuplicatePipelines;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo);
#else
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, nullptr, &pCreateInfo->pipelineInfo);
#endif
    }

    // PAL Pipeline caching
    Util::Result                 cacheResult = Util::Result::Success;

    int64_t cacheTime   = 0;

    bool isUserCacheHit     = false;
    bool isInternalCacheHit = false;

    PipelineBinaryCache* pPipelineBinaryCache = nullptr;

    if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
    {
        pPipelineBinaryCache = pPipelineCache->GetPipelineCache();
    }

    if (shouldCompile && ((pPipelineBinaryCache != nullptr) || (m_pBinaryCache != nullptr)))
    {
        int64_t startTime = Util::GetPerfCpuTime();
        Util::MetroHash128 hash = {};
        hash.Update(pipelineHash);
        hash.Update(pCreateInfo->pipelineInfo.vs.options);
        hash.Update(pCreateInfo->pipelineInfo.tes.options);
        hash.Update(pCreateInfo->pipelineInfo.tcs.options);
        hash.Update(pCreateInfo->pipelineInfo.gs.options);
        hash.Update(pCreateInfo->pipelineInfo.fs.options);
        hash.Update(pCreateInfo->pipelineInfo.options);
        hash.Update(pCreateInfo->pipelineInfo.nggState);
        hash.Update(GetCacheIdControlFlags(pCreateInfo->flags));
        hash.Update(pCreateInfo->dbFormat);
        hash.Update(pCreateInfo->pipelineProfileKey);
        hash.Update(deviceIdx);
        hash.Update(pCreateInfo->compilerType);
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            hash.Update(reinterpret_cast<const uint8_t*>(settings.llpcOptions), sizeof(settings.llpcOptions));
        }
        hash.Finalize(pCacheId->bytes);

        cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
            &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->elfWasCached, &pCreateInfo->pipelineFeedback);
        if (cacheResult == Util::Result::Success)
        {
            shouldCompile = false;
        }

        cacheTime = Util::GetPerfCpuTime() - startTime;
    }

    if (shouldCompile)
    {
        if (pCreateInfo->flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT)
        {
            result = VK_PIPELINE_COMPILE_REQUIRED_EXT;
        }
        else
        {
            if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
            {
                result = m_compilerSolutionLlpc.CreateGraphicsPipelineBinary(
                    pDevice,
                    deviceIdx,
                    pPipelineCache,
                    pCreateInfo,
                    pPipelineBinarySize,
                    ppPipelineBinary,
                    rasterizationStream,
                    shaderInfos,
                    pPipelineDumpHandle,
                    pipelineHash,
                    &compileTime);
            }

        }
    }

    if ((pPipelineBinaryCache != nullptr) &&
        (isUserCacheHit == false) &&
        (result == VK_SUCCESS))
    {
        cacheResult = pPipelineBinaryCache->StorePipelineBinary(
            pCacheId,
            *pPipelineBinarySize,
            *ppPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    if ((m_pBinaryCache != nullptr) &&
        (isInternalCacheHit == false) &&
        (result == VK_SUCCESS))
    {
        cacheResult = m_pBinaryCache->StorePipelineBinary(
            pCacheId,
            *pPipelineBinarySize,
            *ppPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    m_totalTimeSpent += pCreateInfo->elfWasCached ? cacheTime : compileTime;
    m_totalBinaries++;

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, *ppPipelineBinary, *pPipelineBinarySize);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Vkgc::BinaryData pipelineBinary = {};
            pipelineBinary.codeSize = *pPipelineBinarySize;
            pipelineBinary.pCode = *ppPipelineBinary;
            Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineBinary);
        }
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
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
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* pModuleDataBak = nullptr;
    ShaderModuleHandle shaderModuleReplaceHandle = {};
    bool shaderModuleReplaced = false;

    if (settings.enablePipelineDump)
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir                 = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines    = settings.dumpDuplicatePipelines;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 21
        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pComputeInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo);
#else
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, &pCreateInfo->pipelineInfo, nullptr);
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
                pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
            }
        }
    }

    // PAL Pipeline caching
    Util::Result                 cacheResult = Util::Result::Success;

    int64_t cacheTime   = 0;

    bool isUserCacheHit     = false;
    bool isInternalCacheHit = false;

    PipelineBinaryCache* pPipelineBinaryCache = nullptr;

    if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
    {
        pPipelineBinaryCache = pPipelineCache->GetPipelineCache();
    }

    if (shouldCompile && ((pPipelineBinaryCache != nullptr) || (m_pBinaryCache != nullptr)))
    {
        int64_t startTime = Util::GetPerfCpuTime();
        Util::MetroHash128 hash = {};
        hash.Update(pipelineHash);
        hash.Update(pCreateInfo->pipelineInfo.cs.options);
        hash.Update(pCreateInfo->pipelineInfo.options);
        hash.Update(GetCacheIdControlFlags(pCreateInfo->flags));
        hash.Update(pCreateInfo->pipelineProfileKey);
        hash.Update(deviceIdx);
        hash.Update(pCreateInfo->compilerType);
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            hash.Update(reinterpret_cast<const uint8_t*>(settings.llpcOptions), sizeof(settings.llpcOptions));
        }
        hash.Finalize(pCacheId->bytes);

        cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
            &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->elfWasCached, &pCreateInfo->pipelineFeedback);
        if (cacheResult == Util::Result::Success)
        {
            shouldCompile = false;
        }

        cacheTime = Util::GetPerfCpuTime() - startTime;
    }

    if (shouldCompile)
    {
        if (pCreateInfo->flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT)
        {
            result = VK_PIPELINE_COMPILE_REQUIRED_EXT;
        }
        else
        {
            if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
            {
                result = m_compilerSolutionLlpc.CreateComputePipelineBinary(
                    pDevice,
                    deviceIdx,
                    pPipelineCache,
                    pCreateInfo,
                    pPipelineBinarySize,
                    ppPipelineBinary,
                    pPipelineDumpHandle,
                    pipelineHash,
                    &compileTime);
            }

        }
    }

    if ((pPipelineBinaryCache != nullptr) &&
        (isUserCacheHit == false) &&
        (result == VK_SUCCESS))
    {
        cacheResult = pPipelineBinaryCache->StorePipelineBinary(
            pCacheId,
            *pPipelineBinarySize,
            *ppPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    if ((m_pBinaryCache != nullptr) &&
        (isInternalCacheHit == false) &&
        (result == VK_SUCCESS))
    {
        cacheResult = m_pBinaryCache->StorePipelineBinary(
            pCacheId,
            *pPipelineBinarySize,
            *ppPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    m_totalTimeSpent += pCreateInfo->elfWasCached ? cacheTime : compileTime;
    m_totalBinaries++;
    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, *ppPipelineBinary, *pPipelineBinarySize);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Vkgc::BinaryData pipelineBinary = {};
            pipelineBinary.codeSize = *pPipelineBinarySize;
            pipelineBinary.pCode = *ppPipelineBinary;
            Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineBinary);
        }
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
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
// If provided, obtains the the pipeline creation feedback create info pointer and clears the feedback flags.
void PipelineCompiler::GetPipelineCreationInfoNext(
        const VkStructHeader*                             pHeader,
        const VkPipelineCreationFeedbackCreateInfoEXT**   ppPipelineCreationFeadbackCreateInfo)
{
    VK_ASSERT(ppPipelineCreationFeadbackCreateInfo != nullptr);
    for ( ; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<int>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT:
            *ppPipelineCreationFeadbackCreateInfo =
                reinterpret_cast<const VkPipelineCreationFeedbackCreateInfoEXT*>(pHeader);
            VK_ASSERT((*ppPipelineCreationFeadbackCreateInfo)->pPipelineCreationFeedback != nullptr);
            (*ppPipelineCreationFeadbackCreateInfo)->pPipelineCreationFeedback->flags = 0;
            if ((*ppPipelineCreationFeadbackCreateInfo)->pPipelineStageCreationFeedbacks != nullptr)
            {
                for (uint32_t i = 0; i < (*ppPipelineCreationFeadbackCreateInfo)->pipelineStageCreationFeedbackCount; i++)
                {
                    (*ppPipelineCreationFeadbackCreateInfo)->pPipelineStageCreationFeedbacks[i].flags = 0;
                }
            }
            break;
        default:
            break;
        }
    }
}

// =====================================================================================================================
VkResult PipelineCompiler::SetPipelineCreationFeedbackInfo(
    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo,
    const PipelineCreationFeedback*                pPipelineFeedback)
{
    if (pPipelineCreationFeadbackCreateInfo != nullptr)
    {
        if (pPipelineFeedback->feedbackValid)
        {
            pPipelineCreationFeadbackCreateInfo->pPipelineCreationFeedback->flags |=
                VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT;
        }
        if (pPipelineFeedback->hitApplicationCache)
        {
            pPipelineCreationFeadbackCreateInfo->pPipelineCreationFeedback->flags |=
                VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT;
        }
        pPipelineCreationFeadbackCreateInfo->pPipelineCreationFeedback->duration =
            pPipelineFeedback->duration;
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    Device*                                         pDevice,
    const VkGraphicsPipelineCreateInfo*             pIn,
    GraphicsPipelineCreateInfo*                     pCreateInfo,
    VbBindingInfo*                                  pVbInfo,
    const VkPipelineCreationFeedbackCreateInfoEXT** ppPipelineCreationFeadbackCreateInfo)
{
    VkResult               result    = VK_SUCCESS;
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    auto                   flags     = pIn->flags;
    EXTRACT_VK_STRUCTURES_0(
        gfxPipeline,
        GraphicsPipelineCreateInfo,
        pIn,
        GRAPHICS_PIPELINE_CREATE_INFO)

    if ((pIn != nullptr) && (ppPipelineCreationFeadbackCreateInfo != nullptr))
    {
        GetPipelineCreationInfoNext(
            reinterpret_cast<const VkStructHeader*>(pIn->pNext),
            ppPipelineCreationFeadbackCreateInfo);
    }
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

        uint32 activeStages = {};
        for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
        {
            activeStages = activeStages | pGraphicsPipelineCreateInfo->pStages[i].stage;
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

        if (activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        {
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

        if ((pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable != VK_TRUE) && (pMs != nullptr))
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

        if ((pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable != VK_TRUE) && (pCb != nullptr))
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
                    // disable shader writes through that target. There is one exception for alphaToCoverageEnable
                    // and attachment zero, which can be set to VK_ATTACHMENT_UNUSED.
                    if ((cbFormat != VK_FORMAT_UNDEFINED) ||
                        (pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable && (i == 0u)))
                    {
                        pLlpcCbDst->format               = cbFormat != VK_FORMAT_UNDEFINED ? cbFormat  : pRenderPass->GetAttachmentDesc(i).format;
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

    if (m_gfxIp.major >= 10)
    {
        const bool                 hasGs        = pStageInfos[ShaderStageGeometry] != nullptr;
        const bool                 hasTess      = pStageInfos[ShaderStageTessControl] != nullptr;
        const GraphicsPipelineType pipelineType =
            hasTess ? (hasGs ? GraphicsPipelineTypeTessGs : GraphicsPipelineTypeTess) :
                      (hasGs ? GraphicsPipelineTypeGs : GraphicsPipelineTypeVsFs);

        pCreateInfo->pipelineInfo.nggState.enableNgg                  =
            Util::TestAnyFlagSet(settings.enableNgg, pipelineType);
        pCreateInfo->pipelineInfo.nggState.enableGsUse                = settings.nggEnableGsUse;
        pCreateInfo->pipelineInfo.nggState.forceNonPassthrough        = settings.nggForceNonPassthrough;
        pCreateInfo->pipelineInfo.nggState.alwaysUsePrimShaderTable   = settings.nggAlwaysUsePrimShaderTable;
        pCreateInfo->pipelineInfo.nggState.compactMode                =
            static_cast<Vkgc::NggCompactMode>(settings.nggCompactionMode);

        pCreateInfo->pipelineInfo.nggState.enableFastLaunch           = false;
        pCreateInfo->pipelineInfo.nggState.enableVertexReuse          = false;
        pCreateInfo->pipelineInfo.nggState.enableBackfaceCulling      = settings.nggEnableBackfaceCulling;
        pCreateInfo->pipelineInfo.nggState.enableFrustumCulling       = settings.nggEnableFrustumCulling;
        pCreateInfo->pipelineInfo.nggState.enableBoxFilterCulling     = settings.nggEnableBoxFilterCulling;
        pCreateInfo->pipelineInfo.nggState.enableSphereCulling        = settings.nggEnableSphereCulling;
        pCreateInfo->pipelineInfo.nggState.enableSmallPrimFilter      = settings.nggEnableSmallPrimFilter;
        pCreateInfo->pipelineInfo.nggState.enableCullDistanceCulling  = settings.nggEnableCullDistanceCulling;

        if (settings.requireMrtForNggCulling)
        {
            uint32_t numTargets = 0;
            for (uint32_t i = 0; i < Pal::MaxColorTargets; ++i)
            {
                numTargets += pCreateInfo->pipelineInfo.cbState.target[i].channelWriteMask ? 1 : 0;
            }

            if (numTargets < 2)
            {
                pCreateInfo->pipelineInfo.nggState.enableBackfaceCulling     = false;
                pCreateInfo->pipelineInfo.nggState.enableFrustumCulling      = false;
                pCreateInfo->pipelineInfo.nggState.enableBoxFilterCulling    = false;
                pCreateInfo->pipelineInfo.nggState.enableSphereCulling       = false;
                pCreateInfo->pipelineInfo.nggState.enableSmallPrimFilter     = false;
                pCreateInfo->pipelineInfo.nggState.enableCullDistanceCulling = false;
            }
        }

        pCreateInfo->pipelineInfo.nggState.backfaceExponent           = settings.nggBackfaceExponent;
        pCreateInfo->pipelineInfo.nggState.subgroupSizing             =
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 26
            static_cast<Vkgc::NggSubgroupSizingType>(settings.nggSubgroupSizing);
#else
            (settings.nggSubgroupSizing == NggSubgroupSizingType::NggSubgroupAuto) ?
            Vkgc::NggSubgroupSizingType::MaximumSize :
            static_cast<Vkgc::NggSubgroupSizingType>(static_cast<uint32_t>(settings.nggSubgroupSizing) - 1);
#endif
        pCreateInfo->pipelineInfo.nggState.primsPerSubgroup           = settings.nggPrimsPerSubgroup;
        pCreateInfo->pipelineInfo.nggState.vertsPerSubgroup           = settings.nggVertsPerSubgroup;
    }

    pCreateInfo->flags = flags;
    ApplyPipelineOptions(pDevice, flags, &pCreateInfo->pipelineInfo.options);

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
    Vkgc::PipelineShaderInfo* shaderInfos[] =
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
        pShaderInfo->entryStage = static_cast<Vkgc::ShaderStage>(stage);
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

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 31
        if ((pStage->flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT) != 0)
        {
            pShaderInfo->options.allowVaryWaveSize = true;
        }
#endif

        ApplyProfileOptions(pDevice,
                            static_cast<ShaderStage>(stage),
                            pShaderModule,
                            &pCreateInfo->pipelineInfo.options,
                            pShaderInfo,
                            &pCreateInfo->pipelineProfileKey,
                            &pCreateInfo->pipelineInfo.nggState
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

    pCreateInfo->elfWasCached = false;

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
void PipelineCompiler::ApplyPipelineOptions(
    const Device*            pDevice,
    VkPipelineCreateFlags    flags,
    Vkgc::PipelineOptions*   pOptions)
{
    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) ||
        (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_PIPELINE_EXECUTABLE_PROPERTIES) &&
        ((flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) != 0)))
    {
        pOptions->includeDisassembly = true;
        pOptions->includeIr = true;
#if (LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 25) && (LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 27)
        pOptions->includeIrBinary = true;
#endif
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT) ||
        pDevice->GetEnabledFeatures().scalarBlockLayout)
    {
        pOptions->scalarBlockLayout = true;
    }

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 23
    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pOptions->robustBufferAccess = true;
    }
#endif

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 38
    // Setup shadow descriptor table pointer
    const auto& info = m_pPhysicalDevice->PalProperties();
    pOptions->shadowDescriptorTableUsage = Vkgc::ShadowDescriptorTableUsage::Enable;
    pOptions->shadowDescriptorTablePtrHigh =
          static_cast<uint32_t>(info.gpuMemoryProperties.shadowDescTableVaStart >> 32);
    pOptions->enableRelocatableShaderElf = m_pPhysicalDevice->GetRuntimeSettings().enableRelocatableShaders;
#endif

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 40
    if (pDevice->GetEnabledFeatures().extendedRobustness.robustBufferAccess)
    {
        pOptions->extendedRobustness.robustBufferAccess = true;
    }
    if (pDevice->GetEnabledFeatures().extendedRobustness.robustImageAccess)
    {
        pOptions->extendedRobustness.robustImageAccess = true;
    }
    if (pDevice->GetEnabledFeatures().extendedRobustness.nullDescriptor)
    {
        pOptions->extendedRobustness.nullDescriptor = true;
    }
#endif
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertComputePipelineInfo(
    Device*                                         pDevice,
    const VkComputePipelineCreateInfo*              pIn,
    ComputePipelineCreateInfo*                      pCreateInfo,
    const VkPipelineCreationFeedbackCreateInfoEXT** ppPipelineCreationFeadbackCreateInfo)
{
    VkResult result    = VK_SUCCESS;
    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    auto&    settings  = m_pPhysicalDevice->GetRuntimeSettings();

    PipelineLayout* pLayout = nullptr;
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    GetPipelineCreationInfoNext(
        reinterpret_cast<const VkStructHeader*>(pIn->pNext),
        ppPipelineCreationFeadbackCreateInfo);
    if (pIn->layout != VK_NULL_HANDLE)
    {
        pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }
    pCreateInfo->flags  = pIn->flags;
    ApplyPipelineOptions(pDevice, pIn->flags, &pCreateInfo->pipelineInfo.options);

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
    pCreateInfo->pipelineInfo.cs.entryStage          = Vkgc::ShaderStageCompute;
#endif

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION >= 31
    if ((pIn->stage.flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT) != 0)
    {
        pCreateInfo->pipelineInfo.cs.options.allowVaryWaveSize = true;
    }
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
                        nullptr,
                        &pCreateInfo->pipelineInfo.cs,
                        &pCreateInfo->pipelineProfileKey,
                        nullptr
                        );

    return result;
}

// =====================================================================================================================
// Set any non-zero shader option defaults
void PipelineCompiler::ApplyDefaultShaderOptions(
    ShaderStage                  stage,
    Vkgc::PipelineShaderOptions* pShaderOptions
    ) const
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    switch (stage)
    {
    case ShaderStageVertex:
        pShaderOptions->waveSize = settings.vsWaveSize;
        break;
    case ShaderStageTessControl:
        pShaderOptions->waveSize = settings.tcsWaveSize;
        break;
    case ShaderStageTessEvaluation:
        pShaderOptions->waveSize = settings.tesWaveSize;
        break;
    case ShaderStageGeometry:
        pShaderOptions->waveSize = settings.gsWaveSize;
        break;
    case ShaderStageFragment:
        pShaderOptions->waveSize = settings.fsWaveSize;
        break;
    case ShaderStageCompute:
        pShaderOptions->waveSize = settings.csWaveSize;
        break;
    default:
        break;
    }

    pShaderOptions->wgpMode       = ((settings.enableWgpMode & (1 << stage)) != 0);
    pShaderOptions->waveBreakSize = static_cast<Vkgc::WaveBreakSize>(settings.waveBreakSize);

}

// =====================================================================================================================
// Builds app profile key and applies profile options.
void PipelineCompiler::ApplyProfileOptions(
    Device*                      pDevice,
    ShaderStage                  stage,
    ShaderModule*                pShaderModule,
    Vkgc::PipelineOptions*       pPipelineOptions,
    Vkgc::PipelineShaderInfo*    pShaderInfo,
    PipelineOptimizerKey*        pProfileKey,
    Vkgc::NggState*              pNggState
    )
{
    auto&    settings  = m_pPhysicalDevice->GetRuntimeSettings();
    PipelineShaderOptionsPtr options = {};
    options.pPipelineOptions = pPipelineOptions;
    options.pOptions     = &pShaderInfo->options;
    options.pNggState    = pNggState;

    auto& shaderKey = pProfileKey->shaders[stage];
    if (settings.pipelineUseShaderHashAsProfileHash)
    {
        const void* pModuleData = pShaderInfo->pModuleData;
        shaderKey.codeHash.lower = Vkgc::IPipelineDumper::GetShaderHash(pModuleData);
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
    if (pCreateInfo->elfWasCached)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
    else
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
    if (pCreateInfo->elfWasCached)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
    else
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

#if ICD_GPUOPEN_DEVMODE_BUILD
Util::Result PipelineCompiler::RegisterAndLoadReinjectionBinary(
    const Pal::PipelineHash*     pInternalPipelineHash,
    const Util::MetroHash::Hash* pCacheId,
    size_t*                      pBinarySize,
    const void**                 ppPipelineBinary,
    PipelineCache*               pPipelineCache)
{
    Util::Result result = Util::Result::NotFound;

    PipelineBinaryCache* pPipelineBinaryCache = m_pBinaryCache;

    if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
    {
        pPipelineBinaryCache = pPipelineCache->GetPipelineCache();
    }

    if (pPipelineBinaryCache != nullptr)
    {
        pPipelineBinaryCache->RegisterHashMapping(
            pInternalPipelineHash,
            pCacheId);

        static_assert(sizeof(Pal::PipelineHash) == sizeof(PipelineBinaryCache::CacheId), "Structure size mismatch");

        if (m_pBinaryCache != nullptr)
        {
            result = m_pBinaryCache->LoadReinjectionBinary(
                reinterpret_cast<const PipelineBinaryCache::CacheId*>(pInternalPipelineHash),
                pBinarySize,
                ppPipelineBinary);
        }

        if ((result == Util::Result::NotFound) &&
            (pPipelineBinaryCache != m_pBinaryCache))
        {
            result = pPipelineBinaryCache->LoadReinjectionBinary(
                reinterpret_cast<const PipelineBinaryCache::CacheId*>(pInternalPipelineHash),
                pBinarySize,
                ppPipelineBinary);
        }
    }

    return result;
}
#endif

// =====================================================================================================================
// Filter VkPipelineCreateFlags to only values used for pipeline caching
VkPipelineCreateFlags PipelineCompiler::GetCacheIdControlFlags(
    VkPipelineCreateFlags in)
{
    // The following flags should NOT affect cache computation
    static constexpr VkPipelineCreateFlags CacheIdIgnoreFlags = { 0
        | VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR
        | VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR
        | VK_PIPELINE_CREATE_DERIVATIVE_BIT
        | VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT
        | VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT
        | VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT
    };

    return in & (~CacheIdIgnoreFlags);
}

}
