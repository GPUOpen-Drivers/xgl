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

#include <unistd.h>
#include <linux/limits.h>

#include "include/vk_instance.h"
#include "include/vk_device.h"
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

constexpr AppProfilePatternEntry AppEngineSource2 =
{
    PatternEngineNameLower,
    "source2"
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
        AppProfile::Source2Engine,
        {
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
        AppProfile::Feral3DEngine,
        {
            AppEngineFeral3D,
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
    // You can uncomment these if you need to add new hashes for specific strings (which is
    // hopefully never).  DON'T LEAVE THIS UNCOMMENTED:
    //
    // Util::MetroHash::Hash hash = {};
    // Util::MetroHash128::Hash(pTestPattern, strlen(pTestPattern), hash.bytes);

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
    char* pExecutable = nullptr;
    char* pModuleFileName = nullptr;
    char  path[PATH_MAX] = {0};
    pExecutable = static_cast<char*>(malloc(PATH_MAX));
    int lens = readlink("/proc/self/exe", path, PATH_MAX);
    pModuleFileName = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    strcpy(pExecutable, pModuleFileName);
    *pLength = strlen(pExecutable);

    return pExecutable;
}

// =====================================================================================================================
// Parse profile data to uint32_t
static uint32_t ParseProfileDataToUint32(
    const wchar_t* wcharData,
    bool           isUser3DAreaFormat,
    const uint32_t targetAppGpuID = 0u)
{
    uint32_t dataValue;
    bool     haveGoodData = false;

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
                appGpuID = wcstoul(pStart, NULL, 0);;
            }

            if (((appGpuID == targetAppGpuID) || ignoreGpuID) &&
                (pMiddle != nullptr) &&
                (pEnd != nullptr) &&
                (*(pMiddle + 1) == L':') &&
                ((pMiddle + 2) < pEnd))
            {
                pMiddle += 2;  // Move past the first 2 ':' characters
                dataValue = wcstoul(pMiddle, NULL, 0);

                haveGoodData = true;
            }

            pStart  = pEnd + 2;
            pEnd    = wcschr(pStart, L';'); // search pattern ";;" forward
            pMiddle = wcschr(pStart, L':'); // search pattern "::" forward

            if ((pEnd == nullptr) &&
                (!haveGoodData) &&
                (!ignoreGpuID))
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
        dataValue = wcstoul(wcharData, NULL, 0);
    }

    return dataValue;
}

// =====================================================================================================================
// Process profile token
void ProcessProfileEntry(
    const char*        entryName,
    uint32_t           dataSize,
    const void*        data,
    RuntimeSettings*   pRuntimeSettings,
    ChillSettings*     pChillSettings,
    TurboSyncSettings* pTurboSyncSettings,
    bool               isUser3DAreaFormat)
{
    // Skip if the data is empty
    if (dataSize != 0)
    {
        const wchar_t* wcharData      = reinterpret_cast<const wchar_t *>(data);
        bool*          pBoolSetting   = nullptr;
        uint32_t*      pUint32Setting = nullptr;
        bool           assertOnZero   = false;
        bool           doNotSetOnZero = false;
        uint32_t       appGpuID       = 0u;

        if (pRuntimeSettings != nullptr)
        {
            appGpuID = pRuntimeSettings->appGpuID;

            if (strcmp(entryName, "TFQ") == 0 && (pRuntimeSettings != nullptr))
            {
                pUint32Setting = reinterpret_cast<uint32_t*>(&(pRuntimeSettings->vulkanTexFilterQuality));
            }
        }

        if (pBoolSetting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat, appGpuID);
            *pBoolSetting = dataValue ? true : false;
        }
        else if (pUint32Setting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat, appGpuID);
#if DEBUG
            if (assertOnZero)
            {
                VK_ASSERT(dataValue != 0);
            }
#endif
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
    Instance*                     pInstance,
    RuntimeSettings*              pRuntimeSettings,
    ChillSettings*                pChillSettings,
    TurboSyncSettings*            pTurboSyncSettings,
    Pal::ApplicationProfileClient client,
    char*                         exeOrCdnName) // This is the game EXE name or Content Distribution Network name.
{
    bool profilePresent = false;
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
            ProcessProfileEntry(iterator.GetName(),
                                iterator.GetDataSize(),
                                iterator.GetData(),
                                pRuntimeSettings,
                                pChillSettings,
                                pTurboSyncSettings,
                                isUser3DAreaFormat);
            iterator.Next();
        }
    }

    return profilePresent;
}

// =====================================================================================================================
// Queries PAL for app profile settings
bool ReloadAppProfileSettings(
    Instance*          pInstance,
    RuntimeSettings*   pRuntimeSettings,
    ChillSettings*     pChillSettings,
    TurboSyncSettings* pTurboSyncSettings)
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
        foundProfile = QueryPalProfile(pInstance,
                                       pRuntimeSettings,
                                       pChillSettings,
                                       pTurboSyncSettings,
                                       Pal::ApplicationProfileClient::User3D,
                                       pExeNameLower);

        if (foundProfile == false)
        {
            // Try to query the 3D user area again with the Content Distrubution Network (CDN) App ID.
            // TODO: This funciton isn't called very often (once at app start and when CCC settings change),
            //       but we may want to cache the CDN string rather than regenerate it every call.
            constexpr uint32_t CdnBufferSize = 150;
            char cdnApplicationId[CdnBufferSize];
            bool hasValidCdnName = GpuUtil::QueryAppContentDistributionId(cdnApplicationId, CdnBufferSize);

            if (hasValidCdnName == true)
            {
                foundProfile = QueryPalProfile(pInstance,
                                               pRuntimeSettings,
                                               pChillSettings,
                                               pTurboSyncSettings,
                                               Pal::ApplicationProfileClient::User3D,
                                               cdnApplicationId);
            }
        }

        if (foundProfile == false)
        {
            foundProfile = QueryPalProfile(pInstance,
                                           pRuntimeSettings,
                                           pChillSettings,
                                           pTurboSyncSettings,
                                           Pal::ApplicationProfileClient::Chill, //CHILL area
                                           pExeNameLower);
        }

        free(pExeNameLower);
    }

    return true;
}

};
