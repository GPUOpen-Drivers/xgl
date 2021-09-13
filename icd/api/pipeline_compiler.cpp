/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "llpc.h"

#include <inttypes.h>

namespace vk
{

// =====================================================================================================================
// The shader stages of Pre-Rasterization Shaders section
constexpr uint32_t PrsShaderMask = 0
    | ((1 << ShaderStage::ShaderStageVertex)
    |  (1 << ShaderStage::ShaderStageTessControl)
    |  (1 << ShaderStage::ShaderStageTessEval)
    |  (1 << ShaderStage::ShaderStageGeometry));

// =====================================================================================================================
// The shader stages of Fragment Shader (Post-Rasterization) section
constexpr uint32_t FgsShaderMask = (1 << ShaderStage::ShaderStageFragment);

// =====================================================================================================================
// Helper function used to check whether a specific dynamic state is set
static bool IsDynamicStateEnabled(const uint32_t dynamicStateFlags, const DynamicStatesInternal internalState)
{
    return dynamicStateFlags & (1 << static_cast<uint32_t>(internalState));
}

// =====================================================================================================================
// Builds app profile key and applies profile options.
static void ApplyProfileOptions(
    const Device*                pDevice,
    const ShaderStage            stage,
    const Pal::ShaderHash        shaderHash,
    const size_t                 shaderSize,
    Vkgc::PipelineOptions*       pPipelineOptions,
    Vkgc::PipelineShaderInfo*    pShaderInfo,
    PipelineOptimizerKey*        pProfileKey,
    Vkgc::NggState*              pNggState
    )
{
    auto&    settings  = pDevice->GetRuntimeSettings();
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
        shaderKey.codeHash = shaderHash;
    }
    shaderKey.codeSize = shaderSize;

    // Override the compile parameters based on any app profile
    const auto* pShaderOptimizer = pDevice->GetShaderOptimizer();
    pShaderOptimizer->OverrideShaderCreateInfo(*pProfileKey, stage, options);
}

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
    , m_uberFetchShaderInfoFormatMap(8, pPhysicalDevice->Manager()->VkInstance()->Allocator())
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
void PipelineCompiler::DestroyPipelineBinaryCache()
{
    if (m_pBinaryCache != nullptr)
    {
        m_pBinaryCache->Destroy();
        m_pBinaryCache = nullptr;
    }
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
    case Pal::GfxIpLevel::GfxIp10_3:
        m_gfxIp.major = 10;
        m_gfxIp.minor = 3;
        break;

    default:
        VK_NEVER_CALLED();
        break;
    }

    m_gfxIp.stepping = info.gfxStepping;

    // Create compiler objects
    VkResult result = VK_SUCCESS;

    Vkgc::ICache* pCacheAdapter = nullptr;
    if ((result == VK_SUCCESS) &&
        ((settings.usePalPipelineCaching) ||
         (m_pPhysicalDevice->VkInstance()->GetDevModeMgr() != nullptr)))
    {
        m_pBinaryCache = PipelineBinaryCache::Create(
                m_pPhysicalDevice->VkInstance()->GetAllocCallbacks(),
                m_pPhysicalDevice->GetPlatformKey(),
                m_gfxIp,
                settings,
                m_pPhysicalDevice->PalDevice()->GetCacheFilePath(),
#if ICD_GPUOPEN_DEVMODE_BUILD
                m_pPhysicalDevice->VkInstance()->GetDevModeMgr(),
#endif
                0,
                nullptr,
                settings.enableOnDiskInternalPipelineCaches);

        // This isn't a terminal failure, the device can continue without the pipeline cache if need be.
        VK_ALERT(m_pBinaryCache == nullptr);
        if (m_pBinaryCache != nullptr)
        {
            pCacheAdapter = m_pBinaryCache->GetCacheAdapter();
        }
    }

    if (result == VK_SUCCESS)
    {
        result = m_compilerSolutionLlpc.Initialize(m_gfxIp, info.gfxLevel, pCacheAdapter);
    }

    if (result == VK_SUCCESS)
    {
        if (settings.enableUberFetchShader)
        {
            m_uberFetchShaderInfoFormatMap.Init();

            result = InitializeUberFetchShaderFormatTable(m_pPhysicalDevice, &m_uberFetchShaderInfoFormatMap);
        }
    }

    return result;
}

// =====================================================================================================================
// Destroys all compiler instance.
void PipelineCompiler::Destroy()
{
    m_compilerSolutionLlpc.Destroy();

    DestroyPipelineBinaryCache();
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

    char replaceFileName[Pal::MaxPathStrLen] = {};
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

    if ((pSettings->shaderReplaceMode == ShaderReplaceShaderHash) ||
        (pSettings->shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
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
// Check whether the shader module is valid
bool PipelineCompiler::IsValidShaderModule(
    const ShaderModuleHandle* pShaderModule) const
{
    bool isValid = false;

    isValid |= (pShaderModule->pLlpcShaderModule != nullptr);

    return isValid;
}

// =====================================================================================================================
// Frees shader module memory
void PipelineCompiler::FreeShaderModule(
    ShaderModuleHandle* pShaderModule)
{
    m_compilerSolutionLlpc.FreeShaderModule(pShaderModule);
}

// =====================================================================================================================
// Replaces pipeline binary from external replacement file (<pipeline_name>_replace.elf)
template<class PipelineBuildInfo>
bool PipelineCompiler::ReplacePipelineBinary(
        const PipelineBuildInfo* pPipelineBuildInfo,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary,
        uint64_t                 hashCode64)
{
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    char fileName[Pal::MaxFileNameStrLen] = {};
    Vkgc::IPipelineDumper::GetPipelineName(pPipelineBuildInfo, fileName, sizeof(fileName), hashCode64);

    char replaceFileName[Pal::MaxPathStrLen] = {};
    int32_t length = Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf", settings.shaderReplaceDir, fileName);
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
    uint32_t     pipelineIndex,
    const void*  pPipelineBinary,
    size_t       pipelineBinarySize)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    char replaceFileName[Pal::MaxPathStrLen] = {};
    if (pipelineIndex > 0)
    {
        Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/" "0x%016" PRIX64 "_replace.txt.%u",
            settings.shaderReplaceDir, pipelineHash, pipelineIndex);
    }
    else
    {
        Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/" "0x%016" PRIX64 "_replace.txt",
            settings.shaderReplaceDir, pipelineHash);
    }

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
    FreeCompilerBinary*          pFreeCompilerBinary,
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
            cacheResult = m_pBinaryCache->QueryPipelineBinary(pCacheId, 0, &query);
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
        *pFreeCompilerBinary = FreeWithInstanceAllocator;
        cacheResult = Util::Result::Success;
        m_cacheHits++;
    }

    return cacheResult;
}

