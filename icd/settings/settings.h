/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  settings.h
 * @brief Settings Loader class for Vulkan.
 ***********************************************************************************************************************
 */

#pragma once

#ifndef __SETTINGS_SETTINGS_H__
#define __SETTINGS_SETTINGS_H__

#include "palMetroHash.h"
#include <dd_settings_base.h>

// g_settings.h is generated in the same dir on Linux and Windows.
// However, if g_settings.h is generated out of source tree,
// then we need to include this file from "settings" dir.
#include "settings/g_settings.h"
#include "include/app_profile.h"
#include "include/vk_extensions.h"

namespace Pal
{
class IDevice;
class IPlatform;
struct DeviceProperties;
}

namespace vk
{
class ExperimentsLoader;

// =====================================================================================================================
// This class is responsible for loading and processing the Vulkan runtime settings structure encapsulated in the Vulkan
// Settings Loader object.
class VulkanSettingsLoader : public DevDriver::SettingsBase
{
public:
    explicit VulkanSettingsLoader(Pal::IDevice* pDevice, Pal::IPlatform* pPlatform, ExperimentsLoader* pExpLoader);
    virtual ~VulkanSettingsLoader();

    Pal::Result Init();

    VkResult ProcessSettings(
        const VkAllocationCallbacks* pAllocCb,
        uint32_t                     appVersion,
        AppProfile*                  pAppProfile);

    void ValidateSettings();

    void UpdatePalSettings();

    void FinalizeSettings(
        const DeviceExtensions::Enabled& enabledExtensions);

    Util::MetroHash::Hash GetSettingsHash() const { return m_settingsHash; }

    const RuntimeSettings& GetSettings() const { return m_settings; };
    RuntimeSettings* GetSettingsPtr() { return &m_settings; }

    // auto-generated functions
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;
    virtual void ReadSettings() override;
    virtual uint64_t GetSettingsBlobHash() const override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(VulkanSettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(VulkanSettingsLoader);

    // Generate the settings hash
    void GenerateSettingHash();

    bool ReadSetting(
        const char*     pSettingName,
        Util::ValueType valueType,
        void*           pValue,
        size_t          bufferSize = 0);

    VkResult OverrideProfiledSettings(
        const VkAllocationCallbacks* pAllocCb,
        uint32_t                     appVersion,
        AppProfile                   appProfile,
        Pal::DeviceProperties*       pInfo);

    void ReportUnsupportedExperiments(Pal::DeviceProperties* pInfo);

    void OverrideSettingsBySystemInfo();

    void OverrideDefaultsExperimentInfo();

    void FinalizeExperiments();

    void DumpAppProfileChanges(
        AppProfile appProfile);

    void ReadPublicSettings();

    Pal::IDevice*         m_pDevice;
    Pal::IPlatform*       m_pPlatform;
    ExperimentsLoader*    m_pExperimentsLoader;
    RuntimeSettings       m_settings;
    Util::MetroHash::Hash m_settingsHash;
};

} //vk

#endif
