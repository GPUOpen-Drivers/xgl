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

#include "include/barrier_policy.h"
#include "include/vk_device.h"
#include "include/vk_formats.h"

namespace vk
{

// =====================================================================================================================
// Helper class to convert Vulkan layouts to PAL layout usage flags.
class LayoutUsageHelper
{
public:
    // Constructor initializes the lookup table.
    LayoutUsageHelper()
    {
        constexpr uint32_t AllImgLayoutUsages =
            Pal::LayoutUninitializedTarget |
            Pal::LayoutColorTarget |
            Pal::LayoutDepthStencilTarget |
            Pal::LayoutShaderRead |
            Pal::LayoutShaderFmaskBasedRead |
            Pal::LayoutShaderWrite |
            Pal::LayoutCopySrc |
            Pal::LayoutCopyDst |
            Pal::LayoutResolveSrc |
            Pal::LayoutResolveDst |
            Pal::LayoutPresentWindowed |
            Pal::LayoutPresentFullscreen |
            Pal::LayoutUncompressed;

        InitEntry(VK_IMAGE_LAYOUT_UNDEFINED,
                  Pal::LayoutUninitializedTarget);

        InitEntry(VK_IMAGE_LAYOUT_GENERAL,
                  AllImgLayoutUsages & ~Pal::LayoutUninitializedTarget);

        InitEntry(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  Pal::LayoutColorTarget);

        InitEntry(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                  Pal::LayoutDepthStencilTarget);

        InitEntry(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                  Pal::LayoutDepthStencilTarget | Pal::LayoutShaderRead);

        InitEntry(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  Pal::LayoutShaderRead |           // For regular reads
                  Pal::LayoutShaderFmaskBasedRead); // For fmask based reads

        InitEntry(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  Pal::LayoutCopySrc |              // For vkCmdCopy* source
                  Pal::LayoutResolveSrc);           // For vkCmdResolve* source

        InitEntry(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  Pal::LayoutCopyDst |              // Required for vkCmdCopy* dest
                  Pal::LayoutResolveDst |           // Required for vkCmdResolve* dest
                  Pal::LayoutColorTarget |          // For vkCmdClearColorImage gfx clear followed by color render
                  Pal::LayoutDepthStencilTarget |   // For vkCmdClearDepthStencilImage gfx clear followed by depth render
                  Pal::LayoutShaderWrite);          // For vkCmdClear* compute clear followed by UAV writes

        InitEntry(VK_IMAGE_LAYOUT_PREINITIALIZED,
                  Pal::LayoutUninitializedTarget);

        InitEntry(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  Pal::LayoutPresentFullscreen | Pal::LayoutPresentWindowed);

        InitEntry(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
                  Pal::LayoutDepthStencilTarget | Pal::LayoutShaderRead,    // Read-only depth
                  Pal::LayoutDepthStencilTarget);                           // Read-write stencil

        InitEntry(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
                  Pal::LayoutDepthStencilTarget,                            // Read-write depth
                  Pal::LayoutDepthStencilTarget | Pal::LayoutShaderRead);   // Read-only stencil

    }

    // Return layout usage index corresponding to the specified layout.
    VK_FORCEINLINE uint32_t GetLayoutUsageIndex(VkImageLayout layout) const
    {
        uint32_t index = 0;

        if (static_cast<uint32_t>(layout) < VK_IMAGE_LAYOUT_RANGE_SIZE)
        {
            index = static_cast<uint32_t>(layout);
        }
        else
        {
            switch (static_cast<int32_t>(layout))
            {
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                index = VK_IMAGE_LAYOUT_RANGE_SIZE + 0;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
                index = VK_IMAGE_LAYOUT_RANGE_SIZE + 1;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
                index = VK_IMAGE_LAYOUT_RANGE_SIZE + 2;
                break;
            default:
                VK_NEVER_CALLED();
                break;
            }
        }

        return index;
    }

    // Return layout usage corresponding to the specified aspect and usage index.
    VK_FORCEINLINE uint32_t GetLayoutUsage(uint32_t aspectIndex, uint32_t usageIndex) const
    {
        VK_ASSERT(aspectIndex < MaxPalAspectsPerMask);
        VK_ASSERT(usageIndex < LayoutUsageTableSize);
        return m_layoutUsageTable[aspectIndex][usageIndex];
    }

protected:
    void InitEntry(VkImageLayout layout, uint32_t layoutUsage)
    {
        const uint32_t usageIndex = GetLayoutUsageIndex(layout);

        m_layoutUsageTable[0][usageIndex] = layoutUsage;
        m_layoutUsageTable[1][usageIndex] = layoutUsage;
        m_layoutUsageTable[2][usageIndex] = layoutUsage;
    }

    void InitEntry(VkImageLayout layout, uint32_t layoutUsage0, uint32_t layoutUsage1, uint32_t layoutUsage2 = 0)
    {
        const uint32_t usageIndex = GetLayoutUsageIndex(layout);

        m_layoutUsageTable[0][usageIndex] = layoutUsage0;
        m_layoutUsageTable[1][usageIndex] = layoutUsage1;
        m_layoutUsageTable[2][usageIndex] = layoutUsage2;
    }

    enum { LayoutUsageTableSize = VK_IMAGE_LAYOUT_RANGE_SIZE + 6 };