// =====================================================================================================================
// Creates partial pipeline binary.
VkResult PipelineCompiler::CreatePartialPipelineBinary(
    uint32_t                             deviceIdx,
    void*                                pShaderModuleData,
    Vkgc::ShaderModuleEntryData*         pShaderModuleEntryData,
    const Vkgc::ResourceMappingRootNode* pResourceMappingNode,
    uint32_t                             mappingNodeCount,
    Vkgc::ColorTarget*                   pColorTarget)
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
    Device*                           pDevice,
    uint32_t                          deviceIdx,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    size_t*                           pPipelineBinarySize,
    const void**                      ppPipelineBinary,
    Util::MetroHash::Hash*            pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    bool                   shouldCompile = true;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();

    int64_t compileTime = 0;
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* moduleDataBaks[ShaderStage::ShaderStageGfxCount];
    ShaderModuleHandle shaderModuleReplaceHandles[ShaderStage::ShaderStageGfxCount];
    bool shaderModuleReplaced = false;

    Vkgc::PipelineShaderInfo* shaderInfos[ShaderStage::ShaderStageGfxCount] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    if ((settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
        (settings.shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        if (ReplacePipelineBinary(&pCreateInfo->pipelineInfo, pPipelineBinarySize, ppPipelineBinary, pipelineHash))
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
            for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
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

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo, pipelineHash);
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
        hash.Update(pCreateInfo->pipelineInfo.dynamicVertexStride);
        hash.Update(m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash());

        hash.Finalize(pCacheId->bytes);

        cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
            &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->freeCompilerBinary, &pCreateInfo->pipelineFeedback);
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
                    shaderInfos,
                    pPipelineDumpHandle,
                    pipelineHash,
                    &compileTime);
            }

            if (result == VK_SUCCESS)
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;

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

    m_totalTimeSpent += shouldCompile ? compileTime : cacheTime;
    m_totalBinaries++;

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, 0, *ppPipelineBinary, *pPipelineBinarySize);
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
        for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
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
    Device*                          pDevice,
    uint32_t                         deviceIdx,
    PipelineCache*                   pPipelineCache,
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    size_t*                          pPipelineBinarySize,
    const void**                     ppPipelineBinary,
    Util::MetroHash::Hash*           pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();
    bool                   shouldCompile = true;

    pCreateInfo->pipelineInfo.deviceIndex = deviceIdx;

    int64_t compileTime = 0;
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* pModuleDataBak = nullptr;
    ShaderModuleHandle shaderModuleReplaceHandle = {};
    bool shaderModuleReplaced = false;

    if ((settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
        (settings.shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        if (ReplacePipelineBinary(&pCreateInfo->pipelineInfo, pPipelineBinarySize, ppPipelineBinary, pipelineHash))
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

    if (settings.enablePipelineDump)
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir                 = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines    = settings.dumpDuplicatePipelines;

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pComputeInfo = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo, pipelineHash);
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
        hash.Update(m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash());

        hash.Finalize(pCacheId->bytes);

        cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
            &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->freeCompilerBinary, &pCreateInfo->pipelineFeedback);
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

            if (result == VK_SUCCESS)
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;
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

    m_totalTimeSpent += shouldCompile ? compileTime : cacheTime;
    m_totalBinaries++;
    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, 0, *ppPipelineBinary, *pPipelineBinarySize);
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
void PipelineCompiler::GetPipelineCreationFeedback(
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
            (*ppPipelineCreationFeadbackCreateInfo)->pPipelineCreationFeedback->duration = 0;
            if ((*ppPipelineCreationFeadbackCreateInfo)->pPipelineStageCreationFeedbacks != nullptr)
            {
                for (uint32_t i = 0; i < (*ppPipelineCreationFeadbackCreateInfo)->pipelineStageCreationFeedbackCount; i++)
                {
                    (*ppPipelineCreationFeadbackCreateInfo)->pPipelineStageCreationFeedbacks[i].flags = 0;
                    (*ppPipelineCreationFeadbackCreateInfo)->pPipelineStageCreationFeedbacks[i].duration = 0;
                }
            }
            break;
        default:
            break;
        }
    }
}

// =====================================================================================================================
void PipelineCompiler::UpdatePipelineCreationFeedback(
    VkPipelineCreationFeedbackEXT*  pPipelineCreationFeedback,
    const PipelineCreationFeedback* pFeedbackFromCompiler)
{
    pPipelineCreationFeedback->flags = 0;
    if (pFeedbackFromCompiler->feedbackValid)
    {
        pPipelineCreationFeedback->flags |= VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT;

        if (pFeedbackFromCompiler->hitApplicationCache)
        {
            pPipelineCreationFeedback->flags |=
                VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT;
        }
        pPipelineCreationFeedback->duration = pFeedbackFromCompiler->duration;
    }
}

