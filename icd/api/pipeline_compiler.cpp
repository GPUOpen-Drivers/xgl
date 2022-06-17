/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_graphics_pipeline_library.h"
#include "include/pipeline_compiler.h"
#include <vector>

#include "palFile.h"
#include "palHashSetImpl.h"
#include "palListImpl.h"

#include "include/pipeline_binary_cache.h"

#include "palElfReader.h"
#include "palPipelineAbiReader.h"
#include "palPipelineAbiProcessorImpl.h"

#include "llpc.h"

#include <inttypes.h>

namespace vk
{

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
static bool SupportInternalModuleCache(
    const PhysicalDevice* pDevice,
    const uint32_t        compilerMask)
{
    bool supportInternalModuleCache = pDevice->GetRuntimeSettings().enableEarlyCompile;

#if ICD_X86_BUILD
    supportInternalModuleCache = false;
#endif

    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        // LLPC always defers SPIRV conversion, we needn't cache the result
        supportInternalModuleCache = false;
    }

    return supportInternalModuleCache;
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
    , m_shaderModuleHandleMap(8, pPhysicalDevice->Manager()->VkInstance()->Allocator())
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
        result = PalToVkResult(m_shaderModuleHandleMap.Init());
    }

    if (result == VK_SUCCESS)
    {
        result = PalToVkResult(m_uberFetchShaderInfoFormatMap.Init());
    }

    if (result == VK_SUCCESS)
    {
        result = InitializeUberFetchShaderFormatTable(m_pPhysicalDevice, &m_uberFetchShaderInfoFormatMap);
    }

    if (result == VK_SUCCESS)
    {
        uint32_t threadCount = settings.deferCompileOptimizedPipeline ? settings.deferCompileThreadCount : 0;
        m_deferCompileMgr.Init(threadCount, m_pPhysicalDevice->VkInstance()->Allocator());
    }

    return result;
}

// =====================================================================================================================
// Destroys all compiler instance.
void PipelineCompiler::Destroy()
{
    m_compilerSolutionLlpc.Destroy();

    DestroyPipelineBinaryCache();

    if (m_pPhysicalDevice->GetRuntimeSettings().enableEarlyCompile)
    {
        Util::MutexAuto mutexLock(&m_shaderModuleCacheLock);
        for (auto it = m_shaderModuleHandleMap.Begin(); it.Get() != nullptr; it.Next())
        {
            VK_ASSERT(it.Get()->value.pRefCount != nullptr);

            if (*(it.Get()->value.pRefCount) == 1)
            {
                // Force use un-lock version of FreeShaderModule.
                auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
                pInstance->FreeMem(it.Get()->value.pRefCount);
                it.Get()->value.pRefCount = nullptr;
                FreeShaderModule(&it.Get()->value);
            }
            else
            {
                (*(it.Get()->value.pRefCount))--;
            }
        }
        m_shaderModuleHandleMap.Reset();
    }
}

