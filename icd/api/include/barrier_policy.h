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
 * @file  barrier_policy.h
 * @brief Handles the policy used for mapping barrier flags
 **************************************************************************************************
 */

#ifndef __BARRIER_POLICY_H__
#define __BARRIER_POLICY_H__

#include "include/khronos/vulkan.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"
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
        AccessFlags                         srcAccess,
        AccessFlags                         dstAccess,
        VkImageLayout                       srcLayout,
        VkImageLayout                       dstLayout,
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
    PAL_DISALLOW_COPY_AND_ASSIGN(BarrierPolicy);

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
// Ownership transfer priority.
enum class OwnershipTransferPriority : uint32_t
{
    None        = 0,
    Low         = 1,
    Medium      = 2,
    High        = 3,
};

// =====================================================================================================================
// Queue family barrier policy structure.
// Helps limiting the scope of barriers to those applicable to a particular queue family.
struct QueueFamilyBarrierPolicy
{
    uint32_t    palLayoutEngineMask;                // PAL layout engine mask corresponding to the queue family.
    uint32_t    supportedCacheMask;                 // Mask including all caches that are supported in the queue
                                                    // family's scope.
    uint32_t    supportedLayoutUsageMask;           // Mask including all supported image layout usage flags in the
                                                    // queue family's scope.
    OwnershipTransferPriority ownershipTransferPriority;    // Priority this queue family has in performing ownership
                                                            // transfers.
};

// =====================================================================================================================
// Device barrier policy class.
// Limits the scope of barriers to those applicable to this device.
// Used to control the policy for global memory barriers.
class DeviceBarrierPolicy final : public BarrierPolicy
{
public:
    DeviceBarrierPolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const VkDeviceCreateInfo*           pCreateInfo,
        const DeviceExtensions::Enabled&    enabledExtensions);

    VK_FORCEINLINE uint32_t GetSupportedLayoutEngineMask() const
        { return m_supportedLayoutEngineMask; }

    VK_FORCEINLINE const QueueFamilyBarrierPolicy& GetQueueFamilyPolicy(
        uint32_t                            queueFamilyIndex) const
    {
        if ((queueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL) || (queueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT))
        {
            return m_externalQueueFamilyPolicy;
        }
        else
        {
            VK_ASSERT(queueFamilyIndex < Queue::MaxQueueFamilies);
            return m_queueFamilyPolicy[queueFamilyIndex];
        }
    }

protected:
    void InitDeviceLayoutEnginePolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const VkDeviceCreateInfo*           pCreateInfo,
        const DeviceExtensions::Enabled&    enabledExtensions);

    void InitDeviceCachePolicy(
        PhysicalDevice*                     pPhysicalDevice,
        const DeviceExtensions::Enabled&    enabledExtensions);

    void InitQueueFamilyPolicy(
        QueueFamilyBarrierPolicy*   pPolicy,
        uint32_t                    palLayoutEngineMask,
        Pal::QueueType              queueType);

    uint32_t    m_supportedLayoutEngineMask;        // Mask including all supported image layout engine flags.
    uint32_t    m_allowedConcurrentCacheMask;       // Mask including all caches that can be affected by operations
                                                    // outside of the current queue (other queues or host).
    QueueFamilyBarrierPolicy m_queueFamilyPolicy[Queue::MaxQueueFamilies];  // Per queue family policy info.
    QueueFamilyBarrierPolicy m_externalQueueFamilyPolicy;   // Policy for external/foreign queue families.

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DeviceBarrierPolicy);
};

// =====================================================================================================================
// Resource barrier policy class.
// Limits the scope of barriers to those applicable to a particular resource.
// Contains common code for buffer and image barrier policies.
class ResourceBarrierPolicy : public BarrierPolicy
{
public:
    ResourceBarrierPolicy(
        const Device*                       pDevice,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

protected:
    void InitConcurrentCachePolicy(
        const Device*                       pDevice,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

    VK_FORCEINLINE const QueueFamilyBarrierPolicy& GetQueueFamilyPolicy(
        uint32_t                            queueFamilyIndex) const
    {
        return m_pDevicePolicy->GetQueueFamilyPolicy(queueFamilyIndex);
    }

    const DeviceBarrierPolicy* m_pDevicePolicy;     // Device barrier policy.

    uint32_t    m_concurrentCacheMask;              // Mask including all caches supported by any queue family in the
                                                    // concurrent sharing scope.

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ResourceBarrierPolicy);
};

// =====================================================================================================================
// Image barrier policy class.
// Limits the scope of barriers to those applicable to this particular image.
// Used to control the policy for image memory barriers.
class ImageBarrierPolicy final : public ResourceBarrierPolicy
{
public:
    ImageBarrierPolicy(
        const Device*                       pDevice,
        VkImageUsageFlags                   usage,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices,
        bool                                multisampled,
        VkFormat                            format,
        uint32_t                            extraLayoutUsages = 0);