// =====================================================================================================================
VkResult PipelineCompiler::SetPipelineCreationFeedbackInfo(
    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo,
    uint32_t                                       stageCount,
    const VkPipelineShaderStageCreateInfo*         pStages,
    const PipelineCreationFeedback*                pPipelineFeedback,
    const PipelineCreationFeedback*                pStageFeedback)
{
    if (pPipelineCreationFeadbackCreateInfo != nullptr)
    {
        UpdatePipelineCreationFeedback(pPipelineCreationFeadbackCreateInfo->pPipelineCreationFeedback,
                                       pPipelineFeedback);

        auto *stageCreationFeedbacks = pPipelineCreationFeadbackCreateInfo->pPipelineStageCreationFeedbacks;
        if (stageCount == 0)
        {
            UpdatePipelineCreationFeedback(&stageCreationFeedbacks[0], pStageFeedback);
        }
        else
        {
            VK_ASSERT(stageCount <= ShaderStage::ShaderStageGfxCount);
            for (uint32_t i = 0; i < stageCount; ++i)
            {
                uint32_t feedbackStage = 0;
                VK_ASSERT(pStages[i].sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
                switch (pStages[i].stage)
                {
                    case VK_SHADER_STAGE_VERTEX_BIT:
                        feedbackStage = ShaderStage::ShaderStageVertex;
                        break;
                    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                        feedbackStage = ShaderStage::ShaderStageTessControl;
                        break;
                    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                        feedbackStage = ShaderStage::ShaderStageTessEval;
                        break;
                    case VK_SHADER_STAGE_GEOMETRY_BIT:
                        feedbackStage = ShaderStage::ShaderStageGeometry;
                        break;
                    case VK_SHADER_STAGE_FRAGMENT_BIT:
                        feedbackStage = ShaderStage::ShaderStageFragment;
                        break;
                    default:
                        VK_NEVER_CALLED();
                        break;
                }
                UpdatePipelineCreationFeedback(&stageCreationFeedbacks[i], &pStageFeedback[feedbackStage]);
            }
        }
    }
    return VK_SUCCESS;
}

// =====================================================================================================================
// Builds the description of the internal descriptor set used to represent the VB table for SC.  Returns the number
// of ResourceMappingNodes consumed by this function.  This function does not add the node that describes the top-level
// pointer to this set.
void BuildLlpcVertexInputDescriptors(
    const Device*                               pDevice,
    const VkPipelineVertexInputStateCreateInfo* pInput,
    VbBindingInfo*                              pVbInfo)
{
    VK_ASSERT(pVbInfo != nullptr);

    const uint32_t srdDwSize = pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);
    uint32_t activeBindings = 0;

    // Sort the strides by binding slot
    uint32_t strideByBindingSlot[Pal::MaxVertexBuffers] = {};

    for (uint32_t recordIndex = 0; recordIndex < pInput->vertexBindingDescriptionCount; ++recordIndex)
    {
        const VkVertexInputBindingDescription& record = pInput->pVertexBindingDescriptions[recordIndex];

        strideByBindingSlot[record.binding] = record.stride;
    }

    // Build the description of the VB table by inserting all of the active binding slots into it
    pVbInfo->bindingCount = 0;
    pVbInfo->bindingTableSize = 0;
    // Find the set of active vertex buffer bindings by figuring out which vertex attributes are consumed by the
    // pipeline.
    //
    // (Note that this ignores inputs eliminated by whole program optimization, but considering that we have not yet
    // compiled the shader and have not performed whole program optimization, this is the best we can do; it's a
    // chicken-egg problem).

    for (uint32_t aindex = 0; aindex < pInput->vertexAttributeDescriptionCount; ++aindex)
    {
        const VkVertexInputAttributeDescription& attrib = pInput->pVertexAttributeDescriptions[aindex];

        VK_ASSERT(attrib.binding < Pal::MaxVertexBuffers);

        bool isNotActiveBinding = ((1 << attrib.binding) & activeBindings) == 0;

        if (isNotActiveBinding)
        {
            // Write out the meta information that the VB binding manager needs from pipelines
            auto* pOutBinding = &pVbInfo->bindings[pVbInfo->bindingCount++];
            activeBindings |= (1 << attrib.binding);

            pOutBinding->slot = attrib.binding;
            pOutBinding->byteStride = strideByBindingSlot[attrib.binding];

            pVbInfo->bindingTableSize = Util::Max(pVbInfo->bindingTableSize, attrib.binding + 1);
        }
    }
}

// =====================================================================================================================
static void BuildRasterizationState(
    const VkPipelineRasterizationStateCreateInfo* pRs,
    const uint32_t                                dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*             pCreateInfo)
{
    if (pRs != nullptr)
    {
        EXTRACT_VK_STRUCTURES_1(
            rasterizationDepthClipState,
            PipelineRasterizationDepthClipStateCreateInfoEXT,
            PipelineRasterizationStateStreamCreateInfoEXT,
            static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pRs->pNext),
            PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
            PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT);

        pCreateInfo->pipelineInfo.vpState.depthClipEnable         = (pRs->depthClampEnable == VK_FALSE);
        pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = (pRs->rasterizerDiscardEnable != VK_FALSE);
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 48
        pCreateInfo->pipelineInfo.rsState.polygonMode             = pRs->polygonMode;
#endif
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 47
        pCreateInfo->pipelineInfo.rsState.cullMode                = pRs->cullMode;
        pCreateInfo->pipelineInfo.rsState.frontFace               = pRs->frontFace;
#endif
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 46
        pCreateInfo->pipelineInfo.rsState.depthBiasEnable         = pRs->depthBiasEnable;
#endif

        if (pPipelineRasterizationDepthClipStateCreateInfoEXT != nullptr)
        {
            pCreateInfo->pipelineInfo.vpState.depthClipEnable =
                pPipelineRasterizationDepthClipStateCreateInfoEXT->depthClipEnable;
        }

        if (pPipelineRasterizationStateStreamCreateInfoEXT != nullptr)
        {
            pCreateInfo->rasterizationStream = pPipelineRasterizationStateStreamCreateInfoEXT->rasterizationStream;
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt) == true)
        {
            pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = false;
        }
    }
}

