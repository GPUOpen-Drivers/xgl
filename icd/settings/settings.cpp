/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  settings.cpp
 * @brief Contains implementation of Vulkan Settings Loader class.
 ***********************************************************************************************************************
 */

#include "include/vk_utils.h"
#include "settings/settings.h"
#include "vkgcDefs.h"

#include "palFile.h"
#include "palHashMapImpl.h"
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

#include "../layers/include/query_dlist.h"

using namespace DevDriver::SettingsURIService;

#include <sstream>
#include <climits>
#include <cmath>

using namespace Util;

namespace vk
{

// =====================================================================================================================
// Constructor for the SettingsLoader object.
VulkanSettingsLoader::VulkanSettingsLoader(
    Pal::IDevice*   pDevice,
    Pal::IPlatform* pPlatform,
    uint32_t        deviceId)
    :
    ISettingsLoader(pPlatform, static_cast<Pal::DriverSettings*>(&m_settings), g_vulkanNumSettings),
    m_pDevice(pDevice),
    m_pPlatform(pPlatform)
{
    Util::Snprintf(m_pComponentName, sizeof(m_pComponentName), "Vulkan%d", deviceId);
    memset(&m_settings, 0, sizeof(RuntimeSettings));
}

// =====================================================================================================================
VulkanSettingsLoader::~VulkanSettingsLoader()
{
    auto* pDevDriverServer = m_pPlatform->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            pSettingsService->UnregisterComponent(m_pComponentName);
        }
    }
}

Result VulkanSettingsLoader::Init()
{
    Result ret = m_settingsInfoMap.Init();

    if (ret == Result::Success)
    {
        // Init Settings Info HashMap
        InitSettingsInfo();

        // Setup default values for the settings
        SetupDefaults();

        m_state = Pal::SettingsLoaderState::EarlyInit;
    }

    return ret;
}
// =====================================================================================================================
// Append sub path to root path to generate an absolute path.
static char* MakeAbsolutePath(
    char*       pDstPath,     ///< [in,out] destination path which is an absolute path.
    size_t      dstSize,      ///< [in]     Length of the destination path string.
    const char* pRootPath,    ///< [in]     Root path.
    const char* pSubPath)     ///< [in]     *Relative* path.
{
    VK_ASSERT((pDstPath != nullptr) && (pRootPath != nullptr) && (pSubPath != nullptr));

    // '/' works perfectly fine on Windows as file path separator character:
    std::ostringstream s;
    s << pRootPath << "/" << pSubPath;
    Strncpy(pDstPath, s.str().c_str(), dstSize);

    return pDstPath;
}

// =====================================================================================================================
// Override defaults based on system info. This *must* occurs after ReadSettings because it is used to add correct root path
void VulkanSettingsLoader::OverrideSettingsBySystemInfo()
{
    // Overrides all paths for debug files to expected values.
    // Now those directories in setting are all *relative*:
    // Relative to the path in the AMD_DEBUG_DIR environment variable, and if that env var isn't set, the location is
    // platform dependent. So we need to query the root path from device and then concatenate two strings (of the root
    // path and relative path of specific file) to final usable absolute path
    const char* pRootPath = m_pDevice->GetDebugFilePath();

    if (pRootPath != nullptr)
    {
        MakeAbsolutePath(m_settings.pipelineDumpDir, sizeof(m_settings.pipelineDumpDir),
                         pRootPath, m_settings.pipelineDumpDir);
        MakeAbsolutePath(m_settings.shaderReplaceDir, sizeof(m_settings.shaderReplaceDir),
                         pRootPath, m_settings.shaderReplaceDir);

        MakeAbsolutePath(m_settings.appProfileDumpDir, sizeof(m_settings.appProfileDumpDir),
                         pRootPath, m_settings.appProfileDumpDir);
        MakeAbsolutePath(m_settings.pipelineProfileDumpFile, sizeof(m_settings.pipelineProfileDumpFile),
                         pRootPath, m_settings.pipelineProfileDumpFile);
#if ICD_RUNTIME_APP_PROFILE
        MakeAbsolutePath(m_settings.pipelineProfileRuntimeFile, sizeof(m_settings.pipelineProfileRuntimeFile),
                         pRootPath, m_settings.pipelineProfileRuntimeFile);
#endif
        MakeAbsolutePath(m_settings.debugPrintfDumpFolder, sizeof(m_settings.debugPrintfDumpFolder),
                         pRootPath, m_settings.debugPrintfDumpFolder);
    }
}

