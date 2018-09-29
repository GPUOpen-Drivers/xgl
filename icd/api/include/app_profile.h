/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  app_profile.h
* @brief Functions for detecting application profiles
**************************************************************************************************
*/

#ifndef __APP_PROFILE_H__
#define __APP_PROFILE_H__

#pragma once

#include "include/khronos/vulkan.h"

// Forward declare PAL classes used in this file
namespace Pal
{
};

// Forward declare Vulkan classes used in this file
namespace vk
{
class Instance;
struct RuntimeSettings;
};

namespace vk
{

// Enum describing which application is active
enum class AppProfile : uint32_t
{
    Default = 0,           // Default profile
    Doom,                  // Doom (2016) by id Software
    DoomVFR,               // DoomVFR by id Software
    WolfensteinII,         // Wolfenstein 2: The New Colossus by Machine Games
    IdTechEngine,          // id Tech Engine (Default)
    Dota2,                 // Dota 2 by Valve Software
    Source2Engine,         // Source 2 Engine (Default)
    Talos,                 // Talos Principle by Croteam
    SeriousSamFusion,      // Serious Sam Fusion by Croteam
    SedpEngine,            // Serious Engine (Default)
    MadMax,                // MadMax by Feral3D
    F1_2017,               // F1 2017 by Feral3D
    RiseOfTheTombra,       // RiseOfTheTombRaider by Feral3D
    Feral3DEngine,         // Feral3D Engine (Default)
    AshesOfTheSingularity, // Ashes Of The Singularity
    NitrousEngine,         // Nitrous Engine by Oxide (Default)
};

// Struct describing dynamic CHILL settings
struct ChillSettings
{
    bool      chillProfileEnable;  // If per-app chill profile settings is enabled
    uint32_t  chillLevel;          // Chill level and flags
    uint32_t  chillMinFrameRate;   // Min chill frame rate; valid range is 30-300fps.
    uint32_t  chillMaxFrameRate;   // Max chill frame rate; valid range is 30-300fps.
    uint32_t  chillLoadingScreenDrawsThresh;  // The threshold number of draw calls per frame used to distinguish
                                              // between loading screens and gameplay.
};

// Struct describing dynamic TurboSync settings
struct TurboSyncSettings
{
    bool      turboSyncEnable;  // If per-app TurboSync profile settings is enabled
};

extern AppProfile ScanApplicationProfile(const VkInstanceCreateInfo& instanceInfo);

bool ReloadAppProfileSettings(
    Instance*          pInstance,
    RuntimeSettings*   pRuntimeSettings,
    ChillSettings*     pChillSettings,
    TurboSyncSettings* pTurboSyncSettings);

};

#endif /* __GPU_EVENT_MGR_H__ */
