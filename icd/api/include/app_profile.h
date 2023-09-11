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
class PhysicalDevice;
};

namespace vk
{

// Enum describing which application is active
enum class AppProfile : uint32_t
{
    Default = 0,             // Default profile
    Doom,                    // Doom (2016) by id Software
    DoomVFR,                 // DoomVFR by id Software
    WolfensteinII,           // Wolfenstein 2: The New Colossus by Machine Games
    Dota2,                   // Dota 2 by Valve Software
    Talos,                   // Talos Principle by Croteam
    TalosVR,                 // Talos Principle VR by Croteam
    SeriousSamFusion,        // Serious Sam Fusion by Croteam
    MadMax,                  // MadMax by Feral3D
    F1_2017,                 // F1 2017 by Feral3D
    RiseOfTheTombra,         // RiseOfTheTombRaider by Feral3D
    ThronesOfBritannia,      // Total War Saga: Thrones of Britannia by Feral3D
    DawnOfWarIII,            // Dawn of War III by Feral3D
    WarHammerII,             // Total War: WarHammer II by Feral3D
    WarHammerIII,            // Total War: WarHammer III by Feral3D
    RomeRemastered,          // Total War Rome Remastered
    AshesOfTheSingularity,   // Ashes Of The Singularity
    StrangeBrigade,          // Strange Brigade
    WorldWarZ,               // WorldWarZ
    ThreeKingdoms,           // Three Kingdoms by Feral3D
    DiRT4,                   // DiRT4 by Feral3D
    Rage2,                   // Rage2 by Avalanche Studios
    RainbowSixSiege,         // Tom Clancy's Rainbow Six Siege: Operation Phantom by Ubisoft
    WolfensteinYoungblood,   // Wolfenstein Youngblood by Machine Games
    RedDeadRedemption2,      // Red Dead Redemption 2 by Rockstar
    DoomEternal,             // Doom Eternal by id Software
    IdTechLauncher,          // id Software Launcher Application
    ZombieArmy4,             // Zombie Army 4: Dead War by Rebellion Developments
    GhostReconBreakpoint,    // Ghost Recon Breakpoint
    DetroitBecomeHuman,      // Detroit Become Human by Quantic Dream
    ShadowOfTheTombRaider,   // ShadowOfTheTombRaider by Feral3D
    XPlane,                  // XPlane by Laminar Research
    WarThunder,              // WarThunder by Gaijin Distribution Kft
    MetroExodus,             // Metro Exodus
    Quake2RTX,               // Quake2 RTX
    Valheim,                 // Valheim by Coffee Stain Studios
    WolfensteinCyberpilot,   // Wolfenstein Cyberpilot by Machine Games
    EvilGenius2,             // Evil Genius 2
    KnockoutCity,            // Knockout City
    SkyGold,                 // Sky Gold by NetEase
    IdTechEngine,            // id Tech Engine (Default)
    Feral3DEngine,           // Feral3D Engine (Default)
    StrangeEngine,           // Strange Engine (Default)
    SedpEngine,              // Serious Engine (Default)
    Source2Engine,           // Source 2 Engine (Default)
    NitrousEngine,           // Nitrous Engine by Oxide (Default)
    ApexEngine,              // Avalanche Open World Engine (Default)
    ScimitarEngine,          // Scimitar Engine by Ubisoft (Default)
    AnvilNextEngine,         // AnvilNext Engine by Ubisoft (Default)
    QuanticDreamEngine,      // Quantic Dream Engine by Quantic Dream
    XSystemEngine,           // XSystem Engine by Laminar Research
    UnityEngine,             // Unity Engine by Unity Technologies (Default)
    SaschaWillemsExamples,   // Vulkan Examples by Sascha Willems
    Hyperscape,              // Hyperscape by Ubisoft
    SeriousSam4,             // Serious Sam 4 by Croteam
    SniperElite5,            // Sniper Elite 5 by Rebellion
    SeriousSamVrTheLastHope, // Serious Sam VR The Last Hope by Croteam
#if VKI_RAY_TRACING
    ControlDX12,             // VKD3D Control Ultimate Edition
    RayTracingWeekends,      // RayTracingInVulkan demo
#endif
    Maxon,                   // Maxon
    AngleEngine,             // Angle Engine
    CSGO,                    // Counter-Strike: Global Offensive
    DxvkGodOfWar,            // DXVK God of War
    X4Foundations,           // X4: Foundations by Egosoft
    MetalGearSolid5,         // Metal Gear Solid5 : The Phantom Pain
    YamagiQuakeII,           // Yamagi Quake II
    Battlefield1,            // Battlefield 1
    GpuCapsViewer32Bit,      // GPU Caps Viewer demos by GeeXLab
    DDraceNetwork,           // DDrace Network
    SaintsRowV,              // Saints Row V by Volition
    VolitionEngine,          // Volition Engine
    HalfLifeAlyx,            // Half Life Alyx VR by Valve
    Satisfactory,            // Satisfactory by Coffee Stain Studios
    QuakeEnhanced,           // Quake Enhanced by id Software
    Zink,                    // Zink
    SOTTR,                   // Shadow of the tomb raider steam version
    SHARK,                   // SHARK
    SpidermanRemastered,     // Spiderman Remastered
    TheSurge2,               // The Surge 2
    Yuzu,                    // Yuzu Emulator by Team Yuzu | Citra
    AsyncPostProcessLVr,     // AsyncPostProcessing sample app for AMD Liquid VR SDK
};

struct ProfileSettings
{
    uint32_t    texFilterQuality;      // TextureFilterOptimizationSettings

#if VKI_RAY_TRACING
#endif

};

extern AppProfile ScanApplicationProfile(const VkInstanceCreateInfo& instanceInfo);

void ReloadAppProfileSettings(const PhysicalDevice* pPhysicalDevice,
                              Instance*             pInstance,
                              ProfileSettings*      pProfileSettings,
                              uint32_t              appGpuID = 0u);

};

#endif /* __APP_PROFILE_H__ */