// =====================================================================================================================
// Override defaults based on application profile.  This occurs before any CCC settings or private panel settings are
// applied.
VkResult VulkanSettingsLoader::OverrideProfiledSettings(
    const VkAllocationCallbacks* pAllocCb,
    uint32_t                     appVersion,
    AppProfile                   appProfile)
{
    VkResult result = VkResult::VK_SUCCESS;

    Pal::PalPublicSettings* pPalSettings = m_pDevice->GetPublicSettings();

    Pal::DeviceProperties* pInfo = static_cast<Pal::DeviceProperties*>(
                                        pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                                sizeof(Pal::DeviceProperties),
                                                                VK_DEFAULT_MEM_ALIGN,
                                                                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));

    if (pInfo == nullptr)
    {
        result = VkResult::VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VkResult::VK_SUCCESS)
    {
        memset(pInfo, 0, sizeof(Pal::DeviceProperties));
        m_pDevice->GetProperties(pInfo);

        // By allowing the enable/disable to be set by environment variable, any third party platform owners
        // can enable or disable the feature based on their internal feedback and not have to wait for a driver
        // update to catch issues

        const char* pPipelineCacheEnvVar = getenv(m_settings.pipelineCachingEnvironmentVariable);

        if (pPipelineCacheEnvVar != nullptr)
        {
            m_settings.usePalPipelineCaching = (atoi(pPipelineCacheEnvVar) != 0);
        }

        if (pInfo->gfxLevel <= Pal::GfxIpLevel::GfxIp9)
        {
            m_settings.useAcquireReleaseInterface = false;
        }

        // In general, DCC is very beneficial for color attachments, 2D, 3D shader storage resources that have BPP>=32.
        // If this is completely offset, maybe by increased shader read latency or partial writes of DCC blocks, it should
        // be debugged on a case by case basis.
        if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
        {
            m_settings.forceEnableDcc = (ForceDccForColorAttachments |
                                         ForceDccFor2DShaderStorage |
                                         ForceDccFor3DShaderStorage |
                                         ForceDccFor32BppShaderStorage |
                                         ForceDccFor64BppShaderStorage);
            m_settings.optImgMaskToApplyShaderReadUsageForTransferSrc |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

#if VKI_RAY_TRACING
        const char* pMaxInlinedShadersEnvVar = getenv("AMD_VK_MAX_INLINED_SHADERS");

        if (pMaxInlinedShadersEnvVar != nullptr)
        {
            m_settings.maxUnifiedNonRayGenShaders = static_cast<uint32_t>(atoi(pMaxInlinedShadersEnvVar));
        }
#endif

        if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            // Enable NGG culling by default for Navi2x.
            m_settings.nggEnableBackfaceCulling = true;
            m_settings.nggEnableSmallPrimFilter = true;

            // Enable NGG compactionless mode for Navi2x
            m_settings.nggCompactVertex = false;

        }

        {
            m_settings.disableImplicitInvariantExports = false;
        }

#if VKI_BUILD_GFX11
        if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
        {
            // Enable NGG compactionless mode for Navi3x
            m_settings.nggCompactVertex = false;

            // Hardcode wave sizes per shader stage until the ML model is trained and perf lab testing is done
            m_settings.csWaveSize = 64;
            m_settings.fsWaveSize = 64;
        }
#endif

        // Put command buffers in local for large/resizable BAR systems with > 7 GBs of local heap
        constexpr gpusize _1GB = 1024ull * 1024ull * 1024ull;

        if (pInfo->gpuMemoryProperties.barSize > (7ull * _1GB))
        {
            if ((appProfile != AppProfile::WorldWarZ)
               )
            {
                m_settings.cmdAllocatorDataHeap     = Pal::GpuHeapLocal;
                m_settings.cmdAllocatorEmbeddedHeap = Pal::GpuHeapLocal;
            }

            if ((appProfile == AppProfile::DoomEternal)  ||
                (appProfile == AppProfile::SniperElite5) ||
                (appProfile == AppProfile::CSGO)
                )
            {
                m_settings.overrideHeapChoiceToLocal = OverrideChoiceForGartUswc;
            }
        }

        // Allow device memory overallocation for <= 2GBs of VRAM including APUs.
        if (pInfo->gpuMemoryProperties.maxLocalMemSize <= (2ull * _1GB))
        {
            m_settings.memoryDeviceOverallocationAllowed = true;
        }

        if (appProfile == AppProfile::Doom)
        {
            m_settings.enableSpvPerfOptimal = true;

            m_settings.optColorTargetUsageDoesNotContainResolveLayout = true;

            // No gains were seen pre-GFX9
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp9)
            {
                m_settings.barrierFilterOptions = SkipStrayExecutionDependencies |
                    SkipImageLayoutUndefined |
                    SkipDuplicateResourceBarriers;

                m_settings.modifyResourceKeyForAppProfile = true;
                m_settings.forceImageSharingMode = ForceImageSharingMode::ForceImageSharingModeExclusive;
            }

            // Vega 20 has better performance on DOOM when DCC is disabled except for the 32 BPP surfaces
            if (pInfo->revision == Pal::AsicRevision::Vega20)
            {
                m_settings.dccBitsPerPixelThreshold = 32;
            }

            // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
            // can't do any better than returning a non-null function pointer for them.
            m_settings.lenientInstanceFuncQuery = true;
        }

        if (appProfile == AppProfile::DoomVFR)
        {
            // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
            // can't do any better than returning a non-null function pointer for them.
            m_settings.lenientInstanceFuncQuery = true;

            // This works around a crash at app startup.
            m_settings.ignoreSuboptimalSwapchainSize = true;

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;

                if (pInfo->revision == Pal::AsicRevision::Navi14)
                {
                    m_settings.barrierFilterOptions = SkipImageLayoutUndefined;
                }
            }
        }

        if (appProfile == AppProfile::WolfensteinII)
        {
            m_settings.zeroInitIlRegs = true;

            m_settings.disableSingleMipAnisoOverride = false;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                // Mall no alloc settings give a 2.91% gain
                m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
            }

            // Don't enable DCC for color attachments aside from those listed in the app_resource_optimizer
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;
            }
        }

        if (appProfile == AppProfile::WolfensteinYoungblood)
        {
            m_settings.overrideHeapGartCacheableToUswc = true;

            if (pInfo->gpuType == Pal::GpuType::Discrete)
            {
                m_settings.cmdAllocatorDataHeap     = Pal::GpuHeapLocal;
                m_settings.cmdAllocatorEmbeddedHeap = Pal::GpuHeapLocal;
            }

            // Don't enable DCC for color attachments aside from those listed in the app_resource_optimizer
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;
            }

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                if (pInfo->revision == Pal::AsicRevision::Navi21)
                {
                }
            }
#if VKI_BUILD_GFX11
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {

#if VKI_BUILD_NAVI31
                if (pInfo->revision == Pal::AsicRevision::Navi31)
                {
                    {
                        m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                        m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;
                    }
                }
#endif
            }
#endif
        }

        if ((appProfile == AppProfile::WolfensteinII) ||
            (appProfile == AppProfile::WolfensteinYoungblood))
        {
            m_settings.enableSpvPerfOptimal = true;

            m_settings.optColorTargetUsageDoesNotContainResolveLayout = true;

            // No gains were seen pre-GFX9
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp9)
            {
                m_settings.barrierFilterOptions = SkipStrayExecutionDependencies |
                    SkipImageLayoutUndefined;

                m_settings.modifyResourceKeyForAppProfile = true;
                m_settings.forceImageSharingMode = ForceImageSharingMode::ForceImageSharingModeExclusive;
            }

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.asyncComputeQueueLimit = 1;
            }

            // The Vega 20 PAL default is slower on Wolfenstein II, so always allow DCC.
            if (pInfo->revision == Pal::AsicRevision::Vega20)
            {
                m_settings.dccBitsPerPixelThreshold = 0;
            }

            // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
            // can't do any better than returning a non-null function pointer for them.
            m_settings.lenientInstanceFuncQuery = true;
        }

        if (((appProfile == AppProfile::WolfensteinII) ||
             (appProfile == AppProfile::WolfensteinYoungblood) ||
             (appProfile == AppProfile::Doom)) &&
            ((pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1) ||
             (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)))
        {
            m_settings.asyncComputeQueueMaxWavesPerCu = 20;
            m_settings.nggSubgroupSizing = NggSubgroupExplicit;
            m_settings.nggVertsPerSubgroup = 254;
            m_settings.nggPrimsPerSubgroup = 128;
        }

        if (appProfile == AppProfile::WorldWarZ)
        {
            // This application oversubscribes on 4 GB cards during ALT+TAB
            m_settings.memoryDeviceOverallocationAllowed = true;

            if (pInfo->revision != Pal::AsicRevision::Navi21)
            {
                m_settings.optimizeCmdbufMode = EnableOptimizeCmdbuf;
            }

            if (pInfo->revision == Pal::AsicRevision::Vega20)
            {
                m_settings.dccBitsPerPixelThreshold = 16;
            }

            // WWZ performs worse with DCC forced on, so just let the PAL heuristics decide what's best for now.
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = (ForceDccFor64BppShaderStorage |
                    ForceDccForNonColorAttachmentShaderStorage |
                    ForceDccForColorAttachments |
                    ForceDccFor2DShaderStorage);

            }

            // Mall no alloc setting gives a ~0.82% gain
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.csWaveSize = 64;
                m_settings.fsWaveSize = 64;

                if (pInfo->revision == Pal::AsicRevision::Navi21)
                {
                    m_settings.forceEnableDcc = (ForceDccFor64BppShaderStorage |
                                                 ForceDccFor32BppShaderStorage |
                                                 ForceDccForNonColorAttachmentShaderStorage |
                                                 ForceDccForColorAttachments |
                                                 ForceDccFor3DShaderStorage);

                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                }

                if (pInfo->revision == Pal::AsicRevision::Navi22)
                {
                    m_settings.forceEnableDcc = (ForceDccFor3DShaderStorage |
                        ForceDccForColorAttachments |
                        ForceDccForNonColorAttachmentShaderStorage |
                        ForceDccFor64BppShaderStorage);

                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                }

                if (pInfo->revision == Pal::AsicRevision::Navi23)
                {
                    m_settings.forceEnableDcc = (ForceDccFor32BppShaderStorage |
                        ForceDccForNonColorAttachmentShaderStorage |
                        ForceDccForColorAttachments |
                        ForceDccFor3DShaderStorage);
                }

                if (pInfo->revision == Pal::AsicRevision::Navi24)
                {
                    m_settings.forceEnableDcc = (ForceDccFor64BppShaderStorage |
                        ForceDccForNonColorAttachmentShaderStorage |
                        ForceDccForColorAttachments |
                        ForceDccFor3DShaderStorage);

                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                }
            }

