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
* @file  app_profile.cpp
* @brief Functions for determining which application profile is active.
**************************************************************************************************
*/

#include "include/app_profile.h"
#include "include/vk_utils.h"

#include "palMetroHash.h"

#include <cctype>
#include <memory>
#include <string.h>

#if defined(__unix__)
#include <unistd.h>
#include <linux/limits.h>
#endif

#include "include/vk_instance.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "palPlatform.h"
#include "gpuUtil/palAppProfileIterator.h"

namespace vk
{

// This is a type of pattern to match
enum AppProfilePatternType
{
                            // Match against:
    PatternNone = 0,        // - None.  Should be the last entry in a list to identify end of pattern entry list.
    PatternAppName,         // - VkApplicationInfo::pApplicationName
    PatternAppNameLower,    // - Lower-case version of PatternAppName
    PatternEngineName,      // - VkApplicationInfo::pEngineName
    PatternEngineNameLower, // - Lower-case version of PatternEngineName
    PatternExeName,         // - Executable name without file extension
    PatternExeNameLower,    // - Lower-case version of PatternExeName
    PatternStrInExeNameLower, // - Any specific string included in Lower-case PatternExeName
    PatternCount
};

// This is a pattern entry.  It is a pair of type and test hash.  The string of the given type
// is hashed and compared against the hash value.  If the values are equal, this entry matches.
struct AppProfilePatternEntry
{
    constexpr AppProfilePatternEntry(const AppProfilePatternType type, const Util::MetroHash::Hash hash) :
        type(type), hashed(true), hash(hash) { }

    constexpr AppProfilePatternEntry(const AppProfilePatternType type, const char* text) :
        type(type), hashed(false), text(text) { }

    constexpr AppProfilePatternEntry() :
        type(PatternNone), hashed(false), text("") { }

    AppProfilePatternType type;  // Type of pattern to match against
    bool hashed; // Tag to determine if hash or text is used

