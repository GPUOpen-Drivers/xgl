/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palShaderLibrary.h"

#include "include/pipeline_binary_cache.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/ray_tracing_device.h"
#endif

#include "palElfReader.h"
#include "palPipelineAbiReader.h"
#include "palPipelineAbiProcessorImpl.h"

#include "llpc.h"

#include <inttypes.h>

namespace vk
{

// =====================================================================================================================
// Helper function used to check whether a specific dynamic state is set
static bool IsDynamicStateEnabled(const uint64_t dynamicStateFlags, const DynamicStatesInternal internalState)
{
    return dynamicStateFlags & (1ULL << static_cast<uint32_t>(internalState));
}

// =====================================================================================================================
// Check if Wave64 support is force disabled for any shader types via settings
static bool ShouldForceWave32(
    ShaderStage  stage,
    uint32_t     deprecateWave64Mask)
{
    bool shouldForceWave32 = false;

    if (deprecateWave64Mask & DeprecateWave64::DeprecateWave64All)
    {
        shouldForceWave32 = true;
    }
    else if (stage == ShaderStage::ShaderStageCompute)
    {
        if (deprecateWave64Mask & DeprecateWave64::DeprecateWave64Cs)
        {
            shouldForceWave32 = true;
        }
    }
    else if (((1 << stage) & ShaderStageAllGraphicsBit) != 0)
    {
        if (deprecateWave64Mask & DeprecateWave64::DeprecateWave64NonCs)
        {
            shouldForceWave32 = true;
        }
    }
    // For RT stages we don't do anything

    return shouldForceWave32;
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Populates shaderLibrary input flags according to settings
static uint32_t GpuRtShaderLibraryFlags(
    const Device* pDevice)
{
    const RuntimeSettings& settings        = pDevice->GetRuntimeSettings();
    GpuRt::TraceRayCounterMode counterMode = pDevice->RayTrace()->TraceRayCounterMode(DefaultDeviceIndex);

    uint32_t flags = 0;

    if ((counterMode != GpuRt::TraceRayCounterMode::TraceRayCounterDisable) ||
        (settings.rtTraceRayProfileFlags != TraceRayProfileDisable))
    {
        flags |= static_cast<uint32>(GpuRt::ShaderLibraryFeatureFlag::Developer);
    }

    if ((settings.emulatedRtIpLevel > HardwareRtIpLevel1_1)
        )
    {
        flags |= static_cast<uint32>(GpuRt::ShaderLibraryFeatureFlag::SoftwareTraversal);
    }

    return flags;
}
#endif

// =====================================================================================================================
void PipelineCompiler::InitPipelineDumpOption(
    Vkgc::PipelineDumpOptions* pDumpOptions,
    const RuntimeSettings&     settings,
    char*                      pBuffer,
    PipelineCompilerType       type)
{
    pDumpOptions->filterPipelineDumpByType = settings.filterPipelineDumpByType;
    pDumpOptions->filterPipelineDumpByHash = settings.filterPipelineDumpByHash;
    pDumpOptions->dumpDuplicatePipelines   = settings.dumpDuplicatePipelines;
    pDumpOptions->pDumpDir                 = settings.pipelineDumpDir;
}

// =====================================================================================================================
#if VKI_RAY_TRACING
#endif

// =====================================================================================================================
// Builds app profile key and applies profile options.
static void ApplyProfileOptions(
    const Device*                pDevice,
    uint32_t                     shaderIndex,
    Vkgc::PipelineOptions*       pPipelineOptions,
    Vkgc::PipelineShaderInfo*    pShaderInfo,
    const PipelineOptimizerKey*  pProfileKey,
    Vkgc::NggState*              pNggState)
{
    auto& settings = pDevice->GetRuntimeSettings();

    PipelineShaderOptionsPtr options  = {};
    options.pPipelineOptions          = pPipelineOptions;
    options.pOptions                  = &pShaderInfo->options;
    options.pNggState                 = pNggState;

    if (pProfileKey->pShaders != nullptr)
    {
        // Override the compile parameters based on any app profile
        const auto* pShaderOptimizer = pDevice->GetShaderOptimizer();
        pShaderOptimizer->OverrideShaderCreateInfo(*pProfileKey, shaderIndex, options);

        // By default the client hash provided to PAL is more accurate than the one used by pipeline profiles.
        //
        // Optionally (based on panel setting), these can be set to temporarily match by devs. This  can be useful
        // when other tools (such as PAL's profiling layer) are used to measure shaders while building a pipeline
        // profile which uses the profile hash.
        // It is only valid for graphics and compute pipeline.
        if (settings.pipelineUseProfileHashAsClientHash)
        {
            if ((pProfileKey->shaderCount == 1) &&
                (pProfileKey->pShaders[0].stage == ShaderStage::ShaderStageCompute) &&
                (shaderIndex == 0))
            {
                pShaderInfo->options.clientHash.lower = pProfileKey->pShaders[0].codeHash.lower;
                pShaderInfo->options.clientHash.upper = pProfileKey->pShaders[0].codeHash.upper;
            }
            else if (pProfileKey->pShaders[shaderIndex].stage < ShaderStageGfxCount)
            {
                pShaderInfo->options.clientHash.lower = pProfileKey->pShaders[shaderIndex].codeHash.lower;
                pShaderInfo->options.clientHash.upper = pProfileKey->pShaders[shaderIndex].codeHash.upper;
            }
        }
    }
}

// =====================================================================================================================
static bool SupportInternalModuleCache(
    const PhysicalDevice*           pDevice,
    const uint32_t                  compilerMask,
    const VkShaderModuleCreateFlags internalShaderFlags)
{
    bool supportInternalModuleCache = pDevice->GetRuntimeSettings().enableEarlyCompile;

    if (Util::TestAnyFlagSet(internalShaderFlags, VK_INTERNAL_SHADER_FLAGS_FORCE_UNCACHED_BIT))
    {
        supportInternalModuleCache = false;
    }

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
    , m_pipelineCacheMatrix{}
    , m_uberFetchShaderInfoFormatMap(8, pPhysicalDevice->Manager()->VkInstance()->Allocator())
    , m_shaderModuleHandleMap(8, pPhysicalDevice->Manager()->VkInstance()->Allocator())
    , m_colorExportShaderMap(8, pPhysicalDevice->Manager()->VkInstance()->Allocator())
{

}

// =====================================================================================================================
// Dump input PipelineCompileCacheMatrix to a string.
void PipelineCompiler::GetElfCacheMetricString(
    PipelineCompileCacheMatrix* pCacheMatrix,
    const char*                 pPrefixStr,
    char*                       pOutStr,
    size_t                      outStrSize)
{
    const int64_t freq = Util::GetPerfFrequency();
    const int64_t avgUs = (pCacheMatrix->totalBinaries + pCacheMatrix->cacheHits) > 0 ?
        ((pCacheMatrix->totalTimeSpent / (pCacheMatrix->totalBinaries + pCacheMatrix->cacheHits)) * 1000000) / freq :
        0;
    const double  avgMs = avgUs / 1000.0;

    const int64_t totalUs = (pCacheMatrix->totalTimeSpent * 1000000) / freq;
    const double  totalMs = totalUs / 1000.0;

    const double  hitRate = pCacheMatrix->cacheAttempts > 0 ?
        (static_cast<double>(pCacheMatrix->cacheHits) / static_cast<double>(pCacheMatrix->cacheAttempts)) :
        0.0;

    static constexpr char metricFmtString[] =
        "%s\n"
        "Cache hit rate - %0.1f%% (%d/%d)\n"
        "Total new binary - %d\n"
        "Total time spent - %0.1f ms\n"
        "Average time spent per request - %0.3f ms\n\n";

    Util::Snprintf(pOutStr, outStrSize, metricFmtString, pPrefixStr, hitRate * 100, pCacheMatrix->cacheHits,
        pCacheMatrix->cacheAttempts, pCacheMatrix->totalBinaries, totalMs, avgMs);
}

// =====================================================================================================================
// Dump pipeline compile cache metrics to PipelineCacheStat.txt
void PipelineCompiler::DumpCacheMatrix(
    PhysicalDevice*             pPhysicalDevice,
    const char*                 pPrefixStr,
    uint32_t                    countHint,
    PipelineCompileCacheMatrix* pCacheMatrix)
{
    uint32_t dumpPipelineCompileCacheMatrix = pPhysicalDevice->GetRuntimeSettings().dumpPipelineCompileCacheMatrix;
    if (dumpPipelineCompileCacheMatrix != 0)
    {
        if ((countHint == UINT_MAX) ||
            ((countHint % dumpPipelineCompileCacheMatrix) == (dumpPipelineCompileCacheMatrix - 1)))
        {
            char filename[Util::MaxPathStrLen] = {};
            Util::File dumpFile;
            Util::Snprintf(filename, sizeof(filename), "%s/PipelineCacheStat.txt",
                pPhysicalDevice->GetRuntimeSettings().pipelineDumpDir);
            if (dumpFile.Open(filename, Util::FileAccessMode::FileAccessAppend) == Pal::Result::Success)
            {
                char buff[256] = {};
                GetElfCacheMetricString(pCacheMatrix, pPrefixStr, buff, sizeof(buff));
                dumpFile.Write(buff, strlen(buff));
                dumpFile.Close();
            }
        }
    }
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
    case Pal::GfxIpLevel::GfxIp10_1:
        m_gfxIp.major = 10;
        m_gfxIp.minor = 1;
        break;
    case Pal::GfxIpLevel::GfxIp10_3:
        m_gfxIp.major = 10;
        m_gfxIp.minor = 3;
        break;
#if VKI_BUILD_GFX11
    case Pal::GfxIpLevel::GfxIp11_0:
        m_gfxIp.major = 11;
        m_gfxIp.minor = 0;
        break;
#endif

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
                settings.enableInternalPipelineCachingToDisk);

        // This isn't a terminal failure, the device can continue without the pipeline cache if need be.
        VK_ALERT(m_pBinaryCache == nullptr);
    }

    if (result == VK_SUCCESS)
    {
        result = m_compilerSolutionLlpc.Initialize(m_gfxIp, info.gfxLevel, m_pBinaryCache);
    }

    if (result == VK_SUCCESS)
    {
        result = PalToVkResult(m_shaderModuleHandleMap.Init());
    }

    if (result == VK_SUCCESS)
    {
        result = PalToVkResult(m_colorExportShaderMap.Init());
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

    DumpCacheMatrix(m_pPhysicalDevice, "Pipeline", UINT32_MAX, &m_pipelineCacheMatrix);

    m_compilerSolutionLlpc.Destroy();

    DestroyPipelineBinaryCache();

    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    Util::MutexAuto mutexLock(&m_cacheLock);
    if (SupportInternalModuleCache(m_pPhysicalDevice, GetCompilerCollectionMask(), 0))
    {
        for (auto it = m_shaderModuleHandleMap.Begin(); it.Get() != nullptr; it.Next())
        {
            VK_ASSERT(it.Get()->value.pRefCount != nullptr);
            // Free shader module regardless ref count, as the whole map is being destroyed.
            VK_ALERT((*(it.Get()->value.pRefCount)) != 1);
            *(it.Get()->value.pRefCount) = 0;

            // Force use un-lock version of FreeShaderModule.
            pInstance->FreeMem(it.Get()->value.pRefCount);
            it.Get()->value.pRefCount = nullptr;
            FreeShaderModule(&it.Get()->value);

        }
        m_shaderModuleHandleMap.Reset();
    }

    for (auto it = m_colorExportShaderMap.Begin(); it.Get() != nullptr; it.Next())
    {
        // Destory color export shader library
        it.Get()->value->Destroy();
    }
    m_colorExportShaderMap.Reset();
}

// =====================================================================================================================
// Loads shader binary  from replace shader folder with specified shader hash code.
bool PipelineCompiler::LoadReplaceShaderBinary(
    uint64_t          shaderHash,
    Vkgc::BinaryData* pBinary)
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

        pBinary->pCode    = pReplaceCode;
        pBinary->codeSize = replaceCodeSize;
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
    const VkShaderModuleCreateFlags flags,
    const VkShaderModuleCreateFlags internalShaderFlags,
    const uint32_t                  compilerMask,
    const Util::MetroHash::Hash&    uniqueHash,
    ShaderModuleHandle*             pShaderModule)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;

    const bool supportInternalModuleCache =
        SupportInternalModuleCache(m_pPhysicalDevice, compilerMask, internalShaderFlags);
    const bool delayConversion = false;

    VK_ASSERT(pShaderModule->pRefCount == nullptr);