// =====================================================================================================================
static void BuildMultisampleState(
    const Device*                               pDevice,
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    const uint32_t                              dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*           pCreateInfo)
{
    if (pMs != nullptr)
    {
        EXTRACT_VK_STRUCTURES_0(
            SampleLocations,
            PipelineSampleLocationsStateCreateInfoEXT,
            static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(pMs->pNext),
            PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);

        if (pMs->rasterizationSamples != 1)
        {
            uint32_t rasterizationSampleCount = pMs->rasterizationSamples;

            uint32_t subpassCoverageSampleCount = (pRenderPass != nullptr) ?
                pRenderPass->GetSubpassMaxSampleCount(subpass) :
                rasterizationSampleCount;

            uint32_t subpassColorSampleCount = (pRenderPass != nullptr) ?
                pRenderPass->GetSubpassColorSampleCount(subpass) :
                rasterizationSampleCount;

            // subpassCoverageSampleCount would be equal to zero if there are zero attachments.
            subpassCoverageSampleCount = (subpassCoverageSampleCount == 0) ?
                rasterizationSampleCount :
                subpassCoverageSampleCount;

            subpassColorSampleCount = (subpassColorSampleCount == 0) ?
                subpassCoverageSampleCount :
                subpassColorSampleCount;

            if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
            {
                pCreateInfo->pipelineInfo.rsState.perSampleShading =
                    ((subpassColorSampleCount * pMs->minSampleShading) > 1.0f);
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
        if (pPipelineSampleLocationsStateCreateInfoEXT != nullptr)
        {
            pCreateInfo->sampleLocationGridSize =
                pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationGridSize;
        }

        if (pCreateInfo->pipelineInfo.rsState.perSampleShading)
        {
            const RuntimeSettings&       settings   = pDevice->GetRuntimeSettings();
            if (!(pCreateInfo->sampleLocationGridSize.width > 1 || pCreateInfo->sampleLocationGridSize.height > 1
               ))
            {
                pCreateInfo->pipelineInfo.options.enableInterpModePatch = true;
            }
        }
    }
    else
    {
        pCreateInfo->pipelineInfo.rsState.numSamples = 1;
    }
}

// =====================================================================================================================
static void BuildViewportState(
    const Device*                            pDevice,
    const VkPipelineViewportStateCreateInfo* pVs,
    const uint32_t                           dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*        pCreateInfo)
{
    if (pVs != nullptr)
    {
    }
}

// =====================================================================================================================
static void BuildNggState(
    const Device*                     pDevice,
    const VkShaderStageFlagBits       activeStages,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const RuntimeSettings&       settings   = pDevice->GetRuntimeSettings();
    const Pal::DeviceProperties& deviceProp = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    if (deviceProp.gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
    {
        const bool hasGs   = activeStages & VK_SHADER_STAGE_GEOMETRY_BIT;
        const bool hasTess = activeStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

        const GraphicsPipelineType pipelineType =
            hasTess ? (hasGs ? GraphicsPipelineTypeTessGs : GraphicsPipelineTypeTess) :
                      (hasGs ? GraphicsPipelineTypeGs : GraphicsPipelineTypeVsFs);

        pCreateInfo->pipelineInfo.nggState.enableNgg =
            Util::TestAnyFlagSet(settings.enableNgg, pipelineType);
        pCreateInfo->pipelineInfo.nggState.enableGsUse = Util::TestAnyFlagSet(
            settings.enableNgg,
            GraphicsPipelineTypeGs | GraphicsPipelineTypeTessGs);
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 44
        pCreateInfo->pipelineInfo.nggState.forceNonPassthrough   = settings.nggForceCullingMode;
#else
        pCreateInfo->pipelineInfo.nggState.forceCullingMode         = settings.nggForceCullingMode;
#endif
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 47
        pCreateInfo->pipelineInfo.nggState.alwaysUsePrimShaderTable = settings.nggAlwaysUsePrimShaderTable;
#endif
        pCreateInfo->pipelineInfo.nggState.compactMode              =
            static_cast<Vkgc::NggCompactMode>(settings.nggCompactionMode);

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 45
        pCreateInfo->pipelineInfo.nggState.enableFastLaunch       = false;
#endif
        pCreateInfo->pipelineInfo.nggState.enableVertexReuse         = false;
        pCreateInfo->pipelineInfo.nggState.enableBackfaceCulling     = settings.nggEnableBackfaceCulling;
        pCreateInfo->pipelineInfo.nggState.enableFrustumCulling      = settings.nggEnableFrustumCulling;
        pCreateInfo->pipelineInfo.nggState.enableBoxFilterCulling    = settings.nggEnableBoxFilterCulling;
        pCreateInfo->pipelineInfo.nggState.enableSphereCulling       = settings.nggEnableSphereCulling;
        pCreateInfo->pipelineInfo.nggState.enableSmallPrimFilter     = settings.nggEnableSmallPrimFilter;
        pCreateInfo->pipelineInfo.nggState.enableCullDistanceCulling = settings.nggEnableCullDistanceCulling;

        if (settings.disableNggCulling)
        {
            uint32_t disableNggCullingMask = (settings.disableNggCulling & DisableNggCullingAlways);
            uint32_t numTargets = 0;

            for (uint32_t i = 0; i < Pal::MaxColorTargets; ++i)
            {
                numTargets += pCreateInfo->pipelineInfo.cbState.target[i].channelWriteMask ? 1 : 0;
            }

            switch (numTargets)
            {
            case 0:
                disableNggCullingMask |= (settings.disableNggCulling & DisableNggCullingDepthOnly);
                break;
            case 1:
                disableNggCullingMask |= (settings.disableNggCulling & DisableNggCullingSingleColorAttachment);
                break;
            default:
                disableNggCullingMask |= (settings.disableNggCulling & DisableNggCullingMultipleColorAttachments);
                break;
            }

            if (disableNggCullingMask != 0)
            {
                CompilerSolution::DisableNggCulling(&pCreateInfo->pipelineInfo.nggState);
            }
        }

        pCreateInfo->pipelineInfo.nggState.backfaceExponent = settings.nggBackfaceExponent;
        pCreateInfo->pipelineInfo.nggState.subgroupSizing   =
            static_cast<Vkgc::NggSubgroupSizingType>(settings.nggSubgroupSizing);

        pCreateInfo->pipelineInfo.nggState.primsPerSubgroup = settings.nggPrimsPerSubgroup;
        pCreateInfo->pipelineInfo.nggState.vertsPerSubgroup = settings.nggVertsPerSubgroup;
    }
}

// =====================================================================================================================
static void BuildPipelineShaderInfo(
    const Device*                                 pDevice,
    const ShaderStageInfo*                        pShaderInfoIn,
    Vkgc::PipelineShaderInfo*                     pShaderInfoOut,
    Vkgc::PipelineOptions*                        pPipelineOptions,
    PipelineOptimizerKey*                         pOptimizerKey,
    Vkgc::NggState*                               pNggState
    )
{
    if (pShaderInfoIn != nullptr)
    {
        const Pal::DeviceProperties& deviceProp = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
        const Vkgc::ShaderStage      stage      = pShaderInfoIn->stage;

        PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

        pShaderInfoOut->pModuleData         = ShaderModule::GetFirstValidShaderData(pShaderInfoIn->pModuleHandle);
        pShaderInfoOut->pSpecializationInfo = pShaderInfoIn->pSpecializationInfo;
        pShaderInfoOut->pEntryTarget        = pShaderInfoIn->pEntryPoint;
        pShaderInfoOut->entryStage          = stage;

        pCompiler->ApplyDefaultShaderOptions(stage,
                                             &pShaderInfoOut->options
                                             );

        if ((pShaderInfoIn->flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT) != 0)
        {
            pShaderInfoOut->options.allowVaryWaveSize = true;
        }

        ApplyProfileOptions(pDevice,
                            stage,
                            pShaderInfoIn->codeHash,
                            pShaderInfoIn->codeSize,
                            pPipelineOptions,
                            pShaderInfoOut,
                            pOptimizerKey,
                            pNggState
                            );

    }
}

// =====================================================================================================================
static VkResult BuildPipelineResourceMapping(
    const Device*                     pDevice,
    const PipelineLayout*             pLayout,
    const uint32_t                    stageMask,
    VbInfo*                           pVbInfo,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    VkResult result = VK_SUCCESS;

    if ((pLayout != nullptr) && (pLayout->GetPipelineInfo()->mappingBufferSize > 0))
    {

        size_t genericMappingBufferSize = pLayout->GetPipelineInfo()->mappingBufferSize;

        size_t tempBufferSize = genericMappingBufferSize + pCreateInfo->mappingBufferSize;
        pCreateInfo->pTempBuffer = pDevice->VkInstance()->AllocMem(tempBufferSize,
                                                                VK_DEFAULT_MEM_ALIGN,
                                                                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

        if (pCreateInfo->pTempBuffer == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            pCreateInfo->pMappingBuffer = Util::VoidPtrInc(pCreateInfo->pTempBuffer, genericMappingBufferSize);

            // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
            // fields of resource mapping nodes are unused for certain node types. We must initialize
            // them to zeros.
            memset(pCreateInfo->pTempBuffer, 0, tempBufferSize);

            // Build the LLPC resource mapping description. This data contains things about how shader
            // inputs like descriptor set bindings are communicated to this pipeline in a form that
            // LLPC can understand.
            result = pLayout->BuildLlpcPipelineMapping(stageMask,
                                                       pVbInfo,
                                                       pCreateInfo->pTempBuffer,
                                                       pCreateInfo->pipelineInfo.enableUberFetchShader,
                                                       &pCreateInfo->pipelineInfo.resourceMapping);
        }
    }

    return result;
}

// =====================================================================================================================
template <uint32_t shaderMask>
static void BuildPipelineShadersInfo(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{

    pCreateInfo->flags = pIn->flags;

    pDevice->GetCompiler(DefaultDeviceIndex)->ApplyPipelineOptions(pDevice, pIn->flags, &pCreateInfo->pipelineInfo.options);

    Vkgc::PipelineShaderInfo* ppShaderInfoOut[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (pShaderInfo->stages[stage].pModuleHandle != nullptr)
        {
            BuildPipelineShaderInfo(pDevice,
                                    &pShaderInfo->stages[stage],
                                    ppShaderInfoOut[stage],
                                    &pCreateInfo->pipelineInfo.options,
                                    &pCreateInfo->pipelineProfileKey,
                                    &pCreateInfo->pipelineInfo.nggState
            );
        }
    }

    pCreateInfo->compilerType = pDevice->GetCompiler(DefaultDeviceIndex)->CheckCompilerType(&pCreateInfo->pipelineInfo);

    if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
    {
        pCreateInfo->pipelineInfo.enableUberFetchShader = false;
    }

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (((shaderMask & (1 << stage)) != 0) && (pShaderInfo->stages[stage].pModuleHandle != nullptr))
        {
            ppShaderInfoOut[stage]->pModuleData =
                ShaderModule::GetShaderData(pCreateInfo->compilerType, pShaderInfo->stages[stage].pModuleHandle);
        }
    }
}

// =====================================================================================================================
static void BuildColorBlendState(
    const Device*                              pDevice,
    const VkPipelineColorBlendStateCreateInfo* pCb,
    const RenderPass*                          pRenderPass,
    const uint32_t                             subpass,
    GraphicsPipelineBinaryCreateInfo*          pCreateInfo)
{
    bool dualSourceBlendEnabled = false;

    if (pCb != nullptr)
    {
        const uint32_t numColorTargets = Util::Min(pCb->attachmentCount, Pal::MaxColorTargets);

        for (uint32_t i = 0; i < numColorTargets; ++i)
        {
            const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];
            auto pLlpcCbDst = &pCreateInfo->pipelineInfo.cbState.target[i];

            VkFormat cbFormat = VK_FORMAT_UNDEFINED;

            if (pRenderPass != nullptr)
            {
                cbFormat = pRenderPass->GetColorAttachmentFormat(subpass, i);
            }

            // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
            // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
            // disable shader writes through that target. There is one exception for alphaToCoverageEnable
            // and attachment zero, which can be set to VK_ATTACHMENT_UNUSED.
            if ((cbFormat != VK_FORMAT_UNDEFINED) || (i == 0u))
            {
                VkFormat renderPassFormat = ((pRenderPass != nullptr) ?
                                                pRenderPass->GetAttachmentDesc(i).format :
                                                VK_FORMAT_UNDEFINED);

                pLlpcCbDst->format = (cbFormat != VK_FORMAT_UNDEFINED) ?
                                      cbFormat : renderPassFormat;

                pLlpcCbDst->blendEnable = (src.blendEnable == VK_TRUE);
                pLlpcCbDst->blendSrcAlphaToColor =
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcAlphaBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstAlphaBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcColorBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstColorBlendFactor);
                pLlpcCbDst->channelWriteMask = src.colorWriteMask;
            }
        }

        dualSourceBlendEnabled = GraphicsPipelineCommon::GetDualSourceBlendEnableState(pDevice, pCb);
    }

    pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable = dualSourceBlendEnabled;

    VkFormat dbFormat = VK_FORMAT_UNDEFINED;

    if (pRenderPass != nullptr)
    {
        dbFormat = pRenderPass->GetDepthStencilAttachmentFormat(subpass);
    }

    pCreateInfo->dbFormat = dbFormat;
}

// =====================================================================================================================
static void BuildVertexInputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo,
    VbBindingInfo*                      pVbInfo)
{
    VK_ASSERT(pIn->pVertexInputState);

    pCreateInfo->pipelineInfo.pVertexInput               = pIn->pVertexInputState;
    pCreateInfo->pipelineInfo.iaState.topology           = pIn->pInputAssemblyState->topology;
    pCreateInfo->pipelineInfo.iaState.disableVertexReuse = false;

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInputBindingStrideExt) == true)
    {
        pCreateInfo->pipelineInfo.dynamicVertexStride = true;
    }

    if (pDevice->GetRuntimeSettings().enableUberFetchShader)
    {
        pCreateInfo->pipelineInfo.enableUberFetchShader = true;
    }

    BuildLlpcVertexInputDescriptors(pDevice, pIn->pVertexInputState, pVbInfo);
}