#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                m_settings.resourceBarrierOptions &= ~ResourceBarrierOptions::SkipDstCacheInv;
            }
#endif

            m_settings.implicitExternalSynchronization = false;
        }

        if (appProfile == AppProfile::WolfensteinCyberpilot)
        {
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.barrierFilterOptions = SkipImageLayoutUndefined;

            }
        }

        if (appProfile == AppProfile::IdTechEngine)
        {
            m_settings.enableSpvPerfOptimal = true;

            // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
            // can't do any better than returning a non-null function pointer for them.
            m_settings.lenientInstanceFuncQuery = true;
        }

        if (appProfile == AppProfile::Dota2)
        {
            pPalSettings->fastDepthStencilClearMode = Pal::FastDepthStencilClearMode::Graphics;

            //Vega 20 has better performance on Dota 2 when DCC is disabled.
            if (pInfo->revision == Pal::AsicRevision::Vega20)
            {
                m_settings.dccBitsPerPixelThreshold = 128;
            }
            m_settings.disableSmallSurfColorCompressionSize = 511;

            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.useAnisoThreshold = true;
            m_settings.anisoThreshold = 1.0f;

            m_settings.disableMsaaStencilShaderRead = true;

            // Disable image type checking on Navi10 to avoid 2% loss.
            m_settings.disableImageResourceTypeCheck = true;

        }

        if (appProfile == AppProfile::CSGO)
        {
        }

        if (appProfile == AppProfile::Source2Engine)
        {
            pPalSettings->fastDepthStencilClearMode = Pal::FastDepthStencilClearMode::Graphics;

            m_settings.disableSmallSurfColorCompressionSize = 511;

            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.useAnisoThreshold = true;
            m_settings.anisoThreshold = 1.0f;

            m_settings.disableMsaaStencilShaderRead = true;
        }

        if (appProfile == AppProfile::Talos)
        {
            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.optImgMaskToApplyShaderReadUsageForTransferSrc = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            m_settings.forceDepthClampBasedOnZExport = true;
        }

        if (appProfile == AppProfile::SeriousSamFusion)
        {
            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.useAnisoThreshold = true;
            m_settings.anisoThreshold = 1.0f;
        }

        if (appProfile == AppProfile::QuakeEnhanced)
        {
            // Originally applied to QuakeRemastered - this setting applies to QuakeEnhanced now since it's an update
            // to the same game.
            m_settings.disableDisplayDcc = DisplayableDcc::DisplayableDccDisabled;

#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                m_settings.forcePwsMode = PwsMode::NoLateAcquirePoint;
            }
#endif
        }

        if (appProfile == AppProfile::SedpEngine)
        {
            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
        }

        if (appProfile == AppProfile::StrangeBrigade)
        {

            if (pInfo->gpuType == Pal::GpuType::Discrete)
            {
                m_settings.overrideHeapChoiceToLocal = OverrideChoiceForGartUswc;
                m_settings.cmdAllocatorDataHeap      = Pal::GpuHeapLocal;
                m_settings.cmdAllocatorEmbeddedHeap  = Pal::GpuHeapLocal;
            }

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                    ForceDccFor3DShaderStorage |
                    ForceDccForColorAttachments |
                    ForceDccFor64BppShaderStorage);

                m_settings.enableNgg = 0x0;
            }

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {

                m_settings.disableDisplayDcc = DisplayableDcc::DisplayableDccDisabledForMgpu;

                m_settings.overrideWgpMode = WgpMode::WgpModeWgp;
                m_settings.csWaveSize = 64;
                m_settings.fsWaveSize = 64;
                m_settings.forceEnableDcc = (ForceDccFor3DShaderStorage |
                                             ForceDccForColorAttachments |
                                             ForceDccFor32BppShaderStorage);
            }
#if VKI_BUILD_GFX11
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
#if VKI_BUILD_NAVI31
                if (pInfo->revision == Pal::AsicRevision::Navi31)
                {
                    m_settings.mallNoAllocDsPolicy = MallNoAllocDsAsSnsr;
                    m_settings.pipelineBinningMode = PipelineBinningModeEnable;
                }
#endif
            }