    if (supportInternalModuleCache)
    {
        const Util::MetroHash::Hash shaderModuleCacheHash =
            GetShaderModuleCacheHash(flags, compilerMask, uniqueHash);
        ShaderModuleHandle* pHandleInShaderModuleHandleMap = nullptr;

        const void*  pShaderModuleBinary = nullptr;
        size_t       shaderModuleSize    = 0;
        Util::Result cacheResult         = Util::Result::NotFound;

        // 1. Look up in internal cache m_shaderModuleHandleMap.
        if (supportInternalModuleCache)
        {
            Util::MutexAuto mutexLock(&m_cacheLock);

            pHandleInShaderModuleHandleMap = m_shaderModuleHandleMap.FindKey(shaderModuleCacheHash);
            if ((pHandleInShaderModuleHandleMap != nullptr) && IsValidShaderModule(pHandleInShaderModuleHandleMap))
            {
                VK_ASSERT(pHandleInShaderModuleHandleMap->pRefCount != nullptr);
                (*(pHandleInShaderModuleHandleMap->pRefCount))++;
                *pShaderModule = *pHandleInShaderModuleHandleMap;
                result         = VK_SUCCESS;
                cacheResult    = Util::Result::Success;
            }
        }

        // 3. Look up in internal cache m_pBinaryCache
        if ((cacheResult != Util::Result::Success) && (m_pBinaryCache != nullptr) && supportInternalModuleCache)
        {
        }

        // 4. Relocate shader and setup reference counter if cache hits and not come from m_shaderModuleHandleMap.
        if ((result != VK_SUCCESS) && (cacheResult == Util::Result::Success))
        {

            if ((result == VK_SUCCESS) && supportInternalModuleCache && (delayConversion == false))
            {
                Instance* pInstance = m_pPhysicalDevice->VkInstance();
                pShaderModule->pRefCount = reinterpret_cast<uint32_t*>(
                    pInstance->AllocMem(sizeof(uint32_t), VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
                if (pShaderModule->pRefCount != nullptr)
                {
                    Util::MutexAuto mutexLock(&m_cacheLock);

                    // Initialize the reference count to two: one for the runtime cache and one for this shader module.
                    *pShaderModule->pRefCount = 2;
                    if (pHandleInShaderModuleHandleMap == nullptr)
                    {
                        result = PalToVkResult(m_shaderModuleHandleMap.Insert(shaderModuleCacheHash, *pShaderModule));
                    }
                    else
                    {
                        *pHandleInShaderModuleHandleMap = *pShaderModule;
                    }
                    VK_ASSERT(result == VK_SUCCESS);

                    if (result != VK_SUCCESS)
                    {
                        // In case map insertion fails for any reason, free the allocated memory.
                        pInstance->FreeMem(pShaderModule->pRefCount);
                        pShaderModule->pRefCount = nullptr;
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Stores shader module to cache, include both run-time cache and binary cache
void PipelineCompiler::StoreShaderModuleToCache(
    const VkShaderModuleCreateFlags flags,
    const VkShaderModuleCreateFlags internalShaderFlags,
    const uint32_t                  compilerMask,
    const Util::MetroHash::Hash&    uniqueHash,
    ShaderModuleHandle*             pShaderModule)
{
    VK_ASSERT(pShaderModule->pRefCount == nullptr);

    const bool supportInternalModuleCache =
        SupportInternalModuleCache(m_pPhysicalDevice, compilerMask, internalShaderFlags);

    if (supportInternalModuleCache)
    {

        const Util::MetroHash::Hash shaderModuleCacheHash =
            GetShaderModuleCacheHash(flags, compilerMask, uniqueHash);

        // 2. Store in internal cache m_shaderModuleHandleMap and m_pBinaryCache
        if (supportInternalModuleCache)
        {
            Instance* pInstance = m_pPhysicalDevice->VkInstance();
            pShaderModule->pRefCount = reinterpret_cast<uint32_t*>(
                pInstance->AllocMem(sizeof(uint32_t), VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
            if (pShaderModule->pRefCount != nullptr)
            {
                Util::MutexAuto mutexLock(&m_cacheLock);
                // Initialize the reference count to two: one for the runtime cache and one for this shader module.
                *pShaderModule->pRefCount = 2;
                auto palResult = m_shaderModuleHandleMap.Insert(shaderModuleCacheHash, *pShaderModule);
                if (palResult != Util::Result::Success)
                {
                    // In case map insertion fails for any reason, free the allocated memory.
                    pInstance->FreeMem(pShaderModule->pRefCount);
                    pShaderModule->pRefCount = nullptr;
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
    const VkShaderModuleCreateFlags internalShaderFlags,
    const Vkgc::BinaryData&         shaderBinary,
    ShaderModuleHandle*             pShaderModule)
{
    const RuntimeSettings* pSettings = &m_pPhysicalDevice->GetRuntimeSettings();
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    VkResult result = VK_SUCCESS;
    uint32_t compilerMask = GetCompilerCollectionMask();
    Util::MetroHash::Hash stableHash = {};
    Util::MetroHash::Hash uniqueHash = {};

    Util::MetroHash64 hasher;
    hasher.Update(reinterpret_cast<const uint8_t*>(shaderBinary.pCode), shaderBinary.codeSize);
    hasher.Finalize(stableHash.bytes);
    hasher.Finalize(uniqueHash.bytes);

    bool findReplaceShader = false;

    Vkgc::BinaryData finalData = shaderBinary;
    if ((pSettings->shaderReplaceMode == ShaderReplaceShaderHash) ||
        (pSettings->shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        Vkgc::BinaryData replaceBinary = {};
        uint64_t hash64 = Util::MetroHash::Compact64(&stableHash);
        findReplaceShader = LoadReplaceShaderBinary(hash64, &replaceBinary);
        if (findReplaceShader)
        {
            finalData = replaceBinary;
            Util::MetroHash64::Hash(
                reinterpret_cast<const uint8_t*>(replaceBinary.pCode), replaceBinary.codeSize, uniqueHash.bytes);
        }
    }

    result = LoadShaderModuleFromCache(
        flags, internalShaderFlags, compilerMask, uniqueHash, pShaderModule);

    if (result != VK_SUCCESS)
    {
        if (compilerMask & (1 << PipelineCompilerTypeLlpc))
        {
            result = m_compilerSolutionLlpc.BuildShaderModule(
                pDevice,
                flags,
                internalShaderFlags,
                finalData,
                pShaderModule,
                PipelineOptimizerKey{});

        }

        StoreShaderModuleToCache(flags, internalShaderFlags, compilerMask, uniqueHash, pShaderModule);
    }
    else if ((pSettings->enablePipelineDump)
        )
    {
        Vkgc::IPipelineDumper::DumpSpirvBinary(pSettings->pipelineDumpDir, &finalData);
    }

    if (findReplaceShader)
    {
        pInstance->FreeMem(const_cast<void*>(finalData.pCode));
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
    if (pShaderModule != nullptr)
    {
        isValid |= (pShaderModule->pLlpcShaderModule != nullptr);
    }
    return isValid;
}

// =====================================================================================================================
// Frees shader module memory
void PipelineCompiler::FreeShaderModule(
    ShaderModuleHandle* pShaderModule)
{
    if (pShaderModule->pRefCount != nullptr)
    {
        Util::MutexAuto mutexLock(&m_cacheLock);
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
    const PhysicalDevice*    pPhysicalDevice,
    const PipelineBuildInfo* pPipelineBuildInfo,
    Vkgc::BinaryData*        pPipelineBinary,
    uint64_t                 hashCode64)
{
    const RuntimeSettings& settings  = pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = pPhysicalDevice->Manager()->VkInstance();

    char fileName[Util::MaxFileNameStrLen] = {};
    Vkgc::IPipelineDumper::GetPipelineName(pPipelineBuildInfo, fileName, sizeof(fileName), hashCode64);

    char replaceFileName[Util::MaxPathStrLen] = {};
    int32_t length = Util::Snprintf(
        replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf", settings.shaderReplaceDir, fileName);

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

            pPipelineBinary->codeSize = binSize;
            pPipelineBinary->pCode    = pAllocBuf;
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

        Vkgc::BinaryData shaderBinary = {};
        if (LoadReplaceShaderBinary(hash64, &shaderBinary))
        {
            VkResult result =
                BuildShaderModule(pDevice, 0, 0, shaderBinary, pShaderModule);

            if (result == VK_SUCCESS)
            {
                pShaderInfo->pModuleData = ShaderModule::GetShaderData(compilerType, pShaderModule);
                replaced = true;
            }

            pInstance->FreeMem(const_cast<void*>(shaderBinary.pCode));
        }
    }

    return replaced;
}

// =====================================================================================================================
// Drop pipeline binary instruction.
void PipelineCompiler::DropPipelineBinaryInst(
    Device*                 pDevice,
    const RuntimeSettings&  settings,
    const Vkgc::BinaryData& pipelineBinary)
{
    if (settings.enableDropPipelineBinaryInst == true)
    {
        Util::ElfReader::Reader elfReader(pipelineBinary.pCode);
        Util::ElfReader::SectionId codeSectionId = elfReader.FindSection(".text");
        VK_ASSERT(codeSectionId != 0);

        const Util::Elf::SectionHeader& codeSection = elfReader.GetSection(codeSectionId);

        size_t pipelineCodeSize = static_cast<size_t>(codeSection.sh_size);
        void* pPipelineCode = const_cast<void*>(Util::VoidPtrInc(pipelineBinary.pCode,
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
    Device *                pDevice,
    uint64_t                pipelineHash,
    uint32_t                pipelineIndex,
    const Vkgc::BinaryData& pipelineBinary)
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

    Util::Abi::PipelineAbiReader abiReader(pDevice->VkInstance()->Allocator(), pipelineBinary.pCode);
    Pal::Result palResult = abiReader.Init();
    if (palResult != Pal::Result::Success)
    {
        return;
    }

    Util::ElfReader::SectionId codeSectionId = abiReader.GetElfReader().FindSection(".text");
    VK_ASSERT(codeSectionId != 0);

    const Util::Elf::SectionHeader& codeSection = abiReader.GetElfReader().GetSection(codeSectionId);

    void* pPipelineCode = const_cast<void*>(Util::VoidPtrInc(pipelineBinary.pCode,
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
    Vkgc::BinaryData*            pPipelineBinary,
    bool*                        pIsUserCacheHit,
    bool*                        pIsInternalCacheHit,
    FreeCompilerBinary*          pFreeCompilerBinary,
    PipelineCreationFeedback*    pPipelineFeedback)
{
    Util::Result cacheResult = Util::Result::NotFound;
    int64_t      startTime   = Util::GetPerfCpuTime();

    if (pPipelineBinaryCache != nullptr)
    {
        cacheResult = pPipelineBinaryCache->LoadPipelineBinary(
            pCacheId, &pPipelineBinary->codeSize, &pPipelineBinary->pCode);
        if (cacheResult == Util::Result::Success)
        {
            *pIsUserCacheHit = true;
            pPipelineFeedback->hitApplicationCache = true;
        }
    }
    m_pipelineCacheMatrix.cacheAttempts++;

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
            cacheResult = m_pBinaryCache->LoadPipelineBinary(
                pCacheId, &pPipelineBinary->codeSize, &pPipelineBinary->pCode);
        }
        if (cacheResult == Util::Result::Success)
        {
            *pIsInternalCacheHit = true;
        }
    }
    m_pipelineCacheMatrix.totalTimeSpent += Util::GetPerfCpuTime() - startTime;
    if (*pIsUserCacheHit || *pIsInternalCacheHit)
    {
        *pFreeCompilerBinary = FreeWithInstanceAllocator;
        cacheResult = Util::Result::Success;
        m_pipelineCacheMatrix.cacheHits++;
        DumpCacheMatrix(m_pPhysicalDevice,
            "Pipeline_runtime",
            m_pipelineCacheMatrix.totalBinaries + m_pipelineCacheMatrix.cacheHits,
            &m_pipelineCacheMatrix);
    }

    return cacheResult;
}

// =====================================================================================================================
// Store a pipeline binary to the PAL Pipeline cache
void PipelineCompiler::CachePipelineBinary(
    const Util::MetroHash::Hash* pCacheId,
    PipelineBinaryCache*         pPipelineBinaryCache,
    Vkgc::BinaryData*            pPipelineBinary,
    bool                         isUserCacheHit,
    bool                         isInternalCacheHit)
{
    Util::Result cacheResult;

    if ((pPipelineBinaryCache != nullptr) && (isUserCacheHit == false))
    {
        cacheResult = pPipelineBinaryCache->StorePipelineBinary(
            pCacheId,
            pPipelineBinary->codeSize,
            pPipelineBinary->pCode);

        VK_ASSERT(Util::IsErrorResult(cacheResult) == false);
    }

    if ((m_pBinaryCache != nullptr) && (isInternalCacheHit == false))
    {
        cacheResult = m_pBinaryCache->StorePipelineBinary(
            pCacheId,
            pPipelineBinary->codeSize,
            pPipelineBinary->pCode);

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
    const VkPipelineCreateFlags2KHR   flags,
    Vkgc::BinaryData*                 pPipelineBinary,
    Util::MetroHash::Hash*            pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    bool                   shouldCompile = true;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();

    int64_t compileTime   = 0;
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void* pPipelineDumpHandle = nullptr;
    const void* moduleDataBaks[ShaderStage::ShaderStageGfxCount];
    ShaderModuleHandle shaderModuleReplaceHandles[ShaderStage::ShaderStageGfxCount];
    bool shaderModuleReplaced = false;

    Vkgc::PipelineShaderInfo* shaderInfos[ShaderStage::ShaderStageGfxCount] =
    {
        &pCreateInfo->pipelineInfo.task,
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.mesh,
        &pCreateInfo->pipelineInfo.fs,
    };

    if ((settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
        (settings.shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        if (ReplacePipelineBinary(m_pPhysicalDevice, &pCreateInfo->pipelineInfo, pPipelineBinary, pipelineHash))
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

    if (settings.enablePipelineDump && (result == VK_SUCCESS))
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        char tempBuff[Util::MaxPathStrLen];
        InitPipelineDumpOption(&dumpOptions, settings, tempBuff, pCreateInfo->compilerType);

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo = &pCreateInfo->pipelineInfo;
        uint64_t dumpHash          = settings.dumpPipelineWithApiHash ? pCreateInfo->apiPsoHash : pipelineHash;
        pPipelineDumpHandle        = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions,
                                                                              pipelineInfo,
                                                                              dumpHash);
    }

    if (shouldCompile && (result == VK_SUCCESS))
    {
        pCreateInfo->pBinaryMetadata->enableEarlyCompile    = pCreateInfo->pipelineInfo.enableEarlyCompile;
        pCreateInfo->pBinaryMetadata->enableUberFetchShader = pCreateInfo->pipelineInfo.enableUberFetchShader;

        result = GetSolution(pCreateInfo->compilerType)->CreateGraphicsPipelineBinary(
                pDevice,
                deviceIdx,
                pPipelineCache,
                pCreateInfo,
                pPipelineBinary,
                shaderInfos,
                pPipelineDumpHandle,
                pipelineHash,
                pCacheId,
                &compileTime);

        if (result == VK_SUCCESS)
        {
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;
            }
        }
    }

    m_pipelineCacheMatrix.totalTimeSpent += compileTime;
    m_pipelineCacheMatrix.totalBinaries++;

    DumpCacheMatrix(m_pPhysicalDevice,
        "Pipeline_runtime",
        m_pipelineCacheMatrix.totalBinaries + m_pipelineCacheMatrix.cacheHits,
        &m_pipelineCacheMatrix);

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, 0, *pPipelineBinary);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, pPipelineBinary);
            DumpPipelineMetadata(pPipelineDumpHandle, pCreateInfo->pBinaryMetadata);
        }

        char resultMsg[64];
        Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
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

    DropPipelineBinaryInst(pDevice, settings, *pPipelineBinary);

    return result;
}

// =====================================================================================================================
// Create ISA/relocatable shader for a specific shader based on pipeline information
VkResult PipelineCompiler::CreateGraphicsShaderBinary(
    const Device*                     pDevice,
    PipelineCache*                    pPipelineCache,
    GraphicsLibraryType               gplType,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    GplModuleState*                  pModuleState)
{
    VkResult result = VK_SUCCESS;

    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    uint64_t libraryHash = Vkgc::IPipelineDumper::GetGraphicsShaderBinaryHash(
        &pCreateInfo->pipelineInfo,
        (gplType == GraphicsLibraryPreRaster) ? ShaderStageVertex : ShaderStageFragment);
    VK_ASSERT(pCreateInfo->libraryHash[gplType] == libraryHash || pCreateInfo->libraryHash[gplType] == 0);
    pCreateInfo->libraryHash[gplType] = libraryHash;

    void* pPipelineDumpHandle = nullptr;
    if (settings.enablePipelineDump)
    {
        uint64_t dumpHash = settings.dumpPipelineWithApiHash ? pCreateInfo->apiPsoHash : libraryHash;

        Vkgc::PipelineDumpOptions dumpOptions = {};
        char tempBuff[Util::MaxPathStrLen];
        InitPipelineDumpOption(&dumpOptions, settings, tempBuff, pCreateInfo->compilerType);

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pGraphicsInfo           = &pCreateInfo->pipelineInfo;
        pPipelineDumpHandle                  = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions,
                                                                                        pipelineInfo,
                                                                                        dumpHash);
    }

    result = GetSolution(pCreateInfo->compilerType)->CreateGraphicsShaderBinary(
                pDevice,
                pPipelineCache,
                gplType,
                pCreateInfo,
                pPipelineDumpHandle,
                pModuleState);

    if (pPipelineDumpHandle != nullptr)
    {
        char resultMsg[64];
        Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }

    return result;
}

// =====================================================================================================================
// Create ISA/relocatable shader for a specific shader based on pipeline information
VkResult PipelineCompiler::CreateColorExportShaderLibrary(
    const Device*                     pDevice,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*      pAllocator,
    Pal::IShaderLibrary**             ppColExpLib)
{
    VkResult result = VK_SUCCESS;

    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    bool cacheHit                   = false;
    Util::MetroHash::Hash cacheId   = {};
    GetColorExportShaderCacheId(pCreateInfo, &cacheId);
    pCreateInfo->libraryHash[GraphicsLibraryColorExport] = MetroHash::Compact64(&cacheId);

    // Look up cache with respect to the hash
    {
        Util::MutexAuto mutexLock(&m_cacheLock);
        Pal::IShaderLibrary** ppCachedColExpLib = m_colorExportShaderMap.FindKey(cacheId);
        if (ppCachedColExpLib != nullptr)
        {
            *ppColExpLib = *ppCachedColExpLib;
            cacheHit = true;
        }
    }

    if (cacheHit == false)
    {
        Vkgc::BinaryData colExpPackage = {};
        // Temporarily set the unlinked to true for the color export shader
        pCreateInfo->pipelineInfo.unlinked = true;

        void* pPipelineDumpHandle = nullptr;
        if (settings.enablePipelineDump)
        {
            uint64_t dumpHash = settings.dumpPipelineWithApiHash ? pCreateInfo->apiPsoHash :
                                                                   MetroHash::Compact64(&cacheId);

            Vkgc::PipelineDumpOptions dumpOptions = {};
            char tempBuff[Util::MaxPathStrLen];
            InitPipelineDumpOption(&dumpOptions, settings, tempBuff, pCreateInfo->compilerType);

            Vkgc::PipelineBuildInfo pipelineInfo = {};
            GraphicsPipelineBuildInfo graphicsInfo = pCreateInfo->pipelineInfo;
            graphicsInfo.task.pModuleData = nullptr;
            graphicsInfo.task.options.clientHash = {};
            graphicsInfo.vs.pModuleData = nullptr;
            graphicsInfo.vs.options.clientHash = {};
            graphicsInfo.tcs.pModuleData = nullptr;
            graphicsInfo.tcs.options.clientHash = {};
            graphicsInfo.tes.pModuleData = nullptr;
            graphicsInfo.tes.options.clientHash = {};
            graphicsInfo.gs.pModuleData = nullptr;
            graphicsInfo.gs.options.clientHash = {};
            graphicsInfo.mesh.pModuleData = nullptr;
            graphicsInfo.mesh.options.clientHash = {};
            graphicsInfo.fs.pModuleData = nullptr;
            graphicsInfo.fs.options.clientHash = {};
            pipelineInfo.pGraphicsInfo = &graphicsInfo;

            pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions,
                                                                           pipelineInfo,
                                                                           dumpHash);
        }

        FreeCompilerBinary freeMethod = FreeWithCompiler;
        bool elfReplace = false;
        if ((pDevice->GetRuntimeSettings().shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
            (pDevice->GetRuntimeSettings().shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
        {
            elfReplace = ReplacePipelineBinary(m_pPhysicalDevice,
                &pCreateInfo->pipelineInfo,
                &colExpPackage,
                MetroHash::Compact64(&cacheId));
        }

        bool hitAppCache = false;
        bool hitInternalCache = false;
        if (elfReplace == false)
        {
            GetCachedPipelineBinary(
                &cacheId, nullptr, &colExpPackage, &hitAppCache, &hitInternalCache, &freeMethod, nullptr);

            if (hitInternalCache == false)
            {
                result = GetSolution(pCreateInfo->compilerType)->CreateColorExportBinary(
                    pCreateInfo,
                    pPipelineDumpHandle,
                    &colExpPackage);

                if (result == VK_SUCCESS)
                {
                    CachePipelineBinary(&cacheId, nullptr, &colExpPackage, hitAppCache, hitInternalCache);
                }
            }
        }
        else
        {
            freeMethod = FreeWithInstanceAllocator;
        }

        if (result == VK_SUCCESS)
        {
            result = CreateGraphicsShaderLibrary(pDevice, colExpPackage, pAllocator, ppColExpLib);

            if (result == VK_SUCCESS)
            {
                VK_ASSERT(*ppColExpLib != nullptr);
                // Store the color export shader into cache
                Util::MutexAuto mutexLock(&m_cacheLock);
                result = PalToVkResult(m_colorExportShaderMap.Insert(cacheId, *ppColExpLib));
                VK_ASSERT(result == VK_SUCCESS);
            }
        }

        if (pPipelineDumpHandle != nullptr)
        {
            char resultMsg[64];
            Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
            Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
        }
        pCreateInfo->pipelineInfo.unlinked = false;

        if (colExpPackage.pCode != nullptr)
        {
            FreeGraphicsPipelineBinary(pCreateInfo->compilerType, freeMethod, colExpPackage);
        }
    }

    return result;
}

// =====================================================================================================================
// Create shader library object based on pipeline information
VkResult PipelineCompiler::CreateGraphicsShaderLibrary(
    const Device*                pDevice,
    const Vkgc::BinaryData       shaderBinary,
    const VkAllocationCallbacks* pAllocator,
    Pal::IShaderLibrary**        ppShaderLibrary)
{
    Pal::Result palResult    = Pal::Result::Success;
    Pal::IDevice* pPalDevice = pDevice->PalDevice(DefaultDeviceIndex);

    Pal::ShaderLibraryCreateInfo shaderLibCreateInfo = {};
    shaderLibCreateInfo.flags.isGraphics = true;
    shaderLibCreateInfo.codeObjectSize   = shaderBinary.codeSize;
    shaderLibCreateInfo.pCodeObject      = shaderBinary.pCode;

    size_t librarySize = pPalDevice->GetShaderLibrarySize(shaderLibCreateInfo, &palResult);
    void* pShaderLibBuffer = pAllocator->pfnAllocation(pAllocator->pUserData,
                                                       librarySize,
                                                       VK_DEFAULT_MEM_ALIGN,
                                                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    palResult = pPalDevice->CreateShaderLibrary(shaderLibCreateInfo,
                                                pShaderLibBuffer,
                                                ppShaderLibrary);
    VK_ASSERT(palResult == Pal::Result::Success);

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Free variables in GplModuleState
void PipelineCompiler::FreeGplModuleState(
    GplModuleState* pModuleState)
{
    if (IsValidShaderModule(&pModuleState->moduleHandle))
    {
        FreeShaderModule(&pModuleState->moduleHandle);
    }

    if (pModuleState->elfPackage.pCode != nullptr)
    {
        m_pPhysicalDevice->VkInstance()->FreeMem(const_cast<void*>(pModuleState->elfPackage.pCode));
    }

    if (pModuleState->pFsOutputMetaData != nullptr)
    {
        m_pPhysicalDevice->VkInstance()->FreeMem(pModuleState->pFsOutputMetaData);
    }

}

// =====================================================================================================================
// Creates compute pipeline binary.
VkResult PipelineCompiler::CreateComputePipelineBinary(
    Device*                          pDevice,
    uint32_t                         deviceIdx,
    PipelineCache*                   pPipelineCache,
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    Vkgc::BinaryData*                pPipelineBinary,
    Util::MetroHash::Hash*           pCacheId)
{
    VkResult               result        = VK_SUCCESS;
    const RuntimeSettings& settings      = m_pPhysicalDevice->GetRuntimeSettings();

    pCreateInfo->pipelineInfo.deviceIndex = deviceIdx;

    int64_t compileTime   = 0;
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);

    void*              pPipelineDumpHandle       = nullptr;
    const void*        pModuleDataBak            = nullptr;
    ShaderModuleHandle shaderModuleReplaceHandle = {};
    bool               shaderModuleReplaced      = false;
    bool               shouldCompile             = true;

    if ((settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
        (settings.shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        if (ReplacePipelineBinary(m_pPhysicalDevice, &pCreateInfo->pipelineInfo, pPipelineBinary, pipelineHash))
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

    if (settings.enablePipelineDump && (result == VK_SUCCESS))
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};
        char tempBuff[Util::MaxPathStrLen];
        InitPipelineDumpOption(&dumpOptions, settings, tempBuff, pCreateInfo->compilerType);

        Vkgc::PipelineBuildInfo pipelineInfo = {};
        pipelineInfo.pComputeInfo = &pCreateInfo->pipelineInfo;
        uint64_t dumpHash = settings.dumpPipelineWithApiHash ? pCreateInfo->apiPsoHash : pipelineHash;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo, dumpHash);
    }

    if (shouldCompile && (result == VK_SUCCESS))
    {
        result = GetSolution(pCreateInfo->compilerType)->CreateComputePipelineBinary(
            pDevice,
            deviceIdx,
            pPipelineCache,
            pCreateInfo,
            pPipelineBinary,
            pPipelineDumpHandle,
            pipelineHash,
            pCacheId,
            &compileTime);

        if (result == VK_SUCCESS)
        {
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;
            }
        }
    }

    m_pipelineCacheMatrix.totalTimeSpent += compileTime;
    m_pipelineCacheMatrix.totalBinaries++;

    DumpCacheMatrix(m_pPhysicalDevice,
        "Pipeline_runtime",
        m_pipelineCacheMatrix.totalBinaries + m_pipelineCacheMatrix.cacheHits,
        &m_pipelineCacheMatrix);

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        ReplacePipelineIsaCode(pDevice, pipelineHash, 0, *pPipelineBinary);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, pPipelineBinary);
        }
        char resultMsg[64];
        Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }

    if (shaderModuleReplaced)
    {
        pCreateInfo->pipelineInfo.cs.pModuleData = pModuleDataBak;
        FreeShaderModule(&shaderModuleReplaceHandle);
    }

    DropPipelineBinaryInst(pDevice, settings, *pPipelineBinary);

    return result;
}

// =====================================================================================================================
// If provided, initializes the VkPipelineCreationFeedbackCreateInfoEXT struct
void PipelineCompiler::InitPipelineCreationFeedback(
    const VkPipelineCreationFeedbackCreateInfoEXT*   pPipelineCreationFeedbackCreateInfo)
{
    if (pPipelineCreationFeedbackCreateInfo != nullptr)
    {
        pPipelineCreationFeedbackCreateInfo->pPipelineCreationFeedback->flags    = 0;
        pPipelineCreationFeedbackCreateInfo->pPipelineCreationFeedback->duration = 0;

        for (uint32_t i = 0; i < pPipelineCreationFeedbackCreateInfo->pipelineStageCreationFeedbackCount; i++)
        {
            pPipelineCreationFeedbackCreateInfo->pPipelineStageCreationFeedbacks[i].flags    = 0;
            pPipelineCreationFeedbackCreateInfo->pPipelineStageCreationFeedbacks[i].duration = 0;
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
    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo,
    uint32_t                                       stageCount,
    const VkPipelineShaderStageCreateInfo*         pStages,
    const PipelineCreationFeedback*                pPipelineFeedback,
    const PipelineCreationFeedback*                pStageFeedback)
{
    if (pPipelineCreationFeedbackCreateInfo != nullptr)
    {
        UpdatePipelineCreationFeedback(pPipelineCreationFeedbackCreateInfo->pPipelineCreationFeedback,
                                       pPipelineFeedback);

        if (pPipelineCreationFeedbackCreateInfo->pipelineStageCreationFeedbackCount != 0)
        {
            auto *stageCreationFeedbacks = pPipelineCreationFeedbackCreateInfo->pPipelineStageCreationFeedbacks;
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
                        case VK_SHADER_STAGE_TASK_BIT_EXT:
                            feedbackStage = ShaderStage::ShaderStageTask;
                            break;
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
                        case VK_SHADER_STAGE_MESH_BIT_EXT:
                            feedbackStage = ShaderStage::ShaderStageMesh;
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

    // Copy shader libraries
#if VKI_RAY_TRACING
    pCreateInfo->pBinaryMetadata->rayQueryUsed            |= libInfo.pBinaryMetadata->rayQueryUsed;
#endif
    pCreateInfo->pBinaryMetadata->shadingRateUsedInShader |= libInfo.pBinaryMetadata->shadingRateUsedInShader;

    if (shaderMask == PrsShaderMask)
    {
        pCreateInfo->pShaderLibraries[GraphicsLibraryPreRaster] =
            libInfo.pShaderLibraries[GraphicsLibraryPreRaster];
        pCreateInfo->pBinaryMetadata->pointSizeUsed = libInfo.pBinaryMetadata->pointSizeUsed;
        pCreateInfo->pBinaryMetadata->enableUberFetchShader = libInfo.pBinaryMetadata->enableUberFetchShader;
        pCreateInfo->pBinaryMetadata->enableEarlyCompile = libInfo.pBinaryMetadata->enableEarlyCompile;

    }
    else if (shaderMask == FgsShaderMask)
    {
        pCreateInfo->pShaderLibraries[GraphicsLibraryFragment] = libInfo.pShaderLibraries[GraphicsLibraryFragment];
        pCreateInfo->pBinaryMetadata->pFsOutputMetaData        = libInfo.pBinaryMetadata->pFsOutputMetaData;
        pCreateInfo->pBinaryMetadata->fsOutputMetaDataSize     = libInfo.pBinaryMetadata->fsOutputMetaDataSize;
        pCreateInfo->pipelineInfo.enableColorExportShader      =
            (libInfo.pipelineInfo.enableColorExportShader &&
                (libInfo.pBinaryMetadata->pFsOutputMetaData != nullptr));
        pCreateInfo->pBinaryMetadata->postDepthCoverageEnable = libInfo.pBinaryMetadata->postDepthCoverageEnable;
        pCreateInfo->pBinaryMetadata->psOnlyPointCoordEnable  = libInfo.pBinaryMetadata->psOnlyPointCoordEnable;
        pCreateInfo->pBinaryMetadata->dualSrcBlendingUsed     = libInfo.pBinaryMetadata->dualSrcBlendingUsed;
    }

    Vkgc::PipelineShaderInfo* pShaderInfosDst[] =
    {
        &pCreateInfo->pipelineInfo.task,
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.mesh,
        &pCreateInfo->pipelineInfo.fs,
    };

    const Vkgc::PipelineShaderInfo* pShaderInfosSrc[] =
    {
        &libInfo.pipelineInfo.task,
        &libInfo.pipelineInfo.vs,
        &libInfo.pipelineInfo.tcs,
        &libInfo.pipelineInfo.tes,
        &libInfo.pipelineInfo.gs,
        &libInfo.pipelineInfo.mesh,
        &libInfo.pipelineInfo.fs,
    };

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if ((shaderMask & (1 << stage)) != 0)
        {
            *pShaderInfosDst[stage] = *pShaderInfosSrc[stage];
        }
    }

    for (uint32_t gplType = 0; gplType < GraphicsLibraryCount; ++gplType)
    {
        if (libInfo.libraryHash[gplType] != 0)
        {
            pCreateInfo->earlyElfPackage[gplType] = libInfo.earlyElfPackage[gplType];
            pCreateInfo->earlyElfPackageHash[gplType] = libInfo.earlyElfPackageHash[gplType];
            pCreateInfo->libraryHash[gplType] = libInfo.libraryHash[gplType];
        }
    }
}

// =====================================================================================================================
static void CopyVertexInputInterfaceState(
    const Device*                     pDevice,
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    pCreateInfo->pipelineInfo.pVertexInput               = libInfo.pipelineInfo.pVertexInput;
    pCreateInfo->pipelineInfo.iaState.topology           = libInfo.pipelineInfo.iaState.topology;
    pCreateInfo->pipelineInfo.iaState.disableVertexReuse = libInfo.pipelineInfo.iaState.disableVertexReuse;
    pCreateInfo->pipelineInfo.dynamicVertexStride        = libInfo.pipelineInfo.dynamicVertexStride;

    if (pCreateInfo->pipelineInfo.pVertexInput != nullptr)
    {
        BuildLlpcVertexInputDescriptors(
            pDevice, pCreateInfo->pipelineInfo.pVertexInput, &pCreateInfo->pBinaryMetadata->vbInfo);
    }

    if (libInfo.pBinaryMetadata->internalBufferInfo.internalBufferCount > 0)
    {
        VK_ASSERT(libInfo.pBinaryMetadata->internalBufferInfo.internalBufferCount == 1);
        VK_ASSERT(pCreateInfo->pBinaryMetadata->internalBufferInfo.internalBufferCount == 0);
        pCreateInfo->pBinaryMetadata->internalBufferInfo.internalBufferCount = 1;
        pCreateInfo->pBinaryMetadata->internalBufferInfo.internalBufferEntries[0] =
            libInfo.pBinaryMetadata->internalBufferInfo.internalBufferEntries[0];
    }
}

// =====================================================================================================================
static void MergePipelineOptions(
    const Vkgc::PipelineOptions& src,
    Vkgc::PipelineOptions*       pDst)
{
    pDst->includeDisassembly                    |= src.includeDisassembly;
    pDst->scalarBlockLayout                     |= src.scalarBlockLayout;
    pDst->reconfigWorkgroupLayout               |= src.reconfigWorkgroupLayout;
    pDst->forceCsThreadIdSwizzling              |= src.forceCsThreadIdSwizzling;
    pDst->includeIr                             |= src.includeIr;
    pDst->robustBufferAccess                    |= src.robustBufferAccess;
    pDst->enableRelocatableShaderElf            |= src.enableRelocatableShaderElf;
    pDst->disableImageResourceCheck             |= src.disableImageResourceCheck;
    pDst->extendedRobustness.nullDescriptor     |= src.extendedRobustness.nullDescriptor;
    pDst->extendedRobustness.robustBufferAccess |= src.extendedRobustness.robustBufferAccess;
    pDst->extendedRobustness.robustImageAccess  |= src.extendedRobustness.robustImageAccess;
#if VKI_BUILD_GFX11
    pDst->optimizeTessFactor                    |= src.optimizeTessFactor;
#endif
    pDst->enableInterpModePatch                 |= src.enableInterpModePatch;
    pDst->pageMigrationEnabled                  |= src.pageMigrationEnabled;
    pDst->optimizationLevel                     |= src.optimizationLevel;
    pDst->disableTruncCoordForGather            |= src.disableTruncCoordForGather;

    pDst->shadowDescriptorTableUsage   = src.shadowDescriptorTableUsage;
    pDst->shadowDescriptorTablePtrHigh = src.shadowDescriptorTablePtrHigh;
    pDst->overrideThreadGroupSizeX     = src.overrideThreadGroupSizeX;
    pDst->overrideThreadGroupSizeY     = src.overrideThreadGroupSizeY;
    pDst->overrideThreadGroupSizeZ     = src.overrideThreadGroupSizeZ;
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
    pCreateInfo->pipelineInfo.rsState.rasterStream            = libInfo.pipelineInfo.rsState.rasterStream;
    pCreateInfo->pipelineInfo.nggState                        = libInfo.pipelineInfo.nggState;
    pCreateInfo->pipelineInfo.enableUberFetchShader           = libInfo.pipelineInfo.enableUberFetchShader;
    pCreateInfo->pipelineInfo.useSoftwareVertexBufferDescriptors =
        libInfo.pipelineInfo.useSoftwareVertexBufferDescriptors;

    MergePipelineOptions(libInfo.pipelineInfo.options, &pCreateInfo->pipelineInfo.options);

    CopyPipelineShadersInfo<PrsShaderMask>(pLibrary, pCreateInfo);
}

// =====================================================================================================================
static void CopyFragmentShaderState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const GraphicsPipelineBinaryCreateInfo& libInfo = pLibrary->GetPipelineBinaryCreateInfo();

    if (libInfo.pipelineInfo.rsState.perSampleShading || (libInfo.pipelineInfo.rsState.numSamples != 1))
    {
        // pMultisampleState is not NULL.
        pCreateInfo->pipelineInfo.rsState.perSampleShading   = libInfo.pipelineInfo.rsState.perSampleShading;
        pCreateInfo->pipelineInfo.rsState.dynamicSampleInfo  = libInfo.pipelineInfo.rsState.dynamicSampleInfo;
        pCreateInfo->pipelineInfo.rsState.numSamples         = libInfo.pipelineInfo.rsState.numSamples;
        pCreateInfo->pipelineInfo.rsState.samplePatternIdx   = libInfo.pipelineInfo.rsState.samplePatternIdx;
        pCreateInfo->pipelineInfo.rsState.pixelShaderSamples = libInfo.pipelineInfo.rsState.pixelShaderSamples;
    }
    else
    {
        pCreateInfo->pipelineInfo.rsState.numSamples = 1;
    }

    pCreateInfo->pipelineInfo.dsState.depthTestEnable   = libInfo.pipelineInfo.dsState.depthTestEnable;
    pCreateInfo->pipelineInfo.dsState.depthWriteEnable  = libInfo.pipelineInfo.dsState.depthWriteEnable;
    pCreateInfo->pipelineInfo.dsState.depthCompareOp    = libInfo.pipelineInfo.dsState.depthCompareOp;
    pCreateInfo->pipelineInfo.dsState.stencilTestEnable = libInfo.pipelineInfo.dsState.stencilTestEnable;
    pCreateInfo->pipelineInfo.dsState.front             = libInfo.pipelineInfo.dsState.front;
    pCreateInfo->pipelineInfo.dsState.back              = libInfo.pipelineInfo.dsState.back;

    MergePipelineOptions(libInfo.pipelineInfo.options, &pCreateInfo->pipelineInfo.options);

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

    pCreateInfo->pipelineInfo.rsState.perSampleShading   = libInfo.pipelineInfo.rsState.perSampleShading;
    pCreateInfo->pipelineInfo.rsState.dynamicSampleInfo  = libInfo.pipelineInfo.rsState.dynamicSampleInfo;
    pCreateInfo->pipelineInfo.rsState.numSamples         = libInfo.pipelineInfo.rsState.numSamples;
    pCreateInfo->pipelineInfo.rsState.samplePatternIdx   = libInfo.pipelineInfo.rsState.samplePatternIdx;
    pCreateInfo->pipelineInfo.rsState.pixelShaderSamples = libInfo.pipelineInfo.rsState.pixelShaderSamples;

    pCreateInfo->dbFormat                                    = libInfo.dbFormat;
    pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable  = libInfo.pipelineInfo.cbState.alphaToCoverageEnable;
    pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable  = libInfo.pipelineInfo.cbState.dualSourceBlendEnable;
    pCreateInfo->pipelineInfo.cbState.dualSourceBlendDynamic = libInfo.pipelineInfo.cbState.dualSourceBlendDynamic;
    pCreateInfo->pipelineInfo.iaState.enableMultiView        = libInfo.pipelineInfo.iaState.enableMultiView;
    pCreateInfo->cbStateHash                                 = libInfo.cbStateHash;
}

// =====================================================================================================================
static void BuildRasterizationState(
    const VkPipelineRasterizationStateCreateInfo* pRs,
    const uint64_t                                dynamicStateFlags,
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
            pCreateInfo->pipelineInfo.rsState.rasterStream =
                pPipelineRasterizationStateStreamCreateInfoEXT->rasterizationStream;
        }
        else
        {
            pCreateInfo->pipelineInfo.rsState.rasterStream = 0;
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable) == true)
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
static void BuildMultisampleState(
    const Device*                               pDevice,
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    GraphicsPipelineBinaryCreateInfo*           pCreateInfo,
    const uint64_t                              dynamicStateFlags)
{
    if (pMs != nullptr)
    {
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples))
        {
            // This will be updated later
            pCreateInfo->pipelineInfo.rsState.perSampleShading = true;
            pCreateInfo->pipelineInfo.rsState.pixelShaderSamples = 1;
            pCreateInfo->pipelineInfo.rsState.samplePatternIdx = 0;
            pCreateInfo->pipelineInfo.rsState.numSamples = 1;
            pCreateInfo->pipelineInfo.rsState.dynamicSampleInfo = true;
            pCreateInfo->pipelineInfo.options.enableInterpModePatch = false;
        }
        else
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

                // NOTE: The sample pattern index here is actually the offset of sample position pair. This is
                // different from the field of creation info of image view. For image view, the sample pattern
                // index is really table index of the sample pattern.
                pCreateInfo->pipelineInfo.rsState.samplePatternIdx =
                    Device::GetDefaultSamplePatternIndex(subpassCoverageSampleCount) * Pal::MaxMsaaRasterizerSamples;
            }

            pCreateInfo->pipelineInfo.rsState.numSamples = pMs->rasterizationSamples;
            pCreateInfo->pipelineInfo.options.enableInterpModePatch = false;
        }

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
    uint64_t                                    dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*           pCreateInfo)
{
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::AlphaToCoverageEnable) == true)
    {
        pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = true;
    }
    else if (pMs != nullptr)
    {
        pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = (pMs->alphaToCoverageEnable == VK_TRUE);
    }
}

// =====================================================================================================================
void PipelineCompiler::BuildNggState(
    const Device*                     pDevice,
    const VkShaderStageFlagBits       activeStages,
    const bool                        isConservativeOverestimation,
    const bool                        unrestrictedPrimitiveTopology,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    const RuntimeSettings&       settings   = pDevice->GetRuntimeSettings();
    const Pal::DeviceProperties& deviceProp = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    // NOTE: To support unrestrict dynamic primtive topology, we need full disable NGG on gfx10.
    bool disallowNgg = unrestrictedPrimitiveTopology;
#if VKI_BUILD_GFX11
    // On gfx11, we needn't program GS output primitive type on VsPs pipeline, so we can support unrestrict dynamic
    // primtive topology with NGG.
    disallowNgg = (disallowNgg && (deviceProp.gfxLevel < Pal::GfxIpLevel::GfxIp11_0));
#endif
    if (disallowNgg)
    {
        pCreateInfo->pipelineInfo.nggState.enableNgg = false;
    }
    else
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
        pCreateInfo->pipelineInfo.nggState.forceCullingMode          = settings.nggForceCullingMode;

        pCreateInfo->pipelineInfo.nggState.compactVertex             = settings.nggCompactVertex;
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
    const PipelineOptimizerKey*                   pOptimizerKey,
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
                                             pShaderInfoIn->flags,
                                             &pShaderInfoOut->options);

        pShaderInfoOut->options.clientHash.lower = pShaderInfoIn->codeHash.lower;
        pShaderInfoOut->options.clientHash.upper = pShaderInfoIn->codeHash.upper;

        ApplyProfileOptions(pDevice,
                            static_cast<uint32_t>(stage),
                            pPipelineOptions,
                            pShaderInfoOut,
                            pOptimizerKey,
                            pNggState);

        // If DeprecateWave64Cs or DeprecateWave64NonCs is set, driver might not report wave32-only support,
        // but we want to force wavesize to wave32 internally depending on settings and shader stage.
        // We override any wavesize forced via shader opts also here.
        // NOTE: If the app uses subgroup size then wavesize forced here might get overriden later based on
        // subgroupsize. To avoid this behavior, DeprecateWave64Reporting must be set as well in settings.
        pShaderInfoOut->options.waveSize =
            ShouldForceWave32(static_cast<ShaderStage>(stage), pDevice->GetRuntimeSettings().deprecateWave64) ?
                32 : pShaderInfoOut->options.waveSize;
    }
}

// =====================================================================================================================
static VkResult BuildPipelineResourceMapping(
    const Device*                     pDevice,
    const PipelineLayout*             pLayout,
    const uint32_t                    stageMask,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    VkResult result = VK_SUCCESS;

    if ((pLayout != nullptr) && (pLayout->GetPipelineInfo()->mappingBufferSize > 0))
    {
        pCreateInfo->pipelineInfo.pipelineLayoutApiHash = pLayout->GetApiHash();

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
                                                       &pCreateInfo->pBinaryMetadata->vbInfo,
                                                       pCreateInfo->pipelineInfo.enableUberFetchShader,
#if VKI_RAY_TRACING
                                                       false,
#endif
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
        &pCreateInfo->pipelineInfo.task,
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.mesh,
        &pCreateInfo->pipelineInfo.fs,
    };

    if (pCreateInfo->compilerType == PipelineCompilerTypeInvalid)
    {
        pCreateInfo->compilerType =
            pDevice->GetCompiler(DefaultDeviceIndex)->CheckCompilerType(&pCreateInfo->pipelineInfo, 0, 0);
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
template <uint32_t shaderMask>
static void BuildPipelineShadersInfo(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const uint32_t                         dynamicStateFlags,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if (pCreateInfo->pipelineInfo.options.enableRelocatableShaderElf)
    {
        CompilerSolution::DisableNggCulling(&pCreateInfo->pipelineInfo.nggState);
    }

    Vkgc::PipelineShaderInfo* ppShaderInfoOut[] =
    {
        &pCreateInfo->pipelineInfo.task,
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.mesh,
        &pCreateInfo->pipelineInfo.fs,
    };

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (((shaderMask & (1 << stage)) != 0) &&
            ((pShaderInfo->stages[stage].pModuleHandle != nullptr) ||
             (pShaderInfo->stages[stage].codeHash.lower != 0) ||
             (pShaderInfo->stages[stage].codeHash.upper != 0)))
        {
            GraphicsLibraryType gplType = GetGraphicsLibraryType(static_cast<ShaderStage>(stage));

            PipelineCompiler::BuildPipelineShaderInfo(pDevice,
                                    &pShaderInfo->stages[stage],
                                    ppShaderInfoOut[stage],
                                    &pCreateInfo->pipelineInfo.options,
                                    pCreateInfo->pPipelineProfileKey,
                                    &pCreateInfo->pipelineInfo.nggState
                                    );

            if ((stage == ShaderStage::ShaderStageFragment) &&
                (ppShaderInfoOut[stage]->options.allowReZ == true) && settings.disableDepthOnlyReZ)
            {
                bool usesDepthOnlyAttachments = true;

                for (uint32_t i = 0; i < Pal::MaxColorTargets; ++i)
                {
                    if (pCreateInfo->pipelineInfo.cbState.target[i].channelWriteMask != 0)
                    {
                        usesDepthOnlyAttachments = false;
                        break;
                    }
                }

                if (usesDepthOnlyAttachments)
                {
                    ppShaderInfoOut[stage]->options.allowReZ = false;
                }
            }
        }
    }

    // Uber fetch shader is actually used in the following scenes:
    // * enableUberFetchShader or enableEarlyCompile is set as TRUE in panel.
    // * When creating pipeline, GraphicsPipelineBuildInfo::enableUberFetchShader controls the actual enablement. It is
    //   only set when Vertex Input Interface section (VII) is not available and Pre-Rasterization Shader (PRS) is
    //   available, or inherits from its PRS parent (referenced library). However, enableUberFetchShader would also be
    //   set as FALSE even if its parent set it as TRUE if current pipeline want to re-compile pre-rasterization shaders
    //   and VII is available.  This may happen when VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT is set.  More
    //   details can be found in PipelineCompiler::ConvertGraphicsPipelineInfo().
    // PS: For standard gfx pipeline, GraphicsPipelineBuildInfo::enableUberFetchShader is never set as TRUE with default
    //     panel setting because VII and PRS are always available at the same time.
    if (settings.enableUberFetchShader ||
        settings.enableEarlyCompile ||
        (((pCreateInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) == 0) &&
         ((pCreateInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)) ||
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInput) == true)
        )
    {
        pCreateInfo->pipelineInfo.enableUberFetchShader = true;
        pCreateInfo->pBinaryMetadata->enableUberFetchShader = true;
    }
}

// =====================================================================================================================
static void BuildColorBlendState(
    const Device*                              pDevice,
    const VkPipelineColorBlendStateCreateInfo* pCb,
    const GraphicsPipelineExtStructs&          extStructs,
    uint64_t                                   dynamicStateFlags,
    const RenderPass*                          pRenderPass,
    const uint32_t                             subpass,
    GraphicsPipelineBinaryCreateInfo*          pCreateInfo
)
{
    auto pRendering = extStructs.pPipelineRenderingCreateInfo;

    if ((pCb != nullptr) || (pRendering != nullptr))
    {
        const uint32_t numColorTargets = (pRendering != nullptr) ?
            Util::Min(pRendering->colorAttachmentCount, Pal::MaxColorTargets) :
            Util::Min(pCb->attachmentCount, Pal::MaxColorTargets);

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation) ||
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable))
        {
            {
                pCreateInfo->pipelineInfo.cbState.dualSourceBlendDynamic = true;
            }
        }
        else if (pCb != nullptr)
        {
            pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable =
                GraphicsPipelineCommon::GetDualSourceBlendEnableState(pDevice, pCb, extStructs);
        }

        for (uint32_t i = 0; i < numColorTargets; ++i)
        {
            uint32_t location = i;

            if ((extStructs.pRenderingAttachmentLocationInfo                            != nullptr) &&
                (extStructs.pRenderingAttachmentLocationInfo->pColorAttachmentLocations != nullptr))
            {
                location = extStructs.pRenderingAttachmentLocationInfo->pColorAttachmentLocations[i];

                if (location == VK_ATTACHMENT_UNUSED)
                {
                    continue;
                }
            }

            auto pLlpcCbDst = &pCreateInfo->pipelineInfo.cbState.target[location];

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
                VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                       VK_COLOR_COMPONENT_G_BIT |
                                                       VK_COLOR_COMPONENT_B_BIT |
                                                       VK_COLOR_COMPONENT_A_BIT;

                if ((pCb != nullptr) && (i < pCb->attachmentCount))
                {
                    const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];
                    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteMask) == false)
                    {
                        colorWriteMask = src.colorWriteMask;
                    }

                    pLlpcCbDst->blendEnable = (src.blendEnable == VK_TRUE) ||
                        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable);
                    pLlpcCbDst->blendSrcAlphaToColor =
                        GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcAlphaBlendFactor) ||
                        GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstAlphaBlendFactor) ||
                        GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.srcColorBlendFactor) ||
                        GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(src.dstColorBlendFactor) ||
                        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation);
                }
                else
                {
                    pLlpcCbDst->blendEnable =
                        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable);
                    pLlpcCbDst->blendSrcAlphaToColor =
                        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation);
                }

                pLlpcCbDst->format = cbFormat;
                pLlpcCbDst->channelWriteMask = colorWriteMask;
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
            else if (i == 1)
            {
                // Duplicate CB0 state to support dual source blend.
                if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation) &&
                    pCreateInfo->pipelineInfo.cbState.dualSourceBlendDynamic)
                {
                    auto pTarget0 = &pCreateInfo->pipelineInfo.cbState.target[0];
                    if (pTarget0->blendEnable)
                    {
                        *pLlpcCbDst = *pTarget0;
                    }
                }
            }
        }
    }

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
    const uint64_t                      dynamicStateFlags,
    const VkShaderStageFlagBits         activeStages,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo)
{
    pCreateInfo->pipelineInfo.iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if ((pIn->pInputAssemblyState) && (Util::TestAnyFlagSet(activeStages, VK_SHADER_STAGE_MESH_BIT_EXT) == false))
    {
        pCreateInfo->pipelineInfo.iaState.topology           = pIn->pInputAssemblyState->topology;
        pCreateInfo->pipelineInfo.iaState.disableVertexReuse = false;
    }

    if ((activeStages & VK_SHADER_STAGE_MESH_BIT_EXT) == 0)
    {
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInput) == true)
        {
            // Vertex buffer table entry pointer is dependent on bindingTableSize in PAL.
            // So, force size to max size when dynamic vertex input is enabled.
            pCreateInfo->pBinaryMetadata->vbInfo.bindingCount     = 0;
            pCreateInfo->pBinaryMetadata->vbInfo.bindingTableSize = Pal::MaxVertexBuffers;
        }
        else if (pIn->pVertexInputState)
        {
            pCreateInfo->pipelineInfo.pVertexInput = pIn->pVertexInputState;
            if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInputBindingStride) == true)
            {
                pCreateInfo->pipelineInfo.dynamicVertexStride = true;
            }

            BuildLlpcVertexInputDescriptors(pDevice, pIn->pVertexInputState, &pCreateInfo->pBinaryMetadata->vbInfo);
        }
    }
}

