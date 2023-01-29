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
* @file  app_resource_optimizer.h
* @brief Functions for tuning options pertaining to images.
**************************************************************************************************
*/

#ifndef __APP_RESOURCE_OPTIMIZER_H__
#define __APP_RESOURCE_OPTIMIZER_H__
#pragma once

#include "palImage.h"
#include "palDevice.h"

// Forward declare Util classes used in this file
namespace Util
{
class MetroHash128;
};

// Forward declare PAL classes used in this file
namespace Pal
{
struct ImageCreateInfo;
};

// Forward declare Vulkan classes used in this file
namespace vk
{
class Device;
class Instance;
class PhysicalDevice;
struct RuntimeSettings;
};

namespace vk
{

struct ResourceOptimizerKey
{
    union
    {
        struct
        {
            uint32_t height; // mip-0 height
            uint32_t width;  // mip-0 width
        };
        uint64_t dimensions; // merged width and height
    };

    uint64_t apiHash;      // Hash of the *CreateInfo struct
    uint64_t apiHashBegin; // Begin hash for apiHashRange matching
    uint64_t apiHashEnd;   // End hash for apiHashRange matching
};

inline bool ResourceKeysEqual(
    const ResourceOptimizerKey& lhs,
    const ResourceOptimizerKey& rhs)
    { return (lhs.dimensions == rhs.dimensions) && (lhs.apiHash == rhs.apiHash); }

inline bool ResourceKeysNotEqual(
    const ResourceOptimizerKey& lhs,
    const ResourceOptimizerKey& rhs)
    { return (lhs.dimensions != rhs.dimensions) || (lhs.apiHash != rhs.apiHash); }

struct ResourceProfilePattern
{
    // Defines which pattern tests are enabled
    union
    {
        struct
        {
            uint32_t always           : 1; // Pattern always hits
            uint32_t apiHash          : 1; // Test API Hash
            uint32_t apiHashRange     : 1; // Test API Hash Range
            uint32_t dimensions       : 1; // Test dimensions
            uint32_t reserved         : 28;
        };
        uint32_t u32All;
    } match;

    ResourceOptimizerKey targetKey;
};

struct ResourceProfileAction
{
    struct
    {
        // Defines which values are applied
        union
        {
            struct
            {
                uint32_t dccMode      : 1;  // Only valid for image resources
                uint32_t mallNoAlloc  : 1;
                uint32_t reserved     : 30;
            };
            uint32_t u32All;
        } apply;

        DccMode dccMode;
    } resourceCreate;
};

// This struct describes a single entry in a per-application profile of resource parameter tweaks.
struct ResourceProfileEntry
{
    ResourceProfilePattern pattern;
    ResourceProfileAction  action;
};

constexpr uint32_t MaxResourceProfileEntries = 256;

// Describes a collection of entries that can be used to apply application-specific resource tuning
// to different resources.
struct ResourceProfile
{
    uint32_t             entryCount;
    ResourceProfileEntry entries[MaxResourceProfileEntries];
};

// =====================================================================================================================
// This class can tune image and buffer parameters for optimal performance.
//
// These tuning values can be workload specific and have to be tuned on a per-application basis.
class ResourceOptimizer
{
public:
    ResourceOptimizer(
        Device*         pDevice,
        PhysicalDevice* pPhysicalDevice);
    ~ResourceOptimizer();

    void Init();

    void OverrideImageCreateInfo(
        const ResourceOptimizerKey&  resourceKey,
        Pal::ImageCreateInfo*        pPalCreateInfo);

    void OverrideImageViewCreateInfo(
        const ResourceOptimizerKey&  resourceKey,
        Pal::ImageViewInfo*          pPalViewInfo) const;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ResourceOptimizer);

    void ApplyProfileToImageCreateInfo(
        const ResourceProfile&           profile,
        const ResourceOptimizerKey&      resourceKey,
        Pal::ImageCreateInfo*            pCreateInfo);

    void ApplyProfileToImageViewCreateInfo(
        const ResourceProfile&           profile,
        const ResourceOptimizerKey&      resourceKey,
        Pal::ImageViewInfo*              pViewInfo) const;

    bool ResourcePatternMatchesResource(
        const ResourceProfilePattern&   pattern,
        const ResourceOptimizerKey&     resourceKey) const;

    void BuildTuningProfile();
    void BuildAppProfile();

#if ICD_RUNTIME_APP_PROFILE
    void BuildRuntimeProfile();
#endif

    Device*                m_pDevice;
    const RuntimeSettings& m_settings;

    ResourceProfile        m_tuningProfile;
    ResourceProfile        m_appProfile;

#if ICD_RUNTIME_APP_PROFILE
    ResourceProfile        m_runtimeProfile;
#endif

    Pal::MetadataMode dccModeToMetadataMode[static_cast<uint32_t>(Pal::MetadataMode::Count)];
};

};
#endif /* __APP_RESOURCE_OPTIMIZER_H__ */