    VK_FORCEINLINE uint32_t GetSupportedLayoutUsageMask() const
        { return m_supportedLayoutUsageMask; }

    VK_FORCEINLINE uint32_t GetSupportedLayoutUsageMask(
        uint32_t                            queueFamilyIndex) const
    {
        // This version of the function returns the supported layout usage masks in the scope of the specified queue
        // family. Accordingly, the image's supported layout usage mask is limited to the layout usage mask that
        // is supported by the specified queue family or by other queue families that are allowed to concurrently
        // access the image.
        return m_supportedLayoutUsageMask &
               (GetQueueFamilyPolicy(queueFamilyIndex).supportedLayoutUsageMask | m_concurrentLayoutUsageMask);
    }

    VK_FORCEINLINE uint32_t GetPossibleLayoutEngineMasks() const
    { return m_possibleLayoutEngineMask; }

    Pal::ImageLayout GetTransferLayout(
        VkImageLayout                       layout,
        uint32_t                            queueFamilyIndex) const;

    Pal::ImageLayout GetAspectLayout(
        VkImageLayout                       layout,
        uint32_t                            aspectIndex,
        uint32_t                            queueFamilyIndex,
        VkFormat                            format) const;

    template<typename ImageMemoryBarrierType>
    void ApplyImageMemoryBarrier(
        uint32_t                            currentQueueFamilyIndex,
        const ImageMemoryBarrierType&       barrier,
        Pal::BarrierTransition*             pPalBarrier,
        bool*                               pLayoutChanging,
        Pal::ImageLayout                    oldPalLayouts[MaxPalAspectsPerMask],
        Pal::ImageLayout                    newPalLayouts[MaxPalAspectsPerMask],
        bool                                skipMatchingLayouts) const;

protected:
    void InitImageLayoutUsagePolicy(
        const Device*                       pDevice,
        VkImageUsageFlags                   usage,
        bool                                multisampled,
        VkFormat                            format,
        uint32_t                            extraLayoutUsages);

    void InitConcurrentLayoutUsagePolicy(
        const Device*                       pDevice,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

    void InitImageLayoutEnginePolicy(
        const Device*                       pDevice,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

    void InitImageCachePolicy(
        const Device*                       pDevice,
        VkImageUsageFlags                   usage);

    void GetLayouts(
        VkImageLayout                       layout,
        uint32_t                            queueFamilyIndex,
        Pal::ImageLayout                    results[MaxPalAspectsPerMask],
        VkFormat                            format) const;

    uint32_t GetQueueFamilyLayoutEngineMask(
        uint32_t                            queueFamilyIndex) const;

    uint32_t    m_supportedLayoutUsageMask;         // Mask including all supported layout usage flags for the image.
    uint32_t    m_supportedLayoutEngineMask;        // Mask including all supported layout engine flags for the image.
    uint32_t    m_alwaysSetLayoutEngineMask;        // Mask including layout engine flags that should be always set.
                                                    // This contains all engines in the scope of concurrent sharing
                                                    // mode to allow concurrent well-defined access to the image.
    uint32_t    m_concurrentLayoutUsageMask;        // Mask including all layout usage flags supported by any queue
                                                    // family in the concurrent sharing scope.
    uint32_t    m_possibleLayoutEngineMask;         // Mask of possible engines this image may be used on.
                                                    // Used when creating ImageViews for the image.

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ImageBarrierPolicy);
};

// =====================================================================================================================
// Buffer barrier policy class.
// Limits the scope of barriers to those applicable to this particular buffer.
// Used to control the policy for buffer memory barriers.
class BufferBarrierPolicy final : public ResourceBarrierPolicy
{
public:
    BufferBarrierPolicy(
        Device*                             pDevice,
        BufferUsageFlagBits                 usage,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

    template<typename BufferMemoryBarrierType>
    void ApplyBufferMemoryBarrier(
        uint32_t                            currentQueueFamilyIndex,
        const BufferMemoryBarrierType&      barrier,
        Pal::BarrierTransition*             pPalBarrier) const;

protected:
    void InitBufferCachePolicy(
        Device*                             pDevice,
        BufferUsageFlagBits                 usage,
        VkSharingMode                       sharingMode,
        uint32_t                            queueFamilyIndexCount,
        const uint32_t*                     pQueueFamilyIndices);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(BufferBarrierPolicy);
};

} //namespace vk

#endif /* __BARRIER_POLICY_H__ */