// =====================================================================================================================
// Creates shader cache object.
VkResult PipelineCompiler::CreateShaderCache(
    const void*   pInitialData,
    size_t        initialDataSize,
    uint32_t      expectedEntries,
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

    char replaceFileName[Util::MaxPathStrLen] = {};
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
// Generates shader module cache hash ID
Util::MetroHash::Hash PipelineCompiler::GetShaderModuleCacheHash(
    const VkShaderModuleCreateFlags flags,
    const uint32_t                  compilerMask,
    const Util::MetroHash::Hash&    uniqueHash)
{
    Util::MetroHash128 hasher;
    Util::MetroHash::Hash hash;
    hasher.Update(compilerMask);
    hasher.Update(uniqueHash);
    hasher.Update(flags);
    hasher.Update(m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash());
    hasher.Finalize(hash.bytes);
    return hash;
}

// =====================================================================================================================
// Loads shader module from cache, include both run-time cache and binary cache
VkResult PipelineCompiler::LoadShaderModuleFromCache(
    const Device*                   pDevice,
    const VkShaderModuleCreateFlags flags,
    const uint32_t                  compilerMask,
    const Util::MetroHash::Hash&    uniqueHash,
    PipelineBinaryCache*            pBinaryCache,
    PipelineCreationFeedback*       pFeedback,
    ShaderModuleHandle*             pShaderModule)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;

    const bool supportInternalModuleCache = SupportInternalModuleCache(m_pPhysicalDevice, compilerMask);

    VK_ASSERT(pShaderModule->pRefCount == nullptr);

    if ((pBinaryCache != nullptr) || supportInternalModuleCache)
    {
        const Util::MetroHash::Hash shaderModuleCacheHash =
            GetShaderModuleCacheHash(flags, compilerMask, uniqueHash);

        const void*  pShaderModuleBinary = nullptr;
        size_t       shaderModuleSize    = 0;
        Util::Result cacheResult         = Util::Result::NotFound;
        bool hitApplicationCache         = false;

        // 1. Look up in internal cache m_shaderModuleHandleMap.
        if (supportInternalModuleCache)
        {
            Util::MutexAuto mutexLock(&m_shaderModuleCacheLock);

            ShaderModuleHandle* pHandle = m_shaderModuleHandleMap.FindKey(shaderModuleCacheHash);
            if (pHandle != nullptr)
            {
                VK_ASSERT(pHandle->pRefCount != nullptr);
                (*(pHandle->pRefCount))++;
                *pShaderModule = *pHandle;
                result         = VK_SUCCESS;
                cacheResult    = Util::Result::Success;
            }
        }

        // 2. Look up in application cache pBinaryCache.  Only query availability when hits in m_shaderModuleHandleMap.
        if (pBinaryCache != nullptr)
        {

            cacheResult = hitApplicationCache ? Util::Result::Success : cacheResult;
        }

        // 3. Look up in internal cache m_pBinaryCache
        if ((cacheResult != Util::Result::Success) && (m_pBinaryCache != nullptr) && supportInternalModuleCache)
        {
        }

        // 4. Relocate shader and setup reference counter if cache hits and not come from m_shaderModuleHandleMap.
        if ((result != VK_SUCCESS) && (cacheResult == Util::Result::Success))
        {

            if ((result == VK_SUCCESS) && (supportInternalModuleCache))
            {
                Instance* pInstance = m_pPhysicalDevice->VkInstance();
                pShaderModule->pRefCount = reinterpret_cast<uint32_t*>(
                    pInstance->AllocMem(sizeof(uint32_t), VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
                if (pShaderModule->pRefCount != nullptr)
                {
                    Util::MutexAuto mutexLock(&m_shaderModuleCacheLock);

                    // Initialize the reference count to two: one for the runtime cache and one for this shader module.
                    *pShaderModule->pRefCount = 2;
                    result = PalToVkResult(m_shaderModuleHandleMap.Insert(shaderModuleCacheHash, *pShaderModule));
                    VK_ASSERT(result == VK_SUCCESS);
                }
            }
        }

        // 5. Set feedback info
        if (pFeedback != nullptr)
        {
            pFeedback->hitApplicationCache = hitApplicationCache;
        }

        // 6. Store binary in application cache if cache hits but not hits in application cache here. This is because
        //    PipelineCompiler::StoreShaderModuleToCache() would not be called if cache hits.
        if ((cacheResult == Util::Result::Success) && (hitApplicationCache == false))
        {
        }
    }

    return result;
}

// =====================================================================================================================
// Stores shader module to cache, include both run-time cache and binary cache
void PipelineCompiler::StoreShaderModuleToCache(
    const Device*                   pDevice,
    const VkShaderModuleCreateFlags flags,
    const uint32_t                  compilerMask,
    const Util::MetroHash::Hash&    uniqueHash,
    PipelineBinaryCache*            pBinaryCache,
    ShaderModuleHandle*             pShaderModule)
{
    VK_ASSERT(pShaderModule->pRefCount == nullptr);

    const bool supportInternalModuleCache = SupportInternalModuleCache(m_pPhysicalDevice, compilerMask);

    if ((pBinaryCache != nullptr) || supportInternalModuleCache)
    {
        const Util::MetroHash::Hash shaderModuleCacheHash =
            GetShaderModuleCacheHash(flags, compilerMask, uniqueHash);

        // 1. Store in application cache pBinaryCache.
        if (pBinaryCache != nullptr)
        {
        }

        // 2. Store in internal cache m_shaderModuleHandleMap and m_pBinaryCache
        if (supportInternalModuleCache)
        {
            Instance* pInstance = m_pPhysicalDevice->VkInstance();
            pShaderModule->pRefCount = reinterpret_cast<uint32_t*>(
                pInstance->AllocMem(sizeof(uint32_t), VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
            if (pShaderModule->pRefCount != nullptr)
            {
                Util::MutexAuto mutexLock(&m_shaderModuleCacheLock);
                // Initialize the reference count to two: one for the runtime cache and one for this shader module.
                *pShaderModule->pRefCount = 2;
                auto palResult = m_shaderModuleHandleMap.Insert(shaderModuleCacheHash, *pShaderModule);
                if (palResult != Util::Result::Success)
                {
                    // Reset refference count to one if fail to add it to runtime cache
                    *pShaderModule->pRefCount = 1;
                }
            }

            if (m_pBinaryCache != nullptr)
            {
            }
        }
    }
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult PipelineCompiler::BuildShaderModule(
    const Device*                   pDevice,
    const VkShaderModuleCreateFlags flags,
    size_t                          codeSize,
    const void*                     pCode,
    const bool                      adaptForFastLink,
    PipelineBinaryCache*            pBinaryCache,
    PipelineCreationFeedback*       pFeedback,
    ShaderModuleHandle*             pShaderModule)
{
    const RuntimeSettings* pSettings = &m_pPhysicalDevice->GetRuntimeSettings();
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    VkResult result = VK_SUCCESS;
    uint32_t compilerMask = GetCompilerCollectionMask();
    Util::MetroHash::Hash stableHash = {};
    Util::MetroHash::Hash uniqueHash = {};

    Util::MetroHash64 hasher;
    hasher.Update(reinterpret_cast<const uint8_t*>(pCode), codeSize);
    hasher.Finalize(stableHash.bytes);

    hasher.Update(adaptForFastLink);
    hasher.Finalize(uniqueHash.bytes);

    bool findReplaceShader = false;
    if ((pSettings->shaderReplaceMode == ShaderReplaceShaderHash) ||
        (pSettings->shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        size_t replaceCodeSize = 0;
        void* pReplaceCode = nullptr;
        uint64_t hash64 = Util::MetroHash::Compact64(&stableHash);
        findReplaceShader = LoadReplaceShaderBinary(hash64, &replaceCodeSize, &pReplaceCode);
        if (findReplaceShader)
        {
            pCode = pReplaceCode;
            codeSize = replaceCodeSize;
            Util::MetroHash64::Hash(reinterpret_cast<const uint8_t*>(pCode), codeSize, uniqueHash.bytes);
        }
    }

    result = LoadShaderModuleFromCache(
        pDevice, flags, compilerMask, uniqueHash, pBinaryCache, pFeedback, pShaderModule);

    if (result != VK_SUCCESS)
    {
        if (compilerMask & (1 << PipelineCompilerTypeLlpc))
        {
            result = m_compilerSolutionLlpc.BuildShaderModule(
                pDevice, flags, codeSize, pCode, adaptForFastLink, pShaderModule, stableHash);
        }

        StoreShaderModuleToCache(pDevice, flags, compilerMask, uniqueHash, pBinaryCache, pShaderModule);
    }
    else
    {
        if (result == VK_SUCCESS)
        {
            if (pSettings->enablePipelineDump)
            {
                Vkgc::BinaryData spvBin = {};
                spvBin.pCode = pCode;
                spvBin.codeSize = codeSize;
                Vkgc::IPipelineDumper::DumpSpirvBinary(pSettings->pipelineDumpDir, &spvBin);
            }
        }
    }

    if (findReplaceShader)
    {
        pInstance->FreeMem(const_cast<void*>(pCode));
    }
    return result;
}

// =====================================================================================================================
// Try to early compile shader if possible
void PipelineCompiler::TryEarlyCompileShaderModule(
    const Device*       pDevice,
    ShaderModuleHandle* pModule)
{
    const uint32_t compilerMask = GetCompilerCollectionMask();

    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        m_compilerSolutionLlpc.TryEarlyCompileShaderModule(pDevice, pModule);
    }

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
    if (pShaderModule->pRefCount != nullptr)
    {
        Util::MutexAuto mutexLock(&m_shaderModuleCacheLock);
        if (*pShaderModule->pRefCount > 1)
        {
            (*pShaderModule->pRefCount)--;
        }
        else
        {
            m_compilerSolutionLlpc.FreeShaderModule(pShaderModule);
            auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
            pInstance->FreeMem(pShaderModule->pRefCount);
        }
    }
    else
    {
        m_compilerSolutionLlpc.FreeShaderModule(pShaderModule);
    }
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

    char fileName[Util::MaxFileNameStrLen] = {};
    Vkgc::IPipelineDumper::GetPipelineName(pPipelineBuildInfo, fileName, sizeof(fileName), hashCode64);

    char replaceFileName[Util::MaxPathStrLen] = {};
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
            VkResult result = BuildShaderModule(pDevice, 0, codeSize, pCode, false, nullptr, nullptr, pShaderModule);
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

    char replaceFileName[Util::MaxPathStrLen] = {};
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
    Util::Result cacheResult = Util::Result::NotFound;

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
// Store a pipeline binary to the PAL Pipeline cache
void PipelineCompiler::CachePipelineBinary(
    const Util::MetroHash::Hash* pCacheId,
    PipelineBinaryCache*         pPipelineBinaryCache,
    size_t                       pipelineBinarySize,
    const void*                  pPipelineBinary,
    bool                         isUserCacheHit,
    bool                         isInternalCacheHit)
{
    Util::Result cacheResult;

    if ((pPipelineBinaryCache != nullptr) && (isUserCacheHit == false))
    {
        cacheResult = pPipelineBinaryCache->StorePipelineBinary(
            pCacheId,
            pipelineBinarySize,
            pPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    if ((m_pBinaryCache != nullptr) && (isInternalCacheHit == false))
    {
        cacheResult = m_pBinaryCache->StorePipelineBinary(
            pCacheId,
            pipelineBinarySize,
            pPipelineBinary);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }
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
    uint64_t optimizedPipelineHash = 0;

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

    // Generate optimized pipeline hash if both early compile and defer compile are enabled
    if (settings.deferCompileOptimizedPipeline &&
        (pCreateInfo->pipelineInfo.enableEarlyCompile || pCreateInfo->pipelineInfo.enableUberFetchShader))
    {
        bool enableEarlyCompile = pCreateInfo->pipelineInfo.enableEarlyCompile;
        bool enableUberFetchShader = pCreateInfo->pipelineInfo.enableUberFetchShader;

        pCreateInfo->pipelineInfo.enableEarlyCompile = false;
        pCreateInfo->pipelineInfo.enableUberFetchShader = false;
        optimizedPipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
        pCreateInfo->pipelineInfo.enableEarlyCompile = enableEarlyCompile;
        pCreateInfo->pipelineInfo.enableUberFetchShader = enableUberFetchShader;
    }

    // PAL Pipeline caching
    Util::Result cacheResult = Util::Result::Success;

    int64_t cacheTime   = 0;

    bool isUserCacheHit     = false;
    bool isInternalCacheHit = false;

    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

    int64_t startTime = 0;
    if (shouldCompile && ((pPipelineBinaryCache != nullptr) || (m_pBinaryCache != nullptr)))
    {
        startTime = Util::GetPerfCpuTime();

        // Search optimized pipeline first
        if (optimizedPipelineHash != 0)
        {
            GetGraphicsPipelineCacheId(
                deviceIdx,
                pCreateInfo,
                optimizedPipelineHash,
                m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash(),
                pCacheId);

            cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
                &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->freeCompilerBinary, &pCreateInfo->pipelineFeedback);
            if (cacheResult == Util::Result::Success)
            {
                shouldCompile = false;
                // Update pipeline option for optimized pipeline and update dump handle.
                pCreateInfo->pipelineInfo.enableEarlyCompile = false;
                pCreateInfo->pipelineInfo.enableUberFetchShader = false;

            }
        }
    }

    if (settings.enablePipelineDump)
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        dumpOptions.pDumpDir = settings.pipelineDumpDir;
        dumpOptions.filterPipelineDumpByType = settings.filterPipelineDumpByType;
        dumpOptions.filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
        dumpOptions.dumpDuplicatePipelines = settings.dumpDuplicatePipelines;

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo = &pCreateInfo->pipelineInfo;
        uint64_t dumpHash = pipelineHash;
        if (optimizedPipelineHash != 0)
        {
            if (shouldCompile == false)
            {
                // Current pipeline is optimized pipeline if optimized pipeline is valid and pipeline cache is hit
                dumpHash = optimizedPipelineHash;
            }
        }
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo, dumpHash);
    }

    if (shouldCompile && ((pPipelineBinaryCache != nullptr) || (m_pBinaryCache != nullptr)))
    {
        if (shouldCompile)
        {
            GetGraphicsPipelineCacheId(
                deviceIdx,
                pCreateInfo,
                pipelineHash,
                m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash(),
                pCacheId);

            cacheResult = GetCachedPipelineBinary(pCacheId, pPipelineBinaryCache, pPipelineBinarySize, ppPipelineBinary,
                &isUserCacheHit, &isInternalCacheHit, &pCreateInfo->freeCompilerBinary, &pCreateInfo->pipelineFeedback);
            if (cacheResult == Util::Result::Success)
            {
                shouldCompile = false;
            }
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
                    pCacheId,
                    &compileTime);
            }

            if (result == VK_SUCCESS)
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;

                auto                   pInstance     = m_pPhysicalDevice->Manager()->VkInstance();
                // Write PipelineMetadata to ELF section
                Pal::Result palResult = Pal::Result::Success;

                Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());
                palResult = abiProcessor.LoadFromBuffer(*ppPipelineBinary, *pPipelineBinarySize);
                if (palResult == Pal::Result::Success)
                {
                    palResult = abiProcessor.SetGenericSection(".pipelinemetadata", &pCreateInfo->pipelineMetadata,
                        sizeof(pCreateInfo->pipelineMetadata));
                    if (palResult == Pal::Result::Success)
                    {
                        FreeGraphicsPipelineBinary(pCreateInfo, *ppPipelineBinary, *pPipelineBinarySize);
                        *ppPipelineBinary = nullptr;

                        *pPipelineBinarySize     = abiProcessor.GetRequiredBufferSizeBytes();
                        void* pNewPipelineBinary = pInstance->AllocMem(*pPipelineBinarySize, VK_DEFAULT_MEM_ALIGN,
                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

                        if (pNewPipelineBinary == nullptr)
                        {
                            palResult = Pal::Result::ErrorOutOfMemory;
                        }
                        else
                        {
                            abiProcessor.SaveToBuffer(pNewPipelineBinary);
                            *ppPipelineBinary = pNewPipelineBinary;
                            pCreateInfo->freeCompilerBinary = FreeWithInstanceAllocator;
                        }
                    }
                }

                result = PalToVkResult(palResult);
            }
        }
    }
    else
    {
        // Read PipelineMetadata from ELF section
        Pal::Result palResult                  = Pal::Result::Success;
        const void* pPipelineMetadataSection   = nullptr;
        size_t      pipelineMetadataSectionLen = 0;

        Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());
        palResult = abiProcessor.LoadFromBuffer(*ppPipelineBinary, *pPipelineBinarySize);
        if (palResult == Pal::Result::Success)
        {
            abiProcessor.GetGenericSection(".pipelinemetadata", &pPipelineMetadataSection, &pipelineMetadataSectionLen);

            VK_ASSERT(pPipelineMetadataSection   != nullptr);
            VK_ASSERT(pipelineMetadataSectionLen == sizeof(pCreateInfo->pipelineMetadata));

            // Copy metadata
            memcpy(&pCreateInfo->pipelineMetadata, pPipelineMetadataSection, pipelineMetadataSectionLen);
        }

        result = PalToVkResult(palResult);
    }

    if (result == VK_SUCCESS)
    {
        CachePipelineBinary(
            pCacheId,
            pPipelineBinaryCache,
            *pPipelineBinarySize,
            *ppPipelineBinary,
            isUserCacheHit,
            isInternalCacheHit);
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
// Create ISA/relocable shader for a specific shader based on pipeline information
VkResult PipelineCompiler::CreateGraphicsShaderBinary(
    const Device*                           pDevice,
    const ShaderStage                       stage,
    const GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    ShaderModuleHandle*                     pModule)
{
    VkResult result = VK_SUCCESS;
    const uint32_t compilerMask = GetCompilerCollectionMask();

    if (compilerMask & (1 << PipelineCompilerTypeLlpc))
    {
        result = m_compilerSolutionLlpc.CreateGraphicsShaderBinary(pDevice, stage, pCreateInfo, pModule);
    }

    return result;
}

// =====================================================================================================================
// Free and only free early compiled shader in ShaderModuleHandle
void PipelineCompiler::FreeGraphicsShaderBinary(
    ShaderModuleHandle* pShaderModule)
{
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
    Util::Result cacheResult = Util::Result::Success;

    int64_t cacheTime = 0;

    bool isUserCacheHit     = false;
    bool isInternalCacheHit = false;

    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

    if (shouldCompile && ((pPipelineBinaryCache != nullptr) || (m_pBinaryCache != nullptr)))
    {
        int64_t startTime = Util::GetPerfCpuTime();

        GetComputePipelineCacheId(
            deviceIdx,
            pCreateInfo,
            pipelineHash,
            m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash(),
            pCacheId);

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
                    pCacheId,
                    &compileTime);
            }

            if (result == VK_SUCCESS)
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        CachePipelineBinary(
            pCacheId,
            pPipelineBinaryCache,
            *pPipelineBinarySize,
            *ppPipelineBinary,
            isUserCacheHit,
            isInternalCacheHit);
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
        if ((stageCount == 0) && (stageCreationFeedbacks != nullptr))
        {
            UpdatePipelineCreationFeedback(&stageCreationFeedbacks[0], pStageFeedback);
        }
        else if (stageCount != 0)
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
template <uint32_t shaderMask>
static void CopyPipelineShadersInfo(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    pCreateInfo->compilerType = libInfo.compilerType;

    Vkgc::PipelineShaderInfo* pShaderInfosDst[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    const Vkgc::PipelineShaderInfo* pShaderInfosSrc[] =
    {
        &libInfo.pipelineInfo.vs,
        &libInfo.pipelineInfo.tcs,
        &libInfo.pipelineInfo.tes,
        &libInfo.pipelineInfo.gs,
        &libInfo.pipelineInfo.fs,
    };

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if ((shaderMask & (1 << stage)) != 0)
        {
            *pShaderInfosDst[stage]                        = *pShaderInfosSrc[stage];
            pCreateInfo->pipelineProfileKey.shaders[stage] = libInfo.pipelineProfileKey.shaders[stage];
        }
    }
}

// =====================================================================================================================
static void CopyVertexInputInterfaceState(
    const Device*                     pDevice,
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    VbBindingInfo*                    pVbInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    pCreateInfo->pipelineInfo.pVertexInput               = libInfo.pipelineInfo.pVertexInput;
    pCreateInfo->pipelineInfo.iaState.topology           = libInfo.pipelineInfo.iaState.topology;
    pCreateInfo->pipelineInfo.iaState.disableVertexReuse = libInfo.pipelineInfo.iaState.disableVertexReuse;
    pCreateInfo->pipelineInfo.dynamicVertexStride        = libInfo.pipelineInfo.dynamicVertexStride;

    BuildLlpcVertexInputDescriptors(pDevice, pCreateInfo->pipelineInfo.pVertexInput, pVbInfo);
}

// =====================================================================================================================
static void MergePipelineOptions(const Vkgc::PipelineOptions& src, Vkgc::PipelineOptions& dst)
{
    dst.includeDisassembly                    |= src.includeDisassembly;
    dst.scalarBlockLayout                     |= src.scalarBlockLayout;
    dst.reconfigWorkgroupLayout               |= src.reconfigWorkgroupLayout;
    dst.forceCsThreadIdSwizzling              |= src.forceCsThreadIdSwizzling;
    dst.includeIr                             |= src.includeIr;
    dst.robustBufferAccess                    |= src.robustBufferAccess;
    dst.enableRelocatableShaderElf            |= src.enableRelocatableShaderElf;
    dst.disableImageResourceCheck             |= src.disableImageResourceCheck;
    dst.enableScratchAccessBoundsChecks       |= src.enableScratchAccessBoundsChecks;
    dst.extendedRobustness.nullDescriptor     |= src.extendedRobustness.nullDescriptor;
    dst.extendedRobustness.robustBufferAccess |= src.extendedRobustness.robustBufferAccess;
    dst.extendedRobustness.robustImageAccess  |= src.extendedRobustness.robustImageAccess;
    dst.enableInterpModePatch                 |= src.enableInterpModePatch;
    dst.pageMigrationEnabled                  |= src.pageMigrationEnabled;

    dst.shadowDescriptorTableUsage   = src.shadowDescriptorTableUsage;
    dst.shadowDescriptorTablePtrHigh = src.shadowDescriptorTablePtrHigh;
}

// =====================================================================================================================
static void CopyPreRasterizationShaderState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    pCreateInfo->pipelineInfo.iaState.patchControlPoints      = libInfo.pipelineInfo.iaState.patchControlPoints;
    pCreateInfo->pipelineInfo.iaState.switchWinding           = libInfo.pipelineInfo.iaState.switchWinding;
    pCreateInfo->pipelineInfo.vpState.depthClipEnable         = libInfo.pipelineInfo.vpState.depthClipEnable;
    pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = libInfo.pipelineInfo.rsState.rasterizerDiscardEnable;
    pCreateInfo->pipelineInfo.rsState.provokingVertexMode     = libInfo.pipelineInfo.rsState.provokingVertexMode;
    pCreateInfo->pipelineInfo.nggState                        = libInfo.pipelineInfo.nggState;
    pCreateInfo->pipelineInfo.enableUberFetchShader           = libInfo.pipelineInfo.enableUberFetchShader;
    pCreateInfo->rasterizationStream                          = libInfo.rasterizationStream;

    MergePipelineOptions(libInfo.pipelineInfo.options, pCreateInfo->pipelineInfo.options);

    CopyPipelineShadersInfo<PrsShaderMask>(pLibrary, pCreateInfo);
}

// =====================================================================================================================
static void CopyFragmentShaderState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    pCreateInfo->pipelineInfo.rsState.perSampleShading      = libInfo.pipelineInfo.rsState.perSampleShading;
    pCreateInfo->pipelineInfo.rsState.numSamples            = libInfo.pipelineInfo.rsState.numSamples;
    pCreateInfo->pipelineInfo.rsState.samplePatternIdx      = libInfo.pipelineInfo.rsState.samplePatternIdx;
    pCreateInfo->pipelineInfo.rsState.pixelShaderSamples    = libInfo.pipelineInfo.rsState.pixelShaderSamples;

    pCreateInfo->pipelineInfo.dsState.depthTestEnable   = libInfo.pipelineInfo.dsState.depthTestEnable;
    pCreateInfo->pipelineInfo.dsState.depthWriteEnable  = libInfo.pipelineInfo.dsState.depthWriteEnable;
    pCreateInfo->pipelineInfo.dsState.depthCompareOp    = libInfo.pipelineInfo.dsState.depthCompareOp;
    pCreateInfo->pipelineInfo.dsState.stencilTestEnable = libInfo.pipelineInfo.dsState.stencilTestEnable;
    pCreateInfo->pipelineInfo.dsState.front             = libInfo.pipelineInfo.dsState.front;
    pCreateInfo->pipelineInfo.dsState.back              = libInfo.pipelineInfo.dsState.back;

    MergePipelineOptions(libInfo.pipelineInfo.options, pCreateInfo->pipelineInfo.options);

    CopyPipelineShadersInfo<FgsShaderMask>(pLibrary, pCreateInfo);
}

// =====================================================================================================================
static void CopyFragmentOutputInterfaceState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    for (uint32_t i = 0; i < Vkgc::MaxColorTargets; ++i)
    {
        pCreateInfo->pipelineInfo.cbState.target[i] = libInfo.pipelineInfo.cbState.target[i];
    }

    pCreateInfo->dbFormat                                   = libInfo.dbFormat;
    pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = libInfo.pipelineInfo.cbState.alphaToCoverageEnable;
    pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable = libInfo.pipelineInfo.cbState.dualSourceBlendEnable;
    pCreateInfo->pipelineInfo.iaState.enableMultiView       = libInfo.pipelineInfo.iaState.enableMultiView;
}