// =====================================================================================================================
static void BuildPreRasterizationShaderState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineLibraryInfo&     libInfo,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const uint64_t                         dynamicStateFlags,
    const VkShaderStageFlagBits            activeStages,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
{
    const RenderPass* pRenderPass                  = RenderPass::ObjectFromHandle(pIn->renderPass);
    bool              isConservativeOverestimation = false;
    bool              vertexInputAbsent =
        libInfo.flags.isLibrary &&
        (libInfo.pVertexInputInterfaceLib == nullptr) &&
        ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) == 0);

    bool              unrestrictedPrimitiveTopology =
        pDevice->GetEnabledFeatures().assumeDynamicTopologyInLibs ||
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopology) &&
        pDevice->GetEnabledFeatures().dynamicPrimitiveTopologyUnrestricted) ||
        (vertexInputAbsent && pDevice->GetRuntimeSettings().useShaderLibraryForPipelineLibraryFastLink);

    BuildRasterizationState(pIn->pRasterizationState, dynamicStateFlags, &isConservativeOverestimation, pCreateInfo);

    PipelineCompiler::BuildNggState(
        pDevice, activeStages, isConservativeOverestimation, unrestrictedPrimitiveTopology, pCreateInfo);

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
            pCreateInfo->pipelineInfo.iaState.patchControlPoints =
                pPipelineTessellationStateCreateInfo->patchControlPoints;
        }

        if (pPipelineTessellationDomainOriginStateCreateInfo &&
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::TessellationDomainOrigin) == false))
        {
            // Vulkan 1.0 incorrectly specified the tessellation u,v coordinate origin as lower left even though
            // framebuffer and image coordinate origins are in the upper left.  This has since been fixed, but
            // an extension exists to use the previous behavior.  Doing so with flat shading would likely appear
            // incorrect, but Vulkan specifies that the provoking vertex is undefined when tessellation is active.
            if (pPipelineTessellationDomainOriginStateCreateInfo->domainOrigin ==
                VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT)
            {
                pCreateInfo->pipelineInfo.iaState.switchWinding = true;
            }
        }
    }

    BuildPipelineShadersInfo<PrsShaderMask>(pDevice, pIn, dynamicStateFlags, pShaderInfo, pCreateInfo);

    if (libInfo.flags.isLibrary)
    {
        BuildCompilerInfo(pDevice, pShaderInfo, PrsShaderMask, pCreateInfo);
    }

    if (pCreateInfo->pipelineInfo.options.enableRelocatableShaderElf)
    {
        CompilerSolution::DisableNggCulling(&pCreateInfo->pipelineInfo.nggState);
    }
}

