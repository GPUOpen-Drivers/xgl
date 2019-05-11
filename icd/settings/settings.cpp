/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @brief Loads runtime settings for Vulkan applications.
 ***********************************************************************************************************************
 */

#include "include/vk_utils.h"
#include "settings/settings.h"
#include "palFile.h"

#include <sstream>
#include <climits>
#include <cmath>

using namespace Util;

namespace vk
{

// From g_settings.cpp:
extern void SetupDefaults(RuntimeSettings* pSettings);
extern void ReadSettings(const Pal::IDevice* pPalDevice, RuntimeSettings* pSettings);

static void ReadPublicSettings(Pal::IDevice* pPalDevice, RuntimeSettings* pSettings);

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
    // https://msdn.microsoft.com/en-us/library/77859s1t.aspx
    std::ostringstream s;
    s << pRootPath << "/" << pSubPath;
    Strncpy(pDstPath, s.str().c_str(), dstSize);

    return pDstPath;
}

// =====================================================================================================================
// Override defaults based on device info. This *must* occurs after ReadSettings because it is used to add correct root path
static void OverrideSettingsByDevice(
    Pal::IDevice*      pPalDevice,
    RuntimeSettings*   pSettings)
{
    // Overrides all paths for debug files to expected values.
    // Now those directories in setting are all *relative*:
    // Relative to the path in the AMD_DEBUG_DIR environment variable, and if that env var isn't set, the location is
    // platform dependent. So we need to query the root path from device and then concatenate two strings (of the root
    // path and relative path of specific file) to final usable absolute path
    const char* pRootPath = pPalDevice->GetDebugFilePath();

    if (pRootPath != nullptr)
    {
        MakeAbsolutePath(pSettings->renderPassLogDirectory, sizeof(pSettings->renderPassLogDirectory),
                         pRootPath, pSettings->renderPassLogDirectory);
        MakeAbsolutePath(pSettings->pipelineDumpDir, sizeof(pSettings->pipelineDumpDir),
                         pRootPath, pSettings->pipelineDumpDir);
        MakeAbsolutePath(pSettings->shaderReplaceDir, sizeof(pSettings->shaderReplaceDir),
                         pRootPath, pSettings->shaderReplaceDir);

    }
}

