/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "dd_settings_base.h"
#include "pal.h"

#include "settings/g_experiments.h"

// Forward declarations
namespace Pal
{
class IPlatform;
}

namespace vk
{
// =====================================================================================================================
// This class is responsible for loading the ExpSettings structure specified in the
// constructor. This is a helper class that only exists for a short time while the settings
// are initialized.
class ExperimentsLoader final : public DevDriver::SettingsBase
{
public:
    explicit ExperimentsLoader(Pal::IPlatform* pPlatform);

    virtual ~ExperimentsLoader();

    Pal::Result Init();

    void Destroy();

    // Returns a const pointer to the settings struct
    const ExpSettings* GetExpSettings() const { return &m_settings; }
    // Returns a non-const pointer to the settings struct, should only be used when the settings will be modified
    ExpSettings* GetMutableExpSettings() { return &m_settings; }

    // Auto-generated
    uint64_t GetSettingsBlobHash() const override;

    void ReportVsyncState(ExpVSyncControl state) { m_settings.expVerticalSynchronization = state; }

private:

    PAL_DISALLOW_COPY_AND_ASSIGN(ExperimentsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(ExperimentsLoader);

    // Auto-generated functions
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;

    Pal::IPlatform* m_pPlatform;
    ExpSettings     m_settings;
};

}
