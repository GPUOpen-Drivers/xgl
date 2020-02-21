/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
ResourceOptimizer::~ResourceOptimizer()
{
}

// =====================================================================================================================
bool ResourceOptimizer::ResourcePatternMatchesResource(
    const ResourceProfilePattern&   pattern,
    const ResourceOptimizerKey&     resourceKey)
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
    const Pal::AsicRevision asicRevision = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().revision;

    uint32_t i = 0;

    memset(&m_appProfile, 0, sizeof(m_appProfile));

    // Early-out if the panel has dictated that we should ignore any active resource optimizations due to app profile
    if (m_settings.resourceProfileIgnoresAppProfile)
    {
        return;
    }

    // TODO: These need to be auto-generated from source JSON but for now we write profile programmatically

    // Resource parameters based on app profile should go here...
}

#if ICD_RUNTIME_APP_PROFILE
void ResourceOptimizer::BuildRuntimeProfile()
{
    // TODO: JSON parsing should go here
}
#endif

};