// =====================================================================================================================
static void BuildPreRasterizationShaderState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const uint32_t                         dynamicStateFlags,
    const VkShaderStageFlagBits            activeStages,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildRasterizationState(pIn->pRasterizationState, dynamicStateFlags, pCreateInfo);

    if (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false)
    {
        BuildViewportState(pDevice, pIn->pViewportState, dynamicStateFlags, pCreateInfo);
    }

    BuildNggState(pDevice, activeStages, pCreateInfo);

    if (activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
    {
        EXTRACT_VK_STRUCTURES_1(
            Tess,
            PipelineTessellationStateCreateInfo,
            PipelineTessellationDomainOriginStateCreateInfo,
            pIn->pTessellationState,
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

    BuildPipelineShadersInfo<PrsShaderMask>(pDevice, pIn, pShaderInfo, pCreateInfo);
}

// =====================================================================================================================
static void BuildFragmentShaderState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const uint32_t                         dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildMultisampleState(pDevice, pIn->pMultisampleState, pRenderPass, pIn->subpass, dynamicStateFlags, pCreateInfo);

    BuildPipelineShadersInfo<FgsShaderMask>(pDevice, pIn, pShaderInfo, pCreateInfo);

    // Handle VkPipelineDepthStencilStateCreateInfo
    if (pIn->pDepthStencilState != nullptr)
    {
    }
}

// =====================================================================================================================
static void BuildFragmentOutputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildColorBlendState(pDevice,
                         pIn->pColorBlendState,
                         pRenderPass, pIn->subpass, pCreateInfo);

    pCreateInfo->pipelineInfo.iaState.enableMultiView =
        (pRenderPass != nullptr) ? pRenderPass->IsMultiviewEnabled() :
                                   false;

    // Handle VkPipelineDepthStencilStateCreateInfo
    if (pIn->pDepthStencilState != nullptr)
    {
    }
}

