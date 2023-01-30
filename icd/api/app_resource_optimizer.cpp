/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
**************************************************************************************************
* @file  app_resource_optimizer.cpp
* @brief Functions for tuning options pertaining to images.
**************************************************************************************************
*/
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_physical_device.h"
#include "include/vk_utils.h"

#include "include/app_resource_optimizer.h"

namespace vk
{

// =====================================================================================================================
ResourceOptimizer::ResourceOptimizer(
    Device*         pDevice,
    PhysicalDevice* pPhysicalDevice)
    :
    m_pDevice(pDevice),
    m_settings(pPhysicalDevice->GetRuntimeSettings())
{
    static_assert((static_cast<uint32_t>(Pal::MetadataMode::Count) == 4),
        "The number of MetadataMode enum entries has changed. "
        "The DccMode structure may need to be updated as well.");

    dccModeToMetadataMode[DccMode::DccDefaultMode] = Pal::MetadataMode::Default;
    dccModeToMetadataMode[DccMode::DccDisableMode] = Pal::MetadataMode::Disabled;
    dccModeToMetadataMode[DccMode::DccEnableMode]  = Pal::MetadataMode::ForceEnabled;
    dccModeToMetadataMode[DccMode::DccFmaskMode]   = Pal::MetadataMode::FmaskOnly;
}

// =====================================================================================================================
void ResourceOptimizer::Init()
{
    BuildAppProfile();

    BuildTuningProfile();

#if ICD_RUNTIME_APP_PROFILE
    BuildRuntimeProfile();

#endif
}

// =====================================================================================================================
void ResourceOptimizer::ApplyProfileToImageCreateInfo(
    const ResourceProfile&           profile,
    const ResourceOptimizerKey&      resourceKey,
    Pal::ImageCreateInfo*            pCreateInfo)
{
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const ResourceProfileEntry& profileEntry = profile.entries[entry];

        if (ResourcePatternMatchesResource(profileEntry.pattern, resourceKey))
        {
            const auto& resourceCreate = profileEntry.action.resourceCreate;

            if (pCreateInfo != nullptr)
            {
                if (resourceCreate.apply.dccMode)
                {
                    pCreateInfo->metadataMode = dccModeToMetadataMode[resourceCreate.dccMode];
                }
            }

        }
    }
}

// =====================================================================================================================
void ResourceOptimizer::ApplyProfileToImageViewCreateInfo(
    const ResourceProfile&           profile,
    const ResourceOptimizerKey&      resourceKey,
    Pal::ImageViewInfo*              pViewInfo) const
{
    VK_ASSERT(pViewInfo != nullptr);
    for (uint32_t entry = 0; entry < profile.entryCount; ++entry)
    {
        const ResourceProfileEntry& profileEntry = profile.entries[entry];

        if (ResourcePatternMatchesResource(profileEntry.pattern, resourceKey))
        {
            const auto& resourceCreate = profileEntry.action.resourceCreate;

            if (resourceCreate.apply.mallNoAlloc)
            {
                pViewInfo->flags.bypassMallRead = 1;
                pViewInfo->flags.bypassMallWrite = 1;
            }
        }
    }
}
// =====================================================================================================================
void ResourceOptimizer::OverrideImageCreateInfo(
    const ResourceOptimizerKey&  resourceKey,
    Pal::ImageCreateInfo*        pCreateInfo)
{
    ApplyProfileToImageCreateInfo(m_appProfile, resourceKey, pCreateInfo);

    ApplyProfileToImageCreateInfo(m_tuningProfile, resourceKey, pCreateInfo);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToImageCreateInfo(m_runtimeProfile, resourceKey, pCreateInfo);
#endif

}
// =====================================================================================================================
void ResourceOptimizer::OverrideImageViewCreateInfo(
    const ResourceOptimizerKey&  resourceKey,
    Pal::ImageViewInfo*          pPalViewInfo) const
{
    ApplyProfileToImageViewCreateInfo(m_appProfile, resourceKey, pPalViewInfo);

    ApplyProfileToImageViewCreateInfo(m_tuningProfile, resourceKey, pPalViewInfo);

#if ICD_RUNTIME_APP_PROFILE
    ApplyProfileToImageViewCreateInfo(m_runtimeProfile, resourceKey, pPalViewInfo);
#endif
}
// =====================================================================================================================
ResourceOptimizer::~ResourceOptimizer()
{
}