#endif
        }

        if (appProfile == AppProfile::ZombieArmy4)
        {

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp9)
            {
                m_settings.dccBitsPerPixelThreshold = 512;
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                    ForceDccForColorAttachments |
                    ForceDccFor64BppShaderStorage);

                m_settings.enableNgg = 0x0;
                m_settings.enableWgpMode = Vkgc::ShaderStageBit::ShaderStageComputeBit;
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.enableNgg = 0x3;
                m_settings.nggEnableFrustumCulling = true;
                m_settings.enableWgpMode = Vkgc::ShaderStageBit::ShaderStageComputeBit;
            }
        }

        if (appProfile == AppProfile::MadMax)
        {
            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.useAnisoThreshold = true;
            m_settings.anisoThreshold = 1.0f;
            m_settings.disableResetReleaseResources = true;
            m_settings.implicitExternalSynchronization = false;
        }

        if (appProfile == AppProfile::F1_2017)
        {
            // F1 2017 performs worse with DCC forced on, so just let the PAL heuristics decide what's best for now.
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;
            }
        }

        if (appProfile == AppProfile::ThronesOfBritannia)
        {
            m_settings.disableHtileBasedMsaaRead = true;
            m_settings.enableFullCopyDstOnly = true;
        }

        if (appProfile == AppProfile::DiRT4)
        {
            // DiRT 4 performs worse with DCC forced on, so just let the PAL heuristics decide what's best for now.
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;
            }

            m_settings.forceDepthClampBasedOnZExport = true;
        }

        if (appProfile == AppProfile::WarHammerII)
        {
            // WarHammer II performs worse with DCC forced on, so just let the PAL heuristics decide
            // what's best for now.
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = ForceDccDefault;
            }

            m_settings.ac01WaNotNeeded = true;
        }

        if (appProfile == AppProfile::WarHammerIII)
        {
            m_settings.ac01WaNotNeeded = true;
        }

        if (appProfile == AppProfile::RainbowSixSiege)
        {
            m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
            m_settings.useAnisoThreshold = true;
            m_settings.anisoThreshold = 1.0f;

            // Ignore suboptimal swapchain size to fix crash on task switch
            m_settings.ignoreSuboptimalSwapchainSize = true;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp9)
            {
                m_settings.imageTilingOptMode = Pal::TilingOptMode::OptForSpeed;
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                    ForceDccForColorAttachments |
                    ForceDccForNonColorAttachmentShaderStorage);
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.nggEnableBackfaceCulling = false;
                m_settings.nggEnableSmallPrimFilter = false;

                if (pInfo->revision == Pal::AsicRevision::Navi23)
                {
                    m_settings.overrideLocalHeapSizeInGBs = 8;
                    m_settings.memoryDeviceOverallocationAllowed = true;
                }

                if (pInfo->revision == Pal::AsicRevision::Navi24)
                {
                    m_settings.forceEnableDcc = (ForceDccFor3DShaderStorage |
                        ForceDccForColorAttachments |
                        ForceDccForNonColorAttachmentShaderStorage |
                        ForceDccFor32BppShaderStorage);

                    m_settings.overrideLocalHeapSizeInGBs = 8;
                    m_settings.memoryDeviceOverallocationAllowed = true;
                }
            }

        }

        if (appProfile == AppProfile::Rage2)
        {
            //PM4 optimizations give us another 1.5% perf increase
            m_settings.optimizeCmdbufMode = OptimizeCmdbufMode::EnableOptimizeCmdbuf;

            m_settings.enableAceShaderPrefetch = false;

            // Rage 2 currently has all it's images set to VK_SHARING_MODE_CONCURRENT.
            // Forcing these images to use VK_SHARING_MODE_EXCLUSIVE gives us around 5% perf increase.
            m_settings.forceImageSharingMode = ForceImageSharingMode::ForceImageSharingModeExclusiveForNonColorAttachments;

            // Disable image type checking to avoid 1% loss.
            m_settings.disableImageResourceTypeCheck = true;

            m_settings.implicitExternalSynchronization = false;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp8)
            {
                m_settings.asyncComputeQueueLimit = 0;
            }

            // Vega 20 seems to be do better on Rage 2 when dccBitsPerPixelThreshold is set to 16
            // 3-5% gain when exclusive sharing mode is enabled.
            if (pInfo->revision == Pal::AsicRevision::Vega20)
            {
                m_settings.dccBitsPerPixelThreshold = 16;
            }

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                // Rage 2 performs worse with DCC forced on, so just let the PAL heuristics decide what's best for now.
                m_settings.forceEnableDcc = ForceDccDefault;

            }

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.pipelineBinningMode = PipelineBinningModeDisable;
                m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;

                m_settings.forceEnableDcc = (ForceDccFor3DShaderStorage |
                                             ForceDccForColorAttachments |
                                             ForceDccForNonColorAttachmentShaderStorage |
                                             ForceDccFor32BppShaderStorage |
                                             ForceDccFor64BppShaderStorage);

                if (pInfo->revision != Pal::AsicRevision::Navi21)
                {
                    m_settings.forceEnableDcc |= ForceDccFor2DShaderStorage;
                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                }

                {
                    m_settings.csWaveSize = 64;
                    m_settings.fsWaveSize = 64;
                }
            }

#if VKI_BUILD_GFX11
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                m_settings.pipelineBinningMode = PipelineBinningModeDisable;
                m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;

                m_settings.forceEnableDcc = (ForceDccFor3DShaderStorage |
                                             ForceDccForColorAttachments |
                                             ForceDccForNonColorAttachmentShaderStorage |
                                             ForceDccFor32BppShaderStorage |
                                             ForceDccFor64BppShaderStorage);
            }
#endif
        }

        if (appProfile == AppProfile::RedDeadRedemption2)
        {
            m_settings.enableAcquireBeforeSignal = true;

            m_settings.limitSampleCounts = VK_SAMPLE_COUNT_1_BIT |
                VK_SAMPLE_COUNT_2_BIT |
                VK_SAMPLE_COUNT_4_BIT;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
            }

            // Force exclusive sharing mode - 2% gain
            m_settings.forceImageSharingMode = ForceImageSharingMode::ForceImageSharingModeExclusive;
            m_settings.delayFullScreenAcquireToFirstPresent = true;
            m_settings.implicitExternalSynchronization = false;

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.pipelineBinningMode  = PipelineBinningModeDisable;
                m_settings.mallNoAllocCtPolicy  = MallNoAllocCtAsSnsr;
                m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;
                m_settings.forceEnableDcc       = (ForceDccFor2DShaderStorage  |
                                                   ForceDccFor3DShaderStorage  |
                                                   ForceDccForColorAttachments |
                                                   ForceDccFor64BppShaderStorage);

#if VKI_BUILD_GFX11
                if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
                {
                    m_settings.forceEnableDcc      |= ForceDccForNonColorAttachmentShaderStorage;
                }
#endif
            }

            m_settings.ac01WaNotNeeded = true;
        }

        if (appProfile == AppProfile::GhostReconBreakpoint)
        {

            // Override the PAL default for 3D color attachments and storage images to match GFX9's, SW_R/z-slice order.
            m_settings.imageTilingPreference3dGpuWritable = Pal::ImageTilingPattern::YMajor;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                                             ForceDccFor3DShaderStorage |
                                             ForceDccForColorAttachments |
                                             ForceDccForNonColorAttachmentShaderStorage |
                                             ForceDccFor64BppShaderStorage);
            }

            m_settings.implicitExternalSynchronization = false;

            m_settings.syncOsHdrState = false;
        }