// =====================================================================================================================
static VkResult BuildUberFetchShaderInternalData(
    const Device*                     pDevice,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    VbInfo*                           pVbInfo)
{
    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);
    VK_ASSERT(pCreateInfo->pipelineInfo.enableUberFetchShader);

    return pDefaultCompiler->BuildUberFetchShaderInternalData(pCreateInfo->compilerType,
                                                              pCreateInfo->pipelineInfo.pVertexInput,
                                                              pCreateInfo->pipelineInfo.dynamicVertexStride,
                                                              &pVbInfo->uberFetchShaderBuffer);
}
// =====================================================================================================================
static VkResult BuildExecutablePipelineState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo,
    VbInfo*                             pVbInfo)
{
    const RuntimeSettings& settings         = pDevice->GetRuntimeSettings();
    PipelineCompiler*      pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);

    if (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == true)
    {
        pCreateInfo->pipelineInfo.rsState.numSamples            = 1;
        pCreateInfo->pipelineInfo.rsState.perSampleShading      = false;
        pCreateInfo->pipelineInfo.rsState.samplePatternIdx      = 0;
        pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = false;
        pCreateInfo->pipelineInfo.options.enableInterpModePatch = false;

        memset(pCreateInfo->pipelineInfo.cbState.target, 0, sizeof(pCreateInfo->pipelineInfo.cbState.target));

        pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable = false;
    }

    Vkgc::PipelineShaderInfo* shaderInfos[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    uint32_t availableStageMask = 0;

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (shaderInfos[stage]->pModuleData != nullptr)
        {
            availableStageMask |= (1 << stage);
        }
    }

    VkResult result = BuildPipelineResourceMapping(pDevice, pLayout, availableStageMask, pVbInfo, pCreateInfo);

    if ((result == VK_SUCCESS) && pCreateInfo->pipelineInfo.enableUberFetchShader)
    {
        VK_ASSERT(pVbInfo->uberFetchShaderBuffer.userDataOffset > 0);
        result = BuildUberFetchShaderInternalData(pDevice, pCreateInfo, pVbInfo);
    }

    return result;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    const Device*                                   pDevice,
    const VkGraphicsPipelineCreateInfo*             pIn,
    const GraphicsPipelineShaderStageInfo*          pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*               pCreateInfo,
    VbInfo*                                         pVbInfo)
{
    VK_ASSERT(pIn != nullptr);

    VkResult result = VK_SUCCESS;

    const VkGraphicsPipelineCreateInfo* pGraphicsPipelineCreateInfo = pIn;

    VkShaderStageFlagBits activeStages = GraphicsPipelineCommon::GetActiveShaderStages(
                                             pIn
                                             );

    uint32_t dynamicStateFlags = GraphicsPipelineCommon::GetDynamicStateFlags(
                                     pIn->pDynamicState
                                                      );

    BuildVertexInputInterfaceState(pDevice, pIn, dynamicStateFlags, pCreateInfo, &pVbInfo->bindingInfo);

    BuildPreRasterizationShaderState(pDevice, pIn, pShaderInfo, dynamicStateFlags, activeStages, pCreateInfo);

    const bool enableRasterization =
        (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt);

    if (enableRasterization)
    {
        BuildFragmentShaderState(pDevice, pIn, pShaderInfo, dynamicStateFlags, pCreateInfo);

        BuildFragmentOutputInterfaceState(pDevice, pIn, pCreateInfo);
    }

    {
        result = BuildExecutablePipelineState(pDevice, pIn, dynamicStateFlags, pCreateInfo, pVbInfo);
    }

    return result;
}