// =====================================================================================================================
bool ResourceOptimizer::ResourcePatternMatchesResource(
    const ResourceProfilePattern&   pattern,
    const ResourceOptimizerKey&     resourceKey) const
{
    if (pattern.match.always)
    {
        // always flag has priority over the others
        return true;
    }

    if (pattern.match.apiHash &&
        (pattern.targetKey.apiHash != resourceKey.apiHash))
    {
        return false;
    }

    if (pattern.match.apiHashRange &&
        ((resourceKey.apiHash < pattern.targetKey.apiHashBegin) ||
         (resourceKey.apiHash > pattern.targetKey.apiHashEnd)))
    {
        return false;
    }

    if (pattern.match.dimensions &&
        (pattern.targetKey.dimensions != resourceKey.dimensions))
    {
        return false;
    }

    return true;
}

// =====================================================================================================================
void ResourceOptimizer::BuildTuningProfile()
{
    memset(&m_tuningProfile, 0, sizeof(m_tuningProfile));

    if (m_settings.overrideResourceParams == false)
    {
        return;
    }

    // Only a single entry is currently supported
    m_tuningProfile.entryCount = 1;
    ResourceProfileEntry&   entry   = m_tuningProfile.entries[0];
    ResourceProfilePattern& pattern = entry.pattern;
    ResourceProfileAction&  action  = entry.action;

    if (m_settings.overrideResourceHashCrc != 0)
    {
        pattern.match.apiHash = true;
        pattern.targetKey.apiHash = m_settings.overrideResourceHashCrc;
    }

    if (m_settings.overrideResourceHashDimensions != 0)
    {
        pattern.match.dimensions = true;
        pattern.targetKey.dimensions = m_settings.overrideResourceHashDimensions;
    }

    if (pattern.match.u32All == 0)
    {
        pattern.match.always = true;
    }

    if (m_settings.overrideResourceDccOnOff != DccMode::DccDefaultMode)
    {
        action.resourceCreate.apply.dccMode = true;
        action.resourceCreate.dccMode = m_settings.overrideResourceDccOnOff;
    }
}