// =====================================================================================================================
// Override defaults based on application profile.  This occurs before any CCC settings or private panel settings are
// applied.
static void OverrideProfiledSettings(
    uint32_t           appVersion,
    Pal::IDevice*      pPalDevice,
    AppProfile         appProfile,
    RuntimeSettings*   pSettings)
{
    Pal::PalPublicSettings* pPalSettings = pPalDevice->GetPublicSettings();

    Pal::DeviceProperties info;
    pPalDevice->GetProperties(&info);

    if (appProfile == AppProfile::Doom)
    {
        pSettings->enableSpvPerfOptimal = true;

        pSettings->optColorTargetUsageDoesNotContainResolveLayout = true;

        // No gains were seen pre-GFX9
        if (info.gfxLevel >= Pal::GfxIpLevel::GfxIp9)
        {
            pSettings->barrierFilterOptions = SkipStrayExecutionDependencies |
                                              SkipImageLayoutUndefined       |
                                              SkipDuplicateResourceBarriers  |
                                              ForceImageSharingModeExclusive;
        }

        // Vega 20 has better performance on DOOM when DCC is disabled except for the 32 BPP surfaces
        if (info.revision == Pal::AsicRevision::Vega20)
        {
            pSettings->dccBitsPerPixelThreshold = 32;
        }

        // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
        // can't do any better than returning a non-null function pointer for them.
        pSettings->lenientInstanceFuncQuery = true;
    }

    if (appProfile == AppProfile::DoomVFR)
    {
        // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
        // can't do any better than returning a non-null function pointer for them.
        pSettings->lenientInstanceFuncQuery = true;
    }

    if (appProfile == AppProfile::WolfensteinII)
    {
        pSettings->enableSpvPerfOptimal = true;

        pSettings->zeroInitIlRegs = true;

        pSettings->optColorTargetUsageDoesNotContainResolveLayout = true;

        // No gains were seen pre-GFX9
        if (info.gfxLevel >= Pal::GfxIpLevel::GfxIp9)
        {
            pSettings->barrierFilterOptions = SkipStrayExecutionDependencies |
                                              SkipImageLayoutUndefined       |
                                              ForceImageSharingModeExclusive;
        }

        // The Vega 20 PAL default is slower on Wolfenstein II, so always allow DCC.
        if (info.revision == Pal::AsicRevision::Vega20)
        {
            pSettings->dccBitsPerPixelThreshold = 0;
        }

        // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
        // can't do any better than returning a non-null function pointer for them.
        pSettings->lenientInstanceFuncQuery = true;
    }

    if (appProfile == AppProfile::IdTechEngine)
    {
        pSettings->enableSpvPerfOptimal = true;

        // id games are known to query instance-level functions with vkGetDeviceProcAddr illegally thus we
        // can't do any better than returning a non-null function pointer for them.
        pSettings->lenientInstanceFuncQuery = true;
    }

    if (appProfile == AppProfile::Dota2)
    {
        pPalSettings->useGraphicsFastDepthStencilClear = true;

        //Vega 20 has better performance on Dota 2 when DCC is disabled.
        if (info.revision == Pal::AsicRevision::Vega20)
        {
            pSettings->dccBitsPerPixelThreshold = 128;
        }
        pPalSettings->hintDisableSmallSurfColorCompressionSize = 511;

        pSettings->preciseAnisoMode  = DisablePreciseAnisoAll;
        pSettings->useAnisoThreshold = true;
        pSettings->anisoThreshold    = 1.0f;

        pSettings->prefetchShaders = true;
        pSettings->disableMsaaStencilShaderRead = true;

    }

    if (appProfile == AppProfile::Source2Engine)
    {
        pPalSettings->useGraphicsFastDepthStencilClear = true;
        pPalSettings->hintDisableSmallSurfColorCompressionSize = 511;

        pSettings->preciseAnisoMode  = DisablePreciseAnisoAll;
        pSettings->useAnisoThreshold = true;
        pSettings->anisoThreshold    = 1.0f;

        pSettings->prefetchShaders = true;
        pSettings->disableMsaaStencilShaderRead = true;
    }

    if (appProfile == AppProfile::Talos)
    {
        pSettings->preciseAnisoMode = DisablePreciseAnisoAll;
        pSettings->optImgMaskToApplyShaderReadUsageForTransferSrc = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (appProfile == AppProfile::SeriousSamFusion)
    {
        pSettings->preciseAnisoMode  = DisablePreciseAnisoAll;
        pSettings->useAnisoThreshold = true;
        pSettings->anisoThreshold    = 1.0f;

        pSettings->prefetchShaders = true;
    }

    if (appProfile == AppProfile::SedpEngine)
    {
        pSettings->preciseAnisoMode = DisablePreciseAnisoAll;
    }

    if (appProfile == AppProfile::MadMax)
    {
        pSettings->preciseAnisoMode  = DisablePreciseAnisoAll;
        pSettings->useAnisoThreshold = true;
        pSettings->anisoThreshold    = 1.0f;
    }

    if (appProfile == AppProfile::F1_2017)
    {
        pSettings->prefetchShaders = true;
    }

    if (appProfile == AppProfile::ThronesOfBritannia)
    {
        pSettings->disableHtileBasedMsaaRead = true;
    }

}

// =====================================================================================================================
// Writes the enumeration index of the chosen app profile to a file, whose path is determined via the VkPanel. Nothing
// will be written by default.
// TODO: Dump changes made due to app profile
static void DumpAppProfileChanges(
    AppProfile         appProfile,
    RuntimeSettings*   pSettings)
{
    if (pSettings->appProfileDumpDir[0] == '\0')
    {
        // Don't do anything if dump directory has not been set
        return;
    }

    wchar_t executableName[PATH_MAX];
    wchar_t executablePath[PATH_MAX];
    utils::GetExecutableNameAndPath(executableName, executablePath);

    char fileName[512] = {};
    Util::Snprintf(&fileName[0], sizeof(fileName), "%s/vkAppProfile.txt", &pSettings->appProfileDumpDir[0]);

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

// =====================================================================================================================
// Processes public and private panel settings for a particular PAL GPU.  Vulkan private settings and public CCC
// settings are first read and validated to produce the RuntimeSettings structure.  If PAL settings for the given GPU
// need to be updated based on the Vulkan settings, the PAL structure will also be updated.
void ProcessSettings(
    uint32_t           appVersion,
    Pal::IDevice*      pPalDevice,
    AppProfile*        pAppProfile,
    RuntimeSettings*   pSettings)
{

    // Setup default values for the settings.
    SetupDefaults(pSettings);

    const AppProfile origProfile = *pAppProfile;
    // Override defaults based on application profile
    OverrideProfiledSettings(appVersion, pPalDevice, *pAppProfile, pSettings);

    // Read in the public settings from the Catalyst Control Center
    ReadPublicSettings(pPalDevice, pSettings);

    // Read settings from the registry
    ReadSettings(pPalDevice, pSettings);

    // Override defaults based on device info.
    OverrideSettingsByDevice(pPalDevice, pSettings);

    DumpAppProfileChanges(*pAppProfile, pSettings);

    if (pSettings->forceAppProfileEnable)
    {
        // Update application profile to the one from the panel
        *pAppProfile = static_cast<AppProfile>(pSettings->forceAppProfileValue);
    }

    // If we are changing profile via panel setting (i.e. forcing a specific profile), then
    // reload all settings.  This is because certain app profiles may override the default
    // values, and this allows the panel-mandated profile to override those defaults as well.
    if (*pAppProfile != origProfile)
    {
        ProcessSettings(appVersion, pPalDevice, pAppProfile, pSettings);
    }
}

// =====================================================================================================================
// Reads the public settings set up by the Catalyst Control Center and sets the appropriate settings in the settings
// structure.
void ReadPublicSettings(
    Pal::IDevice*      pPalDevice,
    RuntimeSettings*   pSettings)
{
    // Read GPU ID (composed of PCI bus properties)
    uint32_t appGpuID = 0;
    if (pPalDevice->ReadSetting("AppGpuId",
        Pal::SettingScope::Global,
        Util::ValueType::Uint,
        &appGpuID,
        sizeof(appGpuID)))
    {
        pSettings->appGpuID = appGpuID;
    }

    // Read TurboSync global key
    bool turboSyncGlobal = false;
    if (pPalDevice->ReadSetting("TurboSync",
                                Pal::SettingScope::Global,
                                Util::ValueType::Boolean,
                                &turboSyncGlobal,
                                sizeof(turboSyncGlobal)))
    {
        pSettings->enableTurboSync = turboSyncGlobal;
    }

    // Read TFQ global key
    uint32_t texFilterQuality = TextureFilterOptimizationsEnabled;
    if (pPalDevice->ReadSetting("TFQ",
                                Pal::SettingScope::Global,
                                Util::ValueType::Uint,
                                &texFilterQuality,
                                sizeof(texFilterQuality)))
    {
        if (texFilterQuality <= TextureFilterOptimizationsAggressive)
        {
            pSettings->vulkanTexFilterQuality = static_cast<TextureFilterOptimizationSettings>(texFilterQuality);
        }
    }
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also be
// initialized here.
void ValidateSettings(
    Pal::IDevice*      pPalDevice,
    RuntimeSettings*   pSettings)
{
    // Override the default preciseAnisoMode value based on the public CCC vulkanTexFilterQuality (TFQ) setting.
    // Note: This will override any Vulkan app specific profile.
    switch (pSettings->vulkanTexFilterQuality)
    {
    case TextureFilterOptimizationsDisabled:
        // Use precise aniso and disable optimizations.  Highest image quality.
        // This is acutally redundant because TFQ should cause the GPU's PERF_MOD field to be set in such a
        // way that all texture filtering optimizations are disabled anyway.
        pSettings->preciseAnisoMode = EnablePreciseAniso;
        break;

    case TextureFilterOptimizationsAggressive:
        // Enable both aniso and trilinear filtering optimizations. Lowest image quality.
        // This will cause Vulkan to fail conformance tests.
        pSettings->preciseAnisoMode = DisablePreciseAnisoAll;
        break;

    case TextureFilterOptimizationsEnabled:
        // This is the default.  Do nothing and maintain default settings.
        break;
    }

    // Disable FMASK MSAA reads if shadow desc VA range is not supported
    Pal::DeviceProperties deviceProps;
    pPalDevice->GetProperties(&deviceProps);

    if (deviceProps.gpuMemoryProperties.flags.shadowDescVaSupport == 0)
    {
        pSettings->enableFmaskBasedMsaaRead = false;
    }

#if !VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
    // Internal semaphore queue timing is always enabled when ETW is not available
    pSettings->devModeSemaphoreQueueTimingEnable = true;
#endif
}

// =====================================================================================================================
// Updates any PAL public settings based on our runtime settings if necessary.
void UpdatePalSettings(
    Pal::IDevice*            pPalDevice,
    const RuntimeSettings*   pSettings)
{
    Pal::PalPublicSettings* pPalSettings = pPalDevice->GetPublicSettings();

    pPalSettings->textureOptLevel          = pSettings->vulkanTexFilterQuality;
    pPalSettings->dccBitsPerPixelThreshold = pSettings->dccBitsPerPixelThreshold;

    // Setting disableSkipFceOptimization to false enables an optimization in PAL that disregards the FCE in a transition
    // if one of the built in clear colors are used (white/black) and the image is TCC compatible.
    pPalSettings->disableSkipFceOptimization = false;
}

};