// =====================================================================================================================
// Checks which compiler is used
template<class PipelineBuildInfo>
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const PipelineBuildInfo* pPipelineBuildInfo)
{
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
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT) ||
        pDevice->GetEnabledFeatures().scalarBlockLayout)
    {
        pOptions->scalarBlockLayout = true;
    }

    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pOptions->robustBufferAccess = true;
    }

    // Setup shadow descriptor table pointer
    const auto& info = m_pPhysicalDevice->PalProperties();
    pOptions->shadowDescriptorTableUsage = (info.gpuMemoryProperties.flags.shadowDescVaSupport ?
                                            Vkgc::ShadowDescriptorTableUsage::Enable :
                                            Vkgc::ShadowDescriptorTableUsage::Disable);
    pOptions->shadowDescriptorTablePtrHigh =
          static_cast<uint32_t>(info.gpuMemoryProperties.shadowDescTableVaStart >> 32);

    // Apply runtime settings from device
    const auto& settings = m_pPhysicalDevice->GetRuntimeSettings();
    pOptions->enableRelocatableShaderElf = settings.enableRelocatableShaders;
    pOptions->disableImageResourceCheck = settings.disableImageResourceTypeCheck;

    if (pDevice->GetEnabledFeatures().robustBufferAccessExtended)
    {
        pOptions->extendedRobustness.robustBufferAccess = true;
    }
    if (pDevice->GetEnabledFeatures().robustImageAccessExtended)
    {
        pOptions->extendedRobustness.robustImageAccess = true;
    }
    if (pDevice->GetEnabledFeatures().nullDescriptorExtended)
    {
        pOptions->extendedRobustness.nullDescriptor = true;
    }
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertComputePipelineInfo(
    const Device*                                   pDevice,
    const VkComputePipelineCreateInfo*              pIn,
    const ComputePipelineShaderStageInfo*           pShaderInfo,
    ComputePipelineBinaryCreateInfo*                pCreateInfo)
{
    VkResult result    = VK_SUCCESS;

    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    PipelineLayout* pLayout = nullptr;

    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }

    pCreateInfo->flags  = pIn->flags;

    ApplyPipelineOptions(pDevice, pIn->flags, &pCreateInfo->pipelineInfo.options);

    pCreateInfo->pipelineInfo.cs.pModuleData =
        ShaderModule::GetFirstValidShaderData(pShaderInfo->stage.pModuleHandle);
    pCreateInfo->pipelineInfo.cs.pSpecializationInfo = pShaderInfo->stage.pSpecializationInfo;
    pCreateInfo->pipelineInfo.cs.pEntryTarget        = pShaderInfo->stage.pEntryPoint;
    pCreateInfo->pipelineInfo.cs.entryStage          = Vkgc::ShaderStageCompute;

    if ((pShaderInfo->stage.flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT) != 0)
    {
        pCreateInfo->pipelineInfo.cs.options.allowVaryWaveSize = true;
    }

    if ((pLayout != nullptr) && (pLayout->GetPipelineInfo()->mappingBufferSize > 0))
    {

        size_t genericMappingBufferSize = pLayout->GetPipelineInfo()->mappingBufferSize;

        size_t tempBufferSize    = genericMappingBufferSize + pCreateInfo->mappingBufferSize;
        pCreateInfo->pTempBuffer = pInstance->AllocMem(tempBufferSize,
                                                       VK_DEFAULT_MEM_ALIGN,
                                                       VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

        if (pCreateInfo->pTempBuffer == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            pCreateInfo->pMappingBuffer = Util::VoidPtrInc(pCreateInfo->pTempBuffer, genericMappingBufferSize);

            // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
            // fields of resource mapping nodes are unused for certain node types. We must initialize
            // them to zeroes.
            memset(pCreateInfo->pTempBuffer, 0, tempBufferSize);

            // Build the LLPC resource mapping description. This data contains things about how shader
            // inputs like descriptor set bindings are communicated to this pipeline in a form that
            // LLPC can understand.
            result = pLayout->BuildLlpcPipelineMapping(Vkgc::ShaderStageComputeBit,
                                                       nullptr,
                                                       pCreateInfo->pTempBuffer,
                                                       false,
                                                       &pCreateInfo->pipelineInfo.resourceMapping);
        }
    }

    pCreateInfo->compilerType = CheckCompilerType(&pCreateInfo->pipelineInfo);
    pCreateInfo->pipelineInfo.cs.pModuleData =
        ShaderModule::GetShaderData(pCreateInfo->compilerType, pShaderInfo->stage.pModuleHandle);;

    ApplyDefaultShaderOptions(ShaderStage::ShaderStageCompute,
                              &pCreateInfo->pipelineInfo.cs.options
                              );

    ApplyProfileOptions(pDevice,
                        ShaderStage::ShaderStageCompute,
                        pShaderInfo->stage.codeHash,
                        pShaderInfo->stage.codeSize,
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
    case ShaderStage::ShaderStageVertex:
        pShaderOptions->waveSize = settings.vsWaveSize;
        break;
    case ShaderStage::ShaderStageTessControl:
        pShaderOptions->waveSize = settings.tcsWaveSize;
        break;
    case ShaderStage::ShaderStageTessEvaluation:
        pShaderOptions->waveSize = settings.tesWaveSize;
        break;
    case ShaderStage::ShaderStageGeometry:
        pShaderOptions->waveSize = settings.gsWaveSize;
        break;
    case ShaderStage::ShaderStageFragment:
        pShaderOptions->waveSize = settings.fsWaveSize;
        break;
    case ShaderStage::ShaderStageCompute:
        pShaderOptions->waveSize = settings.csWaveSize;
        break;
    default:
        break;
    }

    pShaderOptions->wgpMode       = ((settings.enableWgpMode & (1 << stage)) != 0);
    pShaderOptions->waveBreakSize = static_cast<Vkgc::WaveBreakSize>(settings.waveBreakSize);

}