#if VKI_RAY_TRACING
        if (appProfile == AppProfile::Quake2RTX)
        {
            m_settings.memoryDeviceOverallocationAllowed = true;

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.disableDisplayDcc = DisplayableDcc::DisplayableDccDisabled;

                //////////////////////////////////////////////
                // Ray Tracing Settings
                m_settings.rtEnableBuildParallel = true;

                m_settings.rtEnableUpdateParallel = true;

                m_settings.rtEnableTriangleSplitting = true;

                m_settings.useFlipHint = false;

                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                                             ForceDccFor3DShaderStorage |
                                             ForceDccForColorAttachments |
                                             ForceDccFor64BppShaderStorage);

            }

#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                // Gives ~0.5% gain at 4k
                m_settings.enableAceShaderPrefetch = false;
            }
#endif
        }

        if (appProfile == AppProfile::ControlDX12)
        {
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.rtEnableCompilePipelineLibrary = false;
                m_settings.rtMaxRayRecursionDepth = 2;
            }

#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                // Gives ~2.22% gain at 1080p
                m_settings.enableAceShaderPrefetch = false;
            }
#endif
        }

        if (appProfile == AppProfile::RayTracingWeekends)
        {
#if VKI_BUILD_GFX11
            if ((pInfo->revision != Pal::AsicRevision::Navi31)
#if VKI_BUILD_NAVI32
                && (pInfo->revision != Pal::AsicRevision::Navi32)
#endif
                )
#endif
            {
                {
                    m_settings.rtUnifiedVgprLimit = 64;
                }
            }
        }
#endif

        if (appProfile == AppProfile::DoomEternal)
        {
            m_settings.barrierFilterOptions  = SkipStrayExecutionDependencies;

            m_settings.modifyResourceKeyForAppProfile = true;
            m_settings.forceImageSharingMode = ForceImageSharingMode::ForceImageSharingModeExclusive;

            // PM4 optimizations give us 1% gain
            m_settings.optimizeCmdbufMode = EnableOptimizeCmdbuf;

            m_settings.enableSpvPerfOptimal = true;

            // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
            // can't do any better than returning a non-null function pointer for them.
            m_settings.lenientInstanceFuncQuery = true;

            m_settings.backgroundFullscreenIgnorePresentErrors = true;

            m_settings.implicitExternalSynchronization = false;

            m_settings.alwaysReportHdrFormats = true;

#if VKI_RAY_TRACING
            m_settings.indirectCallConvention = IndirectConvention0;
#endif

            if (pInfo->gpuType == Pal::GpuType::Discrete)
            {
                m_settings.cmdAllocatorDataHeap     = Pal::GpuHeapLocal;
                m_settings.cmdAllocatorEmbeddedHeap = Pal::GpuHeapLocal;
            }

            // Coarse optimizations that apply to multiple GFXIPs go below
            if ((pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1) &&
                (Util::IsPowerOfTwo(pInfo->gpuMemoryProperties.performance.vramBusBitWidth) == false))
            {
                m_settings.resourceBarrierOptions &= ~ResourceBarrierOptions::Gfx9AvoidCpuMemoryCoher;
            }

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
#if VKI_RAY_TRACING
                // Ray Tracing Settings
                m_settings.rtEnableBuildParallel  = true;
                m_settings.rtEnableUpdateParallel = true;

                m_settings.rtBvhBuildModeFastTrace = BvhBuildModeLinear;
                m_settings.rtEnableTopDownBuild    = false;
                m_settings.plocRadius              = 4;

                // 13% Gain @ 4k - Allows overlapping builds
                m_settings.enableAceShaderPrefetch = false;
#endif
                m_settings.enableWgpMode = Vkgc::ShaderStageBit::ShaderStageComputeBit;
            }

            // Finer GFXIP and ASIC specific optimizations go below
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp9)
            {
                m_settings.dccBitsPerPixelThreshold = 1;
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_1)
            {
                //  Doom Eternal performs better when DCC is not forced on. 2% gain on 4k.
                m_settings.forceEnableDcc = ForceDccDefault;

                // Doom Eternal performs better with NGG disabled (3% gain on 4k), likely because idTech runs it's own
                // triangle culling and there are no options in the game to turn it off making NGG somewhat redundant.
                m_settings.enableNgg = false;

                m_settings.asyncComputeQueueMaxWavesPerCu = 20;

                m_settings.enableWgpMode = Vkgc::ShaderStageBit::ShaderStageComputeBit;

                m_settings.csWaveSize = 64;
            }
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.asyncComputeQueueMaxWavesPerCu = 20;
                m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;

                if (pInfo->revision != Pal::AsicRevision::Navi21)
                {
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                }

                m_settings.csWaveSize = 64;
            }
#if VKI_BUILD_GFX11
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                // Navi31 Mall and Tiling Settings
                if ((pInfo->revision == Pal::AsicRevision::Navi31)
#if VKI_BUILD_NAVI32
                    || (pInfo->revision == Pal::AsicRevision::Navi32)
#endif
                    )
                {
                    // Mall no alloc settings give a ~1% gain
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                    m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;

                    // This provides ~6% gain at 4k
                    m_settings.imageTilingPreference3dGpuWritable = Pal::ImageTilingPattern::YMajor;
                }
            }
#endif
        }

        if (appProfile == AppProfile::IdTechLauncher)
        {
            m_settings.enableOnDiskInternalPipelineCaches = false;
        }

        if (appProfile == AppProfile::SaschaWillemsExamples)
        {
            m_settings.forceDepthClampBasedOnZExport = true;
        }

        if (appProfile == AppProfile::AshesOfTheSingularity)
        {
            // Disable image type checking on Navi10 to avoid 2.5% loss in Ashes
            m_settings.disableImageResourceTypeCheck = true;
            m_settings.overrideUndefinedLayoutToTransferSrcOptimal = true;
        }

        if (appProfile == AppProfile::DetroitBecomeHuman)
        {
            // Disable image type checking on Navi10 to avoid 1.5% loss in Detroit
            m_settings.disableImageResourceTypeCheck = true;

            // This restores previous driver behavior where depth compression was disabled for VK_IMAGE_LAYOUT_GENERAL.
            // There is an image memory barrier missing to synchronize DB metadata and L2 causing hair corruption in
            // some scenes.
            m_settings.forceResolveLayoutForDepthStencilTransferUsage = true;

            if ((pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1) &&
                (Util::IsPowerOfTwo(pInfo->gpuMemoryProperties.performance.vramBusBitWidth) == false))
            {
                m_settings.resourceBarrierOptions &= ~ResourceBarrierOptions::Gfx9AvoidCpuMemoryCoher;
            }
            const char *option = "-use-register-field-format=0";
            Util::Strncpy(&m_settings.llpcOptions[0], option, strlen(option) + 1);
        }

        if (appProfile == AppProfile::WarThunder)
        {
            // A larger minImageCount can get a huge performance gain for game WarThunder.
            m_settings.forceMinImageCount = 3;

            m_settings.enableDumbTransitionSync = false;
            m_settings.forceDisableGlobalBarrierCacheSync = true;
        }

        if (appProfile == AppProfile::MetroExodus)
        {
            // A larger minImageCount can get a performance gain for game Metro Exodus.
            m_settings.forceMinImageCount = 3;

#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                // Gives ~0.9% gain at 1080p
                m_settings.enableAceShaderPrefetch = false;
            }
