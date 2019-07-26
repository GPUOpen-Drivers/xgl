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
 * @file  settings.h
 * @brief Settings Loader class for Vulkan.
 ***********************************************************************************************************************
 */

#pragma once

#ifndef __SETTINGS_SETTINGS_H__
#define __SETTINGS_SETTINGS_H__

#include "palSettingsLoader.h"
#include "g_settings.h"

#include "include/app_profile.h"

namespace Pal
{
class IDevice;
class IPlatform;
}

namespace vk
{
// =====================================================================================================================
// This class is responsible for loading and processing the Vulkan runtime settings structure encapsulated in the Vulkan
// Settings Loader object.
class VulkanSettingsLoader : public Pal::ISettingsLoader
{
public:
    explicit VulkanSettingsLoader(Pal::IDevice* pDevice, Pal::IPlatform* pPlatform, uint32_t deviceId);
    virtual ~VulkanSettingsLoader();

    virtual Util::Result Init() override;

    void ProcessSettings(
        uint32_t appVersion,
        AppProfile* pAppProfile);

    void ValidateSettings();

    void UpdatePalSettings();

    void FinalizeSettings();

    const RuntimeSettings& GetSettings() const { return m_settings; };
    RuntimeSettings* GetSettingsPtr() { return &m_settings; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(VulkanSettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(VulkanSettingsLoader);

    // Generate the settings hash
    void GenerateSettingHash();

    void OverrideProfiledSettings(
        uint32_t   appVersion,
        AppProfile appProfile);

    void OverrideSettingsBySystemInfo();

    void DumpAppProfileChanges(
        AppProfile appProfile);

    void ReadPublicSettings();

    Pal::IDevice*   m_pDevice;
    Pal::IPlatform* m_pPlatform;
    RuntimeSettings m_settings;

    // auto-generated functions
    virtual void SetupDefaults() override;
    virtual void ReadSettings() override;
    virtual void InitSettingsInfo() override;
    virtual void DevDriverRegister() override;

    char m_pComponentName[10];
};

} //vk

#endif // __SETTINGS_SETTINGS_H__
