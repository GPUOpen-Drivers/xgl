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
 * @file  barrier_policy.h
 * @brief Handles the policy used for mapping barrier flags
 **************************************************************************************************
 */

#ifndef __BARRIER_POLICY_H__
#define __BARRIER_POLICY_H__

#include "include/khronos/vulkan.h"
#include "include/vk_physical_device.h"
#include "include/vk_extensions.h"

#include "palCmdBuffer.h"

#pragma once

namespace vk
{

class Device;

// =====================================================================================================================
// Barrier policy base class.
// Concrete barrier policy classes are derived from this class.
class BarrierPolicy
{
public:
    void ApplyBarrierCacheFlags(
        VkAccessFlags                       srcAccess,
        VkAccessFlags                       dstAccess,
        Pal::BarrierTransition*             pResult) const;

    VK_FORCEINLINE uint32_t GetSupportedOutputCacheMask() const
        { return m_supportedOutputCacheMask; }

    VK_FORCEINLINE uint32_t GetSupportedInputCacheMask() const
        { return m_supportedInputCacheMask; }

protected:
    BarrierPolicy()
    {}

    void InitCachePolicy(
        PhysicalDevice*                     pPhysicalDevice,
        uint32_t                            supportedOutputCacheMask,
        uint32_t                            supportedInputCacheMask);

private:
    uint32_t    m_supportedOutputCacheMask;         // Mask including all output caches that are supported in the
                                                    // barrier policy's scope.
    uint32_t    m_supportedInputCacheMask;          // Mask including all input caches that are supported in the
                                                    // barrier policy's scope.

    uint32_t    m_keepCoherMask;                    // Mask including caches that are always kept coherent.
    uint32_t    m_avoidCoherMask;                   // Mask including caches that are avoided to be kept coherent
                                                    // unless explicitly requested.

    uint32_t    m_alwaysFlushMask;                  // Mask including caches that should always be flushed.
                                                    // It always includes m_keepCoherMask.
                                                    // It never includes m_avoidCoherMask.
                                                    // It contains all other coherency flags if PreferFlushOverInv is
                                                    // set and CombinedAccessMasks is not set, otherwise it equals
                                                    // m_keepCoherMask as those domains are always kept coherent.
    uint32_t    m_alwaysInvMask;                    // Mask including caches that should always be invalidated.
                                                    // It always includes m_keepCoherMask.
                                                    // It never includes m_avoidCoherMask.
                                                    // It contains all other coherency flags if PreferFlushOverInv and
                                                    // CombinedAccessMasks are neither set, otherwise it equals
                                                    // m_keepCoherMask as those domains are always kept coherent.

    union
    {
        struct
        {
            uint32_t combinedAccessMasks    :  1;   // Indicates to ignore the Vulkan separate access mask rule which
                                                    // require us to always flush or invalidate input/output
                                                    // caches, even if they are not specified.
            uint32_t skipDstCacheInv        :  1;   // Indicates to not invalidate input caches if output cache
                                                    // mask is empty.
            uint32_t preferFlushOverInv     :  1;   // By default we invalidate input caches to accomodate the Vulkan
                                                    // separate access mask rule. When this is set we'll instead
                                                    // flush all output caches instead to achieve the same goal.
                                                    // Mutually exclusive with SkipDstCacheInv.
                                                    // May or may not be beneficial for certain applications.
            uint32_t keepShaderCoher        :  1;   // Keep shader domain always coherent thus avoiding L2 cache
                                                    // flushes/invalidations in shader-to-shader barrier cases at the
                                                    // expense of always flushing/invalidating L1 caches.
                                                    // This does NOT violate the Vulkan separate access mask rule.
                                                    // This behavior is likely preferred on GFX6-GFX8 but may not be
                                                    // beneficial on GFX9+.
            uint32_t avoidCpuMemoryCoher    :  1;   // Avoid CPU and memory domain coherency unless corresponding
                                                    // flags are explicitly requested to lower the number of L2 cache
                                                    // flushes/invalidations.
                                                    // This does NOT violate the Vulkan separate access mask rule.
                                                    // May or may not be beneficial on GFX6-GFX8 but should be
                                                    // preferred on GFX9+ as all other accesses go through the L2.
            uint32_t reserved               : 27;   // Reserved for future use.
        };
        uint32_t        u32All;
    } m_flags;
};

// =====================================================================================================================
// Device barrier policy class.
// Limits the scope of barriers to those applicable to this device.
// Used to control the policy for global memory barriers.
class DeviceBarrierPolicy : public BarrierPolicy
{
public:
    DeviceBarrierPolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const VkDeviceCreateInfo*           pCreateInfo,
        const DeviceExtensions::Enabled&    enabledExtensions);