#endif
        }

        if (appProfile == AppProfile::X4Foundations)
        {
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.disableHtileBasedMsaaRead = true;
            }
        }

        if (appProfile == AppProfile::SHARK)
        {
            m_settings.initializeVramToZero = false;
        }

        if (appProfile == AppProfile::Valheim)
        {
            m_settings.disableDisplayDcc = DisplayableDcc::DisplayableDccDisabled;

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.csWaveSize = 32;
                m_settings.fsWaveSize = 64;

                if (pInfo->revision == Pal::AsicRevision::Navi21)
                {
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                }
                else if (pInfo->revision == Pal::AsicRevision::Navi22)
                {
                    m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                        ForceDccForColorAttachments |
                        ForceDccFor3DShaderStorage |
                        ForceDccForNonColorAttachmentShaderStorage);

                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                }
                else if (pInfo->revision == Pal::AsicRevision::Navi23)
                {
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                    m_settings.mallNoAllocDsPolicy = MallNoAllocDsAsSnsr;
                }
            }
        }

        if (appProfile == AppProfile::SniperElite5)
        {
            m_settings.alwaysReportHdrFormats = true;

            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.csWaveSize = 64;
                m_settings.fsWaveSize = 64;

                if (pInfo->revision == Pal::AsicRevision::Navi21)
                {
                    m_settings.pipelineBinningMode = PipelineBinningModeDisable;
                }
                else if (pInfo->revision == Pal::AsicRevision::Navi22)
                {
                    m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage |
                                                 ForceDccFor32BppShaderStorage);
                }
                else if (pInfo->revision == Pal::AsicRevision::Navi23)
                {
                    m_settings.pipelineBinningMode = PipelineBinningModeDisable;
                }
                else if (pInfo->revision == Pal::AsicRevision::Navi24)
                {
                    m_settings.pipelineBinningMode = PipelineBinningModeDisable;
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                    m_settings.mallNoAllocCtSsrPolicy = MallNoAllocCtSsrAsSnsr;
                    m_settings.mallNoAllocSsrPolicy = MallNoAllocSsrAsSnsr;
                }
            }
#if VKI_BUILD_GFX11
            else if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
#if VKI_BUILD_NAVI31
                if (pInfo->revision == Pal::AsicRevision::Navi31)
                {
                    // This provides ~4.2% gain at 4k
                    m_settings.imageTilingPreference3dGpuWritable = Pal::ImageTilingPattern::YMajor;
                    m_settings.mallNoAllocCtPolicy = MallNoAllocCtAsSnsr;
                }
#endif
#if VKI_BUILD_NAVI33
                if (pInfo->revision == Pal::AsicRevision::Navi33)
                {
                    {
                        m_settings.forceCsThreadIdSwizzling = true;
                    }
                }
#endif
            }
#endif
        }

        if (appProfile == AppProfile::MetalGearSolid5)
        {
            m_settings.padVertexBuffers = true;
        }

        if (appProfile == AppProfile::YamagiQuakeII)
        {
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceImageSharingMode =
                    ForceImageSharingMode::ForceImageSharingModeExclusiveForNonColorAttachments;
            }
        }

        if (appProfile == AppProfile::XPlane)
        {

            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_3)
            {
                m_settings.disableHtileBasedMsaaRead = true;
            }

            m_settings.padVertexBuffers = true;
        }

        if (appProfile == AppProfile::Battlefield1)
        {
            m_settings.forceDisableAnisoFilter = true;
        }

        if (appProfile == AppProfile::DDraceNetwork)
        {
            m_settings.ignorePreferredPresentMode = true;
        }

        if (appProfile == AppProfile::SaintsRowV)
        {
            m_settings.barrierFilterOptions = BarrierFilterOptions::FlushOnHostMask;
        }

        if ((appProfile == AppProfile::HalfLifeAlyx) ||
            (appProfile == AppProfile::Satisfactory))
        {
#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                m_settings.forcePwsMode = PwsMode::NoLateAcquirePoint;
            }
#endif
        }

        if (appProfile == AppProfile::RomeRemastered)
        {
#if VKI_BUILD_GFX11
            if (pInfo->gfxLevel == Pal::GfxIpLevel::GfxIp11_0)
            {
                m_settings.forcePwsMode = PwsMode::NoLateAcquirePoint;
            }
#endif
        }

        if (appProfile == AppProfile::Zink)
        {
            m_settings.padVertexBuffers = true;
        }

        if (appProfile == AppProfile::SpidermanRemastered)
        {
            m_settings.supportMutableDescriptors = false;
        }

        if (appProfile == AppProfile::Yuzu)
        {
            if (pInfo->gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                m_settings.forceEnableDcc = (ForceDccFor2DShaderStorage    |
                                             ForceDccFor3DShaderStorage    |
                                             ForceDccFor32BppShaderStorage |
                                             ForceDccFor64BppShaderStorage);
            }
        }

        pAllocCb->pfnFree(pAllocCb->pUserData, pInfo);
    }

    return result;
}

// =====================================================================================================================
// Writes the enumeration index of the chosen app profile to a file, whose path is determined via the VkPanel. Nothing
// will be written by default.
// TODO: Dump changes made due to app profile
void VulkanSettingsLoader::DumpAppProfileChanges(
    AppProfile         appProfile)
{
    if ((m_settings.appProfileDumpMask & AppProfileDumpFlags::AppProfileValue) != 0)
    {
        wchar_t executableName[PATH_MAX];
        wchar_t executablePath[PATH_MAX];
        utils::GetExecutableNameAndPath(executableName, executablePath);

        char fileName[512] = {};
        Util::Snprintf(&fileName[0], sizeof(fileName), "%s/vkAppProfile.txt", &m_settings.appProfileDumpDir[0]);

        Util::File dumpFile;
        if (dumpFile.Open(fileName, Util::FileAccessAppend) == Pal::Result::Success)
        {
            dumpFile.Printf("Executable: %S%S\nApp Profile Enumeration: %d\n\n",
                            &executablePath[0],
                            &executableName[0],
                            static_cast<uint32_t>(appProfile));
            dumpFile.Close();
        }
    }
}