// =====================================================================================================================
static void BuildFragmentShaderState(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    const GraphicsPipelineLibraryInfo&     libInfo,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo,
    const uint64_t                         dynamicStateFlags)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildMultisampleState(pDevice, pIn->pMultisampleState, pRenderPass, pIn->subpass, pCreateInfo,
        dynamicStateFlags);

    BuildDepthStencilState(pIn->pDepthStencilState, pCreateInfo);

    BuildPipelineShadersInfo<FgsShaderMask>(pDevice, pIn, 0, pShaderInfo, pCreateInfo);

    if (libInfo.flags.isLibrary)
    {
        BuildCompilerInfo(pDevice, pShaderInfo, FgsShaderMask, pCreateInfo);
    }
}

// =====================================================================================================================
static void BuildFragmentOutputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const GraphicsPipelineExtStructs&   extStructs,
    uint64_t                            dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*   pCreateInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);

    BuildMultisampleStateInFoi(pIn->pMultisampleState, dynamicStateFlags, pCreateInfo);

    BuildMultisampleState(pDevice, pIn->pMultisampleState, pRenderPass, pIn->subpass, pCreateInfo,
        dynamicStateFlags);

    BuildColorBlendState(pDevice,
                         pIn->pColorBlendState,
                         extStructs,
                         dynamicStateFlags,
                         pRenderPass, pIn->subpass, pCreateInfo
    );

    auto pPipelineRenderingCreateInfo = extStructs.pPipelineRenderingCreateInfo;
    pCreateInfo->pipelineInfo.iaState.enableMultiView =
        (pRenderPass != nullptr) ? pRenderPass->IsMultiviewEnabled() :
                                   ((pPipelineRenderingCreateInfo != nullptr) &&
                                    (Util::CountSetBits(pPipelineRenderingCreateInfo->viewMask) != 0));

    // Build color export shader partial hash
    Util::MetroHash64 hasher = {};
    Util::MetroHash::Hash cbStateHash = {};
    const auto& cbState = pCreateInfo->pipelineInfo.cbState;
    hasher.Update(pCreateInfo->pipelineInfo.iaState.enableMultiView);
    hasher.Update(cbState.alphaToCoverageEnable);
    hasher.Update(cbState.dualSourceBlendEnable);
    hasher.Update(cbState.dualSourceBlendDynamic);
    for (uint32_t i = 0; i < MaxColorTargets; ++i)
    {
        hasher.Update(cbState.target[i].channelWriteMask);
        hasher.Update(cbState.target[i].blendEnable);
        hasher.Update(cbState.target[i].blendSrcAlphaToColor);
        hasher.Update(cbState.target[i].format);
    }
    hasher.Finalize(cbStateHash.bytes);
    pCreateInfo->cbStateHash = cbStateHash.qwords[0];
}

// =====================================================================================================================
static void BuildExecutablePipelineState(
    Device*                                pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    VkPipelineCreateFlags2KHR              flags,
    const GraphicsPipelineShaderStageInfo* pShaderInfo,
    const GraphicsPipelineLibraryInfo*     pLibInfo,
    const PipelineLayout*                  pPipelineLayout,
    const uint64_t                         dynamicStateFlags,
    GraphicsPipelineBinaryCreateInfo*      pCreateInfo)
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
    constexpr auto StageNone = static_cast<Vkgc::ShaderStageBit>(0);

    // If this pipeline is being linked from libraries, libFlags will determine which state should be taken from
    // VkGraphicsPipelineCreateInfo. Regular pipelines will include the full state.
    if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)
    {
        shaderMask |= (pipelineInfo.task.pModuleData != nullptr) ? Vkgc::ShaderStageTaskBit        : StageNone;
        shaderMask |= (pipelineInfo.vs.pModuleData   != nullptr) ? Vkgc::ShaderStageVertexBit      : StageNone;
        shaderMask |= (pipelineInfo.tcs.pModuleData  != nullptr) ? Vkgc::ShaderStageTessControlBit : StageNone;
        shaderMask |= (pipelineInfo.tes.pModuleData  != nullptr) ? Vkgc::ShaderStageTessEvalBit    : StageNone;
        shaderMask |= (pipelineInfo.gs.pModuleData   != nullptr) ? Vkgc::ShaderStageGeometryBit    : StageNone;
        shaderMask |= (pipelineInfo.mesh.pModuleData != nullptr) ? Vkgc::ShaderStageMeshBit        : StageNone;
    }
    if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0)
    {
        shaderMask |= (pipelineInfo.fs.pModuleData   != nullptr) ? Vkgc::ShaderStageFragmentBit    : StageNone;
    }
    BuildCompilerInfo(pDevice, pShaderInfo, shaderMask, pCreateInfo);

    if (pCreateInfo->pipelineInfo.enableUberFetchShader)
    {
        pDefaultCompiler->BuildPipelineInternalBufferData(pPipelineLayout, true, pCreateInfo);
        pDefaultCompiler->UploadInternalBufferData(pDevice, pCreateInfo);
        pCreateInfo->pBinaryMetadata->enableUberFetchShader = pCreateInfo->pipelineInfo.enableUberFetchShader;
    }

#if VKI_RAY_TRACING
    const Vkgc::PipelineShaderInfo* shaderInfos[] =
    {
        &pCreateInfo->pipelineInfo.task,
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.mesh,
        &pCreateInfo->pipelineInfo.fs,
    };

    bool enableRayQuery = false;

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (shaderInfos[stage]->pModuleData != nullptr)
        {
            const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(shaderInfos[stage]->pModuleData);
            if (pModuleData->usage.enableRayQuery)
            {
                enableRayQuery = true;
                break;
            }
        }
    }

    if (enableRayQuery)
    {
        pDefaultCompiler->SetRayTracingState(pDevice, &(pCreateInfo->pipelineInfo.rtState), 0);

    }
#endif

    pCreateInfo->linkTimeOptimization = (flags & VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT);
    if (pCreateInfo->linkTimeOptimization)
    {
        pCreateInfo->pipelineInfo.enableColorExportShader = false;
    }
}

// =====================================================================================================================
VkResult PipelineCompiler::UploadInternalBufferData(
    Device*                           pDevice,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    VkResult result = VK_SUCCESS;
    PipelineInternalBufferInfo* pInternalBufferInfo = &pCreateInfo->pBinaryMetadata->internalBufferInfo;
    if (pInternalBufferInfo->dataSize > 0)
    {
        InternalMemory* pInternalMem = nullptr;
        void* pMem = pDevice->VkInstance()->AllocMem(sizeof(InternalMemory),
                                                     VK_DEFAULT_MEM_ALIGN,
                                                     VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        if (pMem != nullptr)
        {
            pInternalMem = VK_PLACEMENT_NEW(pMem) InternalMemory;

            InternalMemCreateInfo allocInfo = {};
            allocInfo.pal.size = pInternalBufferInfo->dataSize;
            allocInfo.pal.alignment = VK_DEFAULT_MEM_ALIGN;
            allocInfo.pal.priority = Pal::GpuMemPriority::Normal;

            pDevice->MemMgr()->GetCommonPool(InternalPoolDescriptorTable, &allocInfo);

            result = pDevice->MemMgr()->AllocGpuMem(
                allocInfo,
                pInternalMem,
                pDevice->GetPalDeviceMask(),
                VK_OBJECT_TYPE_PIPELINE,
                0);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        if (result != VK_SUCCESS)
        {
            VK_NEVER_CALLED();
            pCreateInfo->pipelineInfo.enableEarlyCompile = false;
            pCreateInfo->pipelineInfo.enableUberFetchShader = false;
            if (pMem != nullptr)
            {
                pDevice->VkInstance()->FreeMem(pMem);
            }
        }
        else
        {
            pCreateInfo->pInternalMem = pInternalMem;
        }
    }

    if (pCreateInfo->pInternalMem != nullptr)
    {
        utils::IterateMask deviceGroup(pDevice->GetPalDeviceMask());
        do
        {
            const uint32_t deviceIdx      = deviceGroup.Index();
            void*          pBufferCpuAddr = pCreateInfo->pInternalMem->CpuAddr(deviceIdx);
            Pal::gpusize   bufferGpuAddr  = pCreateInfo->pInternalMem->GpuVirtAddr(deviceIdx);
            memcpy(pBufferCpuAddr, pInternalBufferInfo->pData, pInternalBufferInfo->dataSize);

            for (uint32_t i = 0; i < pInternalBufferInfo->internalBufferCount; i++)
            {
                auto pEntry = &pInternalBufferInfo->internalBufferEntries[i];
                if (pEntry->bufferAddress[deviceIdx] == 0)
                {
                    pEntry->bufferAddress[deviceIdx] = bufferGpuAddr + pEntry->bufferOffset;
                }
            }
        } while (deviceGroup.IterateNext());
    }

    return result;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    Device*                                         pDevice,
    const VkGraphicsPipelineCreateInfo*             pIn,
    const GraphicsPipelineExtStructs&               extStructs,
    const GraphicsPipelineLibraryInfo&              libInfo,
    VkPipelineCreateFlags2KHR                       flags,
    const GraphicsPipelineShaderStageInfo*          pShaderInfo,
    const PipelineLayout*                           pPipelineLayout,
    const PipelineOptimizerKey*                     pPipelineProfileKey,
    PipelineMetadata*                               pBinaryMetadata,
    GraphicsPipelineBinaryCreateInfo*               pCreateInfo)
{
    VK_ASSERT(pIn != nullptr);

    VkResult result = VK_SUCCESS;

    if (result == VK_SUCCESS)
    {
        pCreateInfo->pBinaryMetadata = pBinaryMetadata;
        pCreateInfo->pPipelineProfileKey = pPipelineProfileKey;

        pCreateInfo->libFlags = libInfo.libFlags;

        pCreateInfo->libFlags |= (libInfo.pVertexInputInterfaceLib == nullptr) ?
                                 0 : VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
        pCreateInfo->libFlags |= (libInfo.pPreRasterizationShaderLib == nullptr) ?
                                 0 : VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
        pCreateInfo->libFlags |= (libInfo.pFragmentShaderLib == nullptr) ?
                                 0 : VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
        pCreateInfo->libFlags |= (libInfo.pFragmentOutputInterfaceLib == nullptr) ?
                                 0 : VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

        pCreateInfo->flags = flags;
        pDevice->GetCompiler(DefaultDeviceIndex)->ApplyPipelineOptions(pDevice,
                                                                       flags,
                                                                       &pCreateInfo->pipelineInfo.options
        );

        pCreateInfo->pipelineInfo.useSoftwareVertexBufferDescriptors =
            pDevice->GetEnabledFeatures().robustVertexBufferExtend;

    }

    uint64_t dynamicStateFlags = 0;

    if (result == VK_SUCCESS)
    {
        VkShaderStageFlagBits activeStages = GraphicsPipelineCommon::GetActiveShaderStages(pIn, &libInfo);

        dynamicStateFlags = GraphicsPipelineCommon::GetDynamicStateFlags(pIn->pDynamicState, &libInfo);

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
        {
            BuildVertexInputInterfaceState(pDevice, pIn, dynamicStateFlags, activeStages, pCreateInfo);
        }
        else if (libInfo.pVertexInputInterfaceLib != nullptr)
        {
            CopyVertexInputInterfaceState(pDevice, libInfo.pVertexInputInterfaceLib, pCreateInfo);
        }

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
        {
            BuildPreRasterizationShaderState(
                pDevice, pIn, libInfo, pShaderInfo, dynamicStateFlags, activeStages, pCreateInfo);
        }
        else if (libInfo.pPreRasterizationShaderLib != nullptr)
        {
            CopyPreRasterizationShaderState(libInfo.pPreRasterizationShaderLib, pCreateInfo);
        }

        const bool enableRasterization =
            (~libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
            (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false) ||
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable);

        if (enableRasterization)
        {
            if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
            {
                BuildFragmentShaderState(pDevice, pIn, libInfo, pShaderInfo, pCreateInfo, dynamicStateFlags);
                pCreateInfo->pipelineInfo.enableColorExportShader =
                    (libInfo.flags.isLibrary &&
                     pDevice->GetRuntimeSettings().useShaderLibraryForPipelineLibraryFastLink &&
                     ((pShaderInfo->stages[ShaderStageFragment].pModuleHandle != nullptr) ||
                      (pShaderInfo->stages[ShaderStageFragment].codeHash.lower != 0) ||
                      (pShaderInfo->stages[ShaderStageFragment].codeHash.upper != 0)));
            }
            else if (libInfo.pFragmentShaderLib != nullptr)
            {
                CopyFragmentShaderState(libInfo.pFragmentShaderLib, pCreateInfo);
            }

            if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
            {
                BuildFragmentOutputInterfaceState(pDevice, pIn, extStructs, dynamicStateFlags, pCreateInfo);
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
                &pCreateInfo->pipelineInfo.task,
                &pCreateInfo->pipelineInfo.vs,
                &pCreateInfo->pipelineInfo.tcs,
                &pCreateInfo->pipelineInfo.tes,
                &pCreateInfo->pipelineInfo.gs,
                &pCreateInfo->pipelineInfo.mesh,
                &pCreateInfo->pipelineInfo.fs,
            };

            if (pIn->renderPass != VK_NULL_HANDLE)
            {
                const RenderPass* pRenderPass      = RenderPass::ObjectFromHandle(pIn->renderPass);
                pCreateInfo->pipelineInfo.fs.options.forceLateZ = pRenderPass->IsForceLateZNeeded();
            }

            uint32_t availableStageMask = 0;

            for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
            {
                if ((shaderInfos[stage]->pModuleData != nullptr) ||
                    (pShaderInfo->stages[stage].codeHash.lower != 0) ||
                    (pShaderInfo->stages[stage].codeHash.upper != 0))
                {
                    availableStageMask |= (1 << stage);
                }
            }

            if ((libInfo.flags.optimize != 0) &&
                (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInput) == false) &&
                ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) ||
                 (libInfo.pVertexInputInterfaceLib != nullptr)))
            {
                pCreateInfo->pipelineInfo.enableUberFetchShader = false;
            }

            if (libInfo.flags.isLibrary)
            {
                pCreateInfo->pipelineInfo.unlinked = true;
            }
            if (libInfo.flags.isLibrary)
            {
                auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
                pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
                auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
                pPipelineBuildInfo->pInstance = pInstance;
                CompilerSolution::DisableNggCulling(&pPipelineBuildInfo->nggState);
            }

            result = BuildPipelineResourceMapping(pDevice, pPipelineLayout, availableStageMask, pCreateInfo);
        }
    }

    if (result == VK_SUCCESS)
    {
        if (libInfo.flags.isLibrary == false)
        {
            BuildExecutablePipelineState(
                pDevice, pIn, flags, pShaderInfo, &libInfo, pPipelineLayout, dynamicStateFlags, pCreateInfo);
        }
        else if ((libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) != 0)
        {
            bool enableUberFetchShader = pCreateInfo->pipelineInfo.enableUberFetchShader;
            pCreateInfo->pipelineInfo.enableUberFetchShader = true;
            pDevice->GetCompiler(DefaultDeviceIndex)->BuildPipelineInternalBufferData(
                pDevice->GetNullPipelineLayout(), false, pCreateInfo);
            pDevice->GetCompiler(DefaultDeviceIndex)->UploadInternalBufferData(pDevice, pCreateInfo);
            pCreateInfo->pipelineInfo.enableUberFetchShader = enableUberFetchShader;
        }
    }

    return result;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure for graphics library fast link
VkResult PipelineCompiler::BuildGplFastLinkCreateInfo(
    Device*                                         pDevice,
    const VkGraphicsPipelineCreateInfo*             pIn,
    const GraphicsPipelineExtStructs&               extStructs,
    VkPipelineCreateFlags2KHR                       flags,
    const GraphicsPipelineLibraryInfo&              libInfo,
    const PipelineLayout*                           pPipelineLayout,
    PipelineMetadata*                               pBinaryMetadata,
    GraphicsPipelineBinaryCreateInfo*               pCreateInfo)
{

    VK_ASSERT(pIn != nullptr);
    VK_ASSERT(libInfo.pPreRasterizationShaderLib != nullptr);
    VK_ASSERT(libInfo.pFragmentShaderLib != nullptr);

    VkResult result = VK_SUCCESS;

    pCreateInfo->pBinaryMetadata = pBinaryMetadata;
    pCreateInfo->libFlags  = libInfo.libFlags;
    pCreateInfo->libFlags |= (libInfo.pVertexInputInterfaceLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
    pCreateInfo->libFlags |= (libInfo.pPreRasterizationShaderLib == nullptr) ?
                             0 : VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
    pCreateInfo->libFlags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
    pCreateInfo->libFlags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    VkShaderStageFlagBits activeStages = GraphicsPipelineCommon::GetActiveShaderStages(pIn, &libInfo);
    uint64_t dynamicStateFlags = GraphicsPipelineCommon::GetDynamicStateFlags(pIn->pDynamicState, &libInfo);

    pCreateInfo->flags = pIn->flags;
    ApplyPipelineOptions(pDevice,
                         pIn->flags,
                         &pCreateInfo->pipelineInfo.options
    );

    // Copy parameters
    if (result == VK_SUCCESS)
    {
        // Copy the state of pre-raster and fragment.
        CopyPreRasterizationShaderState(libInfo.pPreRasterizationShaderLib, pCreateInfo);
        CopyFragmentShaderState(libInfo.pFragmentShaderLib, pCreateInfo);

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
        {
            BuildVertexInputInterfaceState(pDevice, pIn, dynamicStateFlags, activeStages, pCreateInfo);
        }
        else if (libInfo.pVertexInputInterfaceLib != nullptr)
        {
            CopyVertexInputInterfaceState(pDevice, libInfo.pVertexInputInterfaceLib, pCreateInfo);
        }

        const bool enableRasterization =
            (~libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
            (pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable == false) ||
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable);

        if (enableRasterization)
        {
            if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
            {
                BuildFragmentOutputInterfaceState(pDevice, pIn, extStructs, dynamicStateFlags, pCreateInfo);
            }
            else if (libInfo.pFragmentOutputInterfaceLib != nullptr)
            {
                CopyFragmentOutputInterfaceState(libInfo.pFragmentOutputInterfaceLib, pCreateInfo);
            }
        }
        else
        {
            pCreateInfo->pipelineInfo.rsState.numSamples = 1;
        }
    }

    // Check whether graphics library is compatible with other stages and states
    if (result == VK_SUCCESS)
    {
        const uint32_t    numPalDevices = pDevice->NumPalDevices();
        bool shouldCompile = false;
        for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
        {
            shouldCompile |= (GetSolution(pCreateInfo->compilerType)->
                IsGplFastLinkCompatible(pDevice, deviceIdx, pCreateInfo, libInfo) == false);
        }

        if (shouldCompile)
        {
            result = VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT;
        }
    }

    if (result == VK_SUCCESS)
    {
        if (pCreateInfo->pipelineInfo.enableUberFetchShader)
        {
            // Always build internal buffer data if pipeline dump is enabled.
            const bool enableCache = pDevice->GetRuntimeSettings().enablePipelineDump;
            BuildPipelineInternalBufferData(pPipelineLayout, enableCache, pCreateInfo);
            UploadInternalBufferData(pDevice, pCreateInfo);
        }
    }

    return result;
}

// =====================================================================================================================
// Checks which compiler is used
template<class PipelineBuildInfo>
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const PipelineBuildInfo* pPipelineBuildInfo,
    uint64_t                 preRasterHash,
    uint64_t                 fragmentHash)
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
    const Device*             pDevice,
    VkPipelineCreateFlags2KHR flags,
    Vkgc::PipelineOptions*    pOptions
)
{
    // Provide necessary runtime settings and PAL device properties
    const auto& settings = m_pPhysicalDevice->GetRuntimeSettings();
    const auto& info = m_pPhysicalDevice->PalProperties();

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) ||
        (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_PIPELINE_EXECUTABLE_PROPERTIES) &&
        ((flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) != 0)))
    {
        pOptions->includeDisassembly = true;
        pOptions->includeIr = true;
    }

    switch (settings.pipelineFastCompileMode)
    {
    case PipelineFastCompileApiControlled:
        pOptions->optimizationLevel = ((flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT) != 0 ? 0 : 2);
        break;
    case PipelineFastCompileNeverOptimized:
        pOptions->optimizationLevel = 0;
        break;
    case PipelineFastCompileFastOptimized:
        pOptions->optimizationLevel = 1;
        break;
    case PipelineFastCompileFullOptimized:
        pOptions->optimizationLevel = 2;
        break;
    default:
        VK_NEVER_CALLED();
        pOptions->optimizationLevel = 2;
        break;
    }

    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_SCALAR_BLOCK_LAYOUT) ||
        pDevice->GetEnabledFeatures().scalarBlockLayout
        )
    {
        pOptions->scalarBlockLayout = true;
    }

    if (pDevice->GetEnabledFeatures().robustBufferAccess)
    {
        pOptions->robustBufferAccess = true;
    }

    pOptions->shadowDescriptorTableUsage   = settings.enableFmaskBasedMsaaRead ?
                                             Vkgc::ShadowDescriptorTableUsage::Enable :
                                             Vkgc::ShadowDescriptorTableUsage::Disable;
    pOptions->shadowDescriptorTablePtrHigh =
          static_cast<uint32_t>(info.gpuMemoryProperties.shadowDescTableVaStart >> 32);

    pOptions->pageMigrationEnabled = info.gpuMemoryProperties.flags.pageMigrationEnabled;

    pOptions->enableRelocatableShaderElf = settings.enableRelocatableShaders;
    pOptions->disableImageResourceCheck  = settings.disableImageResourceTypeCheck;