    VK_FORCEINLINE uint32_t GetSupportedLayoutEngineMask() const
        { return m_supportedLayoutEngineMask; }

protected:
    void InitDeviceLayoutEnginePolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const VkDeviceCreateInfo*           pCreateInfo,
        const DeviceExtensions::Enabled&    enabledExtensions);

    void InitDeviceCachePolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const DeviceExtensions::Enabled&    enabledExtensions);

    uint32_t    m_supportedLayoutEngineMask;        // Mask including all supported image layout engine flags.
};

// =====================================================================================================================
// Image barrier policy class.
// Limits the scope of barriers to those applicable to this particular image.
// Used to control the policy for image memory barriers.
class ImageBarrierPolicy : public BarrierPolicy
{
public:
    ImageBarrierPolicy(
        Device*                             pDevice,
        VkImageUsageFlags                   usage,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices,
        bool                                multisampled,
        uint32_t                            extraLayoutUsages = 0);

    VK_FORCEINLINE uint32_t GetSupportedLayoutUsageMask() const
        { return m_supportedLayoutUsageMask; }

    Pal::ImageLayout GetTransferLayout(
        const Device*                       pDevice,
        VkImageLayout                       layout,
        uint32_t                            queueFamilyIndex) const;

    Pal::ImageLayout GetAspectLayout(
        const Device*                       pDevice,
        VkImageLayout                       layout,
        uint32_t                            aspectIndex,
        uint32_t                            queueFamilyIndex) const;

    void GetLayouts(
        const Device*                       pDevice,
        VkImageLayout                       layout,
        uint32_t                            queueFamilyIndex,
        Pal::ImageLayout                    results[MaxPalDepthAspectsPerMask]) const;

protected:
    void InitImageLayoutUsagePolicy(
        Device*                             pDevice,
        VkImageUsageFlags                   usage,
        bool                                multisampled,
        uint32_t                            extraLayoutUsages);

    void InitImageLayoutEnginePolicy(
        Device*                             pDevice,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

    void InitImageCachePolicy(
        Device*                             pDevice,
        VkImageUsageFlags                   usage);

    uint32_t GetQueueFamilyLayoutEngineMask(
        const Device*                       pDevice,
        uint32_t                            queueFamilyIndex) const;

    uint32_t    m_supportedLayoutUsageMask;         // Mask including all supported layout usage flags for the image.
    uint32_t    m_supportedLayoutEngineMask;        // Mask including all supported layout engine flags for the image.
    uint32_t    m_alwaysSetLayoutEngineMask;        // Mask including layout engine flags that should be always set.
                                                    // This contains all engines in the scope of concurrent sharing
                                                    // mode to allow concurrent well-defined access to the image.
};

// =====================================================================================================================
// Buffer barrier policy class.
// Limits the scope of barriers to those applicable to this particular buffer.
// Used to control the policy for buffer memory barriers.
class BufferBarrierPolicy : public BarrierPolicy
{
public:
    BufferBarrierPolicy(
        Device*                             pDevice,
        VkBufferUsageFlags                  usage);

protected:
    void InitBufferCachePolicy(
        Device*                             pDevice,
        VkBufferUsageFlags                  usage);
};

} //namespace vk

#endif /* __BARRIER_POLICY_H__ */
