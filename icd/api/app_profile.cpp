/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palMd5.h"

#include <cctype>
#include <memory>

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

// This is a pattern entry.  It is a pair of type and test MD5 hash.  The string of the given type
// is hashed and compared against the MD5 value.  If the values are equal, this entry matches.
struct AppProfilePatternEntry
{
    AppProfilePatternType type;  // Type of pattern to match against
    uint32_t              hash0; // Words 0-3 of MD5 hash to compare against
    uint32_t              hash1;
    uint32_t              hash2;
    uint32_t              hash3;
};

// This is a pattern that maps to a profile.  It is a list of entries to compare against.  If all entries
// match, the given profile is assigned to this process.
struct AppProfilePattern
{
    AppProfile             profile;
    AppProfilePatternEntry entries[16];
};

constexpr AppProfilePatternEntry AppEngineSource2 =
{
    // EngineName = "source2"
    PatternEngineNameLower,
    0x9654e927,
    0xf86feb28,
    0xb5ec39b8,
    0x7644d6ec
};

constexpr AppProfilePatternEntry AppNameDota2 =
{
    PatternAppNameLower,
    0xcd456249,
    0xed5e0ab3,
    0x0680516b,
    0x8ae11f31
};

constexpr AppProfilePatternEntry AppExeTalos =
{
    PatternExeNameLower,
    0xfde3cef2,
    0x43dbaa74,
    0xf29613c9,
    0xe8f3bc06
};

constexpr AppProfilePatternEntry AppEngineFeral3D =
{
    // EngineName = "Feral3D"
    PatternEngineNameLower,
    0x0915d6eb,
    0x7a8d18b2,
    0x60ec1e51,
    0x4461153a
};

constexpr AppProfilePatternEntry AppNameMadMax =
{
    PatternAppNameLower,
    0xec2d5750,
    0x2224b771,
    0x48dbb724,
    0xf5e66887
};

constexpr AppProfilePatternEntry AppNameF1_2017 =
{
    PatternAppNameLower,
    0xf1d377d6,
    0x2a8ea2e3,
    0xdcea047e,
    0x717bc02a
};

constexpr AppProfilePatternEntry AppNameSeriousSamFusion =
{
    PatternAppNameLower,
    0xffa9ae54,
    0xb8d3d366,
    0x1075c71f,
    0x8a1da3ca
};

constexpr AppProfilePatternEntry AppEngineSedp =
{
    // EngineName = "sedp class"
    PatternEngineNameLower,
    0x8f84c013,
    0x76c6d473,
    0x876ff29b,
    0xbc7337eb
};

constexpr AppProfilePatternEntry PatternEnd = {};