// =====================================================================================================================
// Processes public and private panel settings for a particular PAL GPU.  Vulkan private settings and public CCC
// settings are first read and validated to produce the RuntimeSettings structure.  If PAL settings for the given GPU
// need to be updated based on the Vulkan settings, the PAL structure will also be updated.
VkResult VulkanSettingsLoader::ProcessSettings(
    const VkAllocationCallbacks* pAllocCb,
    uint32_t                     appVersion,
    AppProfile*                  pAppProfile)
{
    VkResult result = VkResult::VK_SUCCESS;

    // The following lines to load profile settings have been copied from g_settings.cpp
    static_cast<Pal::IDevice*>(m_pDevice)->ReadSetting(pForceAppProfileEnableStr,
                                                       Pal::SettingScope::Driver,
                                                       Util::ValueType::Boolean,
                                                       &m_settings.forceAppProfileEnable);
    static_cast<Pal::IDevice*>(m_pDevice)->ReadSetting(pForceAppProfileValueStr,
                                                       Pal::SettingScope::Driver,
                                                       Util::ValueType::Uint,
                                                       &m_settings.forceAppProfileValue);

    // Check for forced application profile
    if (m_settings.forceAppProfileEnable)
    {
        // Update application profile to the one from the panel
        *pAppProfile = static_cast<AppProfile>(m_settings.forceAppProfileValue);
    }

#if ICD_X86_BUILD
    if (m_settings.shaderCacheMode == ShaderCacheEnableRuntimeOnly)
    {
        m_settings.shaderCacheMode = ShaderCacheDisable;
    }
#endif

    // Override defaults based on application profile
    result = OverrideProfiledSettings(pAllocCb, appVersion, *pAppProfile);

    if (result == VkResult::VK_SUCCESS)
    {
        // Read in the public settings from the Catalyst Control Center
        ReadPublicSettings();

        // Read the rest of the settings from the registry
        ReadSettings();

        // We need to override debug file paths settings to absolute paths as per system info
        OverrideSettingsBySystemInfo();

        // Modify defaults based on application profile and panel settings
        if ((*pAppProfile == AppProfile::AngleEngine) && m_settings.deferCompileOptimizedPipeline)
        {
            m_settings.enableEarlyCompile = true;
            m_settings.pipelineLayoutMode = PipelineLayoutMode::PipelineLayoutAngle;
        }

        if (m_settings.ac01WaNotNeeded)
        {
            Pal::PalPublicSettings* pPalSettings = m_pDevice->GetPublicSettings();
            pPalSettings->ac01WaNotNeeded = true;
        }

        DumpAppProfileChanges(*pAppProfile);

        // Register with the DevDriver settings service
        DevDriverRegister();
        m_state = Pal::SettingsLoaderState::LateInit;
    }

    return result;
}