    uint32_t    m_layoutUsageTable[MaxPalAspectsPerMask][LayoutUsageTableSize];
};

static const LayoutUsageHelper g_LayoutUsageHelper;

// =====================================================================================================================
// Converts source access flags to source cache coherency flags.
static VK_INLINE uint32_t SrcAccessToCacheMask(VkAccessFlags accessMask)
{
    uint32_t cacheMask = 0;

    if (accessMask & VK_ACCESS_SHADER_WRITE_BIT)
    {
        cacheMask = Pal::CoherShader;
    }

    if (accessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    {
        cacheMask |= Pal::CoherColorTarget | Pal::CoherClear;
    }

    if (accessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
    {
        cacheMask |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
    }

    if (accessMask & VK_ACCESS_TRANSFER_WRITE_BIT)
    {
        cacheMask |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear | Pal::CoherShader | Pal::CoherTimestamp;
    }

    if (accessMask & VK_ACCESS_HOST_WRITE_BIT)
    {
        cacheMask |= Pal::CoherCpu;
    }

    if (accessMask & VK_ACCESS_MEMORY_WRITE_BIT)
    {
        cacheMask |= Pal::CoherMemory;
    }

    // CoherQueueAtomic: Not used
    // CoherTimestamp: Timestamp write syncs are handled by the timestamp-related write/query funcs and not barriers
    // CoherCeLoad: Not used
    // CoherCeDump: Not used
    // CoherStreamOut: Not used

    return cacheMask;
}

// =====================================================================================================================
// Converts destination access flags to destination cache coherency flags.
static VK_INLINE uint32_t DstAccessToCacheMask(VkAccessFlags accessMask)
{
    uint32_t cacheMask = 0;

    if (accessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
    {
        cacheMask |= Pal::CoherIndirectArgs;
    }

    if (accessMask & VK_ACCESS_INDEX_READ_BIT)
    {
        cacheMask |= Pal::CoherIndexData;
    }

    constexpr VkAccessFlags shaderReadAccessFlags = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                                                    | VK_ACCESS_UNIFORM_READ_BIT
                                                    | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
                                                    | VK_ACCESS_SHADER_READ_BIT;

    if (accessMask & shaderReadAccessFlags)
    {
        cacheMask |= Pal::CoherShader;
    }

    if (accessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
    {
        cacheMask |= Pal::CoherColorTarget;
    }

    if (accessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
    {
        cacheMask |= Pal::CoherDepthStencilTarget;
    }

    if (accessMask & VK_ACCESS_TRANSFER_READ_BIT)
    {
        cacheMask |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherShader;
    }

    if (accessMask & VK_ACCESS_HOST_READ_BIT)
    {
        cacheMask |= Pal::CoherCpu;
    }

    if (accessMask & VK_ACCESS_MEMORY_READ_BIT)
    {
        cacheMask |= Pal::CoherMemory;
    }

    return cacheMask;
}

// =====================================================================================================================
// Initializes the cache policy of the barrier policy.
void BarrierPolicy::InitCachePolicy(
    PhysicalDevice*                     pPhysicalDevice,
    uint32_t                            supportedOutputCacheMask,
    uint32_t                            supportedInputCacheMask)
{
    // Query resource barrier options.
    uint32_t barrierOptions = pPhysicalDevice->GetRuntimeSettings().resourceBarrierOptions;

    // Store provided set of supported output/input cache masks.
    m_supportedOutputCacheMask  = supportedOutputCacheMask;
    m_supportedInputCacheMask   = supportedInputCacheMask;

    // Initialize the rest of the masks. They will later going to be populated based on the barrier options.
    m_keepCoherMask     = 0;
    m_avoidCoherMask    = 0;
    m_alwaysFlushMask   = 0;
    m_alwaysInvMask     = 0;

    // Initialize barrier option flags.
    m_flags.u32All                  = 0;
    m_flags.combinedAccessMasks     = (barrierOptions & CombinedAccessMasks) ? 1 : 0;
    m_flags.skipDstCacheInv         = (barrierOptions & SkipDstCacheInv) ? 1 : 0;
    m_flags.preferFlushOverInv      = (barrierOptions & PreferFlushOverInv) ? 1 : 0;
    if (pPhysicalDevice->PalProperties().gfxLevel < Pal::GfxIpLevel::GfxIp9)
    {
        // GFX6-8 specific configuration.
        m_flags.keepShaderCoher     = (barrierOptions & Gfx6KeepShaderCoher) ? 1 : 0;
        m_flags.avoidCpuMemoryCoher = (barrierOptions & Gfx6AvoidCpuMemoryCoher) ? 1 : 0;
    }
    else
    {
        // GFX9+ specific configuration.
        m_flags.keepShaderCoher     = (barrierOptions & Gfx9KeepShaderCoher) ? 1 : 0;
        m_flags.avoidCpuMemoryCoher = (barrierOptions & Gfx9AvoidCpuMemoryCoher) ? 1 : 0;
    }

    // Setting both SkipDstCacheInv and PreferFlushOverInv isn't supported, as SkipDstCacheInv assumes that the
    // Vulkan separate access mask rule would otherwise be fulfilled by invalidating input caches.
    VK_ASSERT((m_flags.skipDstCacheInv == 0) || (m_flags.preferFlushOverInv == 0));

    // Handle when the shader domain should be always kept coherent.
    if (m_flags.keepShaderCoher)
    {
        m_keepCoherMask |= Pal::CoherShader;
    }

    // Handle when the CPU and memory domain should be avoided to be kept coherent unless explicitly requested.
    if (m_flags.avoidCpuMemoryCoher)
    {
        m_avoidCoherMask |= (Pal::CoherCpu | Pal::CoherMemory);
    }

    // Determine which caches should always be flushed and/or invalidated.
    if (m_flags.combinedAccessMasks)
    {
        // If CombinedAccessMasks is set then we intentionally ignore the Vulkan separate access mask rule and thus
        // we don't flush or invalidate any caches by default.
    }
    else if (m_flags.preferFlushOverInv)
    {
        // If we prefer flushing over invalidation to fulfill the Vulkan separate access mask rule then we always
        // flush all output caches.
        m_alwaysFlushMask |= 0xFFFFFFFF;
    }
    else
    {
        // Otherwise we fulfill the Vulkan separate access mask rule by always invalidating all input caches.
        m_alwaysInvMask |= 0xFFFFFFFF;
    }

    // Include domains that are expected to be always kept coherent.
    m_alwaysFlushMask   |= m_keepCoherMask;
    m_alwaysInvMask     |= m_keepCoherMask;

    // Exclude domains that are expected to be avoided to be kept coherent unless explicitly requested.
    m_alwaysFlushMask   &= ~m_avoidCoherMask;
    m_alwaysInvMask     &= ~m_avoidCoherMask;

    // Make sure none of the derived masks include any unsupported coherency flags.
    m_keepCoherMask     &= (m_supportedOutputCacheMask | m_supportedInputCacheMask);
    m_avoidCoherMask    &= (m_supportedOutputCacheMask | m_supportedInputCacheMask);
    m_alwaysFlushMask   &= m_supportedOutputCacheMask;
    m_alwaysInvMask     &= m_supportedInputCacheMask;
}

// =====================================================================================================================
// Applies the barrier policy to a barrier transition while converting the input access flags to cache masks.
void BarrierPolicy::ApplyBarrierCacheFlags(
    VkAccessFlags                       srcAccess,
    VkAccessFlags                       dstAccess,
    Pal::BarrierTransition*             pResult) const
{
    // Convert access masks to cache coherency masks and exclude any coherency flags that are not supported.
    uint32_t srcCacheMask = SrcAccessToCacheMask(srcAccess) & m_supportedOutputCacheMask;
    uint32_t dstCacheMask = DstAccessToCacheMask(dstAccess) & m_supportedInputCacheMask;

    // Calculate the union of both masks that are used for handling the domains that are always kept coherent and the
    // domains that are avoided to be kept coherent unless explicitly requested.
    uint32_t jointCacheMask = srcCacheMask | dstCacheMask;

    // If there is any domain specified that is avoided to be kept coherent unless explicitly requested then add those
    // to both the source and destination cache mask to ensure they are correctly made coherent with other accesses.
    uint32_t expensiveCoherMask = jointCacheMask & m_avoidCoherMask;
    srcCacheMask |= expensiveCoherMask;
    dstCacheMask |= expensiveCoherMask;

    // If there is any domain specified that is not always kept coherent then flush and invalidate caches that should
    // otherwise always be flushed/invalidated.
    // This guarantees both that the domains supposed to be always kept coherent are included and that the Vulkan
    // separate access mask rule is respected one way or another (depending on the value of preferFlushOverInv).
    // It also ensures that if only such domains are specified that are always kept coherent then we don't apply
    // the always flush/invalidate masks unnecessarily, thus providing a fast path for these cases without violating
    // the Vulkan separate access mask rule.
    if ((jointCacheMask & ~m_keepCoherMask) != 0)
    {
        srcCacheMask |= m_alwaysFlushMask;
        dstCacheMask |= m_alwaysInvMask;
    }

    // If skipDstCacheInv is used then we should skip invalidating input caches unless there was at least one output
    // cache flushed here.
    if ((srcCacheMask == 0) && m_flags.skipDstCacheInv)
    {
        dstCacheMask = 0;
    }

    // Set the determined cache masks in the barrier transition.
    pResult->srcCacheMask = srcCacheMask;
    pResult->dstCacheMask = dstCacheMask;
}

// =====================================================================================================================
// Constructor for device barrier policies.
DeviceBarrierPolicy::DeviceBarrierPolicy(
    PhysicalDevice*                     pPhysicalDevice,
    const VkDeviceCreateInfo*           pCreateInfo,
    const DeviceExtensions::Enabled&    enabledExtensions)
{
    InitDeviceLayoutEnginePolicy(pPhysicalDevice, pCreateInfo, enabledExtensions);
    InitDeviceCachePolicy(pPhysicalDevice, enabledExtensions);
    InitQueueFamilyPolicies(pPhysicalDevice, pCreateInfo, enabledExtensions);
}

// =====================================================================================================================
// Initialize the layout engine policy of the device according to the input parameters.
void DeviceBarrierPolicy::InitDeviceLayoutEnginePolicy(
    PhysicalDevice*                     pPhysicalDevice,
    const VkDeviceCreateInfo*           pCreateInfo,
    const DeviceExtensions::Enabled&    enabledExtensions)
{
    // Initialize the maximum set of layout engines that may be applicable to this device according to the set of
    // enabled features.
    uint32_t maxLayoutEngineMask    = Pal::LayoutUniversalEngine
                                    | Pal::LayoutComputeEngine
                                    | Pal::LayoutDmaEngine;

    // Populate the supported layout engine mask based on the queues the application requested, and exclude any
    // layout engine flags that are beyond the maximum set of layout engines.
    m_supportedLayoutEngineMask = 0;

    for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
    {
        uint32_t queueFamilyIndex = pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex;

        m_supportedLayoutEngineMask |= pPhysicalDevice->GetQueueFamilyPalImageLayoutFlag(queueFamilyIndex);
    }

    m_supportedLayoutEngineMask &= maxLayoutEngineMask;
}

// =====================================================================================================================
// Initialize the cache policy of the device according to the input parameters.
void DeviceBarrierPolicy::InitDeviceCachePolicy(
    PhysicalDevice*                     pPhysicalDevice,
    const DeviceExtensions::Enabled&    enabledExtensions)
{
    uint32_t supportedOutputCacheMask   = 0;
    uint32_t supportedInputCacheMask    = 0;

    // Add all output/input caches supported by default.
    supportedOutputCacheMask   |= Pal::CoherCpu
                                | Pal::CoherShader
                                | Pal::CoherCopy
                                | Pal::CoherColorTarget
                                | Pal::CoherDepthStencilTarget
                                | Pal::CoherResolve
                                | Pal::CoherClear
                                | Pal::CoherMemory;

    supportedInputCacheMask    |= Pal::CoherCpu
                                | Pal::CoherShader
                                | Pal::CoherCopy
                                | Pal::CoherColorTarget
                                | Pal::CoherDepthStencilTarget
                                | Pal::CoherResolve
                                | Pal::CoherClear
                                | Pal::CoherIndirectArgs
                                | Pal::CoherIndexData
                                | Pal::CoherMemory;

    if (enabledExtensions.IsExtensionEnabled(DeviceExtensions::AMD_BUFFER_MARKER))
    {
        // Marker writes are in the timestamp coherency domain. Only add it to the supported cache mask if the
        // extension is enabled.
        supportedOutputCacheMask |= Pal::CoherTimestamp;
        supportedInputCacheMask  |= Pal::CoherTimestamp;
    }

    // Initialize cache policy.
    InitCachePolicy(pPhysicalDevice,
                    supportedOutputCacheMask,
                    supportedInputCacheMask);
}

// =====================================================================================================================
// Initialize the layout engine policy of the device according to the input parameters.
void DeviceBarrierPolicy::InitQueueFamilyPolicies(
    PhysicalDevice*                     pPhysicalDevice,
    const VkDeviceCreateInfo*           pCreateInfo,
    const DeviceExtensions::Enabled&    enabledExtensions)
{
    memset(&m_queueFamilyPolicy[0], 0, sizeof(m_queueFamilyPolicy));

    for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i)
    {
        uint32_t queueFamilyIndex = pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex;
        QueueFamilyBarrierPolicy& policy = m_queueFamilyPolicy[queueFamilyIndex];

        policy.palLayoutEngineMask      = pPhysicalDevice->GetQueueFamilyPalImageLayoutFlag(queueFamilyIndex);

        policy.supportedCacheMask       = Pal::CoherCpu
                                        | Pal::CoherTimestamp
                                        | Pal::CoherMemory;
        policy.supportedLayoutUsageMask = Pal::LayoutUninitializedTarget
                                        | Pal::LayoutPresentWindowed
                                        | Pal::LayoutPresentFullscreen
                                        | Pal::LayoutUncompressed;

        switch (pPhysicalDevice->GetQueueFamilyPalQueueType(queueFamilyIndex))
        {
        case Pal::QueueTypeUniversal:
            policy.supportedCacheMask       |= Pal::CoherShader
                                             | Pal::CoherCopy
                                             | Pal::CoherColorTarget
                                             | Pal::CoherDepthStencilTarget
                                             | Pal::CoherResolve
                                             | Pal::CoherClear
                                             | Pal::CoherIndirectArgs
                                             | Pal::CoherIndexData;
            policy.supportedLayoutUsageMask |= Pal::LayoutColorTarget
                                             | Pal::LayoutDepthStencilTarget
                                             | Pal::LayoutShaderRead
                                             | Pal::LayoutShaderFmaskBasedRead
                                             | Pal::LayoutShaderWrite
                                             | Pal::LayoutCopySrc
                                             | Pal::LayoutCopyDst
                                             | Pal::LayoutResolveSrc
                                             | Pal::LayoutResolveDst;

            // Always prefer executing ownership transfer barriers on the universal queue.
            policy.ownershipTransferPriority = OwnershipTransferPriority::High;
            break;

        case Pal::QueueTypeCompute:
            policy.supportedCacheMask       |= Pal::CoherShader
                                             | Pal::CoherCopy
                                             | Pal::CoherResolve
                                             | Pal::CoherClear
                                             | Pal::CoherIndirectArgs;
            policy.supportedLayoutUsageMask |= Pal::LayoutShaderRead
                                             | Pal::LayoutShaderFmaskBasedRead
                                             | Pal::LayoutShaderWrite
                                             | Pal::LayoutCopySrc
                                             | Pal::LayoutCopyDst;

            // Prefer executing ownership transfer barriers on the compute queue against all but the universal queue.
            policy.ownershipTransferPriority = OwnershipTransferPriority::Medium;
            break;

        case Pal::QueueTypeDma:
            policy.supportedCacheMask       |= Pal::CoherCopy
                                             | Pal::CoherClear;
            policy.supportedLayoutUsageMask |= Pal::LayoutCopySrc
                                             | Pal::LayoutCopyDst;
            policy.ownershipTransferPriority = OwnershipTransferPriority::Low;
            break;

        default:
            VK_ASSERT(!"Unexpected queue type");
        }
    }

    // Set defaults for external/foreign queue families.
    m_externalQueueFamilyPolicy.palLayoutEngineMask         = Pal::LayoutAllEngines;
    m_externalQueueFamilyPolicy.supportedCacheMask          = Pal::CoherAllUsages;
    m_externalQueueFamilyPolicy.supportedLayoutUsageMask    = Pal::LayoutAllUsages;
    m_externalQueueFamilyPolicy.ownershipTransferPriority   = OwnershipTransferPriority::None;
}

// =====================================================================================================================
// Constructor for resource barrier policies.
ResourceBarrierPolicy::ResourceBarrierPolicy(
    Device*                             pDevice,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
  : m_pDevicePolicy(&pDevice->GetBarrierPolicy())
{
    InitConcurrentCachePolicy(pDevice, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices);
}

// =====================================================================================================================
// Initialize the concurrent sharing cache policy of the resource if necessary.
void ResourceBarrierPolicy::InitConcurrentCachePolicy(
    Device*                             pDevice,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
{
    // By default there's no concurrent sharing scope cache mask needed. Only in case of VK_SHARING_MODE_CONCURRENT is
    // this needed and is used to extend the scope of the barrier beyond the current queue family's scope.
    m_concurrentCacheMask = 0;

    if (sharingMode == VK_SHARING_MODE_CONCURRENT)
    {
        for (uint32_t i = 0; i < queueFamilyIndexCount; ++i)
        {
            // Add each queue family's support cache mask to the concurrent cache mask if it participates in the
            // concurrent sharing scope.
            const QueueFamilyBarrierPolicy& policy = GetQueueFamilyPolicy(pQueueFamilyIndices[i]);
            m_concurrentCacheMask |= policy.supportedCacheMask;
        }
    }
}

// =====================================================================================================================
// Constructor for image barrier policies.
ImageBarrierPolicy::ImageBarrierPolicy(
    Device*                             pDevice,
    VkImageUsageFlags                   usage,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices,
    bool                                multisampled,
    VkFormat                            format,
    uint32_t                            extraLayoutUsages)
  : ResourceBarrierPolicy(pDevice, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices)
{
    InitImageLayoutUsagePolicy(pDevice, usage, multisampled, format, extraLayoutUsages);
    InitConcurrentLayoutUsagePolicy(pDevice, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices);
    InitImageLayoutEnginePolicy(pDevice, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices);
    InitImageCachePolicy(pDevice, usage);
}

// =====================================================================================================================
// Initialize the layout usage policy of the image according to the input parameters.
void ImageBarrierPolicy::InitImageLayoutUsagePolicy(
    Device*                             pDevice,
    VkImageUsageFlags                   usage,
    bool                                multisampled,
    VkFormat                            format,
    uint32_t                            extraLayoutUsages)
{
    // Initialize layout usage mask to always allow uninitialized.
    m_supportedLayoutUsageMask = Pal::LayoutUninitializedTarget;

    // Add the extra layout usages requested. This is used to specify the layout usages specific to presentable images.
    m_supportedLayoutUsageMask |= extraLayoutUsages;

    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        m_supportedLayoutUsageMask |= Pal::LayoutCopySrc;

        // Multisampled images can also be used as the source of resolves.
        if (multisampled)
        {
            m_supportedLayoutUsageMask |= Pal::LayoutResolveSrc;
        }
    }

    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        m_supportedLayoutUsageMask |= Pal::LayoutCopyDst;

        // Single sampled images can also be used as the destination of resolves.
        if (multisampled == false)
        {
            m_supportedLayoutUsageMask |= Pal::LayoutResolveDst;
        }
    }

    if ((usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) != 0)
    {
        if ((Formats::IsDepthStencilFormat(format) == false) &&
            multisampled && pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
        {
            // If this is a multisampled color image and fmask based reads are enabled then use it.
            m_supportedLayoutUsageMask |= Pal::LayoutShaderFmaskBasedRead;
        }
        else
        {
            // Otherwise use regular shader reads.
            m_supportedLayoutUsageMask |= Pal::LayoutShaderRead;
        }
    }

    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        m_supportedLayoutUsageMask |= Pal::LayoutShaderWrite;
    }

    // Note that the below code enables clear support for color/depth targets because they can also be cleared inside
    // render passes (either as load op clears or vkCmdClearAttachments) which do not require the transfer destination
    // bit to be set.

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        m_supportedLayoutUsageMask |= Pal::LayoutColorTarget;

        VK_ASSERT(pDevice != VK_NULL_HANDLE);
        const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

        // Note here that we enable resolve support when color attachment bit is set, because MSAA color attachment image
        // is always expected to support ResolveSrc layout for render pass resolves sourcing it which needn't
        // TRANSFER_SRC_BIT to be specified. single sample color attachment image is always expected to support ResolveDst
        // layout for render pass resolves targeting it which needn't TRANSFER_DST_BIT to be specified.
        if (multisampled)
        {
            m_supportedLayoutUsageMask |= Pal::LayoutResolveSrc;
        }
        else
        {
            // If application creates image with usage bit of color_target and then use general layout
            // for the image to be resolve target, we need m_supportedLayoutUsageMask to cover
            // resolve_dst layout.
            // If app uses transfer dst usage bit instead, we should be safely covered. The benefit of
            // not setting resolvedst layout bit is, If application create image with usage of
            // color target and sampling, but some how use general layout the change between the read
            // and the write layout, having resolve_dst bit for all current ASICs means meta data needs to be
            // decompressed. That is not ideal.
            if (settings.optColorTargetUsageDoesNotContainResolveLayout == false)
            {
                m_supportedLayoutUsageMask |= Pal::LayoutResolveDst;
            }
        }
    }

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        // See the above note for CoherClear.
        m_supportedLayoutUsageMask |= Pal::LayoutDepthStencilTarget;
    }

    // We don't do anything special in case of transient attachment images
    VK_IGNORE(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
}

// =====================================================================================================================
// Initialize the concurrent layout usage policy of the image if necessary.
void ImageBarrierPolicy::InitConcurrentLayoutUsagePolicy(
    Device*                             pDevice,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
{
    // By default there's no concurrent sharing scope layout usage mask needed. Only in case of
    // VK_SHARING_MODE_CONCURRENT is this needed and is used to extend the scope of the barrier beyond the current
    // queue family's scope.
    m_concurrentLayoutUsageMask = 0;

    if (sharingMode == VK_SHARING_MODE_CONCURRENT)
    {
        for (uint32_t i = 0; i < queueFamilyIndexCount; ++i)
        {
            // Add each queue family's support cache mask to the concurrent cache mask if it participates in the
            // concurrent sharing scope.
            const QueueFamilyBarrierPolicy& policy = GetQueueFamilyPolicy(pQueueFamilyIndices[i]);
            m_concurrentLayoutUsageMask |= policy.supportedLayoutUsageMask;
        }
    }
}

// =====================================================================================================================
// Initialize the layout engine policy of the image according to the input parameters.
void ImageBarrierPolicy::InitImageLayoutEnginePolicy(
    Device*                             pDevice,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
{
    switch (sharingMode)
    {
    case VK_SHARING_MODE_EXCLUSIVE:
        {
            // In case EXCLUSIVE sharing mode is used set the supported layout engine mask to that of the device's and
            // don't include any layout engine flags in the always set ones.
            m_supportedLayoutEngineMask = pDevice->GetBarrierPolicy().GetSupportedLayoutEngineMask();
            m_alwaysSetLayoutEngineMask = 0;
        }
        break;

    case VK_SHARING_MODE_CONCURRENT:
        {
            // In case CONCURRENT sharing mode is used set the supported layout engine mask and the always set layout
            // engine mask according to the queue family indices participating in the concurrent sharing.
            uint32_t concurrentSharingScope = 0;

            for (uint32_t i = 0; i < queueFamilyIndexCount; ++i)
            {
                concurrentSharingScope |= pDevice->GetQueueFamilyPalImageLayoutFlag(pQueueFamilyIndices[i]);
            }

            // Always mask the resulting scope by the supported layout engine mask of the device.
            concurrentSharingScope &= pDevice->GetBarrierPolicy().GetSupportedLayoutEngineMask();

            m_supportedLayoutEngineMask = concurrentSharingScope;
            m_alwaysSetLayoutEngineMask = concurrentSharingScope;
        }
        break;

    default:
        VK_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Initialize the cache policy of the image according to the input parameters.
void ImageBarrierPolicy::InitImageCachePolicy(
    Device*                             pDevice,
    VkImageUsageFlags                   usage)
{
    // Initialize supported cache masks based on the usage flags provided.
    // Always allow CPU and memory reads/writes.
    uint32_t supportedOutputCacheMask   = Pal::CoherCpu | Pal::CoherMemory;
    uint32_t supportedInputCacheMask    = Pal::CoherCpu | Pal::CoherMemory;

    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        supportedInputCacheMask |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear;
    }

    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        supportedOutputCacheMask |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear;
    }

    constexpr VkImageUsageFlags shaderReadFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                                                | VK_IMAGE_USAGE_STORAGE_BIT
                                                | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    if (usage & shaderReadFlags)
    {
        supportedInputCacheMask |= Pal::CoherShader;
    }

    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        supportedOutputCacheMask |= Pal::CoherShader;
    }

    // Note that the below code enables clear support for color/depth targets because they can also be cleared inside
    // render passes (either as load op clears or vkCmdClearAttachments) which do not require the transfer destination
    // bit to be set.

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        supportedOutputCacheMask |= Pal::CoherColorTarget | Pal::CoherClear;
        supportedInputCacheMask  |= Pal::CoherColorTarget | Pal::CoherClear;
    }

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        // See the above note for CoherClear.
        supportedOutputCacheMask |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
        supportedInputCacheMask  |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
    }

    // We don't do anything special in case of transient attachment images
    VK_IGNORE(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);

    // Apply device specific supported cache masks to limit the scope.
    supportedOutputCacheMask &= pDevice->GetBarrierPolicy().GetSupportedOutputCacheMask();
    supportedInputCacheMask  &= pDevice->GetBarrierPolicy().GetSupportedInputCacheMask();

    // Initialize cache policy.
    InitCachePolicy(pDevice->VkPhysicalDevice(DefaultDeviceIndex),
                    supportedOutputCacheMask,
                    supportedInputCacheMask);
}

// =====================================================================================================================
// Constructs the PAL layout corresponding to a Vulkan layout for transfer use.
Pal::ImageLayout ImageBarrierPolicy::GetTransferLayout(
    VkImageLayout                       layout,
    uint32_t                            queueFamilyIndex) const
{
    Pal::ImageLayout result = {};

    // Only transfer compatible layouts are allowed here.
    VK_ASSERT((layout == VK_IMAGE_LAYOUT_GENERAL) ||
              (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
              (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
              (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

    uint32_t usageIndex = g_LayoutUsageHelper.GetLayoutUsageIndex(layout);

    // The usage flags should match for both aspects in this case.
    VK_ASSERT(g_LayoutUsageHelper.GetLayoutUsage(0, usageIndex) == g_LayoutUsageHelper.GetLayoutUsage(1, usageIndex));

    // Mask determined layout usage flags by the supported layout usage mask on the given queue family index.
    result.usages = g_LayoutUsageHelper.GetLayoutUsage(0, usageIndex)
                  & GetSupportedLayoutUsageMask(queueFamilyIndex);

    // If the layout usage is 0, it likely means that an application is trying to transition to an image layout that
    // is not supported by that image's usage flags.
    VK_ASSERT(result.usages != 0);

    // Calculate engine mask.
    result.engines = GetQueueFamilyLayoutEngineMask(queueFamilyIndex);

    return result;
}

// =====================================================================================================================
// Constructs the PAL layout corresponding to a Vulkan layout for the specified aspect.
Pal::ImageLayout ImageBarrierPolicy::GetAspectLayout(
    VkImageLayout                       layout,
    uint32_t                            aspectIndex,
    uint32_t                            queueFamilyIndex) const
{
    Pal::ImageLayout result = {};

    uint32_t usageIndex = g_LayoutUsageHelper.GetLayoutUsageIndex(layout);

    // Mask determined layout usage flags by the supported layout usage mask on the given queue family index.
    result.usages = g_LayoutUsageHelper.GetLayoutUsage(aspectIndex, usageIndex)
                  & GetSupportedLayoutUsageMask(queueFamilyIndex);

    // If the layout usage is 0, it likely means that an application is trying to transition to an image layout that
    // is not supported by that image's usage flags.
    VK_ASSERT(result.usages != 0);

    // Calculate engine mask.
    result.engines = GetQueueFamilyLayoutEngineMask(queueFamilyIndex);

    return result;
}

// =====================================================================================================================
// Constructs the PAL layouts corresponding to a Vulkan layout for each aspect.
void ImageBarrierPolicy::GetLayouts(
    VkImageLayout                       layout,
    uint32_t                            queueFamilyIndex,
    Pal::ImageLayout                    results[MaxPalAspectsPerMask]) const
{
    uint32_t usageIndex = g_LayoutUsageHelper.GetLayoutUsageIndex(layout);

    // Mask determined layout usage flags by the supported layout usage mask on the corresponding queue family index.
    const uint32_t supportedLayoutUsageMask = GetSupportedLayoutUsageMask(queueFamilyIndex);
    results[0].usages = g_LayoutUsageHelper.GetLayoutUsage(0, usageIndex) & supportedLayoutUsageMask;
    results[1].usages = g_LayoutUsageHelper.GetLayoutUsage(1, usageIndex) & supportedLayoutUsageMask;
    results[2].usages = g_LayoutUsageHelper.GetLayoutUsage(2, usageIndex) & supportedLayoutUsageMask;

    // If the layout usage is 0, it likely means that an application is trying to transition to an image layout that
    // is not supported by that image's usage flags.
    VK_ASSERT((results[0].usages != 0) && (results[1].usages != 0) && (results[2].usages != 0));

    // Calculate engine mask.
    results[0].engines = results[1].engines = results[2].engines = GetQueueFamilyLayoutEngineMask(queueFamilyIndex);
}

// =====================================================================================================================
// Applies the barrier policy to an image memory barrier as follows:
//   * Converts access masks and writes them to pPalBarrier
//   * Determines whether the barrier is a layout changing one and returns the information in pLayoutChanging
//   * If it's a layout changing barrier then returns the old and new PAL layouts in oldPalLayouts and newPalLayouts
void ImageBarrierPolicy::ApplyImageMemoryBarrier(
    uint32_t                            currentQueueFamilyIndex,
    const VkImageMemoryBarrier&         barrier,
    Pal::BarrierTransition*             pPalBarrier,
    bool*                               pLayoutChanging,
    Pal::ImageLayout                    oldPalLayouts[MaxPalAspectsPerMask],
    Pal::ImageLayout                    newPalLayouts[MaxPalAspectsPerMask]) const
{
    // Determine effective queue family indices.
    uint32_t srcQueueFamilyIndex = (barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
                                 ? currentQueueFamilyIndex : barrier.srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex = (barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
                                 ? currentQueueFamilyIndex : barrier.dstQueueFamilyIndex;

    // Either the source or the destination queue family has to match the current queue family.
    VK_ASSERT((srcQueueFamilyIndex == currentQueueFamilyIndex) || (dstQueueFamilyIndex == currentQueueFamilyIndex));

    // By default try to transition the layout on the source queue family in case of ownership transfers.
    bool applyLayoutChanges = (currentQueueFamilyIndex == srcQueueFamilyIndex);

    // Flip that decision if it turns out the destination queue family's ownership transfer priority is greater than
    // that of the source queue family.
    bool isDstQueueFamilyPreferred = (GetQueueFamilyPolicy(dstQueueFamilyIndex).ownershipTransferPriority >
                                      GetQueueFamilyPolicy(srcQueueFamilyIndex).ownershipTransferPriority);
    applyLayoutChanges = (applyLayoutChanges != isDstQueueFamilyPreferred);

    if (applyLayoutChanges)
    {
        // Determine PAL layouts.
        GetLayouts(barrier.oldLayout, srcQueueFamilyIndex, oldPalLayouts);
        GetLayouts(barrier.newLayout, dstQueueFamilyIndex, newPalLayouts);

        // If old and new PAL layouts match then no need to apply layout changes.
        if (memcmp(oldPalLayouts, newPalLayouts, sizeof(Pal::ImageLayout) * MaxPalAspectsPerMask) == 0)
        {
            applyLayoutChanges = false;
        }
    }

    // Apply barrier cache flags to the PAL barrier transition.
    ApplyBarrierCacheFlags(barrier.srcAccessMask, barrier.dstAccessMask, pPalBarrier);

    // We can further restrict the cache masks to the ones supported by the corresponding queue families or by other
    // queue families that are allowed to concurrently access the image.
    pPalBarrier->srcCacheMask &= GetQueueFamilyPolicy(srcQueueFamilyIndex).supportedCacheMask | m_concurrentCacheMask;
    pPalBarrier->dstCacheMask &= GetQueueFamilyPolicy(dstQueueFamilyIndex).supportedCacheMask | m_concurrentCacheMask;

    // If this is a queue family ownership transfer barrier, then we can exclude all cache masks from the barrier that
    // don't have to have an immediately visible effect as ownership transfers can only happen at command buffer
    // boundaries and we already flush and invalidate all caches at command buffer boundaries. Due to layout changes,
    // however, the source cache mask has to be preserved on the releasing side, and the destination cache mask has
    // to be preserved on the acquiring side to make previous/subsequent operations coherent with the layout
    // transition itself.
    if (srcQueueFamilyIndex != dstQueueFamilyIndex)
    {
        const uint32_t immediatelyVisibleCacheMask = Pal::CoherCpu | Pal::CoherMemory;

        if ((applyLayoutChanges == false) || (currentQueueFamilyIndex == dstQueueFamilyIndex))
        {
            pPalBarrier->srcCacheMask &= immediatelyVisibleCacheMask;
        }

        if ((applyLayoutChanges == false) || (currentQueueFamilyIndex == srcQueueFamilyIndex))
        {
            pPalBarrier->dstCacheMask &= immediatelyVisibleCacheMask;
        }
    }

    *pLayoutChanging = applyLayoutChanges;
}

// =====================================================================================================================
// Returns the layout engine mask corresponding to a queue family index.
uint32_t ImageBarrierPolicy::GetQueueFamilyLayoutEngineMask(
    uint32_t                            queueFamilyIndex) const
{
    // VK_QUEUE_FAMILY_IGNORED must be handled at the caller side by replacing it with the current command
    // buffer's queue family index.
    VK_ASSERT(queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED);

    // Get the layout engine mask of the queue family.
    uint32_t layoutEngineMask = GetQueueFamilyPolicy(queueFamilyIndex).palLayoutEngineMask;

    // Add the always set layout engine mask to handle the concurrent sharing mode case.
    layoutEngineMask |= m_alwaysSetLayoutEngineMask;

    // Mask everything by the supported layout engine mask.
    layoutEngineMask &= m_supportedLayoutEngineMask;

    return layoutEngineMask;
}

// =====================================================================================================================
// Constructor for buffer barrier policies.
BufferBarrierPolicy::BufferBarrierPolicy(
    Device*                             pDevice,
    VkBufferUsageFlags                  usage,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
  : ResourceBarrierPolicy(pDevice, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices)
{
    InitBufferCachePolicy(pDevice, usage, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices);
}

// =====================================================================================================================
// Initialize the cache policy of the buffer according to the input parameters.
void BufferBarrierPolicy::InitBufferCachePolicy(
    Device*                             pDevice,
    VkBufferUsageFlags                  usage,
    VkSharingMode                       sharingMode,
    uint32_t                            queueFamilyIndexCount,
    const uint32_t*                     pQueueFamilyIndices)
{
    // Initialize supported cache masks based on the usage flags provided.
    // Always allow CPU and memory reads/writes.
    uint32_t supportedOutputCacheMask   = Pal::CoherCpu | Pal::CoherMemory;
    uint32_t supportedInputCacheMask    = Pal::CoherCpu | Pal::CoherMemory;

    if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    {
        supportedInputCacheMask |= Pal::CoherCopy;
    }

    if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT)
    {
        // Also need Pal::CoherShader here as vkCmdCopyQueryPoolResults uses a compute shader defined in the Vulkan
        // API layer when used with timestamp queries.
        supportedOutputCacheMask |= Pal::CoherCopy | Pal::CoherShader;

        // Buffer markers fall under the same PAL coherency rules as timestamp writes
        if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_BUFFER_MARKER))
        {
            supportedOutputCacheMask |= Pal::CoherTimestamp;
            supportedInputCacheMask  |= Pal::CoherTimestamp;
        }
    }

    if (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
    {
        supportedInputCacheMask |= Pal::CoherShader;
    }

    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
    {
        supportedOutputCacheMask |= Pal::CoherShader;
        supportedInputCacheMask  |= Pal::CoherShader;
    }

    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    {
        supportedInputCacheMask |= Pal::CoherIndexData;
    }

    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    {
        supportedInputCacheMask |= Pal::CoherShader;
    }

    if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
    {
        supportedInputCacheMask |= Pal::CoherIndirectArgs;
    }

    // Apply device specific supported cache masks to limit the scope.
    supportedOutputCacheMask &= pDevice->GetBarrierPolicy().GetSupportedOutputCacheMask();
    supportedInputCacheMask  &= pDevice->GetBarrierPolicy().GetSupportedInputCacheMask();

    // Initialize cache policy.
    InitCachePolicy(pDevice->VkPhysicalDevice(DefaultDeviceIndex),
                    supportedOutputCacheMask,
                    supportedInputCacheMask);
}

// =====================================================================================================================
// Applies the barrier policy to a buffer memory barrier by converting the access masks and writing them to
// pPalBarrier.
void BufferBarrierPolicy::ApplyBufferMemoryBarrier(
    uint32_t                            currentQueueFamilyIndex,
    const VkBufferMemoryBarrier&        barrier,
    Pal::BarrierTransition*             pPalBarrier) const
{
    // Determine effective queue family indices.
    uint32_t srcQueueFamilyIndex = (barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
                                 ? currentQueueFamilyIndex : barrier.srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex = (barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
                                 ? currentQueueFamilyIndex : barrier.dstQueueFamilyIndex;

    // Either the source or the destination queue family has to match the current queue family.
    VK_ASSERT((srcQueueFamilyIndex == currentQueueFamilyIndex) || (dstQueueFamilyIndex == currentQueueFamilyIndex));

    // Apply barrier cache flags to the PAL barrier transition.
    ApplyBarrierCacheFlags(barrier.srcAccessMask, barrier.dstAccessMask, pPalBarrier);

    // We can further restrict the cache masks to the ones supported by the corresponding queue families or by other
    // queue families that are allowed to concurrently access the image.
    pPalBarrier->srcCacheMask &= GetQueueFamilyPolicy(srcQueueFamilyIndex).supportedCacheMask | m_concurrentCacheMask;
    pPalBarrier->dstCacheMask &= GetQueueFamilyPolicy(dstQueueFamilyIndex).supportedCacheMask | m_concurrentCacheMask;

    // If this is a queue family ownership transfer barrier, then we can exclude all cache masks from the barrier that
    // don't have to have an immediately visible effect as ownership transfers can only happen at command buffer
    // boundaries and we already flush and invalidate all caches at command buffer boundaries.
    if (srcQueueFamilyIndex != dstQueueFamilyIndex)
    {
        const uint32_t immediatelyVisibleCacheMask = Pal::CoherCpu | Pal::CoherMemory;

        pPalBarrier->srcCacheMask &= immediatelyVisibleCacheMask;
        pPalBarrier->dstCacheMask &= immediatelyVisibleCacheMask;
    }
}

} //namespace vk