#if VKI_BUILD_GFX11
    pOptions->optimizeTessFactor         = settings.optimizeTessFactor != OptimizeTessFactorDisable;
#endif
    pOptions->forceCsThreadIdSwizzling   = settings.forceCsThreadIdSwizzling;
    pOptions->overrideThreadGroupSizeX   = settings.overrideThreadGroupSizeX;
    pOptions->overrideThreadGroupSizeY   = settings.overrideThreadGroupSizeY;
    pOptions->overrideThreadGroupSizeZ   = settings.overrideThreadGroupSizeZ;

    pOptions->threadGroupSwizzleMode =
        static_cast<Vkgc::ThreadGroupSwizzleMode>(settings.forceCsThreadGroupSwizzleMode);

    pOptions->enableImplicitInvariantExports = (settings.disableImplicitInvariantExports == false);

    pOptions->reverseThreadGroup = settings.enableAlternatingThreadGroupOrder;

    pOptions->disableTruncCoordForGather = settings.disableTruncCoordForGather;

    pOptions->disablePerCompFetch = settings.disablePerCompFetch;

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
    if (pDevice->GetEnabledFeatures().primitivesGeneratedQuery)
    {
        pOptions->enablePrimGeneratedQuery = true;
    }
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertComputePipelineInfo(
    const Device*                                   pDevice,
    const VkComputePipelineCreateInfo*              pIn,
    const ComputePipelineShaderStageInfo*           pShaderInfo,
    const PipelineOptimizerKey*                     pPipelineProfileKey,
    PipelineMetadata*                               pBinaryMetadata,
    ComputePipelineBinaryCreateInfo*                pCreateInfo,
    VkPipelineCreateFlags2KHR                       flags)
{
    VkResult result    = VK_SUCCESS;
    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    auto& settings     = m_pPhysicalDevice->GetRuntimeSettings();

    AppProfile  appProfile = m_pPhysicalDevice->GetAppProfile();

    if (result == VK_SUCCESS)
    {
        PipelineLayout* pLayout = nullptr;

        if (pIn->layout != VK_NULL_HANDLE)
        {
            pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
        }

        pCreateInfo->pBinaryMetadata     = pBinaryMetadata;
        pCreateInfo->pPipelineProfileKey = pPipelineProfileKey;
        pCreateInfo->flags               = flags;

        ApplyPipelineOptions(pDevice,
                             flags,
                             &pCreateInfo->pipelineInfo.options
        );

        pCreateInfo->pipelineInfo.cs.pModuleData =
            ShaderModule::GetFirstValidShaderData(pShaderInfo->stage.pModuleHandle);

        pCreateInfo->pipelineInfo.cs.pSpecializationInfo = pShaderInfo->stage.pSpecializationInfo;
        pCreateInfo->pipelineInfo.cs.pEntryTarget        = pShaderInfo->stage.pEntryPoint;
        pCreateInfo->pipelineInfo.cs.entryStage          = Vkgc::ShaderStageCompute;

        if (pShaderInfo->stage.waveSize != 0)
        {
            pCreateInfo->pipelineInfo.cs.options.waveSize          = pShaderInfo->stage.waveSize;
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
                pCreateInfo->pipelineInfo.pipelineLayoutApiHash = pLayout->GetApiHash();

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
#if VKI_RAY_TRACING
                                                           false,
#endif
                                                           pCreateInfo->pTempBuffer,
                                                           &pCreateInfo->pipelineInfo.resourceMapping,
                                                           &pCreateInfo->pipelineInfo.options.resourceLayoutScheme);
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        pCreateInfo->compilerType = CheckCompilerType(&pCreateInfo->pipelineInfo, 0, 0);

        if (pShaderInfo->stage.pModuleHandle != nullptr)
        {
            pCreateInfo->pipelineInfo.cs.pModuleData =
                ShaderModule::GetShaderData(pCreateInfo->compilerType, pShaderInfo->stage.pModuleHandle);
        }

#if VKI_RAY_TRACING
        const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>
            (pCreateInfo->pipelineInfo.cs.pModuleData);

        if ((pModuleData != nullptr) &&
             pModuleData->usage.enableRayQuery)
        {
            SetRayTracingState(pDevice, &(pCreateInfo->pipelineInfo.rtState), 0);

        }
#endif

    }

    if (result == VK_SUCCESS)
    {
        ApplyDefaultShaderOptions(ShaderStage::ShaderStageCompute,
                                  pShaderInfo->stage.flags,
                                  &pCreateInfo->pipelineInfo.cs.options);

        ApplyProfileOptions(pDevice,
                            0,
                            nullptr,
                            &pCreateInfo->pipelineInfo.cs,
                            pCreateInfo->pPipelineProfileKey,
                            nullptr);

        // If DeprecateWave64Cs is set, driver might not report wave32-only support, but we want to force wavesize
        // to wave32 internally depending on settings.
        // We override any wavesize forced via shader opts also here.
        // NOTE: If the app uses subgroup size then wavesize forced here might get overriden later based on
        // subgroupsize. To avoid this beahvior, DeprecateWave64Reporting must also be set in the bitmask.
        pCreateInfo->pipelineInfo.cs.options.waveSize =
            ShouldForceWave32(ShaderStage::ShaderStageCompute, settings.deprecateWave64) ?
                32 : pCreateInfo->pipelineInfo.cs.options.waveSize;
    }

    // Force enable automatic workgroup reconfigure.
    if (appProfile == AppProfile::DawnOfWarIII)
    {
        pCreateInfo->pipelineInfo.options.reconfigWorkgroupLayout = true;
    }

    const auto threadGroupSwizzleMode =
        pDevice->GetShaderOptimizer()->OverrideThreadGroupSwizzleMode(
            ShaderStageCompute,
            *pCreateInfo->pPipelineProfileKey);

    const bool threadIdSwizzleMode =
        pDevice->GetShaderOptimizer()->OverrideThreadIdSwizzleMode(
            ShaderStageCompute,
            *pCreateInfo->pPipelineProfileKey);

    uint32_t overrideShaderThreadGroupSizeX = 0;
    uint32_t overrideShaderThreadGroupSizeY = 0;
    uint32_t overrideShaderThreadGroupSizeZ = 0;

    pDevice->GetShaderOptimizer()->OverrideShaderThreadGroupSize(
        ShaderStageCompute,
        *pCreateInfo->pPipelineProfileKey,
        &overrideShaderThreadGroupSizeX,
        &overrideShaderThreadGroupSizeY,
        &overrideShaderThreadGroupSizeZ);

    if (threadGroupSwizzleMode != Vkgc::ThreadGroupSwizzleMode::Default)
    {
        pCreateInfo->pipelineInfo.options.threadGroupSwizzleMode = threadGroupSwizzleMode;
    }

    if ((overrideShaderThreadGroupSizeX == NotOverrideThreadGroupSizeX) &&
        (overrideShaderThreadGroupSizeY == NotOverrideThreadGroupSizeX) &&
        (overrideShaderThreadGroupSizeZ == NotOverrideShaderThreadGroupSizeZ) &&
        (settings.overrideThreadGroupSizeX == NotOverrideThreadGroupSizeX) &&
        (settings.overrideThreadGroupSizeY == NotOverrideThreadGroupSizeY) &&
        (settings.overrideThreadGroupSizeZ == NotOverrideThreadGroupSizeZ))
    {
        if (threadIdSwizzleMode)
        {
            pCreateInfo->pipelineInfo.options.forceCsThreadIdSwizzling = threadIdSwizzleMode;
        }
    }
    else
    {
        pCreateInfo->pipelineInfo.options.forceCsThreadIdSwizzling = settings.forceCsThreadIdSwizzling;
    }

    if (overrideShaderThreadGroupSizeX != NotOverrideThreadGroupSizeX)
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeX = overrideShaderThreadGroupSizeX;
    }
    else
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeX = settings.overrideThreadGroupSizeX;
    }

    if (overrideShaderThreadGroupSizeY != NotOverrideThreadGroupSizeY)
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeY = overrideShaderThreadGroupSizeY;
    }
    else
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeY = settings.overrideThreadGroupSizeY;
    }

    if (overrideShaderThreadGroupSizeZ != NotOverrideThreadGroupSizeZ)
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeZ = overrideShaderThreadGroupSizeZ;
    }
    else
    {
        pCreateInfo->pipelineInfo.options.overrideThreadGroupSizeZ = settings.overrideThreadGroupSizeZ;
    }
    return result;
}

// =====================================================================================================================
// Set any non-zero shader option defaults
void PipelineCompiler::ApplyDefaultShaderOptions(
    ShaderStage                      stage,
    VkPipelineShaderStageCreateFlags flags,
    Vkgc::PipelineShaderOptions*     pShaderOptions) const
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();

    switch (stage)
    {
    case ShaderStage::ShaderStageTask:
        pShaderOptions->waveSize = settings.taskWaveSize;
      break;
    case ShaderStage::ShaderStageVertex:
        pShaderOptions->waveSize = settings.vsWaveSize;
        break;
    case ShaderStage::ShaderStageTessControl:
        pShaderOptions->waveSize = settings.tcsWaveSize;
        break;
    case ShaderStage::ShaderStageTessEval:
        pShaderOptions->waveSize = settings.tesWaveSize;
        break;
    case ShaderStage::ShaderStageGeometry:
        pShaderOptions->waveSize = settings.gsWaveSize;
        break;
    case ShaderStage::ShaderStageMesh:
        pShaderOptions->waveSize = settings.meshWaveSize;
      break;
    case ShaderStage::ShaderStageFragment:
        pShaderOptions->waveSize = settings.fsWaveSize;
        pShaderOptions->allowReZ = settings.allowReZ;
        break;
    case ShaderStage::ShaderStageCompute:
        if (pShaderOptions->waveSize == 0)
        {
            pShaderOptions->waveSize = settings.csWaveSize;
        }
        break;
#if VKI_RAY_TRACING
    case ShaderStage::ShaderStageRayTracingRayGen:
    case ShaderStage::ShaderStageRayTracingIntersect:
    case ShaderStage::ShaderStageRayTracingAnyHit:
    case ShaderStage::ShaderStageRayTracingClosestHit:
    case ShaderStage::ShaderStageRayTracingMiss:
    case ShaderStage::ShaderStageRayTracingCallable:
        pShaderOptions->waveSize = settings.rtWaveSize;
        break;
#endif
    default:
        break;
    }

    pShaderOptions->wgpMode           = ((settings.enableWgpMode & (1 << stage)) != 0);
    pShaderOptions->waveBreakSize     = static_cast<Vkgc::WaveBreakSize>(settings.waveBreakSize);
    pShaderOptions->disableLoopUnroll = settings.disableLoopUnrolls;

    if ((((settings.deprecateWave64 & DeprecateWave64Reporting) != 0) &&
         ((settings.deprecateWave64 & DeprecateWave64WaveIntrinsics) == 0)) ||
        (((flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT) != 0) &&
         (((flags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT) != 0) ||
          (m_pPhysicalDevice->GetEnabledAPIVersion() >= VK_MAKE_API_VERSION( 0, 1, 3, 0)))))
    {
        pShaderOptions->allowVaryWaveSize = true;
    }

    pShaderOptions->forwardPropagateNoContract = (settings.disableForwardPropagateNoContract == false);
}

// =====================================================================================================================
// Free compute pipeline binary
void PipelineCompiler::FreeComputePipelineBinary(
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    const Vkgc::BinaryData&          pipelineBinary)
{
    if (pCreateInfo->freeCompilerBinary == FreeWithCompiler)
    {
        GetSolution(pCreateInfo->compilerType)->FreeComputePipelineBinary(pipelineBinary);
    }
    else if (pCreateInfo->freeCompilerBinary == FreeWithInstanceAllocator)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pipelineBinary.pCode));
    }
}