// =====================================================================================================================
// Reads the public settings set up by the Catalyst Control Center and sets the appropriate settings in the settings
// structure.
void VulkanSettingsLoader::ReadPublicSettings()
{
    // Read GPU ID (composed of PCI bus properties)
    uint32_t appGpuID = 0;
    if (m_pDevice->ReadSetting("AppGpuId",
        Pal::SettingScope::Global,
        Util::ValueType::Uint,
        &appGpuID,
        sizeof(appGpuID)))
    {
        m_settings.appGpuID = appGpuID;
    }

    // Read TFQ global key
    uint32_t texFilterQuality = TextureFilterOptimizationsEnabled;
    if (m_pDevice->ReadSetting("TFQ",
                                Pal::SettingScope::Global,
                                Util::ValueType::Uint,
                                &texFilterQuality,
                                sizeof(texFilterQuality)))
    {
        if (texFilterQuality <= TextureFilterOptimizationsAggressive)
        {
            m_settings.vulkanTexFilterQuality = static_cast<TextureFilterOptimizationSettings>(texFilterQuality);
        }
    }

    uint32_t vSyncControl = static_cast<uint32_t>(VSyncControl::VSyncControlOffOrAppSpecify);
    if (m_pDevice->ReadSetting("VSyncControl",
                                Pal::SettingScope::Global,
                                Util::ValueType::Uint,
                                &vSyncControl,
                                sizeof(uint32_t)))
    {
        m_settings.vSyncControl = static_cast<VSyncControl>(vSyncControl);
    }
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also be
// initialized here.
void VulkanSettingsLoader::ValidateSettings()
{
    // Override the default preciseAnisoMode value based on the public CCC vulkanTexFilterQuality (TFQ) setting.
    // Note: This will override any Vulkan app specific profile.
    switch (m_settings.vulkanTexFilterQuality)
    {
    case TextureFilterOptimizationsDisabled:
        // Use precise aniso and disable optimizations.  Highest image quality.
        // This is acutally redundant because TFQ should cause the GPU's PERF_MOD field to be set in such a
        // way that all texture filtering optimizations are disabled anyway.
        m_settings.preciseAnisoMode = EnablePreciseAniso;
        break;

    case TextureFilterOptimizationsAggressive:
        // Enable both aniso and trilinear filtering optimizations. Lowest image quality.
        // This will cause Vulkan to fail conformance tests.
        m_settings.preciseAnisoMode = DisablePreciseAnisoAll;
        break;

    case TextureFilterOptimizationsEnabled:
        // This is the default.  Do nothing and maintain default settings.
        break;
    }

    // Disable FMASK MSAA reads if shadow desc VA range is not supported
    Pal::DeviceProperties deviceProps;
    m_pDevice->GetProperties(&deviceProps);

    if ((deviceProps.gpuMemoryProperties.flags.shadowDescVaSupport == 0) ||
        (deviceProps.gfxipProperties.srdSizes.fmaskView == 0))
    {
        m_settings.enableFmaskBasedMsaaRead = false;
    }

    // Command buffer prefetching was found to be slower for command buffers in local memory.
    if (m_settings.cmdAllocatorDataHeap == Pal::GpuHeapLocal)
    {
        m_settings.prefetchCommands = false;
    }

#if VKI_RAY_TRACING
    if (m_settings.rtBvhBuildModeOverride != BvhBuildModeOverrideDisabled)
    {
        BvhBuildMode buildMode = BvhBuildModeLinear;

        if (m_settings.rtBvhBuildModeOverride == BvhBuildModeOverridePLOC)
        {
            buildMode = BvhBuildModePLOC;
        }

        m_settings.bvhBuildModeOverrideBLAS = buildMode;
        m_settings.bvhBuildModeOverrideTLAS = buildMode;
    }

#if VKI_RAY_TRACING
#endif

    // Clamp target occupancy to [0.0, 1.0]
    m_settings.indirectCallTargetOccupancyPerSimd =
        Util::Clamp(m_settings.indirectCallTargetOccupancyPerSimd, 0.0f ,1.0f);

    const Pal::RayTracingIpLevel rayTracingIpLevel = deviceProps.gfxipProperties.rayTracingIp;
    if (rayTracingIpLevel == Pal::RayTracingIpLevel::RtIp1_1)
    {
        if ((m_settings.boxSortingHeuristic != BoxSortingDisabled) &&
            (m_settings.boxSortingHeuristic != BoxSortingDisabledOnAcceptFirstHit))
        {
            m_settings.boxSortingHeuristic = BoxSortingClosest;
        }
    }

    // Disable ray tracing if enableRaytracingSupport is requested but no hardware or software emulation is available.
    if ((m_settings.enableRaytracingSupport != RaytracingNotSupported) &&
        (m_settings.emulatedRtIpLevel == EmulatedRtIpLevelNone) &&
        (rayTracingIpLevel == Pal::RayTracingIpLevel::None))
    {
        m_settings.enableRaytracingSupport = RaytracingNotSupported;
    }

#if VKI_BUILD_GFX11
    // RTIP 2.0+ is always expected to support hardware traversal stack
    VK_ASSERT((rayTracingIpLevel <= Pal::RayTracingIpLevel::RtIp1_1) ||
        (deviceProps.gfxipProperties.flags.supportRayTraversalStack == 1));
#endif

    // Clamp target occupancy to [0.0, 1.0]
    m_settings.indirectCallTargetOccupancyPerSimd = Util::Clamp(m_settings.indirectCallTargetOccupancyPerSimd, 0.0f, 1.0f);

    // Max number of callee saved registers for indirect functions is 255
    m_settings.indirectCalleeRaygen = Util::Min(255U, m_settings.indirectCalleeRaygen);
    m_settings.indirectCalleeMiss = Util::Min(255U, m_settings.indirectCalleeMiss);
    m_settings.indirectCalleeClosestHit = Util::Min(255U, m_settings.indirectCalleeClosestHit);
    m_settings.indirectCalleeAnyHit = Util::Min(255U, m_settings.indirectCalleeAnyHit);
    m_settings.indirectCalleeIntersection = Util::Min(255U, m_settings.indirectCalleeIntersection);
    m_settings.indirectCalleeCallable = Util::Min(255U, m_settings.indirectCalleeCallable);
    m_settings.indirectCalleeTraceRays = Util::Min(255U, m_settings.indirectCalleeTraceRays);
#endif

    // SkipDstCacheInv should not be enabled by default when acquire-release barrier interface is used, because PAL
    // implements this optimization.
    if (m_settings.useAcquireReleaseInterface)
    {
        m_settings.resourceBarrierOptions &= ~ResourceBarrierOptions::SkipDstCacheInv;
    }
}

// =====================================================================================================================
// Updates any PAL public settings based on our runtime settings if necessary.
void VulkanSettingsLoader::UpdatePalSettings()
{
    Pal::PalPublicSettings* pPalSettings = m_pDevice->GetPublicSettings();

    pPalSettings->textureOptLevel = m_settings.vulkanTexFilterQuality;

    pPalSettings->hintDisableSmallSurfColorCompressionSize = m_settings.disableSmallSurfColorCompressionSize;

    // Setting disableSkipFceOptimization to false enables an optimization in PAL that disregards the FCE in a transition
    // if one of the built in clear colors are used (white/black) and the image is TCC compatible.
    pPalSettings->disableSkipFceOptimization = false;

    // For vulkan driver, forceDepthClampBasedOnZExport should be false by default, this is required to pass
    // depth_range_unrestricted CTS tests. Set it to true for applications that have perf drops
    pPalSettings->depthClampBasedOnZExport = m_settings.forceDepthClampBasedOnZExport;
    pPalSettings->cpDmaCmdCopyMemoryMaxBytes = m_settings.cpDmaCmdCopyMemoryMaxBytes;

    // The color cache fetch size is limited to 256Bytes MAX regardless of other register settings.
    pPalSettings->limitCbFetch256B = m_settings.limitCbFetch256B;

    // Controls PWS enable mode: disabled, fully enabled or partially enabled. Only takes effect if HW supports PWS and
    // Acq-rel barriers
    if (m_settings.useAcquireReleaseInterface)
    {
        static_assert(((static_cast<uint32_t>(Pal::PwsMode::Disabled)           == PwsMode::Disabled)      &&
                       (static_cast<uint32_t>(Pal::PwsMode::Enabled)            == PwsMode::Enabled)       &&
                       (static_cast<uint32_t>(Pal::PwsMode::NoLateAcquirePoint) == PwsMode::NoLateAcquirePoint)),
                       "The PAL::PwsMode enum has changed. Vulkan settings might need to be updated.");

        pPalSettings->pwsMode = static_cast<Pal::PwsMode>(m_settings.forcePwsMode);
    }

}

// =====================================================================================================================
// The settings hashes are used during pipeline loading to verify that the pipeline data is compatible between when it
// was stored and when it was loaded.  The CCC controls some of the settings though, and the CCC doesn't set it
// identically across all GPUs in an MGPU configuration.  Since the CCC keys don't affect pipeline generation, just
// ignore those values when it comes to hash generation.
void VulkanSettingsLoader::GenerateSettingHash()
{
    // Temporarily ignore these CCC settings when computing a settings hash as described in the function header.
    uint32 appGpuID = m_settings.appGpuID;
    m_settings.appGpuID = 0;
    TextureFilterOptimizationSettings vulkanTexFilterQuality = m_settings.vulkanTexFilterQuality;
    m_settings.vulkanTexFilterQuality = TextureFilterOptimizationsDisabled;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(RuntimeSettings),
        m_settingHash.bytes);

    m_settings.appGpuID = appGpuID;
    m_settings.vulkanTexFilterQuality = vulkanTexFilterQuality;
}

// =====================================================================================================================
// Completes the initialization of the settings by overriding values from the registry and validating the final settings
// struct
void VulkanSettingsLoader::FinalizeSettings(
    const DeviceExtensions::Enabled& enabledExtensions)
{
    if (enabledExtensions.IsExtensionEnabled(DeviceExtensions::KHR_PUSH_DESCRIPTOR)
    || enabledExtensions.IsExtensionEnabled(DeviceExtensions::EXT_DESCRIPTOR_BUFFER))
    {
        m_settings.enableFmaskBasedMsaaRead = false;
    }

    m_state = Pal::SettingsLoaderState::Final;

    GenerateSettingHash();
}

};