// This is a table of patterns.  The first matching pattern in this table will be returned.
AppProfilePattern AppPatternTable[] =
{
    {
        AppProfile::Dota2,
        {
            AppNameDota2,
            AppEngineSource2,
            PatternEnd
        }
    },
    {
        AppProfile::Talos,
        {
            AppExeTalos,
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
        AppProfile::SeriousSamFusion,
        {
            AppNameSeriousSamFusion,
            AppEngineSedp,
            PatternEnd
        }
    },

};

static char* GetExecutableName(size_t* pLength, bool includeExtension = false);
static bool QueryCdnApplicationId(char* pAppIdString, size_t bufferLength);

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
// Returns true if the given string's MD5 hash matches the given entry's MD5 hash.
static bool StringHashMatches(
    const char*                   pString,
    size_t                        strSize,
    const AppProfilePatternEntry& entry)
{
    // Generate MD5 from app hash
    Util::Md5::Hash hash = Util::Md5::GenerateHashFromBuffer(pString, strSize);

    return (hash.hashValue[0] == entry.hash0 &&
            hash.hashValue[1] == entry.hash1 &&
            hash.hashValue[2] == entry.hash2 &&
            hash.hashValue[3] == entry.hash3);
}

// =====================================================================================================================
// Goes through all patterns and returns an application profile that matches the first matched pattern.  Patterns
// compare things like VkApplicationInfo values or executable names, etc.  This profile may further be overridden
// by private panel settings.
AppProfile ScanApplicationProfile(
    const VkInstanceCreateInfo& instanceInfo)
{
    // You can uncomment these if you need to add new MD5 hashes for specific strings (which is
    // hopefully never).  DON'T LEAVE THIS UNCOMMENTED:
    //
    // Util::Md5::Hash testHash = Util::Md5::GenerateHashFromBuffer(pTestPattern, strlen(pTestPattern));

    AppProfile profile = AppProfile::Default;

    // NOTE: This function is deliberately using malloc/free directly, rather than application callbacks, for
    // private memory so that the app can't (at least easily) detect we are messing around with the app name
    // strings, etc.

    // Generate hashes for all of the tested pattern entries
    Util::Md5::Hash hashes[PatternCount] = {};
    bool valid[PatternCount] = {};

    if (instanceInfo.pApplicationInfo != nullptr)
    {
        if (instanceInfo.pApplicationInfo->pApplicationName != nullptr)
        {
            const char* pAppName = instanceInfo.pApplicationInfo->pApplicationName;
            size_t appNameLength = strlen(pAppName);

            hashes[PatternAppName] = Util::Md5::GenerateHashFromBuffer(pAppName, appNameLength);
            valid[PatternAppName] = true;

            char* pAppNameLower = StringToLower(pAppName, appNameLength);

            if (pAppNameLower != nullptr)
            {
                hashes[PatternAppNameLower] = Util::Md5::GenerateHashFromBuffer(pAppNameLower, appNameLength);
                valid[PatternAppNameLower]  = true;

                free(pAppNameLower);
            }
        }

        if (instanceInfo.pApplicationInfo->pEngineName != nullptr)
        {
            const char* pEngineName = instanceInfo.pApplicationInfo->pEngineName;
            size_t engineNameLength = strlen(pEngineName);

            hashes[PatternEngineName] = Util::Md5::GenerateHashFromBuffer(pEngineName, engineNameLength);
            valid[PatternEngineName] = true;

            char* pEngineNameLower = StringToLower(pEngineName, engineNameLength);

            if (pEngineNameLower != nullptr)
            {
                hashes[PatternEngineNameLower] = Util::Md5::GenerateHashFromBuffer(pEngineNameLower, engineNameLength);
                valid[PatternEngineNameLower]  = true;

                free(pEngineNameLower);
            }
        }
    }

    size_t exeNameLength = 0;
    char* pExeName = GetExecutableName(&exeNameLength);

    if (pExeName != nullptr)
    {
        hashes[PatternExeName] = Util::Md5::GenerateHashFromBuffer(pExeName, exeNameLength);
        valid[PatternExeName]  = true;

        char* pExeNameLower = StringToLower(pExeName, exeNameLength);

        if (pExeNameLower != nullptr)
        {
            hashes[PatternExeNameLower] = Util::Md5::GenerateHashFromBuffer(pExeNameLower, exeNameLength);
            valid[PatternExeNameLower]  = true;

            free(pExeNameLower);
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

            // If there is a hash for this pattern type available and it matches the tested hash, then
            // keep going.  Otherwise, this pattern doesn't match.
            if ((valid[entry.type] == false) ||
                (hashes[entry.type].hashValue[0] != entry.hash0) ||
                (hashes[entry.type].hashValue[1] != entry.hash1) ||
                (hashes[entry.type].hashValue[2] != entry.hash2) ||
                (hashes[entry.type].hashValue[3] != entry.hash3))
            {
                patternMatches = false;
            }
        }

        if (patternMatches)
        {
            profile = pattern.profile;
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
    bool           isUser3DAreaFormat)
{
    uint32_t dataValue;
    bool haveGoodData = false;
    if (isUser3DAreaFormat == true)
    {
        // The data in user 3D area is in format: "0x0200::2;;".
        // On MGPU systems, the data can look like this: "0x0300::2;;0x0400::2;;"
        // We need to skip the first part which is GPU ID and take the first number between :: and ;;
        const wchar_t* pStart = wcschr(wcharData, L':'); // search pattern "::" forward
        const wchar_t* pEnd   = wcschr(wcharData, L';'); // search pattern ";;" forward
        if ((pStart != nullptr) &&
            (pEnd   != nullptr) &&
            (*(pStart + 1) == L':') &&
            ((pStart + 2) < pEnd))
        {
            constexpr uint32_t stringLen = 50;
            wchar_t pParsedVal[stringLen];
            pStart += 2;  // Move past the first 2 ':' characters
            VK_ASSERT(pStart < pEnd); //This should never happen due to above checks.
            uint32_t size = static_cast<uint32_t>(pEnd - pStart);
            VK_ASSERT(size <= stringLen); // If this happens we will truncate data but not crash.
            size = Util::Min(size, stringLen);

            wcsncpy(pParsedVal, pStart, size);
            pParsedVal[size] = '\0';

            dataValue = wcstoul(pParsedVal, NULL, 0);
            haveGoodData = true;
        }
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
    const char*       entryName,
    uint32_t          dataSize,
    const void*       data,
    RuntimeSettings*  pRuntimeSettings,
    ChillSettings*    pChillSettings,
    bool              isUser3DAreaFormat)
{
    // Skip if the data is empty
    if (dataSize != 0)
    {
        const wchar_t* wcharData = reinterpret_cast<const wchar_t *>(data);
        bool*     pBoolSetting   = nullptr;
        uint32_t* pUint32Setting = nullptr;
        bool assertOnZero   = false;
        bool doNotSetOnZero = false;

        if (pBoolSetting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat);
            *pBoolSetting = dataValue ? true : false;
        }
        else if (pUint32Setting != nullptr)
        {
            uint32_t dataValue = ParseProfileDataToUint32(wcharData, isUser3DAreaFormat);
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
                                isUser3DAreaFormat);
            iterator.Next();
        }
    }

    return profilePresent;
}

// =====================================================================================================================
// Queries PAL for app profile settings
bool ReloadAppProfileSettings(
    Instance*        pInstance,
    RuntimeSettings* pRuntimeSettings,
    ChillSettings*   pChillSettings)
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
                                               Pal::ApplicationProfileClient::User3D,
                                               cdnApplicationId);
            }
        }

        if (foundProfile == false)
        {
            foundProfile = QueryPalProfile(pInstance,
                                           pRuntimeSettings,
                                           pChillSettings,
                                           Pal::ApplicationProfileClient::Chill, //CHILL area
                                           pExeNameLower);
        }

        free(pExeNameLower);
    }

    return true;
}

};