// =====================================================================================================================
// Free graphics pipeline binary
void PipelineCompiler::FreeGraphicsPipelineBinary(
    const PipelineCompilerType        compilerType,
    const FreeCompilerBinary          freeCompilerBinary,
    const Vkgc::BinaryData&           pipelineBinary)
{
    if (freeCompilerBinary == FreeWithCompiler)
    {
        GetSolution(compilerType)->FreeGraphicsPipelineBinary(pipelineBinary);
    }
    else if (freeCompilerBinary == FreeWithInstanceAllocator)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pipelineBinary.pCode));
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
    Device*                           pDevice,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    bool                              keepConvertTempMemory,
    bool                              keepInternalMem)
{
    auto pInstance = pDevice->VkInstance();

    if ((pCreateInfo->pTempBuffer != nullptr) && (keepConvertTempMemory == false))
    {
        pInstance->FreeMem(pCreateInfo->pTempBuffer);
        pCreateInfo->pTempBuffer = nullptr;
    }

    if ((pCreateInfo->pBinaryMetadata != nullptr) &&
        (pCreateInfo->pBinaryMetadata->internalBufferInfo.pData != nullptr))
    {
        pInstance->FreeMem(pCreateInfo->pBinaryMetadata->internalBufferInfo.pData);
        pCreateInfo->pBinaryMetadata->internalBufferInfo.pData = nullptr;
        pCreateInfo->pBinaryMetadata->internalBufferInfo.dataSize = 0;
    }

    if ((pCreateInfo->pInternalMem != nullptr) && (keepInternalMem == false))
    {
        pDevice->MemMgr()->FreeGpuMem(pCreateInfo->pInternalMem);
        Util::Destructor(pCreateInfo->pInternalMem);
        pInstance->FreeMem(pCreateInfo->pInternalMem);
        pCreateInfo->pInternalMem = nullptr;
    }
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Converts Vulkan ray tracing pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertRayTracingPipelineInfo(
    const Device*                                   pDevice,
    const VkRayTracingPipelineCreateInfoKHR*        pIn,
    VkPipelineCreateFlags2KHR                       flags,
    const RayTracingPipelineShaderStageInfo*        pShaderInfo,
    const PipelineOptimizerKey*                     pPipelineProfileKey,
    RayTracingPipelineBinaryCreateInfo*             pCreateInfo)
{
    VkResult result    = VK_SUCCESS;
    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    auto&    settings  = m_pPhysicalDevice->GetRuntimeSettings();

    if (result == VK_SUCCESS)
    {
        PipelineLayout* pLayout = nullptr;

        if (pIn->layout != VK_NULL_HANDLE)
        {
            pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
        }

        pCreateInfo->pPipelineProfileKey = pPipelineProfileKey;
        pCreateInfo->flags               = flags;

        bool hasLibraries  = ((pIn->pLibraryInfo != nullptr) && (pIn->pLibraryInfo->libraryCount > 0)) &&
                             settings.rtEnableCompilePipelineLibrary;
        bool isLibrary     = Util::TestAnyFlagSet(flags, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) &&
                             settings.rtEnableCompilePipelineLibrary;
        bool hasProcedural = false;

        bool isReplay      = ((pIn->groupCount > 0) && (pIn->pGroups[0].pShaderGroupCaptureReplayHandle != nullptr));

        pCreateInfo->pipelineInfo.libraryMode = isLibrary ? LibraryMode::Library : LibraryMode::Pipeline;

        if (hasLibraries)
        {
            VkShaderStageFlags libraryStageMask = 0;

            // Visit the library shader groups
            for (uint32_t libraryIdx = 0; libraryIdx < pIn->pLibraryInfo->libraryCount; ++libraryIdx)
            {
                VkPipeline             libraryHandle     = pIn->pLibraryInfo->pLibraries[libraryIdx];
                RayTracingPipeline*    pLibrary          = RayTracingPipeline::ObjectFromHandle(libraryHandle);
                const ShaderGroupInfo* pShaderGroupInfos = pLibrary->GetShaderGroupInfos();

                if (pLibrary->CheckHasTraceRay())
                {
                    libraryStageMask |= VK_SHADER_STAGE_COMPUTE_BIT;
                }

                for (uint32_t groupIdx = 0; groupIdx < pLibrary->GetShaderGroupCount(); groupIdx++)
                {
                    libraryStageMask |= pShaderGroupInfos[groupIdx].stages;

                    if (pShaderGroupInfos[groupIdx].type ==
                        VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR)
                    {
                        hasProcedural = true;
                    }
                }
            }

            pCreateInfo->pipelineInfo.pipelineLibStageMask = VkToVkgcShaderStageMask(libraryStageMask);
        }

        // Implicitly include the SKIP_AABBS pipeline flag if there are no procedural
        // shader groups. This should be common for triangle-only setups and will
        // simplify the traversal routine. Note this guarantee cannot be made for
        // pipeline libraries
        if (settings.rtAutoSkipAabbIntersections && (isLibrary == false))
        {
            for (uint32_t groupIdx = 0; groupIdx < pIn->groupCount; groupIdx++)
            {
                if (pIn->pGroups[groupIdx].type ==
                    VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR)
                {
                    hasProcedural = true;

                    break;
                }
            }

            if (hasProcedural == false)
            {
                pCreateInfo->flags |= VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR;
            }
        }

        ApplyPipelineOptions(pDevice, flags, &pCreateInfo->pipelineInfo.options
        );

        pCreateInfo->pipelineInfo.options.disableImageResourceCheck = settings.disableRayTracingImageResourceTypeCheck;

        pCreateInfo->pipelineInfo.maxRecursionDepth = pIn->maxPipelineRayRecursionDepth;
        pCreateInfo->pipelineInfo.indirectStageMask = settings.rtIndirectStageMask;
        static_assert(RaytracingNone == static_cast<unsigned>(Vkgc::LlpcRaytracingMode::None));
        static_assert(RaytracingLegacy == static_cast<unsigned>(Vkgc::LlpcRaytracingMode::Legacy));
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 69
        static_assert(RaytracingContinufy == static_cast<unsigned>(Vkgc::LlpcRaytracingMode::Gpurt2));
#else
        static_assert(RaytracingContinufy == static_cast<unsigned>(Vkgc::LlpcRaytracingMode::Continufy));
#endif
        static_assert(RaytracingContinuations == static_cast<unsigned>(Vkgc::LlpcRaytracingMode::Continuations));
        pCreateInfo->pipelineInfo.mode = static_cast<Vkgc::LlpcRaytracingMode>(settings.llpcRaytracingMode);

        static_assert(CpsFlagStackInGlobalMem == static_cast<unsigned>(Vkgc::CpsFlagStackInGlobalMem));
        pCreateInfo->pipelineInfo.cpsFlags = settings.cpsFlags;

        pCreateInfo->pipelineInfo.isReplay = isReplay;

        // pLibraryInterface must be populated (per spec) if the pipeline is a library or has libraries
        VK_ASSERT((pIn->pLibraryInterface != nullptr) || ((isLibrary || hasLibraries) == false));

        if (isLibrary || hasLibraries)
        {
            // When pipeline libraries are involved maxPayloadSize and maxAttributeSize are read from
            pCreateInfo->pipelineInfo.payloadSizeMaxInLib   = pIn->pLibraryInterface->maxPipelineRayPayloadSize;
            pCreateInfo->pipelineInfo.attributeSizeMaxInLib = pIn->pLibraryInterface->maxPipelineRayHitAttributeSize;
        }

        if (hasLibraries)
        {
            // pipeline library, or pipeline that contains pipeline library(s)
            pCreateInfo->pipelineInfo.hasPipelineLibrary = true;
        }
        else
        {
            pCreateInfo->pipelineInfo.hasPipelineLibrary = false;
        }

        size_t pipelineInfoBufferSize = pShaderInfo->stageCount * sizeof(Vkgc::PipelineShaderInfo);
        size_t tempBufferSize         = pipelineInfoBufferSize;

        size_t genericMappingBufferSize = 0;
        if (pLayout != nullptr)
        {
            genericMappingBufferSize = pLayout->GetPipelineInfo()->mappingBufferSize;

            tempBufferSize += genericMappingBufferSize + pCreateInfo->mappingBufferSize;
        }

        if (hasLibraries)
        {
            tempBufferSize += sizeof(BinaryData) * pIn->pLibraryInfo->libraryCount;
        }

        const auto& gpurtOptions = pDevice->RayTrace()->GetGpurtOptions();
        tempBufferSize += gpurtOptions.size() * sizeof(Vkgc::GpurtOption);

        // We can't have a pipeline with 0 shader stages
        VK_ASSERT(tempBufferSize > 0);

        pCreateInfo->pTempBuffer = pInstance->AllocMem(tempBufferSize,
                                                       VK_DEFAULT_MEM_ALIGN,
                                                       VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

        size_t tempBufferOffset = 0;

        if (pCreateInfo->pTempBuffer == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
            // fields of resource mapping nodes are unused for certain node types. We must initialize
            // them to zeroes.
            memset(pCreateInfo->pTempBuffer, 0, tempBufferSize);

            if ((pLayout != nullptr) && (pLayout->GetPipelineInfo()->mappingBufferSize > 0))
            {
                pCreateInfo->pipelineInfo.pipelineLayoutApiHash = pLayout->GetApiHash();

                pCreateInfo->pMappingBuffer = pCreateInfo->pTempBuffer;
                tempBufferOffset           += pCreateInfo->mappingBufferSize;

                constexpr uint32_t RayTracingStageMask = Vkgc::ShaderStageRayTracingRayGenBit |
                                                         Vkgc::ShaderStageRayTracingIntersectBit |
                                                         Vkgc::ShaderStageRayTracingAnyHitBit |
                                                         Vkgc::ShaderStageRayTracingClosestHitBit |
                                                         Vkgc::ShaderStageRayTracingMissBit |
                                                         Vkgc::ShaderStageRayTracingCallableBit;

                // Build the LLPC resource mapping description. This data contains things about how shader
                // inputs like descriptor set bindings are communicated to this pipeline in a form that
                // LLPC can understand.
                result = pLayout->BuildLlpcPipelineMapping(RayTracingStageMask,
                                                           nullptr,
                                                           false,
                                                           isReplay,
                                                           Util::VoidPtrInc(pCreateInfo->pTempBuffer, tempBufferOffset),
                                                           &pCreateInfo->pipelineInfo.resourceMapping,
                                                           &pCreateInfo->pipelineInfo.options.resourceLayoutScheme);

                tempBufferOffset += genericMappingBufferSize;
            }
        }

        if (result == VK_SUCCESS)
        {
            pCreateInfo->pipelineInfo.shaderCount = pShaderInfo->stageCount;
            pCreateInfo->pipelineInfo.pShaderGroups = pIn->pGroups;
            pCreateInfo->pipelineInfo.shaderGroupCount = pIn->groupCount;
            pCreateInfo->pipelineInfo.pShaders = static_cast<Vkgc::PipelineShaderInfo*>(
                Util::VoidPtrInc(pCreateInfo->pTempBuffer, tempBufferOffset));
            tempBufferOffset += pipelineInfoBufferSize;

            uint32_t nonRayGenCount = 0;
            bool shaderCanInline = (settings.rtCompileMode != RtCompileMode::RtCompileModeIndirect);
            size_t shaderTotalSize = 0;

            bool hasRayQuery = false;
            for (uint32_t i = 0; i < pShaderInfo->stageCount; ++i)
            {
                pCreateInfo->pipelineInfo.pShaders[i].pModuleData =
                    ShaderModule::GetFirstValidShaderData(pShaderInfo->pStages[i].pModuleHandle);
                pCreateInfo->pipelineInfo.pShaders[i].pSpecializationInfo =
                    pShaderInfo->pStages[i].pSpecializationInfo;
                pCreateInfo->pipelineInfo.pShaders[i].pEntryTarget = pShaderInfo->pStages[i].pEntryPoint;
                pCreateInfo->pipelineInfo.pShaders[i].entryStage = pShaderInfo->pStages[i].stage;
                shaderTotalSize += pShaderInfo->pStages[i].codeSize;

                const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>
                    (pCreateInfo->pipelineInfo.pShaders[i].pModuleData);
                hasRayQuery |= ((pModuleData != nullptr) && (pModuleData->usage.enableRayQuery));

                if (pShaderInfo->pStages[i].stage != ShaderStage::ShaderStageRayTracingRayGen)
                {
                    ++nonRayGenCount;
                }

                if (shaderCanInline && (settings.shaderInlineFlags != ShaderInlineFlags::InlineAll))
                {
                    switch (pShaderInfo->pStages[i].stage)
                    {
                    case ShaderStage::ShaderStageRayTracingRayGen:
                        // Raygen can always be inlined.
                        break;
                    case ShaderStage::ShaderStageRayTracingMiss:
                        shaderCanInline =
                            Util::TestAnyFlagSet(settings.shaderInlineFlags, ShaderInlineFlags::InlineMissShader);
                        break;
                    case ShaderStage::ShaderStageRayTracingClosestHit:
                        shaderCanInline =
                            Util::TestAnyFlagSet(settings.shaderInlineFlags, ShaderInlineFlags::InlineClosestHitShader);
                        break;
                    case ShaderStage::ShaderStageRayTracingAnyHit:
                        shaderCanInline =
                            Util::TestAnyFlagSet(settings.shaderInlineFlags, ShaderInlineFlags::InlineAnyHitShader);
                        break;
                    case ShaderStage::ShaderStageRayTracingIntersect:
                        shaderCanInline =
                            Util::TestAnyFlagSet(settings.shaderInlineFlags, ShaderInlineFlags::InlineIntersectionShader);
                        break;
                    case ShaderStage::ShaderStageRayTracingCallable:
                        shaderCanInline =
                            Util::TestAnyFlagSet(settings.shaderInlineFlags, ShaderInlineFlags::InlineCallableShader);
                        break;
                    default:
                        VK_NEVER_CALLED();
                        break;
                    }
                }
            }

            const uint32_t raygenCount = pShaderInfo->stageCount - nonRayGenCount;

            pCreateInfo->allowShaderInlining = (shaderCanInline &&
                (nonRayGenCount <= settings.maxUnifiedNonRayGenShaders) &&
                (raygenCount <= settings.maxUnifiedRayGenShaders) &&
                (shaderTotalSize <= settings.maxTotalSizeOfUnifiedShaders));
            // if it is a pipeline library, or a main pipeline which would link to a library,
            // force indirect path by set pCreateInfo->allowShaderInlining = false
            if (isLibrary || hasLibraries)
            {
                pCreateInfo->allowShaderInlining = false;
            }

            {
                pCreateInfo->compilerType = CheckCompilerType(&pCreateInfo->pipelineInfo, 0, 0);
            }

            for (uint32_t i = 0; i < pShaderInfo->stageCount; ++i)
            {
                ApplyDefaultShaderOptions(pShaderInfo->pStages[i].stage,
                                          pShaderInfo->pStages[i].flags,
                                          &pCreateInfo->pipelineInfo.pShaders[i].options);
            }

            if (pCreateInfo->compilerType == PipelineCompilerTypeLlpc)
            {
                // TODO: move it to llpc
                if (pCreateInfo->allowShaderInlining)
                {
                    pCreateInfo->pipelineInfo.indirectStageMask = 0;
                }

                uint32_t vgprLimit =
                    m_compilerSolutionLlpc.GetRayTracingVgprLimit(pCreateInfo->pipelineInfo.indirectStageMask != 0);

                for (uint32_t i = 0; i < pShaderInfo->stageCount; ++i)
                {
                    pCreateInfo->pipelineInfo.pShaders[i].options.vgprLimit = vgprLimit;
                }
            }

            {
                for (uint32_t i = 0; i < pShaderInfo->stageCount; ++i)
                {

                    ApplyProfileOptions(pDevice,
                                        i,
                                        &pCreateInfo->pipelineInfo.options,
                                        &pCreateInfo->pipelineInfo.pShaders[i],
                                        pPipelineProfileKey,
                                        nullptr);

                    // Don't check for DeprecateWave64 here because currently we don't do anything for RT shaders
                }

            }

            SetRayTracingState(pDevice, &pCreateInfo->pipelineInfo.rtState, pCreateInfo->flags);

            if (hasLibraries)
            {
                uint32_t libraryCount = pIn->pLibraryInfo->libraryCount;
                BinaryData* pSummaries = reinterpret_cast<BinaryData *>(
                    VoidPtrInc(pCreateInfo->pTempBuffer, tempBufferOffset));
                tempBufferOffset += sizeof(BinaryData) * libraryCount;

                pCreateInfo->pipelineInfo.libraryCount = libraryCount;
                pCreateInfo->pipelineInfo.pLibrarySummaries = pSummaries;

                for (uint32_t i = 0; i != libraryCount; ++i)
                {
                    VkPipeline          libraryHandle = pIn->pLibraryInfo->pLibraries[i];
                    RayTracingPipeline* pLibrary      = RayTracingPipeline::ObjectFromHandle(libraryHandle);

                    const BinaryData& summary = pLibrary->GetLibrarySummary(pCreateInfo->pipelineInfo.deviceIndex);

                    pSummaries[i] = summary;
                }
            }

            if (gpurtOptions.size() > 0)
            {
                Vkgc::GpurtOption* pGpurtOptions = reinterpret_cast<Vkgc::GpurtOption *>(
                    VoidPtrInc(pCreateInfo->pTempBuffer, tempBufferOffset));
                size_t gpurtOptionsSize = sizeof(Vkgc::GpurtOption) * gpurtOptions.size();
                tempBufferOffset += gpurtOptionsSize;
                pCreateInfo->pipelineInfo.pGpurtOptions = pGpurtOptions;
                pCreateInfo->pipelineInfo.gpurtOptionCount = gpurtOptions.size();
                memcpy(pGpurtOptions, gpurtOptions.Data(), gpurtOptionsSize);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Creates ray tracing pipeline binary.
VkResult PipelineCompiler::CreateRayTracingPipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    RayTracingPipelineBinaryCreateInfo* pCreateInfo,
    RayTracingPipelineBinary*           pPipelineBinary,
    Util::MetroHash::Hash*              pCacheId)
{
    VkResult               result = VK_SUCCESS;
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    bool                   shouldCompile = true;

    auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
    pPipelineBuildInfo->deviceIndex = deviceIdx;

    int64_t compileTime = 0;
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
    void* pPipelineDumpHandle = nullptr;
    void* pShaderReplaceBuf = nullptr;
    const void** pModuleDataBak = nullptr;
    ShaderModuleHandle* pShaderModuleReplaceHandle = nullptr;

    bool shaderModuleReplaced = false;

    if (settings.enablePipelineDump && (result == VK_SUCCESS))
    {
        Vkgc::PipelineDumpOptions dumpOptions = {};

        char tempBuff[Util::MaxPathStrLen];
        InitPipelineDumpOption(&dumpOptions, settings, tempBuff, pCreateInfo->compilerType);

        Vkgc::PipelineBuildInfo pipelineInfo = {};

        pipelineInfo.pRayTracingInfo = &pCreateInfo->pipelineInfo;
        uint64_t dumpHash = settings.dumpPipelineWithApiHash ? pCreateInfo->apiPsoHash : pipelineHash;
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(
            &dumpOptions, pipelineInfo, dumpHash);
    }

    uint32_t shaderCount = pCreateInfo->pipelineInfo.shaderCount;

    if ((settings.shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
        (settings.shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
    {
        if (ReplaceRayTracingPipelineBinary(pCreateInfo, pPipelineBinary, pipelineHash))
        {
            shouldCompile = false;
            pCreateInfo->freeCompilerBinary = FreeWithInstanceAllocator;
        }
    }
    else if (settings.shaderReplaceMode == ShaderReplaceShaderPipelineHash)
    {
        char pipelineHashString[64];
        Util::Snprintf(pipelineHashString, 64, "0x%016" PRIX64, pipelineHash);

        if (strstr(settings.shaderReplacePipelineHashes, pipelineHashString) != nullptr)
        {
            uint32_t tempBufSize = (sizeof(void*) + sizeof(ShaderModuleHandle)) * shaderCount;

            pShaderReplaceBuf = pInstance->AllocMem(
                tempBufSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            memset(pShaderReplaceBuf, 0, sizeof(tempBufSize));

            pModuleDataBak = reinterpret_cast<const void**>(pShaderReplaceBuf);
            pShaderModuleReplaceHandle = reinterpret_cast<ShaderModuleHandle*>(
                Util::VoidPtrInc(pShaderReplaceBuf, sizeof(void*) * shaderCount));

            for (uint32_t i = 0; i < shaderCount; ++i)
            {
                pModuleDataBak[i] = pCreateInfo->pipelineInfo.pShaders[i].pModuleData;
                shaderModuleReplaced |= ReplacePipelineShaderModule(
                    pDevice,
                    pCreateInfo->compilerType,
                    &pCreateInfo->pipelineInfo.pShaders[i],
                    &pShaderModuleReplaceHandle[i]);
            }

            if (shaderModuleReplaced)
            {
                pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&pCreateInfo->pipelineInfo);
            }
        }
    }

    if (shouldCompile && (result == VK_SUCCESS))
    {
        int64_t startTime = Util::GetPerfCpuTime();

        result = GetSolution(pCreateInfo->compilerType)->CreateRayTracingPipelineBinary(
            pDevice,
            deviceIdx,
            pPipelineCache,
            pCreateInfo,
            pPipelineBinary,
            pPipelineDumpHandle,
            pipelineHash,
            pCacheId,
            &compileTime);

        compileTime = Util::GetPerfCpuTime() - startTime;

        if (result == VK_SUCCESS)
        {
            {
                pCreateInfo->freeCompilerBinary = FreeWithCompiler;
            }
        }
    }

    m_pipelineCacheMatrix.totalTimeSpent += compileTime;
    m_pipelineCacheMatrix.totalBinaries++;

    DumpCacheMatrix(m_pPhysicalDevice,
        "Pipeline_runtime",
        m_pipelineCacheMatrix.totalBinaries + m_pipelineCacheMatrix.cacheHits,
        &m_pipelineCacheMatrix);

    if (settings.shaderReplaceMode == ShaderReplaceShaderISA)
    {
        uint32_t pipelineIndex = 0;

        for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
        {
            auto pBin = &pPipelineBinary->pPipelineBins[i];

            if (pBin->pCode != nullptr)
            {
                ReplacePipelineIsaCode(pDevice, pipelineHash, pipelineIndex, *pBin);

                pipelineIndex++;
            }
        }
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            // Dump ELF binaries
            for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
            {
                if (pPipelineBinary->pPipelineBins[i].pCode != nullptr)
                {
                    Vkgc::BinaryData pipelineBinary = {};
                    pipelineBinary.codeSize = pPipelineBinary->pPipelineBins[i].codeSize;
                    pipelineBinary.pCode = pPipelineBinary->pPipelineBins[i].pCode;
                    Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineBinary);
                }
            }

            // Dump metadata
            Vkgc::BinaryData pipelineMeta = {};

            pipelineMeta.pCode = pPipelineBinary->pElfCache;

            if (pipelineMeta.pCode == nullptr)
            {
                BuildRayTracingPipelineBinary(pPipelineBinary, &pipelineMeta);
            }

            pipelineMeta.codeSize = GetRayTracingPipelineMetaSize(pPipelineBinary);

            Vkgc::IPipelineDumper::DumpRayTracingPipelineMetadata(pPipelineDumpHandle, &pipelineMeta);

            Vkgc::IPipelineDumper::DumpRayTracingLibrarySummary(pPipelineDumpHandle, &pPipelineBinary->librarySummary);

            if (pPipelineBinary->pElfCache == nullptr)
            {
                pInstance->FreeMem(const_cast<void*>(pipelineMeta.pCode));
            }
        }

        char resultMsg[64];
        Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }

    if (shaderModuleReplaced)
    {
        for (uint32_t i = 0; i < pCreateInfo->pipelineInfo.shaderCount; ++i)
        {
            pCreateInfo->pipelineInfo.pShaders[i].pModuleData = pModuleDataBak[i];

            FreeShaderModule(&pShaderModuleReplaceHandle[i]);
        }
    }

    pInstance->FreeMem(pShaderReplaceBuf);

    for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
    {
        auto pBin = &pPipelineBinary->pPipelineBins[i];

        if (pBin->pCode != nullptr)
        {
            DropPipelineBinaryInst(pDevice, settings, *pBin);
        }
    }

    return result;
}

// =====================================================================================================================
// Free ray tracing pipeline binary and assiocated shader group handles.
void PipelineCompiler::FreeRayTracingPipelineBinary(
    RayTracingPipelineBinaryCreateInfo* pCreateInfo,
    RayTracingPipelineBinary*           pPipelineBinary)
{
    if (pCreateInfo->freeCompilerBinary == FreeWithCompiler)
    {
        GetSolution(pCreateInfo->compilerType)->FreeRayTracingPipelineBinary(pPipelineBinary);
    }
    else if (pCreateInfo->freeCompilerBinary == FreeWithInstanceAllocator)
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(pPipelineBinary->pElfCache);
    }
}

// =====================================================================================================================
// Free ray tracing pipeline create info object
void PipelineCompiler::FreeRayTracingPipelineCreateInfo(
    RayTracingPipelineBinaryCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pTempBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pTempBuffer);
        pCreateInfo->pTempBuffer = nullptr;
    }
}

// =====================================================================================================================

// =====================================================================================================================
// Set the Rtstate info from device and gpurt info
void PipelineCompiler::SetRayTracingState(
    const Device*  pDevice,
    Vkgc::RtState* pRtState,
    uint32_t       createFlags)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    const Pal::DeviceProperties& deviceProp = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    if (deviceProp.gfxipProperties.rayTracingIp != Pal::RayTracingIpLevel::None)
    {
        Pal::BvhInfo bvhInfo = {};
        bvhInfo.numNodes                 = GpuRt::RayTracingMaxNumNodes;
        bvhInfo.boxGrowValue             = GpuRt::RayTracingBoxGrowthNumUlpsDefault;
        bvhInfo.boxSortHeuristic         = Pal::BoxSortHeuristic::ClosestFirst;
        bvhInfo.flags.useZeroOffset      = 1;
        bvhInfo.flags.returnBarycentrics = 1;
#if VKI_BUILD_GFX11
        bvhInfo.flags.pointerFlags       = settings.rtEnableNodePointerFlags;
#endif

        // Bypass Mall cache read/write if no alloc policy is set for SRDs.
        // This global setting applies to every BVH SRD.
        if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocBvh))
        {
            bvhInfo.flags.bypassMallRead  = 1;
            bvhInfo.flags.bypassMallWrite = 1;
        }

        m_pPhysicalDevice->PalDevice()->CreateBvhSrds(1, &bvhInfo, &(pRtState->bvhResDesc.descriptorData));
        pRtState->bvhResDesc.dataSizeInDwords = Util::NumBytesToNumDwords(pDevice->GetProperties().descriptorSizes.bvh);
    }

    pRtState->nodeStrideShift = 7;
    pRtState->pipelineFlags = createFlags;

    VK_ASSERT(static_cast<unsigned>(1 << pRtState->nodeStrideShift) == GpuRt::RayTracingQBVH32NodeSize);

    RayTracingPipeline::ConvertStaticPipelineFlags(pDevice,
                                                   &pRtState->staticPipelineFlags,
                                                   &pRtState->counterMode,
                                                   pRtState->pipelineFlags
    );

    // Set the indirect function calling convention and callee saved registers per shader type from settings
    pRtState->exportConfig.indirectCallingConvention = settings.indirectCallConvention;
    pRtState->exportConfig.indirectCalleeSavedRegs.raygen = settings.indirectCalleeRaygen;
    pRtState->exportConfig.indirectCalleeSavedRegs.traceRays = settings.indirectCalleeTraceRays;
    pRtState->exportConfig.indirectCalleeSavedRegs.miss = settings.indirectCalleeMiss;
    pRtState->exportConfig.indirectCalleeSavedRegs.closestHit = settings.indirectCalleeClosestHit;
    pRtState->exportConfig.indirectCalleeSavedRegs.anyHit = settings.indirectCalleeAnyHit;
    pRtState->exportConfig.indirectCalleeSavedRegs.intersection = settings.indirectCalleeIntersection;
    pRtState->exportConfig.indirectCalleeSavedRegs.callable = settings.indirectCalleeCallable;
    pRtState->exportConfig.enableUniformNoReturn = settings.enableUniformNoReturn;

    pRtState->exportConfig.emitRaytracingShaderDataToken = settings.rtEmitRayTracingShaderDataToken ||
        m_pPhysicalDevice->Manager()->VkInstance()->PalPlatform()->IsRaytracingShaderDataTokenRequested();

    // Set ray query swizzle
    pRtState->rayQueryCsSwizzle = settings.rayQueryCsSwizzle;

    if (settings.rtFlattenThreadGroupSize == 0)
    {
        if ((settings.overrideThreadGroupSizeX != 0) ||
            (settings.overrideThreadGroupSizeY != 0) ||
            (settings.overrideThreadGroupSizeZ != 0))
        {
            pRtState->threadGroupSizeX       = settings.overrideThreadGroupSizeX;
            pRtState->threadGroupSizeY       = settings.overrideThreadGroupSizeY;
            pRtState->threadGroupSizeZ       = settings.overrideThreadGroupSizeZ;
        }
        else
        {
            pRtState->threadGroupSizeX       = settings.rtThreadGroupSizeX;
            pRtState->threadGroupSizeY       = settings.rtThreadGroupSizeY;
            pRtState->threadGroupSizeZ       = settings.rtThreadGroupSizeZ;
        }
        pRtState->dispatchDimSwizzleMode = Vkgc::DispatchDimSwizzleMode::Native;
    }
    else
    {
        pRtState->threadGroupSizeX       = settings.rtFlattenThreadGroupSize;
        pRtState->threadGroupSizeY       = 1;
        pRtState->threadGroupSizeZ       = 1;
        pRtState->dispatchDimSwizzleMode = Vkgc::DispatchDimSwizzleMode::FlattenWidthHeight;
    }

    pRtState->boxSortHeuristicMode                  = settings.boxSortingHeuristic;
    pRtState->triCompressMode                       = settings.rtTriangleCompressionMode;
    pRtState->outerTileSize                         = settings.rtOuterTileSize;
    pRtState->enableRayQueryCsSwizzle               = settings.rtEnableRayQueryCsSwizzle;
    pRtState->enableDispatchRaysInnerSwizzle        = settings.rtEnableDispatchRaysInnerSwizzle;
    pRtState->enableDispatchRaysOuterSwizzle        = settings.rtEnableDispatchRaysOuterSwizzle;
    pRtState->ldsStackSize                          = settings.ldsStackSize;
    pRtState->enableOptimalLdsStackSizeForIndirect  = settings.enableOptimalLdsStackSizeForIndirect;
    pRtState->enableOptimalLdsStackSizeForUnified   = settings.enableOptimalLdsStackSizeForUnified;
    pRtState->dispatchRaysThreadGroupSize           = settings.dispatchRaysThreadGroupSize;
    pRtState->ldsSizePerThreadGroup                 = deviceProp.gfxipProperties.shaderCore.ldsSizePerThreadGroup;
    pRtState->maxRayLength                          = settings.rtMaxRayLength;

    // Enables trace ray staticId and parentId handling (necessary for ray history dumps)
    auto rtCounterMode = pDevice->RayTrace()->TraceRayCounterMode(DefaultDeviceIndex);
    pRtState->enableRayTracingCounters = (rtCounterMode != GpuRt::TraceRayCounterMode::TraceRayCounterDisable);

#if VKI_BUILD_GFX11
    // Enable hardware traversal stack on RTIP 2.0+
    if (settings.emulatedRtIpLevel > EmulatedRtIpLevel1_1)
    {
        pRtState->enableRayTracingHwTraversalStack = 1;
    }

    if (deviceProp.gfxipProperties.rayTracingIp >= Pal::RayTracingIpLevel::RtIp2_0)
    {
        if (settings.emulatedRtIpLevel == HardwareRtIpLevel1_1)
        {
            pRtState->enableRayTracingHwTraversalStack = 0;
        }
        else
        {
            pRtState->enableRayTracingHwTraversalStack = 1;
        }
    }
#endif

    Pal::RayTracingIpLevel rayTracingIp =
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.rayTracingIp;

    // Optionally, override RTIP level based on software emulation setting
    switch (settings.emulatedRtIpLevel)
    {
    case EmulatedRtIpLevelNone:
        break;
    case HardwareRtIpLevel1_1:
    case EmulatedRtIpLevel1_1:
        rayTracingIp = Pal::RayTracingIpLevel::RtIp1_1;
        break;
#if VKI_BUILD_GFX11
    case EmulatedRtIpLevel2_0:
        rayTracingIp = Pal::RayTracingIpLevel::RtIp2_0;
        break;
#endif
    default:
        VK_ASSERT(false);
        break;
    }

    // Set frontend compiler version from rayTracingIp
    switch (rayTracingIp)
    {
    case Pal::RayTracingIpLevel::RtIp1_0:
        pRtState->rtIpVersion = Vkgc::RtIpVersion({ 1, 0 });
        break;
    case Pal::RayTracingIpLevel::RtIp1_1:
        pRtState->rtIpVersion = Vkgc::RtIpVersion({ 1, 1 });
        break;
#if VKI_BUILD_GFX11
    case Pal::RayTracingIpLevel::RtIp2_0:
        pRtState->rtIpVersion = Vkgc::RtIpVersion({ 2, 0 });
        break;
#endif
    default:
        VK_NEVER_CALLED();
        break;
    }

    pRtState->gpurtFeatureFlags = GpuRtShaderLibraryFlags(pDevice);

    const GpuRt::PipelineShaderCode codePatch = GpuRt::GetShaderLibraryCode(pRtState->gpurtFeatureFlags);
    VK_ASSERT(codePatch.dxilSize > 0);

    pRtState->gpurtShaderLibrary.pCode = codePatch.pSpvCode;
    pRtState->gpurtShaderLibrary.codeSize = codePatch.spvSize;

    CompilerSolution::UpdateRayTracingFunctionNames(pDevice, rayTracingIp, pRtState);

    pRtState->rtIpOverride = settings.emulatedRtIpLevel != EmulatedRtIpLevelNone;

}