// =====================================================================================================================
// Free compute pipeline binary
void PipelineCompiler::FreeComputePipelineBinary(
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    const void*                      pPipelineBinary,
    size_t                           binarySize)
{
    if (pCreateInfo->freeCompilerBinary == FreeWithCompiler)
    {
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            m_compilerSolutionLlpc.FreeComputePipelineBinary(pPipelineBinary, binarySize);
        }

    }
    else if (pCreateInfo->freeCompilerBinary == FreeWithInstanceAllocator)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
}

// =====================================================================================================================
// Free graphics pipeline binary
void PipelineCompiler::FreeGraphicsPipelineBinary(
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    const void*                       pPipelineBinary,
    size_t                            binarySize)
{
    if (pCreateInfo->freeCompilerBinary == FreeWithCompiler)
    {
        if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
        {
            m_compilerSolutionLlpc.FreeGraphicsPipelineBinary(pPipelineBinary, binarySize);
        }

    }
    else if (pCreateInfo->freeCompilerBinary == FreeWithInstanceAllocator)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
}

// =====================================================================================================================
// Free the temp memories in compute pipeline create info
void PipelineCompiler::FreeComputePipelineCreateInfo(
    ComputePipelineBinaryCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pTempBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pTempBuffer);
        pCreateInfo->pTempBuffer = nullptr;
    }
}

// =====================================================================================================================
// Free the temp memories in graphics pipeline create info
void PipelineCompiler::FreeGraphicsPipelineCreateInfo(
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pTempBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pTempBuffer);
        pCreateInfo->pTempBuffer = nullptr;
    }
}

// =====================================================================================================================
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

// =====================================================================================================================
VkResult PipelineCompiler::BuildUberFetchShaderInternalData(
    PipelineCompilerType                        compilerType,
    const VkPipelineVertexInputStateCreateInfo* pVertexInput,
    bool                                        isDynamicStride,
   UberFetchShaderBufferInfo*                   pFetchShaderBufferInfo)
{

    VkResult result = VK_SUCCESS;
    if (compilerType == PipelineCompilerTypeLlpc)
    {
        VK_NOT_IMPLEMENTED;
    }

    return result;
}

}