    // Hash or text to compare against.
    union
    {
        Util::MetroHash::Hash hash;
        const char* text;
    };
};

// This is a pattern that maps to a profile.  It is a list of entries to compare against.  If all entries
// match, the given profile is assigned to this process.
struct AppProfilePattern
{
    AppProfile             profile;
    AppProfilePatternEntry entries[16];
};

constexpr AppProfilePatternEntry AppNameDoom =
{
    PatternAppNameLower,
    "doom"
};

constexpr AppProfilePatternEntry AppNameDoomVFR =
{
    PatternAppNameLower,
    "doom_vfr"
};

constexpr AppProfilePatternEntry AppNameWolfensteinII =
{
    PatternAppNameLower,
    "wolfenstein ii the new colossus"
};

constexpr AppProfilePatternEntry AppEngineIdTech =
{
    PatternEngineNameLower,
    "idtech"
};

constexpr AppProfilePatternEntry AppNameDota2 =
{
    PatternAppNameLower,
    "dota"
};

constexpr AppProfilePatternEntry AppNameHalfLifeAlyx =
{
    PatternAppNameLower,
    "hlvr"
};

constexpr AppProfilePatternEntry AppEngineSource2 =
{
    PatternEngineNameLower,
    "source2"
};

constexpr AppProfilePatternEntry AppEngineDXVK =
{
    PatternEngineNameLower,
    "dxvk"
};

constexpr AppProfilePatternEntry AppEngineZink =
{
    PatternEngineNameLower,
    "mesa zink"
};

constexpr AppProfilePatternEntry AppNameTalosWin32Bit =
{
    PatternAppNameLower,
    "talos"
};

constexpr AppProfilePatternEntry AppNameTalosWin64Bit =
{
    PatternAppNameLower,
    "talos - 64bit"
};

constexpr AppProfilePatternEntry AppNameTalosVRWin64Bit =
{
    PatternAppNameLower,
    "talos - 64bit- vr"
};

constexpr AppProfilePatternEntry AppNameTalosLinux32Bit =
{
    PatternAppNameLower,
    "talos - linux"
};

constexpr AppProfilePatternEntry AppNameTalosLinux64Bit =
{
    PatternAppNameLower,
    "talos - linux - 64bit"
};

constexpr AppProfilePatternEntry AppNameTalosVRLinux64Bit =
{
    PatternAppNameLower,
    "talos - linux - 64bit- vr"
};

constexpr AppProfilePatternEntry AppNameSeriousSamFusionWin =
{
    PatternAppNameLower,
    "serious sam fusion 2017 - 64bit"
};

constexpr AppProfilePatternEntry AppNameSeriousSamFusionLinux =
{
    PatternAppNameLower,
    "serious sam fusion 2017 - linux - 64bit"
};

constexpr AppProfilePatternEntry AppNameSeriousSam4Win =
{
    PatternAppNameLower,
    "serious sam 4 - 64bit"
};

constexpr AppProfilePatternEntry AppNameRomeRemasteredLinux =
{
    PatternAppNameLower,
    "rome"
};

constexpr AppProfilePatternEntry AppEngineSedp =
{
    PatternEngineNameLower,
    "sedp class"
};

constexpr AppProfilePatternEntry AppNameMadMax =
{
    PatternAppNameLower,
    "madmax"
};

constexpr AppProfilePatternEntry AppNameF1_2017 =
{
    PatternAppNameLower,
    "f12017"
};

constexpr AppProfilePatternEntry AppNameRiseOfTheTombra =
{
    PatternAppNameLower,
    "riseofthetombra"
};

constexpr AppProfilePatternEntry AppNameThronesOfBritannia =
{
    PatternAppNameLower,
    "thronesofbritan"
};

constexpr AppProfilePatternEntry AppNameDawnOfWarIII =
{
    PatternAppNameLower,
    "dawnofwar3"
};

constexpr AppProfilePatternEntry AppNameWarHammerII =
{
    PatternAppNameLower,
    "totalwarhammer2"
};

constexpr AppProfilePatternEntry AppNameWarHammerIII =
{
    PatternAppNameLower,
    "totalwarhammer3"
};

constexpr AppProfilePatternEntry AppEngineFeral3D =
{
    PatternEngineNameLower,
    "feral3d"
};

constexpr AppProfilePatternEntry AppNameAshesOfTheSingularity =
{
    PatternAppNameLower,
    "ashes of the singularity: escalation"
};

constexpr AppProfilePatternEntry AppEngineNitrous =
{
    PatternEngineNameLower,
    "nitrous by oxide games"
};

constexpr AppProfilePatternEntry AppNameStrangeBrigade =
{
    PatternAppNameLower,
    "strange"
};

constexpr AppProfilePatternEntry AppEngineStrangeBrigade =
{
    PatternEngineNameLower,
    "strange"
};

constexpr AppProfilePatternEntry AppNameSkyGold =
{
    PatternAppNameLower,
    "sky"
};

constexpr AppProfilePatternEntry AppEngineSkyGold =
{
    PatternEngineNameLower,
    "sky"
};

constexpr AppProfilePatternEntry AppNameWWZ =
{
    PatternAppNameLower,
    "wwz"
};

constexpr AppProfilePatternEntry AppEngineHusky =
{
    PatternEngineNameLower,
    "husky"
};

constexpr AppProfilePatternEntry AppNameThreeKingdoms =
{
    PatternAppNameLower,
    "threekingdoms"
};

constexpr AppProfilePatternEntry AppNameDiRT4 =
{
    PatternAppNameLower,
    "dirt4"
};

constexpr AppProfilePatternEntry AppNameShadowOfTheTombRaider =
{
    PatternAppNameLower,
    "shadowofthetomb"
};

constexpr AppProfilePatternEntry AppNameXPlane =
{
    PatternAppNameLower,
    "x-plane"
};

constexpr AppProfilePatternEntry AppNameWarThunder =
{
    PatternAppNameLower,
    "dagor"
};

constexpr AppProfilePatternEntry AppEngineDagorEngine =
{
    PatternEngineNameLower,
    "dagor"
};

constexpr AppProfilePatternEntry AppNameMetroExodus =
{
    PatternAppNameLower,
    "metroexodus"
};

constexpr AppProfilePatternEntry AppEngineMetroExodus =
{
    PatternEngineNameLower,
    "metroexodus"
};

constexpr AppProfilePatternEntry AppEngineXSystem =
{
    PatternEngineNameLower,
    "x-system"
};

constexpr AppProfilePatternEntry AppNameSaschaWillemsExamples =
{
    PatternAppNameLower,
    "vulkanexample"
};

constexpr AppProfilePatternEntry AppEngineSaschaWillemsExamples =
{
    PatternEngineNameLower,
    "vulkanexample"
};

//steam version of shadow of tomb raider
constexpr AppProfilePatternEntry AppNameSOTTR =
{
    PatternAppNameLower,
    "sottr.exe"
};

constexpr AppProfilePatternEntry AppNameSHARK =
{
    PatternAppNameLower,
    "iree-ml"
};

constexpr AppProfilePatternEntry AppNameSpidermanRemastered =
{
    PatternExeNameLower,
    "spider-man.exe"
};

constexpr AppProfilePatternEntry AppNameYuzu =
{
    PatternAppNameLower,
    "yuzu emulator"
};

constexpr AppProfilePatternEntry AppEngineYuzu =
{
    PatternEngineNameLower,
    "yuzu emulator"
};

#if VKI_RAY_TRACING
constexpr AppProfilePatternEntry AppEngineVKD3D =
{
    PatternEngineNameLower,
    Util::MetroHash::Hash{{{
        0x32778d0a,
        0x05b56a84,
        0x8f0c25bc,
        0x1d75f3eb
    }}}
};

constexpr AppProfilePatternEntry AppNameControlDX12 =
{
    PatternAppNameLower,
    Util::MetroHash::Hash{{{
        0x75f46e9f,
        0x66e3de7b,
        0x57150c75,
        0xa990df0c
    }}}
};

constexpr AppProfilePatternEntry AppNameRayTracingWeekends =
{
    PatternAppNameLower,
    "raytracingweekends"
};
#endif

constexpr AppProfilePatternEntry AppNameIdTechLauncher =
{
    PatternAppNameLower,
    "idtechlauncher"
};

constexpr AppProfilePatternEntry AppNameWolfensteinYoungblood =
{
    PatternAppNameLower,
    "wolfenstein: youngblood"
};

constexpr AppProfilePatternEntry AppNameWolfensteinCyberpilot =
{
    PatternAppNameLower,
    "wolfenstein: cyberpilot"
};

constexpr AppProfilePatternEntry AppNameRainbowSixSiege =
{
    PatternAppNameLower,
    "rainbow six siege"
};

constexpr AppProfilePatternEntry AppNameHyperscape =
{
    PatternAppNameLower,
    "hyperscape"
};

constexpr AppProfilePatternEntry AppEngineScimitar =
{
    PatternEngineNameLower,
    "scimitar"
};

constexpr AppProfilePatternEntry AppNameRage2 =
{
    PatternAppNameLower,
    "rage 2"
};

constexpr AppProfilePatternEntry AppEngineApex =
{
    PatternEngineNameLower,
    "apex engine"
};

constexpr AppProfilePatternEntry AppNameRDR2 =
{
    PatternAppNameLower,
    "red dead redemption 2"
};

constexpr AppProfilePatternEntry  AppEngineRAGE
{
    PatternEngineNameLower,
    "sga"
};

constexpr AppProfilePatternEntry AppNameDoomEternal =
{
    PatternAppNameLower,
    "doometernal"
};

constexpr AppProfilePatternEntry AppNameZombieArmy4 =
{
    PatternAppNameLower,
    "za4"
};

constexpr AppProfilePatternEntry AppEngineZombieArmy4
{
    PatternEngineNameLower,
    "za4"
};

constexpr AppProfilePatternEntry AppNameGhostReconBreakpoint =
{
    PatternAppNameLower,
    "ghost recon breakpoint"
};

constexpr AppProfilePatternEntry AppNameQuake2RTX =
{
    PatternAppNameLower,
    "quake 2 pathtracing"
};

constexpr AppProfilePatternEntry AppEngineVKPT =
{
    PatternEngineNameLower,
    "vkpt"
};

constexpr AppProfilePatternEntry AppEngineAnvilNext =
{
    PatternEngineNameLower,
    "anvilnext"
};

constexpr AppProfilePatternEntry AppEngineUnity =
{
    PatternEngineNameLower,
    "unity"
};

constexpr AppProfilePatternEntry AppEngineAngle =
{
    PatternEngineNameLower,
    "angle"
};

constexpr AppProfilePatternEntry AppNameValheim =
{
    PatternExeNameLower,
    "valheim"
};

constexpr AppProfilePatternEntry AppExeKnockoutcity =
{
    PatternExeNameLower,
    "knockoutcity"
};

constexpr AppProfilePatternEntry AppNameEvilGenius2 =
{
    PatternAppNameLower,
    "evil genius 2"
};

constexpr AppProfilePatternEntry AppNameCSGO =
{
    PatternAppNameLower,
    "csgo"
};

constexpr AppProfilePatternEntry AppNameCSGOLinux32Bit =
{
    PatternAppNameLower,
    "csgo_linux"
};

constexpr AppProfilePatternEntry AppNameCSGOLinux64Bit =
{
    PatternAppNameLower,
    "csgo_linux64"
};

constexpr AppProfilePatternEntry AppNameGodOfWar
{
    PatternAppNameLower,
    "gow.exe"
};

constexpr AppProfilePatternEntry AppNameX4Foundations
{
    PatternAppNameLower,
    "x4"
};

constexpr AppProfilePatternEntry AppNameX4Engine
{
    PatternEngineNameLower,
    "engine name"
};

constexpr AppProfilePatternEntry AppNameSniperElite5 =
{
    PatternAppNameLower,
    "sniper5"
};

constexpr AppProfilePatternEntry AppEngineSniperElite5 =
{
    PatternEngineNameLower,
    "sniper5"
};

constexpr AppProfilePatternEntry AppNameMetalGearSolid5 =
{
    PatternAppNameLower,
    "mgsvtpp.exe"
};

constexpr AppProfilePatternEntry AppEngineIdTech2 =
{
    PatternEngineNameLower,
    "id tech 2"
};

constexpr AppProfilePatternEntry AppNameYamagiQuake2 =
{
    PatternAppNameLower,
    "quake 2"
};

constexpr AppProfilePatternEntry AppNameBattlefield1 =
{
    PatternAppNameLower,
    "bf1.exe"
};

constexpr AppProfilePatternEntry AppNameGpuCapsViewer32Bit =
{
    PatternAppNameLower,
    "geexlab"
};

constexpr AppProfilePatternEntry AppNameDDraceNetwork =
{
    PatternAppNameLower,
    "ddnet"
};

constexpr AppProfilePatternEntry AppNameSaintsRowV =
{
    PatternAppNameLower,
    "saintsrow5"
};

constexpr AppProfilePatternEntry AppEngineVolition =
{
    PatternEngineNameLower,
    "volition ctg engine"
};

constexpr AppProfilePatternEntry AppNameSeriousSamVr =
{
    PatternAppNameLower,
    "serious sam vr: the last hope - 64bit- vr"
};

constexpr AppProfilePatternEntry AppNameSatisfactory =
{
    PatternAppNameLower,
    "factorygame"
};

constexpr AppProfilePatternEntry AppNameQuakeEnhanced =
{
    PatternAppNameLower,
    "quake"
};

constexpr AppProfilePatternEntry AppNameLiquidVrSdk =
{
    PatternAppNameLower,
    "liquidvr sdk"
};

constexpr AppProfilePatternEntry AppExeAsyncPostProcessing =
{
    PatternExeNameLower,
    "asyncpostprocessing"
};

constexpr AppProfilePatternEntry AppNameTheSurge2 =
{
    PatternAppNameLower,
    "fledge"
};

constexpr AppProfilePatternEntry PatternEnd = {};

// This is a table of patterns.  The first matching pattern in this table will be returned.
AppProfilePattern AppPatternTable[] =
{
    {
        AppProfile::Doom,
        {
            AppNameDoom,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::DoomEternal,
        {
            AppNameDoomEternal,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::DoomVFR,
        {
            AppNameDoomVFR,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::WolfensteinII,
        {
            AppNameWolfensteinII,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::WolfensteinYoungblood,
        {
            AppNameWolfensteinYoungblood,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::WolfensteinCyberpilot,
        {
            AppNameWolfensteinCyberpilot,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::IdTechLauncher,
        {
            AppNameIdTechLauncher,
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::IdTechEngine,
        {
            AppEngineIdTech,
            PatternEnd
        }
    },

    {
        AppProfile::Dota2,
        {
            AppNameDota2,
            AppEngineSource2,
            PatternEnd
        }
    },

    {
        AppProfile::HalfLifeAlyx,
        {
            AppNameHalfLifeAlyx,
            AppEngineSource2,
            PatternEnd
        }
    },

    {
        AppProfile::Talos,
        {
            AppNameTalosWin64Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::Talos,
        {
            AppNameTalosWin32Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::Talos,
        {
            AppNameTalosLinux64Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::Talos,
        {
            AppNameTalosLinux32Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::TalosVR,
        {
            AppNameTalosVRWin64Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::TalosVR,
        {
            AppNameTalosVRLinux64Bit,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::SeriousSamFusion,
        {
            AppNameSeriousSamFusionWin,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::SeriousSamFusion,
        {
            AppNameSeriousSamFusionLinux,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::SeriousSam4,
        {
            AppNameSeriousSam4Win,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::SeriousSamVrTheLastHope,
        {
            AppNameSeriousSamVr,
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::SedpEngine,
        {
            AppEngineSedp,
            PatternEnd
        }
    },

    {
        AppProfile::MadMax,
        {
            AppNameMadMax,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::F1_2017,
        {
            AppNameF1_2017,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::RiseOfTheTombra,
        {
            AppNameRiseOfTheTombra,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::ThronesOfBritannia,
        {
            AppNameThronesOfBritannia,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::DawnOfWarIII,
        {
            AppNameDawnOfWarIII,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::WarHammerII,
        {
            AppNameWarHammerII,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::WarHammerIII,
        {
            AppNameWarHammerIII,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::RomeRemastered,
        {
            AppNameRomeRemasteredLinux,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::ThreeKingdoms,
        {
            AppNameThreeKingdoms,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::DiRT4,
        {
            AppNameDiRT4,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::ShadowOfTheTombRaider,
        {
            AppNameShadowOfTheTombRaider,
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::Feral3DEngine,
        {
            AppEngineFeral3D,
            PatternEnd
        }
    },

    {
        AppProfile::XPlane,
        {
            AppNameXPlane,
            AppEngineXSystem,
            PatternEnd
        }
    },

    {
        AppProfile::XSystemEngine,
        {
            AppEngineXSystem,
            PatternEnd
        }
    },

    {
        AppProfile::WarThunder,
        {
            AppNameWarThunder,
            AppEngineDagorEngine,
            PatternEnd
        }
    },

    {
        AppProfile::MetroExodus,
        {
            AppNameMetroExodus,
            AppEngineMetroExodus,
            PatternEnd
        }
    },

    {
        AppProfile::AshesOfTheSingularity,
        {
            AppNameAshesOfTheSingularity,
            AppEngineNitrous,
            PatternEnd
        }
    },

    {
        AppProfile::NitrousEngine,
        {
            AppEngineNitrous,
            PatternEnd
        }
    },

    {
        AppProfile::StrangeBrigade,
        {
            AppNameStrangeBrigade,
            AppEngineStrangeBrigade,
            PatternEnd
        }
    },

    {
        AppProfile::StrangeEngine,
        {
            AppEngineStrangeBrigade,
            PatternEnd
        }
    },

    {
        AppProfile::SkyGold,
        {
            AppNameSkyGold,
            AppEngineSkyGold,
            PatternEnd
        }
    },

    {
        AppProfile::WorldWarZ,
        {
            AppNameWWZ,
            AppEngineHusky,
            PatternEnd
        }
    },

    {
        AppProfile::SaschaWillemsExamples,
        {
            AppNameSaschaWillemsExamples,
            AppEngineSaschaWillemsExamples,
            PatternEnd
        }
    },

    {
        AppProfile::Rage2,
        {
            AppNameRage2,
            AppEngineApex,
            PatternEnd
        }
    },

    {
        AppProfile::SaintsRowV,
        {
            AppNameSaintsRowV,
            AppEngineVolition,
            PatternEnd
        }
    },

    {
        AppProfile::ApexEngine,
        {
            AppEngineApex,
            PatternEnd
        }
    },

    {
        AppProfile::RainbowSixSiege,
        {
            AppNameRainbowSixSiege,
            AppEngineScimitar,
            PatternEnd
        }
    },
    {
       AppProfile::KnockoutCity,
       {
           AppExeKnockoutcity,
           PatternEnd
       }
    },
    {
       AppProfile::EvilGenius2,
       {
           AppNameEvilGenius2,
           PatternEnd
       }
    },

    {
        AppProfile::Hyperscape,
        {
            AppNameHyperscape,
            AppEngineScimitar,
            PatternEnd
        }
    },

    {
        AppProfile::ScimitarEngine,
        {
            AppEngineScimitar,
            PatternEnd
        }
    },

    {
        AppProfile::RedDeadRedemption2,
        {
            AppNameRDR2,
            AppEngineRAGE,
            PatternEnd
        }
    },

    {
        AppProfile::ZombieArmy4,
        {
            AppNameZombieArmy4,
            AppEngineZombieArmy4,
            PatternEnd
        }
    },

    {
        AppProfile::GhostReconBreakpoint,
        {
            AppNameGhostReconBreakpoint,
            AppEngineAnvilNext,
            PatternEnd
        }
    },

    {
        AppProfile::Quake2RTX,
        {
            AppNameQuake2RTX,
            AppEngineVKPT,
            PatternEnd
        }
    },

    {
        AppProfile::Valheim,
        {
            AppNameValheim,
            AppEngineUnity,
            PatternEnd
        }
    },

    {
        AppProfile::UnityEngine,
        {
            AppEngineUnity,
            PatternEnd
        }
    },

    {
        AppProfile::SniperElite5,
        {
            AppNameSniperElite5,
            AppEngineSniperElite5,
            PatternEnd
        }
    },

    {
        AppProfile::SOTTR,
        {
            AppNameSOTTR,
            AppEngineDXVK,
            PatternEnd
        }
    },

    {
        AppProfile::SHARK,
        {
            AppNameSHARK,
            PatternEnd
        }
    },

    {
        AppProfile::SpidermanRemastered,
        {
            AppNameSpidermanRemastered,
            PatternEnd
        }
    },

    {
        AppProfile::Yuzu,
        {
            AppNameYuzu,
            AppEngineYuzu,
            PatternEnd
        }
    },

#if VKI_RAY_TRACING
    {
        AppProfile::ControlDX12,
        {
            AppNameControlDX12,
            AppEngineVKD3D,
            PatternEnd
        }
    },

    {
        AppProfile::RayTracingWeekends,
        {
            AppNameRayTracingWeekends,
            PatternEnd
        }
    },
#endif

    {
        AppProfile::AngleEngine,
        {
            AppEngineAngle,
            PatternEnd
        }
    },

    {
        AppProfile::CSGO,
        {
            AppNameCSGO,
            PatternEnd
        }
    },

    {
        AppProfile::CSGO,
        {
            AppNameCSGOLinux32Bit,
            PatternEnd
        }
    },

    {
        AppProfile::CSGO,
        {
            AppNameCSGOLinux64Bit,
            PatternEnd
        }
    },

    {
        AppProfile::Source2Engine,
        {
            AppEngineSource2,
            PatternEnd
        }
    },

    {
        AppProfile::DxvkGodOfWar,
        {
            AppNameGodOfWar,
            AppEngineDXVK,
            PatternEnd
        }
    },

    {
        AppProfile::X4Foundations,
        {
            AppNameX4Foundations,
            AppNameX4Engine,
            PatternEnd
        }
    },

    {
        AppProfile::MetalGearSolid5,
        {
            AppNameMetalGearSolid5,
            PatternEnd
        }
    },

    {
        AppProfile::YamagiQuakeII,
        {
            AppNameYamagiQuake2,
            AppEngineIdTech2,
            PatternEnd
        }
    },

    {
        AppProfile::Battlefield1,
        {
            AppNameBattlefield1,
            AppEngineDXVK,
            PatternEnd
        }
    },

    {
        AppProfile::GpuCapsViewer32Bit,
        {
            AppNameGpuCapsViewer32Bit,
            PatternEnd
        }
    },

    {
        AppProfile::DDraceNetwork,
        {
            AppNameDDraceNetwork,
            PatternEnd
        }
    },

    {
        AppProfile::Satisfactory,
        {
            AppNameSatisfactory,
            PatternEnd
        }
    },

    {
        AppProfile::QuakeEnhanced,
        {
            AppNameQuakeEnhanced,
            PatternEnd
        }
    },

    {
        AppProfile::AsyncPostProcessLVr,
        {
            AppNameLiquidVrSdk,
            AppExeAsyncPostProcessing,
            PatternEnd
        }
    },

    {
        AppProfile::TheSurge2,
        {
            AppNameTheSurge2,
            PatternEnd
        }
    },

    {
        AppProfile::Zink,
        {
            AppEngineZink,
            PatternEnd
        }
    }
};

static char* GetExecutableName(size_t* pLength, bool includeExtension = false);

// =====================================================================================================================
// Returns the lower-case version of a string.  The returned string must be freed by the caller using, specifically,
// free().
char* StringToLower(const char* pString, size_t strLength)
{
    char* pStringLower = nullptr;

    if (pString != nullptr)
    {
        pStringLower = static_cast<char*>(malloc(strLength));

        if (pStringLower != nullptr)
        {
            // Convert app name to lower case
            for (size_t i = 0; i < strLength; ++i)
            {
                pStringLower[i] = tolower(pString[i]);
            }
        }
    }

    return pStringLower;
}

// =====================================================================================================================
// Goes through all patterns and returns an application profile that matches the first matched pattern.  Patterns
// compare things like VkApplicationInfo values or executable names, etc.  This profile may further be overridden
// by private panel settings.
AppProfile ScanApplicationProfile(
    const VkInstanceCreateInfo& instanceInfo)
{
    AppProfile profile = AppProfile::Default;

    // Generate hashes for all of the tested pattern entries
    Util::MetroHash::Hash hashes[PatternCount] = {};
    char* texts[PatternCount] = {};
    bool valid[PatternCount] = {};

    if (instanceInfo.pApplicationInfo != nullptr)
    {
        if (instanceInfo.pApplicationInfo->pApplicationName != nullptr)
        {
            const char* pAppName = instanceInfo.pApplicationInfo->pApplicationName;
            size_t appNameLength = strlen(pAppName);

            Util::MetroHash128::Hash(
                reinterpret_cast<const uint8_t*>(pAppName), appNameLength, hashes[PatternAppName].bytes);
            valid[PatternAppName] = true;

            char* pAppNameLower = StringToLower(pAppName, appNameLength + 1); // Add 1 for null terminator!

            if (pAppNameLower != nullptr)
            {
                Util::MetroHash128::Hash(
                    reinterpret_cast<const uint8_t*>(pAppNameLower), appNameLength, hashes[PatternAppNameLower].bytes);
                texts[PatternAppNameLower] = pAppNameLower;
                valid[PatternAppNameLower] = true;
            }
        }

        if (instanceInfo.pApplicationInfo->pEngineName != nullptr)
        {
            const char* pEngineName = instanceInfo.pApplicationInfo->pEngineName;
            size_t engineNameLength = strlen(pEngineName);

            Util::MetroHash128::Hash(
                reinterpret_cast<const uint8_t*>(pEngineName), engineNameLength, hashes[PatternEngineName].bytes);
            valid[PatternEngineName] = true;

            char* pEngineNameLower = StringToLower(pEngineName, engineNameLength + 1);

            if (pEngineNameLower != nullptr)
            {
                Util::MetroHash128::Hash(
                    reinterpret_cast<const uint8_t*>(pEngineNameLower), engineNameLength, hashes[PatternEngineNameLower].bytes);
                texts[PatternEngineNameLower] = pEngineNameLower;
                valid[PatternEngineNameLower] = true;
            }
        }
    }

    size_t exeNameLength = 0;
    char* pExeName = GetExecutableName(&exeNameLength);

    if (pExeName != nullptr)
    {
        Util::MetroHash128::Hash(
            reinterpret_cast<const uint8_t*>(pExeName), exeNameLength, hashes[PatternExeName].bytes);
        valid[PatternExeName]  = true;

        char* pExeNameLower = StringToLower(pExeName, exeNameLength + 1); // Add 1 for null terminator!

        if (pExeNameLower != nullptr)
        {
            Util::MetroHash128::Hash(
                reinterpret_cast<const uint8_t*>(pExeNameLower), exeNameLength, hashes[PatternExeNameLower].bytes);
            texts[PatternExeNameLower] = pExeNameLower;
            valid[PatternExeNameLower] = true;
        }

        free(pExeName);
    }

    // Go through every pattern until we find a matching app profile
    constexpr size_t PatternTableCount = sizeof(AppPatternTable) / sizeof(AppPatternTable[0]);

    for (size_t patIdx = 0; (patIdx < PatternTableCount) && (profile == AppProfile::Default); ++patIdx)
    {
        const AppProfilePattern& pattern = AppPatternTable[patIdx];

        // There must be at least one entry in each pattern
        VK_ASSERT(pattern.entries[0].type != PatternNone);

        // Test every entry in this pattern
        bool patternMatches = true;

        constexpr size_t MaxEntries = sizeof(pattern.entries) / sizeof(pattern.entries[0]);

        for (size_t entryIdx = 0;
            patternMatches && (entryIdx < MaxEntries) && (pattern.entries[entryIdx].type != PatternNone);
            entryIdx++)
        {
            const AppProfilePatternEntry& entry = pattern.entries[entryIdx];

            // If there is a hash/text for this pattern type available and it matches the tested hash/text, then
            // keep going.  Otherwise, this pattern doesn't match.
            if ((valid[entry.type] == false) ||
                (entry.hashed &&
                 ((hashes[entry.type].dwords[0] != entry.hash.dwords[0]) ||
                  (hashes[entry.type].dwords[1] != entry.hash.dwords[1]) ||
                  (hashes[entry.type].dwords[2] != entry.hash.dwords[2]) ||
                  (hashes[entry.type].dwords[3] != entry.hash.dwords[3]))) ||
                ((!entry.hashed) &&
                 (strcmp(texts[entry.type], entry.text) != 0)))
            {
                patternMatches = false;
            }

            // If specific string is found in the exe name, pattern matches
            if ((pattern.entries[entryIdx].type == PatternStrInExeNameLower) &&
                (patternMatches == false) &&
                (valid[PatternExeNameLower] == true) &&
                (strstr(texts[PatternExeNameLower], entry.text) != nullptr))
            {
                patternMatches = true;
            }
        }

        if (patternMatches)
        {
            profile = pattern.profile;
        }
    }

    // Clean up memory used for text strings
    for (int i = 0; i < PatternCount; i++)
    {
        if (valid[i])
        {
            free(texts[i]);
        }
    }

    return profile;
}

// =====================================================================================================================
// Returns the current process's executable file name without path or file extension.  This function allocates memory
// using malloc() and the caller needs to free it using free().
static char* GetExecutableName(
    size_t* pLength,
    bool    includeExtension)  // true if you want the extension on the file name.
{
    pid_t pid = getpid();
    char* pExecutable = nullptr;
    char* pModuleFileName = nullptr;
    char  path[PATH_MAX] = {0};
    char  commandStringBuffer[PATH_MAX] = {0};
    sprintf(commandStringBuffer, "cat /proc/%d/cmdline", pid);
    FILE* pCommand = popen(commandStringBuffer, "r");
    if (pCommand != nullptr)
    {
        if (fgets(path, PATH_MAX, pCommand) != nullptr)
        {
            pExecutable = static_cast<char*>(malloc(PATH_MAX));
            pModuleFileName = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
            pModuleFileName = strrchr(pModuleFileName, '\\') ? strrchr(pModuleFileName, '\\') + 1 : pModuleFileName;
            strcpy(pExecutable, pModuleFileName);
            *pLength = strlen(pExecutable);
        }
        pclose(pCommand);
    }
    return pExecutable;
}

// =====================================================================================================================
// Get a pointer to a profile data string
static const wchar_t* FindProfileData(
    const wchar_t* wcharData,
    bool           isUser3DAreaFormat,
    uint32_t       targetAppGpuID)
{
    const wchar_t* pDataPosition;
    bool           haveGoodData = false;

    if (isUser3DAreaFormat == true)
    {
        // The data in user 3D area is in format: "0x0200::2;;".
        // On MGPU systems, the data can look like this: "0x0300::2;;0x0400::2;;"
        const wchar_t* pStart      = wcharData;
        const wchar_t* pMiddle     = wcschr(wcharData, L':'); // search pattern "::" forward
        const wchar_t* pEnd        = wcschr(wcharData, L';'); // search pattern ";;" forward
        bool           ignoreGpuID = false;

        do
        {
            uint32_t appGpuID = 0u;

            if ((pStart != nullptr) &&
                (pMiddle != nullptr) &&
                (pStart < pMiddle))
            {
                appGpuID = wcstoul(pStart, NULL, 0);
            }

            if (((appGpuID == targetAppGpuID) || ignoreGpuID) &&
                (pMiddle != nullptr) &&
                (pEnd != nullptr) &&
                (*(pMiddle + 1) == L':') &&
                ((pMiddle + 2) < pEnd))
            {
                pMiddle += 2;  // Move past the first 2 ':' characters
                pDataPosition = pMiddle;

                haveGoodData = true;
            }

            if (pEnd != nullptr)
            {
                pStart = pEnd + 2;
                pEnd = wcschr(pStart, L';'); // search pattern ";;" forward
                pMiddle = wcschr(pStart, L':'); // search pattern "::" forward
            }

            if ((pEnd == nullptr) &&
                (!haveGoodData) &&
                (!ignoreGpuID) &&
                (wcharData != nullptr))
            {
                // We could not find our target GPU ID, so we will use the data for
                // the GPU that was listed first
                pStart      = wcharData; // point to beginning
                pMiddle     = wcschr(wcharData, L':'); // search pattern "::" forward from beginning
                pEnd        = wcschr(wcharData, L';'); // search pattern ";;" forward from beginning
                ignoreGpuID = true;
            }

         } while ((pEnd != nullptr) &&
                  (!haveGoodData));
    }

    if (haveGoodData == false)
    {
        pDataPosition = wcharData;
    }

    return pDataPosition;
}

// =====================================================================================================================
static uint32_t ParseProfileDataToUint32(
    const wchar_t* wcharData,
    bool           isUser3DAreaFormat,
    uint32_t       targetAppGpuID)
{
    auto pData = FindProfileData(wcharData, isUser3DAreaFormat, targetAppGpuID);

    return wcstoul(pData, NULL, 0);
}

// =====================================================================================================================
static float ParseProfileDataToFloat(
    const wchar_t* wcharData,
    bool           isUser3DAreaFormat,
    uint32_t       targetAppGpuID)
{
    auto pData = FindProfileData(wcharData, isUser3DAreaFormat, targetAppGpuID);

    return wcstof(pData, NULL);
}

// =====================================================================================================================
// Process profile token
void ProcessProfileEntry(
    const PhysicalDevice*   pPhysicalDevice,
    const char*             entryName,
    uint32_t                dataSize,
    const void*             data,
    ProfileSettings*        pProfileSettings,
    uint32_t                appGpuID,
    bool                    isUser3DAreaFormat)
{
    // Skip if the data is empty
    if (dataSize != 0)
    {
        const wchar_t* wcharData      = reinterpret_cast<const wchar_t *>(data);
        bool*          pBoolSetting   = nullptr;
        uint32_t*      pUint32Setting = nullptr;
        float*         pFloatSetting  = nullptr;
        bool           doNotSetOnZero = false;

        if (strcmp(entryName, "TFQ") == 0)
        {
            pUint32Setting = &(pProfileSettings->texFilterQuality);
        }

#if VKI_RAY_TRACING
#endif

        if (pBoolSetting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat, appGpuID);
            *pBoolSetting = dataValue ? true : false;
        }
        else if (pFloatSetting != nullptr)
        {
            *pFloatSetting = ParseProfileDataToFloat(wcharData, isUser3DAreaFormat, appGpuID);
        }
        else if (pUint32Setting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat, appGpuID);
            if ((doNotSetOnZero == false) ||
                (dataValue != 0))
            {
                *pUint32Setting = dataValue;
            }
        }
    }
}

// =====================================================================================================================
// Queries PAL for app profile settings
// exeOrCdnName is something like "doom.exe" or "SteamAppId:570"
// Return true if a profile is present.
static bool QueryPalProfile(
    const PhysicalDevice*         pPhysicalDevice,
    Instance*                     pInstance,
    ProfileSettings*              pProfileSettings,
    uint32_t                      appGpuID,
    Pal::ApplicationProfileClient client,
    const wchar_t*                exeOrCdnName) // This is the game EXE name or Content Distribution Network name.
{
    bool        profilePresent = false;
    const char* rawProfile;

    Pal::Result result =
        pInstance->PalPlatform()->QueryRawApplicationProfile(exeOrCdnName, nullptr, client, &rawProfile);

    if (result == Pal::Result::Success)
    {
        profilePresent = true;
        bool isUser3DAreaFormat = (client == Pal::ApplicationProfileClient::User3D);
        GpuUtil::AppProfileIterator iterator(rawProfile);
        while (iterator.IsValid())
        {
            ProcessProfileEntry(pPhysicalDevice,
                                iterator.GetName(),
                                iterator.GetDataSize(),
                                iterator.GetData(),
                                pProfileSettings,
                                appGpuID,
                                isUser3DAreaFormat);
            iterator.Next();
        }
    }

    return profilePresent;
}
// =====================================================================================================================
static bool QueryPalProfile(
    const PhysicalDevice*         pPhysicalDevice,
    Instance*                     pInstance,
    ProfileSettings*              pProfileSettings,
    uint32_t                      appGpuID,
    Pal::ApplicationProfileClient client,
    const char*                   exeOrCdnName) // This is the game EXE name or Content Distribution Network name
{
    wchar_t exeName[Util::MaxFileNameStrLen];
    VK_ASSERT(strlen(exeOrCdnName) < Util::MaxFileNameStrLen);
    Mbstowcs(exeName, exeOrCdnName, Util::MaxFileNameStrLen);

    return QueryPalProfile(pPhysicalDevice,
                           pInstance,
                           pProfileSettings,
                           appGpuID,
                           client,
                           exeName);
}

// =====================================================================================================================
// Queries PAL for app profile settings
void ReloadAppProfileSettings(
    const PhysicalDevice*   pPhysicalDevice,
    Instance*               pInstance,
    ProfileSettings*        pProfileSettings,
    uint32_t                appGpuID)
{
    size_t exeNameLength = 0;
    char* pExeName = GetExecutableName(&exeNameLength, true);
    char* pExeNameLower = nullptr;

    if (pExeName != nullptr)
    {
        // Add 1 for null terminator!
        exeNameLength += 1;
        pExeNameLower = StringToLower(pExeName, exeNameLength);
        free(pExeName);
    }

    if (pExeNameLower != nullptr)
    {

        bool foundProfile = false;
        // User 3D has highest priority, so query it first
        foundProfile = QueryPalProfile(pPhysicalDevice,
                                       pInstance,
                                       pProfileSettings,
                                       appGpuID,
                                       Pal::ApplicationProfileClient::User3D,
                                       pExeNameLower);

        if (foundProfile == false)
        {
            // Try to query the 3D user area again with the Content Distrubution Network (CDN) App ID.
            // TODO: This funciton isn't called very often (once at app start and when CCC settings change),
            //       but we may want to cache the CDN string rather than regenerate it every call.
            constexpr uint32_t CdnBufferSize = 150;
            wchar_t cdnApplicationId[CdnBufferSize];
            bool hasValidCdnName = GpuUtil::QueryAppContentDistributionId(cdnApplicationId, CdnBufferSize);

            if (hasValidCdnName == true)
            {
                foundProfile = QueryPalProfile(pPhysicalDevice,
                                               pInstance,
                                               pProfileSettings,
                                               appGpuID,
                                               Pal::ApplicationProfileClient::User3D,
                                               cdnApplicationId);
            }
        }

        free(pExeNameLower);
    }
}

};