// =====================================================================================================================
static void BuildRasterizationState(
    const VkPipelineRasterizationStateCreateInfo* pRs,
    const uint32_t                                dynamicStateFlags,
    bool*                                         pIsConservativeOverestimation,
    GraphicsPipelineBinaryCreateInfo*             pCreateInfo)
{
    if (pRs != nullptr)
    {
        EXTRACT_VK_STRUCTURES_3(
            rasterizationDepthClipState,
            PipelineRasterizationDepthClipStateCreateInfoEXT,
            PipelineRasterizationStateStreamCreateInfoEXT,
            PipelineRasterizationConservativeStateCreateInfoEXT,
            PipelineRasterizationProvokingVertexStateCreateInfoEXT,
            static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pRs->pNext),
            PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
            PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,
            PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
            PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT);

        if (pPipelineRasterizationProvokingVertexStateCreateInfoEXT != nullptr)
        {
            pCreateInfo->pipelineInfo.rsState.provokingVertexMode =
                pPipelineRasterizationProvokingVertexStateCreateInfoEXT->provokingVertexMode;
        }

        pCreateInfo->pipelineInfo.vpState.depthClipEnable         = (pRs->depthClampEnable == VK_FALSE);
        pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = (pRs->rasterizerDiscardEnable != VK_FALSE);

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

        if ((pPipelineRasterizationConservativeStateCreateInfoEXT != nullptr) &&
            (pPipelineRasterizationConservativeStateCreateInfoEXT->conservativeRasterizationMode ==
            VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT))
        {
            *pIsConservativeOverestimation = true;
        }
        else
        {
            *pIsConservativeOverestimation = false;
        }

    }
}

