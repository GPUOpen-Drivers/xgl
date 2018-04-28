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

namespace vk
{

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
    m_alwaysFlushMask   &= (m_supportedOutputCacheMask | m_supportedInputCacheMask);
    m_alwaysInvMask     &= (m_supportedOutputCacheMask | m_supportedInputCacheMask);
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
// Constructor for image barrier policies.
ImageBarrierPolicy::ImageBarrierPolicy(
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
    InitCachePolicy(pDevice->VkPhysicalDevice(),
                    supportedOutputCacheMask,
                    supportedInputCacheMask);
}

// =====================================================================================================================
// Constructor for buffer barrier policies.
BufferBarrierPolicy::BufferBarrierPolicy(
    Device*                             pDevice,
    VkBufferUsageFlags                  usage)
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
    InitCachePolicy(pDevice->VkPhysicalDevice(),
                    supportedOutputCacheMask,
                    supportedInputCacheMask);
}

} //namespace vk