// =====================================================================================================================
void ResourceOptimizer::BuildAppProfile()
{
    const AppProfile        appProfile = m_pDevice->GetAppProfile();
    const Pal::GfxIpLevel   gfxIpLevel = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxLevel;

    const bool isNotPowerOfTwoMemoryBus = (Util::IsPowerOfTwo(m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->
        PalProperties().gpuMemoryProperties.performance.vramBusBitWidth) == false);

    uint32_t i = 0;

    memset(&m_appProfile, 0, sizeof(m_appProfile));

    // Early-out if the panel has dictated that we should ignore any active resource optimizations due to app profile
    if (m_settings.resourceProfileIgnoresAppProfile)
    {
        return;
    }

    // TODO: These need to be auto-generated from source JSON but for now we write profile programmatically

    if (appProfile == AppProfile::Doom)
    {
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp9)
        {
            // Disable DCC for resource causing corruption on clear because of the change to reset FCE counts in
            // the command buffer when an implicit reset is triggered.
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x0bb76acc72ad6492;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // Same issue as above but for image when viewed via Renderdoc which adds the Transfer_Dst usage
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x1237495e0bf5594b;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
    else if (appProfile == AppProfile::DoomEternal)
    {
        if (gfxIpLevel > Pal::GfxIpLevel::GfxIp10_1)
        {
            // Disable DCC for texture causing corruption due to undefined layout transitions when
            // ForceDccForColorAttachments is set to true.
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xad4094b212ff6083;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // Same issue as above, but when viewed via RenderDoc which adds the Transfer_Dst usage
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x3a70c52a65527761;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x14ed743568704236;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
    else if (appProfile == AppProfile::SkyGold)
    {
        if (gfxIpLevel >= Pal::GfxIpLevel::GfxIp10_1)
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xdd5e41b92c928478;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = 1;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
    else if (appProfile == AppProfile::WolfensteinII)
    {
        // The resource profile created by disabling DCC for usage containing:
        //     VK_IMAGE_USAGE_STORAGE_BIT & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        // except for format:
        //     VK_FORMAT_R8G8B8A8_UNORM
        if (gfxIpLevel >= Pal::GfxIpLevel::GfxIp10_1)
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xf07d02f4cd182cfc;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;

            // This resource is just for RenderDoc
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xa93766a8cca3df9d;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;
        }
    }
    else if (appProfile == AppProfile::WolfensteinYoungblood)
    {
        // Reuse Wolfenstein II tuning for Navi1x.
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_1)
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xf07d02f4cd182cfc;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;

            // This resource is just for RenderDoc
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xa93766a8cca3df9d;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;
        }
        else if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            // 000003c00000021c84f475a87fdb8b6a,False,RESDCC,1,0.72%,0.72%,0.97%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x84f475a87fdb8b6a;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000f0000000870cd48459e32729751,False,RESDCC,1,0.3%,0.3%,6.55%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xcd48459e32729751;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
    else if (appProfile == AppProfile::StrangeBrigade)
    {
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            if (isNotPowerOfTwoMemoryBus == false)
            {
                // 00000f00000008708a03574f7d0e4d17,False,RESDCC,2,1.84%,1.84%,1.23%
                i = m_appProfile.entryCount++;
                m_appProfile.entries[i].pattern.match.apiHash = true;
                m_appProfile.entries[i].pattern.targetKey.apiHash = 0x8a03574f7d0e4d17;
                m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
                m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;
            }

            // 00000f00000008708bcf1c20f5a6c4a7,False,RESDCC,1,0.64%,0.64%,2.02%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x8bcf1c20f5a6c4a7;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 0000078000000438f1d2e696ab27d939,False,RESDCC,1,0.06%,0.06%,2.13%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xf1d2e696ab27d939;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }

        if (isNotPowerOfTwoMemoryBus)
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x8a03574f7d0e4d17;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
    else if (appProfile == AppProfile::RainbowSixSiege)
    {
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            // 00a9c000005f8dd07784b18629d6e,False,RESDCC,1,3.26%,3.26%,3.49%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xdd07784b18629d6e;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000a9c000005f8fcea8016ffda572a,False,RESDCC,1,2.73%,2.73%,7.75%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xfcea8016ffda572a;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000a9c000005f88bcf1c20f5a6c4a7,False,RESDCC,2,1%,1%,8.08%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x8bcf1c20f5a6c4a7;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;
        }
    }
    else if (appProfile == AppProfile::GhostReconBreakpoint)
    {
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            // 00000f000000087094dcd846befd983e,False,RESDCC,1,0.33%,0.33%,0.49%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x94dcd846befd983e;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000f0000000870fe51515a12ef5aa0,False,RESDCC,1,0.31%,0.31%,0.6%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xfe51515a12ef5aa0;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000f0000000870d7eb29d36795fc2a,False,RESDCC,1,0.14%,0.14%,0.72%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xd7eb29d36795fc2a;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000780000004387e872e67edab5a42,False,RESDCC,1,0.1%,0.1%,0.98%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x7e872e67edab5a42;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // 00000200000002005ca0007c064cc05a,False,RESDCC,2,0.03%,0.03%,1.09%
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x5ca0007c064cc05a;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccEnableMode;
        }
    }
    else if (appProfile == AppProfile::Rage2)
    {
        // Disable DCC for resource causing corruption
        if (gfxIpLevel == Pal::GfxIpLevel::GfxIp10_3)
        {
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0xb92ea6fe16e91aba;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;

            // This resource is just for Renderdoc
            i = m_appProfile.entryCount++;
            m_appProfile.entries[i].pattern.match.apiHash = true;
            m_appProfile.entries[i].pattern.targetKey.apiHash = 0x111fa3cb932fb5fa;
            m_appProfile.entries[i].action.resourceCreate.apply.dccMode = true;
            m_appProfile.entries[i].action.resourceCreate.dccMode = DccMode::DccDisableMode;
        }
    }
}

#if ICD_RUNTIME_APP_PROFILE
void ResourceOptimizer::BuildRuntimeProfile()
{
    memset(&m_runtimeProfile, 0, sizeof(m_runtimeProfile));
    // TODO: JSON parsing should go here
}
#endif

};