// =====================================================================================================================
static void BuildMultisampleStateInFgs(
    const Device*                               pDevice,
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    GraphicsPipelineBinaryCreateInfo*           pCreateInfo)
{
    if (pMs != nullptr)
    {
        if (pMs->rasterizationSamples != 1)
        {
            uint32_t subpassCoverageSampleCount;
            uint32_t subpassColorSampleCount;
            GraphicsPipelineCommon::GetSubpassSampleCount(
                pMs, pRenderPass, subpass, &subpassCoverageSampleCount, &subpassColorSampleCount, nullptr);

            if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
            {
                pCreateInfo->pipelineInfo.rsState.perSampleShading =
                    ((subpassColorSampleCount * pMs->minSampleShading) > 1.0f);
                pCreateInfo->pipelineInfo.rsState.pixelShaderSamples =
                    Pow2Pad(static_cast<uint32_t>(ceil(subpassColorSampleCount * pMs->minSampleShading)));
            }
            else
            {
                pCreateInfo->pipelineInfo.rsState.perSampleShading = false;
                pCreateInfo->pipelineInfo.rsState.pixelShaderSamples = 1;
            }

            pCreateInfo->pipelineInfo.rsState.numSamples = pMs->rasterizationSamples;

            // NOTE: The sample pattern index here is actually the offset of sample position pair. This is
            // different from the field of creation info of image view. For image view, the sample pattern
            // index is really table index of the sample pattern.
            pCreateInfo->pipelineInfo.rsState.samplePatternIdx =
                Device::GetDefaultSamplePatternIndex(subpassCoverageSampleCount) * Pal::MaxMsaaRasterizerSamples;
        }

        pCreateInfo->pipelineInfo.options.enableInterpModePatch = false;

        if (pCreateInfo->pipelineInfo.rsState.perSampleShading)
        {
            EXTRACT_VK_STRUCTURES_0(
                SampleLocations,
                PipelineSampleLocationsStateCreateInfoEXT,
                static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(pMs->pNext),
                PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);

            VkExtent2D gridSize = {};
            if (pPipelineSampleLocationsStateCreateInfoEXT != nullptr)
            {
                gridSize = pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationGridSize;
            }

            if ((gridSize.width <= 1) && (gridSize.height <= 1)
               )
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
static void BuildMultisampleStateInFoi(
    const VkPipelineMultisampleStateCreateInfo* pMs,
    GraphicsPipelineBinaryCreateInfo*           pCreateInfo)
{
    if (pMs != nullptr)
    {
        pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = (pMs->alphaToCoverageEnable == VK_TRUE);
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
void PipelineCompiler::BuildNggState(
    const Device*                     pDevice,
    const VkShaderStageFlagBits       activeStages,
    const bool                        isConservativeOverestimation,
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
        pCreateInfo->pipelineInfo.nggState.forceCullingMode         = settings.nggForceCullingMode;
        pCreateInfo->pipelineInfo.nggState.compactMode              =
            static_cast<Vkgc::NggCompactMode>(settings.nggCompactionMode);

        pCreateInfo->pipelineInfo.nggState.enableVertexReuse         = false;
        pCreateInfo->pipelineInfo.nggState.enableBackfaceCulling     = (isConservativeOverestimation ?
                                                                        false : settings.nggEnableBackfaceCulling);
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
static void BuildDepthStencilState(
    const VkPipelineDepthStencilStateCreateInfo* pDs,
    GraphicsPipelineBinaryCreateInfo*            pCreateInfo)
{
    if (pDs != nullptr)
    {
        pCreateInfo->pipelineInfo.dsState.depthTestEnable   = pDs->depthTestEnable;
        pCreateInfo->pipelineInfo.dsState.depthWriteEnable  = pDs->depthWriteEnable;
        pCreateInfo->pipelineInfo.dsState.depthCompareOp    = pDs->depthCompareOp;
        pCreateInfo->pipelineInfo.dsState.front             = pDs->front;
        pCreateInfo->pipelineInfo.dsState.back              = pDs->back;
        pCreateInfo->pipelineInfo.dsState.stencilTestEnable = pDs->stencilTestEnable;
    }
}

// =====================================================================================================================
void PipelineCompiler::BuildPipelineShaderInfo(
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
    VbBindingInfo*                    pVbInfo,
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
                                                       pCreateInfo->pipelineInfo.enableUberFetchShader,
                                                       pCreateInfo->pTempBuffer,
                                                       &pCreateInfo->pipelineInfo.resourceMapping,
                                                       &pCreateInfo->pipelineInfo.options.resourceLayoutScheme);
        }
    }

    return result;
}

// =====================================================================================================================
static void BuildCompilerInfo(
    const Device*                          pDevice,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const uint32_t                         shaderMask,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    Vkgc::PipelineShaderInfo* ppShaderInfoOut[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs,
    };

    pCreateInfo->compilerType = pDevice->GetCompiler(DefaultDeviceIndex)->CheckCompilerType(&pCreateInfo->pipelineInfo);

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
template <uint32_t shaderMask>
static void BuildPipelineShadersInfo(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{

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
        if (((shaderMask & (1 << stage)) != 0) && (pShaderInfo->stages[stage].pModuleHandle != nullptr))
        {
            PipelineCompiler::BuildPipelineShaderInfo(pDevice,
                                    &pShaderInfo->stages[stage],
                                    ppShaderInfoOut[stage],
                                    &pCreateInfo->pipelineInfo.options,
                                    &pCreateInfo->pipelineProfileKey,
                                    &pCreateInfo->pipelineInfo.nggState
            );
        }
    }

    // Uber fetch shader is actully used in the following scenes:
    // * enableUberFetchShader or enableEarlyCompile is set as TRUE in panel.
    // * When creating shader module, adaptForFastLink parameter of PipelineCompiler::BuildShaderModule() is set as
    //   TRUE.  This may happen when shader is created during pipeline creation, and that pipeline is a library, not
    //   executable.  More details can be found in Pipeline::BuildShaderStageInfo().
    // * When creating pipeline, GraphicsPipelineBuildInfo::enableUberFetchShader controls the actual enablement. It is
    //   only set when Vertex Input Interface section (VII) is not avaible and Pre-Rasterization Shader section (PRS)is
    //   available, or inherits from its PRS parent (referenced library). However, enableUberFetchShader would also be
    //   set as FALSE even if its parent set it as TRUE if current pipeline want to re-compile pre-rasterazation shaders
    //   and VII is available.  This may happen when VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT is set.  More
    //   details can be found in PipelineCompiler::ConvertGraphicsPipelineInfo().
    // PS: For standard gfx pipeline, GraphicsPipelineBuildInfo::enableUberFetchShader is never set as TRUE with default
    //     panel setting because VII and PRS are always available at the same time.
    if (pDevice->GetRuntimeSettings().enableUberFetchShader ||
        pDevice->GetRuntimeSettings().enableEarlyCompile ||
        (((pCreateInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) == 0) &&
         ((pCreateInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0))
        )
    {
        pCreateInfo->pipelineInfo.enableUberFetchShader = true;
    }
}

// =====================================================================================================================
static void BuildColorBlendState(
    const Device*                              pDevice,
    const VkPipelineColorBlendStateCreateInfo* pCb,
    const VkPipelineRenderingCreateInfoKHR*    pRendering,
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
            else if ((pRendering != nullptr) &&
                     (i < pRendering->colorAttachmentCount))
            {
                cbFormat = pRendering->pColorAttachmentFormats[i];
            }

            // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
            // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
            // disable shader writes through that target. There is one exception for alphaToCoverageEnable
            // and attachment zero, which can be set to VK_ATTACHMENT_UNUSED.
            if (cbFormat != VK_FORMAT_UNDEFINED)
            {
                pLlpcCbDst->format = cbFormat;

                pLlpcCbDst->blendEnable = (src.blendEnable == VK_TRUE);
                pLlpcCbDst->blendSrcAlphaToColor =
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcAlphaBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstAlphaBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcColorBlendFactor) ||
                    GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstColorBlendFactor);
                pLlpcCbDst->channelWriteMask = src.colorWriteMask;
            }
            else if (i == 0)
            {
                // VK_FORMAT_UNDEFINED will cause the shader output to be dropped for alphaToCoverageEnable.
                // Any supported format should be fine.
                if ((pRenderPass != nullptr) && (pRenderPass->GetAttachmentCount() > 0))
                {
                    pLlpcCbDst->format = pRenderPass->GetAttachmentDesc(i).format;
                }
                else if (pRendering != nullptr)
                {
                    if (pRendering->colorAttachmentCount > 0)
                    {
                        // Pick any VK_FORMAT that is not VK_FORMAT_UNDEFINED.
                        for (uint32_t j = 0; j < pRendering->colorAttachmentCount; ++j)
                        {
                            if (pRendering->pColorAttachmentFormats[j] != VK_FORMAT_UNDEFINED)
                            {
                                pLlpcCbDst->format = pRendering->pColorAttachmentFormats[j];
                                break;
                            }
                        }
                    }
                    else
                    {
                        // If the color attachment is not available.
                        pLlpcCbDst->format = (pRendering->depthAttachmentFormat != VK_FORMAT_UNDEFINED) ?
                            pRendering->depthAttachmentFormat : pRendering->stencilAttachmentFormat;
                    }
                }
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
    else if (pRendering != nullptr)
    {
        dbFormat = (pRendering->depthAttachmentFormat != VK_FORMAT_UNDEFINED) ?
            pRendering->depthAttachmentFormat : pRendering->stencilAttachmentFormat;
    }

    pCreateInfo->dbFormat = dbFormat;
}

// =====================================================================================================================
static void BuildVertexInputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
    const VkShaderStageFlagBits         activeStages,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo,
    VbBindingInfo*                      pVbInfo)
{
    {
        // According to the spec this should never be null except when mesh is enabled
        VK_ASSERT(pIn->pVertexInputState);

        pCreateInfo->pipelineInfo.pVertexInput               = pIn->pVertexInputState;
        pCreateInfo->pipelineInfo.iaState.topology           = pIn->pInputAssemblyState->topology;
        pCreateInfo->pipelineInfo.iaState.disableVertexReuse = false;

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInputBindingStrideExt) == true)
        {
            pCreateInfo->pipelineInfo.dynamicVertexStride = true;
        }

        BuildLlpcVertexInputDescriptors(pDevice, pIn->pVertexInputState, pVbInfo);
    }
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
    const RenderPass* pRenderPass                  = RenderPass::ObjectFromHandle(pIn->renderPass);
    bool              isConservativeOverestimation = false;

    BuildRasterizationState(pIn->pRasterizationState, dynamicStateFlags, &isConservativeOverestimation, pCreateInfo);

    if (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false)
    {
        BuildViewportState(pDevice, pIn->pViewportState, dynamicStateFlags, pCreateInfo);
    }

    PipelineCompiler::BuildNggState(pDevice, activeStages, isConservativeOverestimation, pCreateInfo);

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

    BuildCompilerInfo(pDevice, pShaderInfo, PrsShaderMask, pCreateInfo);

    if (pCreateInfo->pipelineInfo.options.enableRelocatableShaderElf)
    {
        CompilerSolution::DisableNggCulling(&pCreateInfo->pipelineInfo.nggState);
    }
}

// =====================================================================================================================
static void BuildFragmentShaderState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildMultisampleStateInFgs(pDevice, pIn->pMultisampleState, pRenderPass, pIn->subpass, pCreateInfo);

    BuildDepthStencilState(pIn->pDepthStencilState, pCreateInfo);

    BuildPipelineShadersInfo<FgsShaderMask>(pDevice, pIn, pShaderInfo, pCreateInfo);

    BuildCompilerInfo(pDevice, pShaderInfo, FgsShaderMask, pCreateInfo);
}