// =====================================================================================================================
// Replaces ray tracing pipeline from external metadata and ELF binary.
bool PipelineCompiler::ReplaceRayTracingPipelineBinary(
    RayTracingPipelineBinaryCreateInfo* pCreateInfo,
    RayTracingPipelineBinary*           pPipelineBinary,
    uint64_t                            hashCode64)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    char fileName[Util::MaxFileNameStrLen] = {};
    Vkgc::IPipelineDumper::GetPipelineName(&pCreateInfo->pipelineInfo, fileName, sizeof(fileName), hashCode64);

    char replaceFileName[Util::MaxPathStrLen] = {};
    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.meta", settings.shaderReplaceDir, fileName);

    RayTracingPipelineBinary* pHeader = nullptr;
    size_t headerSize = 0;
    size_t binarySize = 0;

    // Load ray-tracing pipeline metadata
    Util::Result result = Util::File::Exists(replaceFileName) ? Util::Result::Success : Util::Result::ErrorUnavailable;
    if (result == Util::Result::Success)
    {
        Util::File elfFile;
        result = elfFile.Open(replaceFileName, Util::FileAccessRead | Util::FileAccessBinary);
        if (result == Util::Result::Success)
        {
            headerSize = Util::File::GetFileSize(replaceFileName);
            void* pAllocBuf = pInstance->AllocMem(
                headerSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            elfFile.Read(pAllocBuf, headerSize, nullptr);
            pHeader = reinterpret_cast<RayTracingPipelineBinary*>(pAllocBuf);
        }
    }

    // Check the size of ray-tracing pipeline binaries
    if (result == Util::Result::Success)
    {
        uint32_t          binaryIndex = 0;
        Vkgc::BinaryData* pBinary     = reinterpret_cast<Vkgc::BinaryData *>(
            VoidPtrInc(pHeader, reinterpret_cast<size_t>(pHeader->pPipelineBins)));

        for (uint32_t i = 0; i < pHeader->pipelineBinCount && result == Util::Result::Success; ++i)
        {
            if (pBinary[i].codeSize > 0)
            {
                if (binaryIndex == 0)
                {
                    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf",
                        settings.shaderReplaceDir, fileName);
                }
                else
                {
                    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf.%u",
                        settings.shaderReplaceDir, fileName, binaryIndex);
                }
                result = Util::File::Exists(replaceFileName) ? Util::Result::Success : Util::Result::ErrorUnavailable;
                if (result == Util::Result::Success)
                {
                    // Modify binary size and final offset according to external file
                    pBinary[i].codeSize = Util::File::GetFileSize(replaceFileName);
                    pBinary[i].pCode = reinterpret_cast<const void*>(headerSize + binarySize);
                    binarySize += pBinary[i].codeSize;
                }
                binaryIndex++;
            }
        }
    }

    if (result == Util::Result::Success)
    {
        // Allocate final binary memory
        void* pBinaryBuf = pInstance->AllocMem(
            headerSize + binarySize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

        // Copy header
        memcpy(pBinaryBuf, pHeader, headerSize);

        // Copy ELF binaries
        uint32_t          binaryIndex = 0;
        Vkgc::BinaryData* pBinary     = reinterpret_cast<Vkgc::BinaryData*>(
            VoidPtrInc(pHeader, reinterpret_cast<size_t>(pHeader->pPipelineBins)));
        void*             pData       = VoidPtrInc(pBinaryBuf, headerSize);
        for (uint32_t i = 0; i < pHeader->pipelineBinCount && result == Util::Result::Success; ++i)
        {
            if (pBinary[i].codeSize > 0)
            {
                if (binaryIndex == 0)
                {
                    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf",
                        settings.shaderReplaceDir, fileName);
                }
                else
                {
                    Util::Snprintf(replaceFileName, sizeof(replaceFileName), "%s/%s_replace.elf.%u",
                        settings.shaderReplaceDir, fileName, binaryIndex);
                }

                Util::File elfFile;
                result = elfFile.Open(replaceFileName, Util::FileAccessRead | Util::FileAccessBinary);
                size_t readSize = 0;
                if (result == Util::Result::Success)
                {
                    elfFile.Read(pData, pBinary[i].codeSize, &readSize);
                    VK_ASSERT(readSize == pBinary[i].codeSize);
                    pData = VoidPtrInc(pData, pBinary[i].codeSize);
                }
                binaryIndex++;
            }
        }

        if (result == Util::Result::Success)
        {
            Vkgc::BinaryData bin;
            bin.pCode = pBinaryBuf;
            bin.codeSize = headerSize + binarySize;
            ExtractRayTracingPipelineBinary(&bin, pPipelineBinary);
        }
        else
        {
            pInstance->FreeMem(pBinaryBuf);
        }
    }

    if (pHeader != nullptr)
    {
        pInstance->FreeMem(pHeader);
    }

    return result == Util::Result::Success;
}

// =====================================================================================================================
// Extracts ray tracing pipeline from combined binary data.
// NOTE: This function will modify the content in pBinary, i.e. this function can't be called twice for the same binary.
void PipelineCompiler::ExtractRayTracingPipelineBinary(
    Vkgc::BinaryData*         pBinary,
    RayTracingPipelineBinary* pPipelineBinary)
{
    void* pBase = const_cast<void*>(pBinary->pCode);
    // Copy pipeline binary
    memcpy(pPipelineBinary, pBinary->pCode, sizeof(RayTracingPipelineBinary));

    // Replace offset with real pointer
    pPipelineBinary->shaderGroupHandle.shaderHandles = reinterpret_cast<Vkgc::RayTracingShaderIdentifier*>(
        VoidPtrInc(pBase, reinterpret_cast<size_t>(pPipelineBinary->shaderGroupHandle.shaderHandles)));
    pPipelineBinary->shaderPropSet.shaderProps = reinterpret_cast<Vkgc::RayTracingShaderProperty*>(
        VoidPtrInc(pBase, reinterpret_cast<size_t>(pPipelineBinary->shaderPropSet.shaderProps)));
    pPipelineBinary->pPipelineBins = reinterpret_cast<Vkgc::BinaryData*>(
        VoidPtrInc(pBase, reinterpret_cast<size_t>(pPipelineBinary->pPipelineBins)));
    pPipelineBinary->librarySummary.pCode =
        VoidPtrInc(pBase, reinterpret_cast<size_t>(pPipelineBinary->librarySummary.pCode));

    for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
    {
        if (pPipelineBinary->pPipelineBins[i].codeSize != 0)
        {
            pPipelineBinary->pPipelineBins[i].pCode =
                VoidPtrInc(pBase, reinterpret_cast<size_t>((pPipelineBinary->pPipelineBins[i].pCode)));
        }
    }

    // Store ELF cache base pointer
    pPipelineBinary->pElfCache = pBase;
}

// =====================================================================================================================
// Gets ray tracing pipeline metadata size
size_t PipelineCompiler::GetRayTracingPipelineMetaSize(
    const RayTracingPipelineBinary* pPipelineBinary) const
{
    return sizeof(RayTracingPipelineBinary) +
        sizeof(Vkgc::RayTracingShaderIdentifier) * pPipelineBinary->shaderGroupHandle.shaderHandleCount +
        sizeof(Vkgc::RayTracingShaderProperty) * pPipelineBinary->shaderPropSet.shaderCount +
        sizeof(Vkgc::BinaryData) * pPipelineBinary->pipelineBinCount +
        Pow2Align(pPipelineBinary->librarySummary.codeSize, 8);
}

// =====================================================================================================================
// Builds ray tracing combined binary data from RayTracingPipelineBinary struct.
bool PipelineCompiler::BuildRayTracingPipelineBinary(
    const RayTracingPipelineBinary* pPipelineBinary,
    Vkgc::BinaryData*               pResult)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    // Calculate total size
    size_t headerSize = GetRayTracingPipelineMetaSize(pPipelineBinary);
    size_t binarySize = 0;
    for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
    {
        binarySize += pPipelineBinary->pPipelineBins[i].codeSize;
    }

    // Allocate memory
    void* pAllocBuf = pInstance->AllocMem(
        binarySize + headerSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
    if (pAllocBuf == nullptr)
    {
        return false;
    }

    // Copy metadata and replace pointer with the offset in binary data.
    RayTracingPipelineBinary* pHeader = reinterpret_cast<RayTracingPipelineBinary*>(pAllocBuf);
    *pHeader = *pPipelineBinary;

    Vkgc::RayTracingShaderIdentifier* pGroup = reinterpret_cast<Vkgc::RayTracingShaderIdentifier*>(pHeader + 1);
    memcpy(pGroup, pPipelineBinary->shaderGroupHandle.shaderHandles,
        sizeof(Vkgc::RayTracingShaderIdentifier) * pPipelineBinary->shaderGroupHandle.shaderHandleCount);
    pHeader->shaderGroupHandle.shaderHandles =
        reinterpret_cast<Vkgc::RayTracingShaderIdentifier*>(VoidPtrDiff(pGroup, pAllocBuf));

    Vkgc::RayTracingShaderProperty* pProperty =
        reinterpret_cast<Vkgc::RayTracingShaderProperty*>(pGroup + pPipelineBinary->shaderGroupHandle.shaderHandleCount);
    memcpy(pProperty, pPipelineBinary->shaderPropSet.shaderProps,
        sizeof(Vkgc::RayTracingShaderProperty) * pPipelineBinary->shaderPropSet.shaderCount);
    pHeader->shaderPropSet.shaderProps =
        reinterpret_cast<Vkgc::RayTracingShaderProperty*>(VoidPtrDiff(pProperty, pAllocBuf));

    void *pLibrarySummary = pProperty + pPipelineBinary->shaderPropSet.shaderCount;
    size_t librarySummaryAlignedSize = Pow2Align(pPipelineBinary->librarySummary.codeSize, 8);
    memcpy(pLibrarySummary, pPipelineBinary->librarySummary.pCode, pPipelineBinary->librarySummary.codeSize);
    memset(VoidPtrInc(pLibrarySummary, pPipelineBinary->librarySummary.codeSize),
           0,
           librarySummaryAlignedSize - pPipelineBinary->librarySummary.codeSize);
    pHeader->librarySummary.pCode = reinterpret_cast<void*>(VoidPtrDiff(pLibrarySummary, pAllocBuf));

    Vkgc::BinaryData* pBinary =
        reinterpret_cast<Vkgc::BinaryData*>(VoidPtrInc(pLibrarySummary, librarySummaryAlignedSize));
    memcpy(pBinary, pPipelineBinary->pPipelineBins, sizeof(Vkgc::BinaryData) * pPipelineBinary->pipelineBinCount);
    pHeader->pPipelineBins = reinterpret_cast<Vkgc::BinaryData*>(VoidPtrDiff(pBinary, pAllocBuf));

    // Copy pipeline ELF binaries
    void* pData = pBinary + pPipelineBinary->pipelineBinCount;
    for (uint32_t i = 0; i < pPipelineBinary->pipelineBinCount; ++i)
    {
        if (pBinary[i].codeSize != 0)
        {
            memcpy(pData, pBinary[i].pCode, pBinary[i].codeSize);
            pBinary[i].pCode = reinterpret_cast<void*>(VoidPtrDiff(pData, pAllocBuf));
            pData = VoidPtrInc(pData, pBinary[i].codeSize);
        }
        else
        {
            pBinary[i].pCode = nullptr;
        }
    }

    // Fill results
    pResult->pCode = pAllocBuf;
    pResult->codeSize = binarySize + headerSize;
    return true;
}
#endif

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
// Filter VkPipelineCreateFlags2KHR to only values used for pipeline caching
static VkPipelineCreateFlags2KHR GetCacheIdControlFlags(
    VkPipelineCreateFlags2KHR in)
{
    // The following flags should NOT affect cache computation
    static constexpr VkPipelineCreateFlags2KHR CacheIdIgnoreFlags = { 0
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
    VkPipelineCreateFlags2KHR        flags,
    const PipelineOptimizerKey*      pPipelineProfileKey,
    PipelineCompilerType             compilerType,
    uint64_t                         pipelineHash,
    const Util::MetroHash::Hash&     settingsHash,
    Util::MetroHash128*              pHash)
{
    pHash->Update(pipelineHash);
    pHash->Update(deviceIdx);
    pHash->Update(GetCacheIdControlFlags(flags));
    pHash->Update(compilerType);
    pHash->Update(settingsHash);
    pHash->Update(pPipelineProfileKey->shaderCount);

    for (uint32_t shaderIdx = 0; shaderIdx < pPipelineProfileKey->shaderCount; ++shaderIdx)
    {
        pHash->Update(pPipelineProfileKey->pShaders[shaderIdx]);
    }
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
        pCreateInfo->pPipelineProfileKey,
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
        pCreateInfo->pPipelineProfileKey,
        pCreateInfo->compilerType,
        pipelineHash,
        settingsHash,
        &hash);

    hash.Update(pCreateInfo->pipelineInfo.task.options);
    hash.Update(pCreateInfo->pipelineInfo.vs.options);
    hash.Update(pCreateInfo->pipelineInfo.tes.options);
    hash.Update(pCreateInfo->pipelineInfo.tcs.options);
    hash.Update(pCreateInfo->pipelineInfo.gs.options);
    hash.Update(pCreateInfo->pipelineInfo.mesh.options);
    hash.Update(pCreateInfo->pipelineInfo.fs.options);
    hash.Update(pCreateInfo->pipelineInfo.options);
    hash.Update(pCreateInfo->pipelineInfo.nggState);
    hash.Update(pCreateInfo->dbFormat);
    hash.Update(pCreateInfo->pipelineInfo.dynamicVertexStride);
    hash.Update(pCreateInfo->pipelineInfo.enableUberFetchShader);
    hash.Update(pCreateInfo->pipelineInfo.rsState);

    hash.Update(pCreateInfo->pBinaryMetadata->pointSizeUsed);

    hash.Finalize(pCacheId->bytes);
}