// =====================================================================================================================
static void BuildFragmentOutputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    EXTRACT_VK_STRUCTURES_0(
        dynamicRendering,
        PipelineRenderingCreateInfoKHR,
        reinterpret_cast<const VkPipelineRenderingCreateInfoKHR*>(pIn->pNext),
        PIPELINE_RENDERING_CREATE_INFO_KHR)

    BuildMultisampleStateInFoi(pIn->pMultisampleState, pCreateInfo);

    BuildColorBlendState(pDevice,
                         pIn->pColorBlendState,
                         pPipelineRenderingCreateInfoKHR,
                         pRenderPass, pIn->subpass, pCreateInfo);

    pCreateInfo->pipelineInfo.iaState.enableMultiView =
        (pRenderPass != nullptr) ? pRenderPass->IsMultiviewEnabled() :
                                   ((pPipelineRenderingCreateInfoKHR != nullptr) &&
                                    (Util::CountSetBits(pPipelineRenderingCreateInfoKHR->viewMask) != 0));
}

// =====================================================================================================================
static void BuildExecutablePipelineState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const PipelineLayout*                  pPipelineLayout,
    const uint32_t                         dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo,
    PipelineInternalBufferInfo*            pInternalBufferInfo)
{
    const RuntimeSettings& settings         = pDevice->GetRuntimeSettings();
    PipelineCompiler*      pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

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

    // Compiler info is required to be re-built here since we may need to change the compiler when all the states
    // of an executable graphics pipeline are available. The shader mask here refers to the shader stages which
    // are valid in this pipeline.
    const Vkgc::GraphicsPipelineBuildInfo& pipelineInfo = pCreateInfo->pipelineInfo;
    uint32_t shaderMask = 0;
    shaderMask |= (pipelineInfo.vs.pModuleData  != nullptr) ? ShaderStageBit::ShaderStageVertexBit      : 0;
    shaderMask |= (pipelineInfo.tcs.pModuleData != nullptr) ? ShaderStageBit::ShaderStageTessControlBit : 0;
    shaderMask |= (pipelineInfo.tes.pModuleData != nullptr) ? ShaderStageBit::ShaderStageTessEvalBit    : 0;
    shaderMask |= (pipelineInfo.gs.pModuleData  != nullptr) ? ShaderStageBit::ShaderStageGeometryBit    : 0;
    shaderMask |= (pipelineInfo.fs.pModuleData  != nullptr) ? ShaderStageBit::ShaderStageFragmentBit    : 0;
    BuildCompilerInfo(pDevice, pShaderInfo, shaderMask, pCreateInfo);

    if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
    {
        pCreateInfo->pipelineInfo.enableUberFetchShader = false;
    }

    if (pCreateInfo->pipelineInfo.enableUberFetchShader)
    {
        pDefaultCompiler->BuildPipelineInternalBufferData(pPipelineLayout, pCreateInfo, pInternalBufferInfo);
    }

}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    const Device*                                   pDevice,
    const VkGraphicsPipelineCreateInfo*             pIn,
    const GraphicsPipelineShaderStageInfo*          pShaderInfo,
    const PipelineLayout*                           pPipelineLayout,
    GraphicsPipelineBinaryCreateInfo*               pCreateInfo,
    VbBindingInfo*                                  pVbInfo,
    PipelineInternalBufferInfo*                     pInternalBufferInfo)
{
    VK_ASSERT(pIn != nullptr);

    VkResult result = VK_SUCCESS;

    GraphicsPipelineLibraryInfo libInfo;
    GraphicsPipelineCommon::ExtractLibraryInfo(pIn, &libInfo);

    pCreateInfo->libFlags = libInfo.libFlags;

    pCreateInfo->libFlags |= (libInfo.pVertexInputInterfaceLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
    pCreateInfo->libFlags |= (libInfo.pPreRasterizationShaderLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
    pCreateInfo->libFlags |= (libInfo.pFragmentShaderLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
    pCreateInfo->libFlags |= (libInfo.pFragmentOutputInterfaceLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    VkShaderStageFlagBits activeStages = GraphicsPipelineCommon::GetActiveShaderStages(pIn, &libInfo);

    uint32_t dynamicStateFlags = GraphicsPipelineCommon::GetDynamicStateFlags(pIn->pDynamicState, &libInfo);

    pCreateInfo->flags = pIn->flags;

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
    {
        BuildVertexInputInterfaceState(pDevice, pIn, dynamicStateFlags, activeStages, pCreateInfo, pVbInfo);
    }
    else if (libInfo.pVertexInputInterfaceLib != nullptr)
    {
        CopyVertexInputInterfaceState(pDevice, libInfo.pVertexInputInterfaceLib, pCreateInfo, pVbInfo);
    }

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        BuildPreRasterizationShaderState(pDevice, pIn, pShaderInfo, dynamicStateFlags, activeStages, pCreateInfo);
    }
    else if (libInfo.pPreRasterizationShaderLib != nullptr)
    {
        CopyPreRasterizationShaderState(libInfo.pPreRasterizationShaderLib, pCreateInfo);
    }

    const bool enableRasterization =
        (~libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
        (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt);

    if (enableRasterization)
    {
        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
        {
            BuildFragmentShaderState(pDevice, pIn, pShaderInfo, pCreateInfo);
        }
        else if (libInfo.pFragmentShaderLib != nullptr)
        {
            CopyFragmentShaderState(libInfo.pFragmentShaderLib, pCreateInfo);
        }

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
        {
            BuildFragmentOutputInterfaceState(pDevice, pIn, pCreateInfo);
        }
        else if (libInfo.pFragmentOutputInterfaceLib != nullptr)
        {
            CopyFragmentOutputInterfaceState(libInfo.pFragmentOutputInterfaceLib, pCreateInfo);
        }
    }

    if (GraphicsPipelineCommon::NeedBuildPipelineBinary(&libInfo, enableRasterization))
    {
        const Vkgc::PipelineShaderInfo* shaderInfos[] =
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

        if ((libInfo.flags.optimize != 0) &&
            ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) ||
             (libInfo.pVertexInputInterfaceLib != nullptr)))
        {
            pCreateInfo->pipelineInfo.enableUberFetchShader = false;
        }

        result = BuildPipelineResourceMapping(pDevice, pPipelineLayout, availableStageMask, pVbInfo, pCreateInfo);
    }

    if ((result == VK_SUCCESS) && (libInfo.flags.isLibrary == false))
    {
        BuildExecutablePipelineState(
            pDevice, pIn, pShaderInfo, pPipelineLayout, dynamicStateFlags, pCreateInfo, pInternalBufferInfo);
    }

    return result;
}

// =====================================================================================================================
// Fill partial pipeline binary info in GraphicsPipelineBinaryCreateInfo
void PipelineCompiler::SetPartialGraphicsPipelineBinaryInfo(
    const ShaderModuleHandle*         pShaderModuleHandle,
    const ShaderStage                 stage,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
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

    pOptions->optimizationLevel = ((flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT) != 0 ? 0 : 2);

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT) ||
        pDevice->GetEnabledFeatures().scalarBlockLayout)
    {
        pOptions->scalarBlockLayout = true;
    }

    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pOptions->robustBufferAccess = true;
    }

    // Provide necessary runtime settings and PAL device properties
    const auto& settings = m_pPhysicalDevice->GetRuntimeSettings();
    const auto& info     = m_pPhysicalDevice->PalProperties();

    pOptions->shadowDescriptorTableUsage   = settings.enableFmaskBasedMsaaRead ?
                                             Vkgc::ShadowDescriptorTableUsage::Enable :
                                             Vkgc::ShadowDescriptorTableUsage::Disable;
    pOptions->shadowDescriptorTablePtrHigh =
          static_cast<uint32_t>(info.gpuMemoryProperties.shadowDescTableVaStart >> 32);

    pOptions->pageMigrationEnabled = info.gpuMemoryProperties.flags.pageMigrationEnabled;

    pOptions->enableRelocatableShaderElf = settings.enableRelocatableShaders;
    pOptions->disableImageResourceCheck  = settings.disableImageResourceTypeCheck;

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
                                                       false,
                                                       pCreateInfo->pTempBuffer,
                                                       &pCreateInfo->pipelineInfo.resourceMapping,
                                                       &pCreateInfo->pipelineInfo.options.resourceLayoutScheme);
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
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    bool                              keepConvertTempMemory)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if ((pCreateInfo->pTempBuffer != nullptr) && (keepConvertTempMemory == false))
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

    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

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
static VkPipelineCreateFlags GetCacheIdControlFlags(
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
// The pipeline cache ID contains additional inputs outside the shader creation information for pipeline executable
// properties as well as options to avoid user error when changing performance tuning, compiler, or any other settings.
static void GetCommonPipelineCacheId(
    uint32_t                         deviceIdx,
    VkPipelineCreateFlags            flags,
    PipelineOptimizerKey*            pPipelineProfileKey,
    PipelineCompilerType             compilerType,
    uint64_t                         pipelineHash,
    const Util::MetroHash::Hash&     settingsHash,
    Util::MetroHash128*              pHash)
{
    pHash->Update(pipelineHash);
    pHash->Update(deviceIdx);
    pHash->Update(GetCacheIdControlFlags(flags));
    pHash->Update(*pPipelineProfileKey);
    pHash->Update(compilerType);
    pHash->Update(settingsHash);
}

// =====================================================================================================================
void PipelineCompiler::GetComputePipelineCacheId(
    uint32_t                         deviceIdx,
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    uint64_t                         pipelineHash,
    const Util::MetroHash::Hash&     settingsHash,
    Util::MetroHash::Hash*           pCacheId)
{
    Util::MetroHash128 hash = {};

    GetCommonPipelineCacheId(
        deviceIdx,
        pCreateInfo->flags,
        &pCreateInfo->pipelineProfileKey,
        pCreateInfo->compilerType,
        pipelineHash,
        settingsHash,
        &hash);

    hash.Update(pCreateInfo->pipelineInfo.cs.options);
    hash.Update(pCreateInfo->pipelineInfo.options);

    hash.Finalize(pCacheId->bytes);
}

// =====================================================================================================================
void PipelineCompiler::GetGraphicsPipelineCacheId(
    uint32_t                          deviceIdx,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    uint64_t                          pipelineHash,
    const Util::MetroHash::Hash&      settingsHash,
    Util::MetroHash::Hash*            pCacheId)
{
    Util::MetroHash128 hash = {};

    GetCommonPipelineCacheId(
        deviceIdx,
        pCreateInfo->flags,
        &pCreateInfo->pipelineProfileKey,
        pCreateInfo->compilerType,
        pipelineHash,
        settingsHash,
        &hash);

    hash.Update(pCreateInfo->pipelineInfo.vs.options);
    hash.Update(pCreateInfo->pipelineInfo.tes.options);
    hash.Update(pCreateInfo->pipelineInfo.tcs.options);
    hash.Update(pCreateInfo->pipelineInfo.gs.options);
    hash.Update(pCreateInfo->pipelineInfo.fs.options);
    hash.Update(pCreateInfo->pipelineInfo.options);
    hash.Update(pCreateInfo->pipelineInfo.nggState);
    hash.Update(pCreateInfo->dbFormat);
    hash.Update(pCreateInfo->pipelineInfo.dynamicVertexStride);
    hash.Update(pCreateInfo->pipelineInfo.rsState);

    hash.Update(pCreateInfo->pipelineMetadata.pointSizeUsed);

    hash.Finalize(pCacheId->bytes);
}

// =====================================================================================================================
void PipelineCompiler::BuildPipelineInternalBufferData(
    const PipelineLayout*             pPipelineLayout,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    PipelineInternalBufferInfo*       pInternalBufferInfo)
{
    uint32_t fetchShaderConstBufRegBase  = PipelineLayout::InvalidReg;
    uint32_t specConstBufVertexRegBase   = PipelineLayout::InvalidReg;
    uint32_t specConstBufFragmentRegBase = PipelineLayout::InvalidReg;

    const UserDataLayout& layout = pPipelineLayout->GetInfo().userDataLayout;

    switch (layout.scheme)
    {
    case PipelineLayoutScheme::Compact:
        fetchShaderConstBufRegBase  = layout.compact.uberFetchConstBufRegBase;
        specConstBufVertexRegBase   = layout.compact.specConstBufVertexRegBase;
        specConstBufFragmentRegBase = layout.compact.specConstBufFragmentRegBase;
        break;
    case PipelineLayoutScheme::Indirect:
        fetchShaderConstBufRegBase = layout.indirect.uberFetchConstBufRegBase;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
    {
        VK_NOT_IMPLEMENTED;
    }

}

// =====================================================================================================================
void PipelineCompiler::ExecuteDeferCompile(
    DeferredCompileWorkload* pWorkload)
{
    auto pThread = m_deferCompileMgr.GetCompileThread();
    if (pThread != nullptr)
    {
        pThread->AddTask(pWorkload);
    }
    else
    {
        pWorkload->Execute(pWorkload->pPayloads);
        if (pWorkload->pEvent != nullptr)
        {
            pWorkload->pEvent->Set();
        }
    }
}

}