// =====================================================================================================================
void PipelineCompiler::GetColorExportShaderCacheId(
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    Util::MetroHash::Hash*            pCacheId)
{
    Util::MetroHash64 hash = {};

    // Update hash based on fragment output state

    hash.Update(pCreateInfo->pBinaryMetadata->dualSrcBlendingUsed);
    hash.Update(pCreateInfo->cbStateHash);

    // Update hash based on fragment shader output metadata
    hash.Update(static_cast<const uint8_t*>(pCreateInfo->pBinaryMetadata->pFsOutputMetaData),
                                            pCreateInfo->pBinaryMetadata->fsOutputMetaDataSize);
    hash.Finalize(pCacheId->bytes);
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void PipelineCompiler::GetRayTracingPipelineCacheId(
    uint32_t                            deviceIdx,
    uint32_t                            numDevices,
    RayTracingPipelineBinaryCreateInfo* pCreateInfo,
    uint64_t                            pipelineHash,
    const Util::MetroHash::Hash&        settingsHash,
    Util::MetroHash::Hash*              pCacheId)
{
    Util::MetroHash128 hash = {};

    GetCommonPipelineCacheId(
        deviceIdx,
        pCreateInfo->flags,
        pCreateInfo->pPipelineProfileKey,
        pCreateInfo->compilerType,
        pipelineHash,
        settingsHash,
        &hash);

    hash.Update(numDevices);
    hash.Update(pCreateInfo->pipelineInfo.options);

    hash.Finalize(pCacheId->bytes);
}
#endif

// =====================================================================================================================
void PipelineCompiler::BuildPipelineInternalBufferData(
    const PipelineLayout*             pPipelineLayout,
    bool                              needCache,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
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

    GetSolution(pCreateInfo->compilerType)->BuildPipelineInternalBufferData(
        this,
        fetchShaderConstBufRegBase,
        specConstBufVertexRegBase,
        specConstBufFragmentRegBase,
        needCache,
        pCreateInfo);
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

// =====================================================================================================================
// Parses a given ELF binary and retrieves the binary metadata chunk
void PipelineCompiler::ReadBinaryMetadata(
    const Device*           pDevice,
    const Vkgc::BinaryData& elfBinary,
    PipelineMetadata*       pMetadata)
{
    VK_ASSERT(elfBinary.pCode != nullptr);
    VK_ASSERT(elfBinary.codeSize > 0);

    bool found = false;

    // Read PipelineMetadata from ELF section
    Util::ElfReader::Reader elfReader(elfBinary.pCode);
    Util::ElfReader::SectionId sectionId = elfReader.FindSection(".pipelinemetadata");

    // If section ".pipelinemetadata" isn't found (sectionId == 0), we count on pCreateInfo->pipelineMetadata
    // being initialized to 0.
    if (sectionId > 0)
    {
        const void* pSection     = elfReader.GetSectionData(sectionId);
        size_t      pSectionSize = elfReader.GetSection(sectionId).sh_size;

        if ((pSection != nullptr) && (pSectionSize >= sizeof(PipelineMetadata)))
        {
            memcpy(pMetadata, pSection, sizeof(PipelineMetadata));

            if (pMetadata->internalBufferInfo.dataSize > 0)
            {
                if (pMetadata->internalBufferInfo.dataSize <= (pSectionSize - sizeof(PipelineMetadata)))
                {
                    pMetadata->internalBufferInfo.pData = pDevice->VkInstance()->AllocMem(
                        pMetadata->internalBufferInfo.dataSize,
                        VK_DEFAULT_MEM_ALIGN,
                        VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

                    if (pMetadata->internalBufferInfo.pData != nullptr)
                    {
                        memcpy(
                            pMetadata->internalBufferInfo.pData,
                            Util::VoidPtrInc(pSection, sizeof(PipelineMetadata)),
                            pMetadata->internalBufferInfo.dataSize);
                    }
                    else
                    {
                        // Out of memory
                        VK_NEVER_CALLED();
                        pMetadata->internalBufferInfo.dataSize = 0;
                    }
                }
                else
                {
                    // Unable to read the internal buffer info
                    VK_NEVER_CALLED();
                    pMetadata->internalBufferInfo.dataSize = 0;
                    pMetadata->internalBufferInfo.pData    = nullptr;
                }
            }
        }
        else
        {
            // Unable to read the metadata
            VK_NEVER_CALLED();
        }
    }
}

// =====================================================================================================================
// Checks if a metadata chunk is empty, in which case there is no need to pack it inside the binary
bool PipelineCompiler::IsDefaultPipelineMetadata(
    const PipelineMetadata* pPipelineMetadata)
{
    PipelineMetadata emptyMetadata = {};
    return (memcmp(pPipelineMetadata, &emptyMetadata, sizeof(PipelineMetadata)) == 0);
}

// =====================================================================================================================
// Parses a given ELF binary and injects the provided metadata chunk
VkResult PipelineCompiler::WriteBinaryMetadata(
    const Device*                     pDevice,
    PipelineCompilerType              compilerType,
    FreeCompilerBinary*               pFreeCompilerBinary,
    Vkgc::BinaryData*                 pElfBinary,
    PipelineMetadata*                 pMetadata)
{
    Pal::Result palResult = Pal::Result::Success;

    if (IsDefaultPipelineMetadata(pMetadata) == false)
    {
        void*  pSection        = pMetadata;
        size_t sectionSize     = sizeof(PipelineMetadata);
        auto   pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
        auto   pInstance       = pPhysicalDevice->Manager()->VkInstance();

        Util::Abi::PipelineAbiProcessor<PalAllocator> abiProcessor(pDevice->VkInstance()->Allocator());
        palResult = abiProcessor.LoadFromBuffer(pElfBinary->pCode, pElfBinary->codeSize);

        if (palResult == Pal::Result::Success)
        {
            if (pMetadata->internalBufferInfo.dataSize > 0)
            {
                // Pack the internal buffer info
                sectionSize += pMetadata->internalBufferInfo.dataSize;
                pSection = pInstance->AllocMem(sectionSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

                if (pSection != nullptr)
                {
                    memcpy(pSection, pMetadata, sizeof(PipelineMetadata));
                    PipelineMetadata* pDstMeta = static_cast<PipelineMetadata*>(pSection);
                    pDstMeta->internalBufferInfo.pData = nullptr;
                    for (uint32_t i = 0; i < pDstMeta->internalBufferInfo.internalBufferCount; i++)
                    {
                        for (uint32_t j = 0; j < MaxPalDevices; j++)
                        {
                            pDstMeta->internalBufferInfo.internalBufferEntries[i].bufferAddress[j] = 0;
                        }
                    }
                    memcpy(
                        Util::VoidPtrInc(pSection, sizeof(PipelineMetadata)),
                        pMetadata->internalBufferInfo.pData,
                        pMetadata->internalBufferInfo.dataSize);
                }
                else
                {
                    palResult = Pal::Result::ErrorOutOfMemory;
                }
            }
        }

        if (palResult == Pal::Result::Success)
        {
            palResult = abiProcessor.SetGenericSection(".pipelinemetadata", pSection, sectionSize);
        }

        if (palResult == Pal::Result::Success)
        {
            pPhysicalDevice->GetCompiler()->FreeGraphicsPipelineBinary(
                compilerType, *pFreeCompilerBinary, *pElfBinary);

            pElfBinary->pCode = nullptr;
            pElfBinary->codeSize = abiProcessor.GetRequiredBufferSizeBytes();

            void* pNewPipelineBinary = pInstance->AllocMem(
                pElfBinary->codeSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            if (pNewPipelineBinary == nullptr)
            {
                palResult = Pal::Result::ErrorOutOfMemory;
            }
            else
            {
                abiProcessor.SaveToBuffer(pNewPipelineBinary);
                pElfBinary->pCode    = pNewPipelineBinary;
                *pFreeCompilerBinary = FreeWithInstanceAllocator;
            }
        }

        if ((pSection != nullptr) && (sectionSize > sizeof(PipelineMetadata)))
        {
            pInstance->FreeMem(pSection);
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Gets max size of uber-fetch shader internal data.
size_t PipelineCompiler::GetMaxUberFetchShaderInternalDataSize()
{
    return sizeof(Vkgc::UberFetchShaderAttribInfo) * Vkgc::MaxVertexAttribs + sizeof(uint64_t);
}

// =====================================================================================================================
// Gets the uber-fetch shader internal data size according to vertex input info.
size_t PipelineCompiler::GetUberFetchShaderInternalDataSize(
    const VkPipelineVertexInputStateCreateInfo* pVertexInput)
{
    size_t memSize = 0;

    if ((pVertexInput != nullptr) && (pVertexInput->vertexAttributeDescriptionCount > 0))
    {
        // Calculate internal data size
        uint32_t maxLocation = 0;
        for (uint32_t i = 0; i < pVertexInput->vertexAttributeDescriptionCount; i++)
        {
            auto pAttrib = &pVertexInput->pVertexAttributeDescriptions[i];

            if (pAttrib->location >= maxLocation)
            {
                maxLocation = Formats::IsDvec3Or4(pAttrib->format) ? pAttrib->location + 1 : pAttrib->location;
            }
        }
        VK_ASSERT(maxLocation < Vkgc::MaxVertexAttribs);

        memSize =
            static_cast<uint32_t>(sizeof(Vkgc::UberFetchShaderAttribInfo) * (maxLocation + 1)) + sizeof(uint64_t);
    }

    return memSize;
}

// =====================================================================================================================
template<class VertexInputBinding>
static const VertexInputBinding* GetVertexInputBinding(
    uint32_t                  binding,
    uint32_t                  vertexBindingDescriptionCount,
    const VertexInputBinding* pVertexBindingDescriptions)
{
    const VertexInputBinding* pBinding = nullptr;
    for (uint32_t i = 0; i < vertexBindingDescriptionCount; i++)
    {
        if (pVertexBindingDescriptions[i].binding == binding)
        {
            pBinding = &pVertexBindingDescriptions[i];
            break;
        }
    }

    VK_ASSERT(pBinding != nullptr);
    return pBinding;
}

// =====================================================================================================================
template<class VertexInputDivisor>
static uint32_t GetVertexInputDivisor(
    uint32_t                  binding,
    uint32_t                  vertexDivisorDescriptionCount,
    const VertexInputDivisor* pVertexDivisorDescriptions)
{
    uint32_t divisor = 1;

    for (uint32_t i = 0; i < vertexDivisorDescriptionCount; i++)
    {
        if (pVertexDivisorDescriptions[i].binding == binding)
        {
            divisor = pVertexDivisorDescriptions[i].divisor;
            break;
        }
    }

    return divisor;
}

// =====================================================================================================================
// Implementation of build uber-fetch shdaer internal data.
template<class VertexInputBinding, class VertexInputAttribute, class VertexInputDivisor>
uint32_t PipelineCompiler::BuildUberFetchShaderInternalDataImp(
    uint32_t                    vertexBindingDescriptionCount,
    const VertexInputBinding*   pVertexBindingDescriptions,
    uint32_t                    vertexAttributeDescriptionCount,
    const VertexInputAttribute* pVertexAttributeDescriptions,
    uint32_t                    vertexDivisorDescriptionCount,
    const VertexInputDivisor*   pVertexDivisorDescriptions,
    bool                        isDynamicStride,
    bool                        isOffsetMode,
    void*                       pUberFetchShaderInternalData) const
{
    const auto& settings = m_pPhysicalDevice->GetRuntimeSettings();

    bool requirePerIntanceFetch = false;
    bool requirePerCompFetch = false;

    uint32_t maxLocation = 0;
    void* pAttribInternalBase = pUberFetchShaderInternalData;
    uint64_t locationMask = 0;
    for (uint32_t i = 0; i < vertexAttributeDescriptionCount; i++)
    {
        Vkgc::UberFetchShaderAttribInfo attribInfo = {};
        auto pAttrib = &pVertexAttributeDescriptions[i];
        bool hasDualLocation = Formats::IsDvec3Or4(pAttrib->format);
        if (pAttrib->location >= maxLocation)
        {
            maxLocation = hasDualLocation ? (pAttrib->location + 1) : pAttrib->location;
        }
        locationMask |= (1ull << pAttrib->location);
        if (hasDualLocation)
        {
            locationMask |= (1ull << (pAttrib->location + 1));
        }
    }

    pAttribInternalBase = Util::VoidPtrInc(pUberFetchShaderInternalData, sizeof(uint64_t));
    if (vertexAttributeDescriptionCount > 0)
    {
        memcpy(pUberFetchShaderInternalData, &locationMask, sizeof(uint64_t));
        memset(pAttribInternalBase, 0, (maxLocation + 1) * sizeof(Vkgc::UberFetchShaderAttribInfo));
    }

    for (uint32_t i = 0; i < vertexAttributeDescriptionCount; i++)
    {
        Vkgc::UberFetchShaderAttribInfo attribInfo = {};
        auto pAttrib  = &pVertexAttributeDescriptions[i];
        auto pBinding = GetVertexInputBinding(pAttrib->binding,
                                              vertexBindingDescriptionCount,
                                              pVertexBindingDescriptions);

        uint32_t stride = pBinding->stride;
        if (isDynamicStride)
        {
            stride = settings.forceAlignedForDynamicStride ? 0 : 1;
        }

        if (settings.forcePerComponentFetchForUnalignedVbFormat == 1)
        {
            // Force stride to 1, to handle unaligned offsets
            switch (pAttrib->format)
            {
            case VK_FORMAT_R8G8_SSCALED:
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SNORM:
            case VK_FORMAT_R8G8_USCALED:
            case VK_FORMAT_R8G8_SINT:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R16G16_SFLOAT:
            case VK_FORMAT_R16G16B16A16_USCALED:
                stride = 1;
                break;
            default:
                break;
            }
        }

        auto attribFormatInfo =
            GetUberFetchShaderFormatInfo(&m_uberFetchShaderInfoFormatMap, pAttrib->format, (stride == 0), isOffsetMode);

        void* pAttribInternalData =
            Util::VoidPtrInc(pAttribInternalBase, sizeof(Vkgc::UberFetchShaderAttribInfo) * pAttrib->location);

        if (pAttrib->location >= maxLocation)
        {
            maxLocation = Formats::IsDvec3Or4(pAttrib->format) ? (pAttrib->location + 1) : pAttrib->location;
        }

        VK_ASSERT(attribFormatInfo.bufferFormat != 0);
        attribInfo.binding = pAttrib->binding;
        attribInfo.offset = pAttrib->offset;
        attribInfo.componentMask = (1 << attribFormatInfo.componentCount) - 1;
        attribInfo.componentSize = attribFormatInfo.componentSize;
        if ((attribFormatInfo.unpackedBufferFormat == 0) ||
            (attribFormatInfo.isPacked == false))
        {
            // This format only support one kind load (either packed only or per-channel only)
            attribInfo.bufferFormat = attribFormatInfo.bufferFormat;
            attribInfo.isPacked = attribFormatInfo.isPacked;
        }
        else
        {
            if (((stride % attribFormatInfo.alignment) == 0) &&
                ((pAttrib->offset % attribFormatInfo.alignment) == 0))
            {
                attribInfo.bufferFormat = attribFormatInfo.bufferFormat;
                attribInfo.isPacked = true;
            }
            else
            {
                attribInfo.bufferFormat = attribFormatInfo.unpackedBufferFormat;
                attribInfo.isPacked = false;
            }
        }

        switch (pAttrib->format)
        {
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
            attribInfo.isBgra = true;
            break;
        default:
            break;
        }

        attribInfo.isFixed = false;
        attribInfo.isCurrent = false;
        if (pBinding->inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
        {
            attribInfo.perInstance = false;
            attribInfo.instanceDivisor = 0;
        }
        else
        {
            attribInfo.perInstance = true;
            uint32_t stepRate = GetVertexInputDivisor(
                pAttrib->binding, vertexDivisorDescriptionCount, pVertexDivisorDescriptions);

            if (stepRate == 0)
            {
                attribInfo.instanceDivisor = 0;
            }
            else
            {
                if (settings.disableInstanceDivisorOpt)
                {
                    attribInfo.instanceDivisor = stepRate;
                }
                else
                {
                    union
                    {
                        float    divisorRcpf;
                        uint32_t divisorRcpU;
                    };
                    divisorRcpf = 1.0000f / stepRate;
                    attribInfo.instanceDivisor = divisorRcpU;
                }
            }
        }

        memcpy(pAttribInternalData, &attribInfo, sizeof(attribInfo));
        if (Formats::IsDvec3Or4(pAttrib->format))
        {
            attribInfo.offset += 16;
            memcpy(Util::VoidPtrInc(pAttribInternalData, sizeof(Vkgc::UberFetchShaderAttribInfo)),
                &attribInfo,
                sizeof(attribInfo));
        }

        if (attribInfo.perInstance)
        {
            requirePerIntanceFetch = true;
        }

        if (attribInfo.isPacked == false)
        {
            requirePerCompFetch = true;
        }
    }

    bool success = true;
    if (settings.disablePerInstanceFetch)
    {
        if (requirePerIntanceFetch)
        {
            success = false;
        }
    }

    if (settings.disablePerCompFetch)
    {
        if (requirePerCompFetch)
        {
            success = false;
        }
    }

    uint32_t internalMemSize = 0;
    if (success && (vertexAttributeDescriptionCount > 0))
    {
        internalMemSize = Util::VoidPtrDiff(pAttribInternalBase, pUberFetchShaderInternalData) +
                          + static_cast<uint32_t>(sizeof(Vkgc::UberFetchShaderAttribInfo) * (maxLocation + 1));
    }

    return internalMemSize;
}

// =====================================================================================================================
// Builds uber-fetch shader internal data according to dynamic vertex input info.
uint32_t PipelineCompiler::BuildUberFetchShaderInternalData(
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions,
    void*                                        pUberFetchShaderInternalData,
    bool                                         isOffsetMode)
{

    uint32_t dataSize = BuildUberFetchShaderInternalDataImp(vertexBindingDescriptionCount,
                                                            pVertexBindingDescriptions,
                                                            vertexAttributeDescriptionCount,
                                                            pVertexAttributeDescriptions,
                                                            vertexBindingDescriptionCount,
                                                            pVertexBindingDescriptions,
                                                            false,
                                                            isOffsetMode,
                                                            pUberFetchShaderInternalData);

    return dataSize;
}

// =====================================================================================================================
// Build uber-fetch shder internal data according to pipeline vertex input info.
uint32_t PipelineCompiler::BuildUberFetchShaderInternalData(
    const VkPipelineVertexInputStateCreateInfo* pVertexInput,
    bool                                        dynamicStride,
    bool                                        isOffsetMode,
    void*                                       pUberFetchShaderInternalData) const
{
    const VkPipelineVertexInputDivisorStateCreateInfoEXT* pVertexDivisor = nullptr;
    const vk::VkStructHeader* pStructHeader =
        reinterpret_cast<const vk::VkStructHeader*>(pVertexInput->pNext);
    while (pStructHeader != nullptr)
    {
        if (pStructHeader->sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT)
        {
            pVertexDivisor = reinterpret_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pStructHeader);
            break;
        }
        else
        {
            pStructHeader = pStructHeader->pNext;
        }
    }

    return BuildUberFetchShaderInternalDataImp(pVertexInput->vertexBindingDescriptionCount,
                                               pVertexInput->pVertexBindingDescriptions,
                                               pVertexInput->vertexAttributeDescriptionCount,
                                               pVertexInput->pVertexAttributeDescriptions,
                                               pVertexDivisor != nullptr ? pVertexDivisor->vertexBindingDivisorCount : 0,
                                               pVertexDivisor != nullptr ? pVertexDivisor->pVertexBindingDivisors : nullptr,
                                               dynamicStride,
                                               isOffsetMode,
                                               pUberFetchShaderInternalData);
}

// =====================================================================================================================
void PipelineCompiler::DumpPipelineMetadata(
    void*                   pPipelineDumpHandle,
    const PipelineMetadata* pBinaryMetadata)
{
    char metaString[512];

    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n;[PipelineMetadata]\n");

#if VKI_RAY_TRACING
    Util::Snprintf(metaString,
        sizeof(metaString),
        ";rayQueryUsed                  = %u\n",
        pBinaryMetadata->rayQueryUsed);
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
#endif

    Util::Snprintf(metaString,
        sizeof(metaString),
        ";pointSizeUsed                 = %u\n"
        ";dualSrcBlendingUsed           = %u\n"
        ";shadingRateUsedInShader       = %u\n"
        ";enableEarlyCompile            = %u\n"
        ";enableUberFetchShader         = %u\n"
        ";postDepthCoverageEnable       = %u\n"
        ";psOnlyPointCoordEnable        = %u\n",
        pBinaryMetadata->pointSizeUsed,
        pBinaryMetadata->dualSrcBlendingUsed,
        pBinaryMetadata->shadingRateUsedInShader,
        pBinaryMetadata->enableEarlyCompile,
        pBinaryMetadata->enableUberFetchShader,
        pBinaryMetadata->postDepthCoverageEnable,
        pBinaryMetadata->psOnlyPointCoordEnable);
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);

    Util::Snprintf(metaString,
        sizeof(metaString),
        ";vbInfo.bindingTableSize       = %u\n"
        ";vbInfo.bindingCount           = %u\n"
        ";vbInfo.bindings: {",
        pBinaryMetadata->vbInfo.bindingTableSize,
        pBinaryMetadata->vbInfo.bindingCount);
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
    for (uint32_t i = 0; i < pBinaryMetadata->vbInfo.bindingCount; i++)
    {
        Util::Snprintf(metaString,
            sizeof(metaString),
            "{%u, %u},",
            pBinaryMetadata->vbInfo.bindings[i].slot,
            pBinaryMetadata->vbInfo.bindings[i].byteStride);
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
    }
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "}\n");

    Util::Snprintf(metaString,
        sizeof(metaString),
        ";internalBuffer (count = %u, dataSize = %u) \n",
        pBinaryMetadata->internalBufferInfo.internalBufferCount,
        pBinaryMetadata->internalBufferInfo.dataSize);
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);

    for (uint32_t i = 0; i < pBinaryMetadata->internalBufferInfo.internalBufferCount; i++)
    {
        Util::Snprintf(metaString,
            sizeof(metaString),
            ";internalBufferEntries[%u]      = {.userDataOffset = %u, .bufferOffset = %u, .bufferAddress = %llx} \n",
            i,
            pBinaryMetadata->internalBufferInfo.internalBufferEntries[i].userDataOffset,
            pBinaryMetadata->internalBufferInfo.internalBufferEntries[i].bufferOffset,
            pBinaryMetadata->internalBufferInfo.internalBufferEntries[i].bufferAddress[0]);
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
    }

    if (pBinaryMetadata->internalBufferInfo.pData != nullptr)
    {
        const uint32_t* pData = static_cast<const uint32_t*>(pBinaryMetadata->internalBufferInfo.pData);
        for (uint32_t i = 0; i < pBinaryMetadata->internalBufferInfo.dataSize / sizeof(uint32_t); i++)
        {
            if ((i % 8) == 0)
            {
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, ";");
            }
            Util::Snprintf(metaString, sizeof(metaString), "0x%08X ", pData[i]);
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
            if((i % 8) == 7)
            {
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n");
            }
        }
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n");
    }

    if (pBinaryMetadata->pFsOutputMetaData != nullptr)
    {
        Util::Snprintf(metaString,
            sizeof(metaString),
            ";fsOutputMetaData (dataSize = %u) \n",
            pBinaryMetadata->fsOutputMetaDataSize);
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
        const uint32_t* pData = static_cast<const uint32_t*>(pBinaryMetadata->pFsOutputMetaData);
        for (uint32_t i = 0; i < pBinaryMetadata->fsOutputMetaDataSize / sizeof(uint32_t); i++)
        {
            if ((i % 8) == 0)
            {
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, ";");
            }
            Util::Snprintf(metaString, sizeof(metaString), "0x%08X ", pData[i]);
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, metaString);
            if ((i % 8) == 7)
            {
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n");
            }
        }
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n");
    }
}

// =====================================================================================================================
void PipelineCompiler::DumpPipeline(
    const RuntimeSettings&         settings,
    const Vkgc::PipelineBuildInfo& pipelineInfo,
    uint64_t                       apiPsoHash,
    uint32_t                       binaryCount,
    const Vkgc::BinaryData*        pElfBinaries,
    VkResult                       result)
{
    Vkgc::PipelineDumpOptions dumpOptions = {};
    char tempBuff[Util::MaxPathStrLen];
    InitPipelineDumpOption(&dumpOptions, settings, tempBuff, PipelineCompilerTypeInvalid);

    void* pPipelineDumpHandle = nullptr;
    if (settings.dumpPipelineWithApiHash)
    {
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(
            &dumpOptions, pipelineInfo, apiPsoHash);
    }
    else
    {
        pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(
            &dumpOptions, pipelineInfo);
    }

    for (uint32_t i = 0; i < binaryCount; i++)
    {
        if (pElfBinaries[i].codeSize > 0 && pElfBinaries[i].pCode != nullptr)
        {
            Vkgc::IPipelineDumper::DumpPipelineBinary(
                pPipelineDumpHandle, m_gfxIp, &pElfBinaries[i]);
        }
    }

    char resultMsg[64];
    Util::Snprintf(resultMsg, sizeof(resultMsg), "\n;CompileResult=%s\n", VkResultName(result));
    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, resultMsg);
    Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
}

// =====================================================================================================================
// Template instantiation needed for references in other files.  Linux complains if we don't do this.

template
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const Vkgc::ComputePipelineBuildInfo* pPipelineBuildInfo,
    uint64_t                              preRrasterHash,
    uint64_t                              fragmentHash);

template
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const Vkgc::GraphicsPipelineBuildInfo* pPipelineBuildInfo,
    uint64_t                               preRrasterHash,
    uint64_t                               fragmentHash);

#if VKI_RAY_TRACING
template
PipelineCompilerType PipelineCompiler::CheckCompilerType(
    const Vkgc::RayTracingPipelineBuildInfo* pPipelineBuildInfo,
    uint64_t                                 preRrasterHash,
    uint64_t                                 fragmentHash);
#endif

}
