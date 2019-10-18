/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 ***********************************************************************************************************************
 * @file  vk_device.cpp
 * @brief Contains implementation of Vulkan device object.
 ***********************************************************************************************************************
 */

#ifdef VK_USE_PLATFORM_XCB_KHR
#include <xcb/xcb.h>
#endif

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_buffer.h"
#include "include/vk_buffer_view.h"
#include "include/vk_descriptor_pool.h"
#include "include/vk_descriptor_set.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_descriptor_update_template.h"
#include "include/vk_device.h"
#include "include/vk_fence.h"
#include "include/vk_formats.h"
#include "include/vk_framebuffer.h"
#include "include/vk_physical_device.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_instance.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_event.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_memory.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_query.h"
#include "include/vk_queue.h"
#include "include/vk_render_pass.h"
#include "include/vk_semaphore.h"
#include "include/vk_shader.h"
#include "include/vk_sampler.h"
#include "include/vk_swapchain.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"

#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"
#include "sqtt/sqtt_rgp_annotations.h"

#include "appopt/async_layer.h"

#include "appopt/barrier_filter_layer.h"
#include "appopt/strange_brigade_layer.h"

#include "appopt/wolfenstein2_layer.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

#include "llpc.h"

#include "palCmdBuffer.h"
#include "palCmdAllocator.h"
#include "palGpuMemory.h"
#include "palLib.h"
#include "palLinearAllocator.h"
#include "palListImpl.h"
#include "palHashMapImpl.h"
#include "palDevice.h"
#include "palSwapChain.h"
#include "palSysMemory.h"
#include "palQueue.h"
#include "palQueueSemaphore.h"

namespace vk
{

// MSAA sample pattern tables. Extra entries up to 16 (max the GCN HW supports) are padded with zeros.

// 1x MSAA
#define VK_DEFAULT_SAMPLE_PATTERN_1X \
    {  0,  0 }

// 2x MSAA
#define VK_DEFAULT_SAMPLE_PATTERN_2X \
    {  4,  4 }, \
    { -4, -4 }

// 4x MSAA
#define VK_DEFAULT_SAMPLE_PATTERN_4X \
    { -2, -6 }, \
    {  6, -2 }, \
    { -6,  2 }, \
    {  2,  6 }

// 8x MSAA
#define VK_DEFAULT_SAMPLE_PATTERN_8X \
    {  1, -3 }, \
    { -1,  3 }, \
    {  5,  1 }, \
    { -3, -5 }, \
    { -5,  5 }, \
    { -7, -1 }, \
    {  3,  7 }, \
    {  7, -7 }

// 16x MSAA
#define VK_DEFAULT_SAMPLE_PATTERN_16X \
    {  1,  1 }, \
    { -1, -3 }, \
    { -3,  2 }, \
    {  4, -1 }, \
    { -5, -2 }, \
    {  2,  5 }, \
    {  5,  3 }, \
    {  3, -5 }, \
    { -2,  6 }, \
    {  0, -7 }, \
    { -4, -6 }, \
    { -6,  4 }, \
    { -8,  0 }, \
    {  7, -4 }, \
    {  6,  7 }, \
    { -7, -8 }

// =====================================================================================================================
// Returns VK_SUCCESS if all requested features are supported, VK_ERROR_FEATURE_NOT_PRESENT otherwise.
template <typename T>
static VkResult VerifyRequestedPhysicalDeviceFeatures(
    const PhysicalDevice* pPhysicalDevice,
    const T*              pRequestedFeatures)
{
    T      supportedFeatures;
    size_t headerSize;

    // Start by making a copy of the requested features all so that uninitialized struct padding always matches below.
    memcpy(&supportedFeatures, pRequestedFeatures, sizeof(T));

    if (std::is_same<T, VkPhysicalDeviceFeatures>::value)
    {
        pPhysicalDevice->GetFeatures(reinterpret_cast<VkPhysicalDeviceFeatures*>(&supportedFeatures));

        // The original VkPhysicalDeviceFeatures struct doesn't contain a VkStructHeader
        headerSize = 0;
    }
    else
    {
        VkStructHeaderNonConst* pHeader = reinterpret_cast<VkStructHeaderNonConst*>(&supportedFeatures);

        pHeader->pNext = nullptr; // Make sure GetFeatures2 doesn't clobber the original requested features chain
        pPhysicalDevice->GetFeatures2(pHeader);

        headerSize = offsetof(VkPhysicalDeviceFeatures2, features);
    }

    const size_t numFeatures = (sizeof(T) - headerSize) / sizeof(VkBool32); // Struct padding may give us extra features
    const auto   supported   = static_cast<const VkBool32*>(Util::VoidPtrInc(&supportedFeatures, headerSize));
    const auto   requested   = static_cast<const VkBool32*>(Util::VoidPtrInc(pRequestedFeatures, headerSize));

    for (size_t featureNdx = 0; featureNdx < numFeatures; ++featureNdx)
    {
        if (requested[featureNdx] && !supported[featureNdx])
        {
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
Device::InternalPipeline::InternalPipeline()
{
    memset(pPipeline,0, sizeof(pPipeline));

    memset(userDataNodeOffsets, 0, sizeof(userDataNodeOffsets));
}

// =====================================================================================================================
Device::Device(
    uint32_t                            palDeviceCount,
    PhysicalDevice**                    pPhysicalDevices,
    Pal::IDevice**                      pPalDevices,
    const DeviceBarrierPolicy&          barrierPolicy,
    const DeviceExtensions::Enabled&    enabledExtensions,
    const VkPhysicalDeviceFeatures*     pFeatures,
    bool                                useComputeAsTransferQueue)
    :
    m_pInstance(pPhysicalDevices[DefaultDeviceIndex]->VkInstance()),
    m_settings(pPhysicalDevices[DefaultDeviceIndex]->GetRuntimeSettings()),
    m_palDeviceCount(palDeviceCount),
    m_internalMemMgr(this, pPhysicalDevices[DefaultDeviceIndex]->VkInstance()),
    m_shaderOptimizer(this, pPhysicalDevices[DefaultDeviceIndex]),
    m_resourceOptimizer(this, pPhysicalDevices[DefaultDeviceIndex]),
    m_renderStateCache(this),
    m_barrierPolicy(barrierPolicy),
    m_enabledExtensions(enabledExtensions),
    m_dispatchTable(DispatchTable::Type::DEVICE, m_pInstance, this),
    m_pSqttMgr(nullptr),
    m_pAsyncLayer(nullptr),
    m_pAppOptLayer(nullptr),
    m_pBarrierFilterLayer(nullptr),
    m_allocationSizeTracking(m_settings.memoryDeviceOverallocationAllowed ? false : true),
    m_useComputeAsTransferQueue(useComputeAsTransferQueue)
{
    memset(m_pBltMsaaState, 0, sizeof(m_pBltMsaaState));

    for (uint32_t deviceIdx = 0; deviceIdx < palDeviceCount; ++deviceIdx)
    {
        m_perGpu[deviceIdx].pPhysicalDevice = pPhysicalDevices[deviceIdx];
        m_perGpu[deviceIdx].pPalDevice      = pPalDevices[deviceIdx];

        m_perGpu[deviceIdx].pSharedPalCmdAllocator = nullptr;

        m_perGpu[deviceIdx].pSwCompositingMemory    = nullptr;
        m_perGpu[deviceIdx].pSwCompositingQueue     = nullptr;
        m_perGpu[deviceIdx].pSwCompositingSemaphore = nullptr;
    }

    if (pFeatures != nullptr)
    {
        memcpy(&m_enabledFeatures, pFeatures, sizeof(VkPhysicalDeviceFeatures));
    }
    else
    {
        memset(&m_enabledFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
    }

    if (m_settings.robustBufferAccess == FeatureForceEnable)
    {
        m_enabledFeatures.robustBufferAccess = true;
    }
    else if (m_settings.robustBufferAccess == FeatureForceDisable)
    {
        m_enabledFeatures.robustBufferAccess = false;
    }

    m_allocatedCount = 0;
    m_maxAllocations = pPhysicalDevices[DefaultDeviceIndex]->GetLimits().maxMemoryAllocationCount;

    m_shaderOptimizer.Init();
    m_resourceOptimizer.Init();

    memset(m_overallocationRequestedForPalHeap, 0, sizeof(m_overallocationRequestedForPalHeap));
}

// =====================================================================================================================
static void ConstructQueueCreateInfo(
    PhysicalDevice**            pPhysicalDevices,
    uint32_t                    deviceIdx,
    uint32_t                    queueFamilyIndex,
    uint32_t                    queueIndex,
    uint32_t                    dedicatedComputeUnits,
    VkQueueGlobalPriorityEXT    queuePriority,
    Pal::QueueCreateInfo*       pQueueCreateInfo,
    bool                        useComputeAsTransferQueue)
{
    const Pal::QueueType palQueueType =
        pPhysicalDevices[deviceIdx]->GetQueueFamilyPalQueueType(queueFamilyIndex);

    const Pal::QueuePriority palQueuePriority =
        VkToPalGlobalPriority(queuePriority);

    // Get the sub engine index of vr high priority
    // UINT32_MAX is returned if the required vr high priority sub engine is not available
    uint32_t vrHighPriorityIndex           = pPhysicalDevices[deviceIdx]->GetVrHighPrioritySubEngineIndex();
    uint32_t rtCuHighComputeSubEngineIndex = pPhysicalDevices[deviceIdx]->GetRtCuHighComputeSubEngineIndex();

    if ((dedicatedComputeUnits > 0) &&
        (rtCuHighComputeSubEngineIndex != UINT32_MAX))
    {
        VK_ASSERT(queuePriority == VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT);

        pQueueCreateInfo->engineType    = Pal::EngineType::EngineTypeCompute;
        pQueueCreateInfo->engineIndex   = rtCuHighComputeSubEngineIndex;
        pQueueCreateInfo->numReservedCu = dedicatedComputeUnits;
    }
    else if (palQueueType == Pal::QueueType::QueueTypeCompute)
    {
        pQueueCreateInfo->engineType = Pal::EngineType::EngineTypeCompute;

        if ((palQueuePriority > Pal::QueuePriority::Idle) &&
            (vrHighPriorityIndex != UINT32_MAX))
        {
            pQueueCreateInfo->engineIndex    = vrHighPriorityIndex;
        }
        else
        {
            pQueueCreateInfo->engineIndex   = pPhysicalDevices[deviceIdx]->GetCompQueueEngineIndex(queueIndex);
        }
    }
    else
    {
        pQueueCreateInfo->engineType  =
            pPhysicalDevices[deviceIdx]->GetQueueFamilyPalEngineType(queueFamilyIndex);

        if (palQueueType == Pal::QueueType::QueueTypeUniversal)
        {
            pQueueCreateInfo->engineIndex = pPhysicalDevices[deviceIdx]->GetUniversalQueueEngineIndex(queueIndex);
        }
        else
        {
            pQueueCreateInfo->engineIndex = queueIndex;
        }
    }

    pQueueCreateInfo->queueType = palQueueType;
    pQueueCreateInfo->priority  = palQueuePriority;

    if ((pQueueCreateInfo->queueType == Pal::QueueType::QueueTypeDma) && useComputeAsTransferQueue)
    {
        pQueueCreateInfo->queueType  = Pal::QueueType::QueueTypeCompute;
        pQueueCreateInfo->engineType = Pal::EngineType::EngineTypeCompute;
    }

#if defined(__unix__) && (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 479)
    pQueueCreateInfo->enableGpuMemoryPriorities = 1;
#endif
}

// =====================================================================================================================
// Creates a new Vulkan API device object
VkResult Device::Create(
    PhysicalDevice*                 pPhysicalDevice,
    const VkDeviceCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    DispatchableDevice**            ppDevice)
{
    Pal::Result palResult = Pal::Result::Success;
    uint32_t queueCounts[Queue::MaxQueueFamilies] = {};
    uint32_t queueFlags[Queue::MaxQueueFamilies] = {};

    // VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT is the default value.
    VkQueueGlobalPriorityEXT queuePriority[Queue::MaxQueueFamilies] = { VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT };

    // Dedicated Compute Units
    static constexpr uint32_t MaxEngineCount = 8;
    uint32_t dedicatedComputeUnits[Queue::MaxQueueFamilies][MaxEngineCount] = { 0 };

    VkResult vkResult = VK_SUCCESS;
    void*    pMemory  = nullptr;
    union
    {
        const VkStructHeader*       pHeader;
        const VkDeviceCreateInfo*   pDeviceCreateInfo;
    };

    DeviceExtensions::Enabled enabledDeviceExtensions;

    VK_ASSERT(pCreateInfo != nullptr);

    // Make sure the caller only requests extensions we actually support.
    if (pCreateInfo->enabledExtensionCount > 0)
    {
        if (!DeviceExtensions::EnableExtensions(pCreateInfo->ppEnabledExtensionNames,
                                                pCreateInfo->enabledExtensionCount,
                                                pPhysicalDevice->GetAllowedExtensions(),
                                                enabledDeviceExtensions))
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        // VK_KHR_maintenance1 introduces negative viewport height feature in a slightly different way.
        // The specification says it is invalid usage to request both extensions at the same time.
        // Assert here because the app is either blindly enabling all supported extensions or unknowingly accepting
        // the behavior of VK_KHR_maintenance1, which has been promoted to core and takes priority.
        VK_ASSERT(enabledDeviceExtensions.IsExtensionEnabled(DeviceExtensions::AMD_NEGATIVE_VIEWPORT_HEIGHT) == false ||
                  enabledDeviceExtensions.IsExtensionEnabled(DeviceExtensions::KHR_MAINTENANCE1)             == false);
    }

    uint32_t                          numDevices                      = 1;
    PhysicalDevice*                   pPhysicalDevices[MaxPalDevices] = { pPhysicalDevice              };
    Pal::IDevice*                     pPalDevices[MaxPalDevices]      = { pPhysicalDevice->PalDevice() };
    Instance*                         pInstance                       = pPhysicalDevice->VkInstance();
    const VkPhysicalDeviceFeatures*   pEnabledFeatures                = pCreateInfo->pEnabledFeatures;
    VkMemoryOverallocationBehaviorAMD overallocationBehavior          = VK_MEMORY_OVERALLOCATION_BEHAVIOR_DEFAULT_AMD;
    bool                              deviceCoherentMemoryEnabled     = false;

    for (pDeviceCreateInfo = pCreateInfo; ((pHeader != nullptr) && (vkResult == VK_SUCCESS)); pHeader = pHeader->pNext)
    {
        switch (static_cast<int>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
        {
            const VkDeviceGroupDeviceCreateInfo* pDeviceGroupCreateInfo =
                        reinterpret_cast<const VkDeviceGroupDeviceCreateInfo*>(pHeader);

            numDevices = pDeviceGroupCreateInfo->physicalDeviceCount;

            VK_ASSERT(numDevices <= MaxPalDevices);
            numDevices = Util::Min(numDevices, MaxPalDevices);

            for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
            {
                pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(
                        pDeviceGroupCreateInfo->pPhysicalDevices[deviceIdx]);

                pPalDevices[deviceIdx]      = pPhysicalDevice->PalDevice();
                pPhysicalDevices[deviceIdx] = pPhysicalDevice;

                VK_ASSERT(pInstance == pPhysicalDevice->VkInstance());
            }
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
        {
            const auto pPhysicalDeviceFeatures2 = reinterpret_cast<const VkPhysicalDeviceFeatures2*>(pHeader);

            VK_ASSERT(pCreateInfo->pEnabledFeatures == nullptr);
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceFeatures2>(
                pPhysicalDevice,
                pPhysicalDeviceFeatures2);

            // If present, VkPhysicalDeviceFeatures2 controls which features are enabled instead of pEnabledFeatures
            pEnabledFeatures = &pPhysicalDeviceFeatures2->features;

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDevice16BitStorageFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDevice16BitStorageFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDevice8BitStorageFeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDevice8BitStorageFeaturesKHR*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_FEATURES_AMD:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceGpaFeaturesAMD>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceGpaFeaturesAMD*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceSamplerYcbcrConversionFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceSamplerYcbcrConversionFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceVariablePointerFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceVariablePointerFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceProtectedMemoryFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceProtectedMemoryFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceMultiviewFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceMultiviewFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceShaderDrawParameterFeatures>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceShaderDrawParameterFeatures*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceDescriptorIndexingFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceDescriptorIndexingFeaturesEXT *>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceFloat16Int8FeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceFloat16Int8FeaturesKHR*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceInlineUniformBlockFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceInlineUniformBlockFeaturesEXT*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceTransformFeedbackFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceTransformFeedbackFeaturesEXT*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceVulkanMemoryModelFeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceVulkanMemoryModelFeaturesKHR*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceShaderAtomicInt64FeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceShaderAtomicInt64FeaturesKHR*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceMemoryPriorityFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceMemoryPriorityFeaturesEXT*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceDepthClipEnableFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceDepthClipEnableFeaturesEXT*>(pHeader));

            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceHostQueryResetFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceHostQueryResetFeaturesEXT*>(pHeader));

            break;
            }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceBufferAddressFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceBufferAddressFeaturesEXT*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceLineRasterizationFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceLineRasterizationFeaturesEXT*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceSubgroupSizeControlFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceImagelessFramebufferFeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceImagelessFramebufferFeaturesKHR*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD:
        {
            const VkDeviceMemoryOverallocationCreateInfoAMD* pMemoryOverallocationCreateInfo =
                reinterpret_cast<const VkDeviceMemoryOverallocationCreateInfoAMD*>(pHeader);

            overallocationBehavior = pMemoryOverallocationCreateInfo->overallocationBehavior;

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT:
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT>(
                pPhysicalDevice,
                reinterpret_cast<const VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*>(pHeader));

            break;
        }

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD:
        {
            const VkPhysicalDeviceCoherentMemoryFeaturesAMD * pDeviceCoherentMemory =
                reinterpret_cast<const VkPhysicalDeviceCoherentMemoryFeaturesAMD *>(pHeader);
            vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceCoherentMemoryFeaturesAMD>(
                pPhysicalDevice,
                pDeviceCoherentMemory);

            if (vkResult == VK_SUCCESS)
            {
                deviceCoherentMemoryEnabled = enabledDeviceExtensions.IsExtensionEnabled(
                                                  DeviceExtensions::AMD_DEVICE_COHERENT_MEMORY) &&
                                              pDeviceCoherentMemory->deviceCoherentMemory;
            }

            break;
        }

        default:
            break;
        }
    }

    // If the pNext chain includes a VkPhysicalDeviceFeatures2 structure, then pEnabledFeatures must be NULL.
    if (pCreateInfo->pEnabledFeatures != nullptr)
    {
        vkResult = VerifyRequestedPhysicalDeviceFeatures<VkPhysicalDeviceFeatures>(
            pPhysicalDevice,
            pCreateInfo->pEnabledFeatures);
    }

    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    uint32_t totalQueues = 0;

    Pal::DeviceProperties properties = {};
    pPhysicalDevice->PalDevice()->GetProperties(&properties);

    for (pDeviceCreateInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
            case VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO:
                for (uint32_t i = 0; i < pDeviceCreateInfo->queueCreateInfoCount; i++)
                {
                    const VkDeviceQueueCreateInfo* pQueueInfo = &pDeviceCreateInfo->pQueueCreateInfos[i];

                    queueCounts[pQueueInfo->queueFamilyIndex] = pQueueInfo->queueCount;
                    totalQueues += pQueueInfo->queueCount;

                    queueFlags[pQueueInfo->queueFamilyIndex] = pQueueInfo->flags;

                    // handle global priority
                    union
                    {
                        const VkStructHeader*                                  pSubHeader;
                        const VkDeviceQueueGlobalPriorityCreateInfoEXT*        pPriorityInfo;
                    };

                    for (pSubHeader = reinterpret_cast<const VkStructHeader*>(pQueueInfo->pNext);
                         pSubHeader != nullptr;
                         pSubHeader = pSubHeader->pNext)
                    {

                        uint32_t totalDedicatedComputeUnits = 0;

                        switch (static_cast<uint32_t>(pSubHeader->sType))
                        {
                            case VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT:
                                queuePriority[pQueueInfo->queueFamilyIndex] = pPriorityInfo->globalPriority;
                                break;
                            default:
                                // Skip any unknown extension structures
                                break;
                        }
                    }
                }
                break;

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
            {
                const VkPhysicalDeviceMultiviewFeatures* pMultiviewFeatures =
                    reinterpret_cast<const VkPhysicalDeviceMultiviewFeatures*>(pHeader);

                // The implementation of multiview does not require special handling,
                // therefore the multview features can be ignored.
                VK_IGNORE(pMultiviewFeatures->multiview);
                VK_IGNORE(pMultiviewFeatures->multiviewGeometryShader);
                VK_IGNORE(pMultiviewFeatures->multiviewTessellationShader);

                break;
            }

            default:
                // Skip any unknown extension structures
                break;
        }
    }

    bool useComputeAsTransferQueue = false;

    // If the app requested any sparse residency features, but we can't reliably support PRT on transfer queues,
    // we have to fall-back on a compute queue instead and avoid using DMA.
    if ((pEnabledFeatures != nullptr) &&
        (pPhysicalDevices[DefaultDeviceIndex]->IsPrtSupportedOnDmaEngine() == false))
    {
        useComputeAsTransferQueue = (pEnabledFeatures->sparseResidencyBuffer  ||
                                     pEnabledFeatures->sparseResidencyImage2D ||
                                     pEnabledFeatures->sparseResidencyImage3D);
    }

    // Create the queues for the device up-front and hand them to the new device object.
    size_t apiDeviceSize = ObjectSize(sizeof(DispatchableDevice), numDevices);
    size_t apiQueueSize  = sizeof(DispatchableQueue);

    // Compute the amount of memory required for each queue type.
    size_t   palQueueMemorySize = 0;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;

    for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
    {
        for (queueIndex = 0; queueIndex < queueCounts[queueFamilyIndex]; queueIndex++)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
            {
                Pal::QueueCreateInfo queueCreateInfo = {};
                ConstructQueueCreateInfo(pPhysicalDevices,
                    deviceIdx,
                    queueFamilyIndex,
                    queueIndex,
                    dedicatedComputeUnits[queueFamilyIndex][queueIndex],
                    queuePriority[queueFamilyIndex],
                    &queueCreateInfo,
                    useComputeAsTransferQueue);

                palQueueMemorySize += pPalDevices[deviceIdx]->GetQueueSize(queueCreateInfo, &palResult);

                VK_ASSERT(palResult == Pal::Result::Success);
            }
        }
    }

    pMemory = pInstance->AllocMem(
        apiDeviceSize + (totalQueues * apiQueueSize) + palQueueMemorySize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;

    if ((pCreateInfo != nullptr) && (pMemory != nullptr))
    {
        vkResult = VK_SUCCESS;

        // Create barrier policy for the device.
        DeviceBarrierPolicy barrierPolicy(pPhysicalDevice,
                                          pCreateInfo,
                                          enabledDeviceExtensions);

        // Construct API device object.
        VK_INIT_DISPATCHABLE(Device, pMemory, (
            numDevices,
            pPhysicalDevices,
            pPalDevices,
            barrierPolicy,
            enabledDeviceExtensions,
            pEnabledFeatures,
            useComputeAsTransferQueue));

        DispatchableDevice* pDispatchableDevice = static_cast<DispatchableDevice*>(pMemory);
        DispatchableQueue*  pDispatchableQueues[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily] = {};

        void* pApiQueueMemory = Util::VoidPtrInc(pMemory, apiDeviceSize);
        void* pPalQueueMemory = Util::VoidPtrInc(pApiQueueMemory, (apiQueueSize * totalQueues));

        Pal::IQueue* pPalQueues[MaxPalDevices] = {};
        size_t       palQueueMemoryOffset      = 0;

        wchar_t executableName[PATH_MAX];
        wchar_t executablePath[PATH_MAX];
        utils::GetExecutableNameAndPath(executableName, executablePath);

        for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
        {
            for (queueIndex = 0; queueIndex < queueCounts[queueFamilyIndex]; queueIndex++)
            {
                // Create the Pal queues per device
                uint32_t deviceIdx;
                for (deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
                {
                    Pal::QueueCreateInfo queueCreateInfo = {};
                    ConstructQueueCreateInfo(pPhysicalDevices,
                                             deviceIdx,
                                             queueFamilyIndex,
                                             queueIndex,
                                             dedicatedComputeUnits[queueFamilyIndex][queueIndex],
                                             queuePriority[queueFamilyIndex],
                                             &queueCreateInfo,
                                             useComputeAsTransferQueue);

                    palResult = pPalDevices[deviceIdx]->CreateQueue(queueCreateInfo,
                                                            Util::VoidPtrInc(pPalQueueMemory, palQueueMemoryOffset),
                                                            &pPalQueues[deviceIdx]);

                    if (palResult != Pal::Result::Success)
                    {
                        break;
                    }

                    // On the creation of each command queue, the escape
                    // KMD_ESUBFUNC_UPDATE_APP_PROFILE_POWER_SETTING needs to be called, to provide the app's
                    // executable name and path. This lets KMD use the context created per queue for tracking
                    // the app.
                    palResult = pPalQueues[deviceIdx]->UpdateAppPowerProfile(static_cast<const wchar_t*>(executableName),
                                                                             static_cast<const wchar_t*>(executablePath));

                    if ((palResult != Pal::Result::Success) &&
                        (palResult != Pal::Result::Unsupported) &&
                        (palResult != Pal::Result::ErrorInvalidValue) &&
                        (palResult != Pal::Result::ErrorUnavailable))
                    {
                        pPalQueues[deviceIdx]->Destroy();
                        break;
                    }

                    palResult = Pal::Result::Success;

                    palQueueMemoryOffset += pPalDevices[deviceIdx]->GetQueueSize(queueCreateInfo, &palResult);
                }

                VirtualStackAllocator* pQueueStackAllocator = nullptr;

                if (palResult == Pal::Result::Success)
                {
                    palResult = pInstance->StackMgr()->AcquireAllocator(&pQueueStackAllocator);
                }

                if (palResult == Pal::Result::Success)
                {
                    // Create the vk::Queue object
                    VK_INIT_DISPATCHABLE(Queue, pApiQueueMemory, (
                        *pDispatchableDevice,
                        queueFamilyIndex,
                        queueIndex,
                        queueFlags[queueFamilyIndex],
                        pPalQueues,
                        pQueueStackAllocator));

                    pDispatchableQueues[queueFamilyIndex][queueIndex] = static_cast<DispatchableQueue*>(pApiQueueMemory);

                    pApiQueueMemory = Util::VoidPtrInc(pApiQueueMemory, apiQueueSize);
                }
                else
                {
                    while (deviceIdx-- > 0)
                    {
                        pPalQueues[deviceIdx]->Destroy();
                    }
                }
            }
        }

        // No matter how we exited the loops above, convert the PAL result and decide if we should continue
        // processing.
        vkResult = PalToVkResult(palResult);

        if (vkResult != VK_SUCCESS)
        {
            // Cleanup any successfully created queues before failure
            for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
            {
                for (queueIndex = 0; queueIndex < queueCounts[queueFamilyIndex]; queueIndex++)
                {
                    if (pDispatchableQueues[queueFamilyIndex][queueIndex] != nullptr)
                    {
                        Util::Destructor(static_cast<Queue*>(*(pDispatchableQueues[queueFamilyIndex][queueIndex])));
                    }
                }
            }

            // Free memory
            pInstance->FreeMem(pMemory);
        }
        else
        {
            vkResult = (*pDispatchableDevice)->Initialize(
                pPhysicalDevice,
                &pDispatchableQueues[0][0],
                enabledDeviceExtensions,
                overallocationBehavior,
                deviceCoherentMemoryEnabled);

            // If we've failed to Initialize, make sure we destroy anything we might have allocated.
            if (vkResult != VK_SUCCESS)
            {
                (*pDispatchableDevice)->Destroy(pAllocator);
            }
            else
            {
                *ppDevice = pDispatchableDevice;
            }
        }
    }

    return vkResult;
}

// =====================================================================================================================
// Bring up the Vulkan device.
VkResult Device::Initialize(
    PhysicalDevice*                         pPhysicalDevice,
    DispatchableQueue**                     pQueues,
    const DeviceExtensions::Enabled&        enabled,
    const VkMemoryOverallocationBehaviorAMD overallocationBehavior,
    const bool                              deviceCoherentMemoryEnabled)
{
    // Initialize the internal memory manager
    VkResult result = m_internalMemMgr.Init();

    // Initialize the render state cache
    if (result == VK_SUCCESS)
    {
        result = m_renderStateCache.Init();
    }

    if (result == VK_SUCCESS)
    {
        // Create a common CmdAllocator for internal use. For the driver setting, useSharedCmdAllocator,
        // this CmdAllocator will be used by all command buffers created by this device.
        // It must be thread safe because two threads could modify two command buffers at once
        // which may cause those command buffers to access the allocator simultaneously.
        Pal::CmdAllocatorCreateInfo createInfo = {};

        createInfo.flags.threadSafe               = 1;
        createInfo.flags.autoMemoryReuse          = 1;
        createInfo.flags.disableBusyChunkTracking = 1;

        // Initialize command data chunk allocation size
        createInfo.allocInfo[Pal::CommandDataAlloc].allocHeap = m_settings.cmdAllocatorDataHeap;
        createInfo.allocInfo[Pal::CommandDataAlloc].allocSize = m_settings.cmdAllocatorDataAllocSize;
        createInfo.allocInfo[Pal::CommandDataAlloc].suballocSize = m_settings.cmdAllocatorDataSubAllocSize;

        // Initialize embedded data chunk allocation size
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocHeap = m_settings.cmdAllocatorEmbeddedHeap;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocSize = m_settings.cmdAllocatorEmbeddedAllocSize;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].suballocSize = m_settings.cmdAllocatorEmbeddedSubAllocSize;

        // Initialize GPU scratch memory chunk allocation size
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocHeap = m_settings.cmdAllocatorScratchHeap;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocSize = m_settings.cmdAllocatorScratchAllocSize;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].suballocSize = m_settings.cmdAllocatorScratchSubAllocSize;

        Pal::Result  palResult = Pal::Result::Success;
        const size_t allocatorSize = PalDevice(DefaultDeviceIndex)->GetCmdAllocatorSize(createInfo, &palResult);

        if (palResult == Pal::Result::Success)
        {
            void* pAllocatorMem = m_pInstance->AllocMem(
                allocatorSize * NumPalDevices(), VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

            if (pAllocatorMem != NULL)
            {
                for (uint32_t deviceIdx = 0;
                    (deviceIdx < NumPalDevices()) && (palResult == Pal::Result::Success);
                    deviceIdx++)
                {
                    VK_ASSERT(allocatorSize == PalDevice(deviceIdx)->GetCmdAllocatorSize(createInfo, &palResult));

                    palResult = PalDevice(deviceIdx)->CreateCmdAllocator(createInfo,
                        Util::VoidPtrInc(pAllocatorMem, allocatorSize * deviceIdx),
                        &m_perGpu[deviceIdx].pSharedPalCmdAllocator);
                }
                result = PalToVkResult(palResult);

                if (result != VK_SUCCESS)
                {
                    m_pInstance->FreeMem(pAllocatorMem);
                }
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
        else
        {
            result = PalToVkResult(palResult);
        }
    }

    memcpy(&m_pQueues, pQueues, sizeof(m_pQueues));

    Pal::DeviceProperties deviceProps = {};
    result = PalToVkResult(PalDevice(DefaultDeviceIndex)->GetProperties(&deviceProps));

    m_properties.virtualMemAllocGranularity = deviceProps.gpuMemoryProperties.virtualMemAllocGranularity;
    m_properties.virtualMemPageSize         = deviceProps.gpuMemoryProperties.virtualMemPageSize;
    m_properties.descriptorSizes.bufferView = deviceProps.gfxipProperties.srdSizes.bufferView;
    m_properties.descriptorSizes.imageView  = deviceProps.gfxipProperties.srdSizes.imageView;
    m_properties.descriptorSizes.fmaskView  = deviceProps.gfxipProperties.srdSizes.fmaskView;
    m_properties.descriptorSizes.sampler    = deviceProps.gfxipProperties.srdSizes.sampler;
    // Size of combined image samplers is the sum of the image and sampler SRD sizes (8DW + 4DW)
    m_properties.descriptorSizes.combinedImageSampler =
        m_properties.descriptorSizes.imageView +
        m_properties.descriptorSizes.sampler;

    // The worst case alignment requirement of descriptors is always 2DWs. There's no way to query this from PAL yet,
    // but for now a hard coded value will do the job.
    m_properties.descriptorSizes.alignment = 2 * sizeof(uint32_t);

    m_properties.palSizes.colorTargetView  = PalDevice(DefaultDeviceIndex)->GetColorTargetViewSize(nullptr);
    m_properties.palSizes.depthStencilView = PalDevice(DefaultDeviceIndex)->GetDepthStencilViewSize(nullptr);

    m_properties.connectThroughThunderBolt = (deviceProps.pciProperties.flags.gpuConnectedViaThunderbolt) ? true : false;

    m_properties.timestampQueryPoolSlotSize =
        deviceProps.engineProperties[Pal::EngineTypeDma].minTimestampAlignment > 0 ?
        deviceProps.engineProperties[Pal::EngineTypeDma].minTimestampAlignment :
        deviceProps.engineProperties[Pal::EngineTypeUniversal].minTimestampAlignment;

    m_deviceCoherentMemoryEnabled = deviceCoherentMemoryEnabled;

    if (result == VK_SUCCESS)
    {
        result = CreateInternalPipelines();
    }

    if (result == VK_SUCCESS)
    {
        result = CreateBltMsaaStates();
    }

    if (result == VK_SUCCESS)
    {
        Pal::SamplePatternPalette palette = {};
        InitSamplePatternPalette(&palette);
        result = PalToVkResult(PalDevice(DefaultDeviceIndex)->SetSamplePatternPalette(palette));
    }

    if ((result == VK_SUCCESS) && VkInstance()->IsTracingSupportEnabled())
    {
        uint32_t queueFamilyIndex;
        uint32_t queueIndex;
        size_t   sqttQueueTotalSize = 0;

        for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
        {
            for (queueIndex = 0; m_pQueues[queueFamilyIndex][queueIndex] != nullptr; queueIndex++)
            {
                sqttQueueTotalSize += sizeof(SqttQueueState);
            }
        }

        void* pSqttStorage = VkInstance()->AllocMem(
            sizeof(SqttMgr) + sqttQueueTotalSize,
            VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pSqttStorage != nullptr)
        {
            m_pSqttMgr = VK_PLACEMENT_NEW(pSqttStorage) SqttMgr(this);
            pSqttStorage = Util::VoidPtrInc(pSqttStorage, sizeof(SqttMgr));
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        if (result == VK_SUCCESS)
        {
            for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
            {
                for (queueIndex = 0;
                    (m_pQueues[queueFamilyIndex][queueIndex] != nullptr) && (result == VK_SUCCESS);
                    queueIndex++)
                {
                    result = (*m_pQueues[queueFamilyIndex][queueIndex])->CreateSqttState(pSqttStorage);
                    pSqttStorage = Util::VoidPtrInc(pSqttStorage, sizeof(SqttQueueState));
                }
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        switch (GetAppProfile())
        {
        case AppProfile::StrangeBrigade:
        {
            void* pMemory = VkInstance()->AllocMem(sizeof(StrangeBrigadeLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

            if (pMemory != nullptr)
            {
                m_pAppOptLayer = VK_PLACEMENT_NEW(pMemory) StrangeBrigadeLayer();
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            break;
        }
        case AppProfile::WolfensteinII:
            // This application optimization layer is currently GFX10-specific
            if (deviceProps.gfxLevel >= Pal::GfxIpLevel::GfxIp10_1)
            {
                void* pMemory = VkInstance()->AllocMem(sizeof(Wolfenstein2Layer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

                if (pMemory != nullptr)
                {
                    m_pAppOptLayer = VK_PLACEMENT_NEW(pMemory) Wolfenstein2Layer();
                }
                else
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
            break;
        default:
            break;
        }
    }

    if ((result == VK_SUCCESS) && (m_settings.barrierFilterOptions != BarrierFilterDisabled))
    {
        void* pMemory = VkInstance()->AllocMem(sizeof(BarrierFilterLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMemory != nullptr)
        {
            m_pBarrierFilterLayer = VK_PLACEMENT_NEW(pMemory) BarrierFilterLayer();
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if ((result == VK_SUCCESS) && m_settings.enableAsyncCompile)
    {
        void* pMemory = VkInstance()->AllocMem(sizeof(AsyncLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMemory != nullptr)
        {
            m_pAsyncLayer = VK_PLACEMENT_NEW(pMemory) AsyncLayer(this);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    if (result == VK_SUCCESS)
    {
        result = PalToVkResult(m_memoryMutex.Init());
    }

    if (result == VK_SUCCESS)
    {
        if (m_settings.memoryDeviceOverallocationNonOverridable == false)
        {
            AppProfile profile = GetAppProfile();

            switch (profile)
            {
            case AppProfile::Doom:
            case AppProfile::DoomVFR:
            case AppProfile::WolfensteinII:
            case AppProfile::Dota2:
            case AppProfile::Talos:
            case AppProfile::TalosVR:
            case AppProfile::SeriousSamFusion:
            case AppProfile::MadMax:
            case AppProfile::F1_2017:
            case AppProfile::RiseOfTheTombra:
            case AppProfile::ThronesOfBritannia:
            case AppProfile::DawnOfWarIII:
            case AppProfile::AshesOfTheSingularity:
            case AppProfile::StrangeBrigade:
                m_allocationSizeTracking = false;
                break;
            default:
                break;
            }

            if (enabled.IsExtensionEnabled(DeviceExtensions::ExtensionId::AMD_MEMORY_OVERALLOCATION_BEHAVIOR))
            {
                switch (overallocationBehavior)
                {
                case VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD:
                    m_allocationSizeTracking = false;
                    m_overallocationRequestedForPalHeap[Pal::GpuHeap::GpuHeapInvisible] = true;
                    m_overallocationRequestedForPalHeap[Pal::GpuHeap::GpuHeapLocal] = true;
                    break;
                case VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD:
                    m_allocationSizeTracking = true;
                    break;
                default:
                    break;
                }
            }
        }
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    if ((result == VK_SUCCESS) && (VkInstance()->GetDevModeMgr() != nullptr))
    {
        VkInstance()->GetDevModeMgr()->PostDeviceCreate(this);
    }
#endif

    if (result == VK_SUCCESS)
    {
        // Finalize the device settings after driver intitalization is done
        // This essentially generates settings hash
        pPhysicalDevice->GetSettingsLoader()->FinalizeSettings();

        // Get the current values of driver features, from an app profile or global settings.
        UpdateFeatureSettings();
    }

    if (result == VK_SUCCESS)
    {
        InitDispatchTable();
    }

    return result;
}

// =====================================================================================================================
// This function initializes the device dispatch table and allows the chance to override entries in it if necessary.
// NOTE: Any entry points overridden in the instance dispatch table may need to be also overriden in the device dispatch
// table as the overrides are not inherited.
void Device::InitDispatchTable()
{
    // =================================================================================================================
    // Initialize dispatch table.
    m_dispatchTable.Init();

    // =================================================================================================================
    // Override dispatch table entries.
    EntryPoints* ep = m_dispatchTable.OverrideEntryPoints();

    ep->vkUpdateDescriptorSets      = DescriptorUpdate::GetUpdateDescriptorSetsFunc(this);
    ep->vkCmdBindDescriptorSets     = CmdBuffer::GetCmdBindDescriptorSetsFunc(this);
    ep->vkCreateDescriptorPool      = DescriptorPool::GetCreateDescriptorPoolFunc(this);
    ep->vkFreeDescriptorSets        = DescriptorPool::GetFreeDescriptorSetsFunc(this);
    ep->vkResetDescriptorPool       = DescriptorPool::GetResetDescriptorPoolFunc(this);
    ep->vkAllocateDescriptorSets    = DescriptorPool::GetAllocateDescriptorSetsFunc(this);

    // =================================================================================================================
    // After generic overrides, apply any internal layer specific dispatch table override.

    // Install SQTT marker annotation layer if needed
    if (m_pSqttMgr != nullptr)
    {
        SqttOverrideDispatchTable(&m_dispatchTable, m_pSqttMgr);
    }

    // Install the app-specific layer if needed
    if (m_pAppOptLayer != nullptr)
    {
        m_pAppOptLayer->OverrideDispatchTable(&m_dispatchTable);
    }

    // Install the barrier filter layer if needed
    if (m_pBarrierFilterLayer != nullptr)
    {
        m_pBarrierFilterLayer->OverrideDispatchTable(&m_dispatchTable);
    }

    // Install the async compile layer if needed
    if (m_pAsyncLayer != nullptr)
    {
        m_pAsyncLayer->OverrideDispatchTable(&m_dispatchTable);
    }
}

// =====================================================================================================================
// Initialize the specified sample pattern palette with default values.
void Device::InitSamplePatternPalette(
    Pal::SamplePatternPalette* pPalette     // [in,out] Sample pattern palette to be filled
    ) const
{
    Pal::SamplePos* pSamplePos = pPalette[0][0];

    // Initialize sample pattern palette with zeros
    memset(pSamplePos, 0, sizeof(Pal::SamplePatternPalette));

    // Default sample patterns
    static const Pal::Offset2d DefaultSamplePattern1x[Pal::MaxMsaaRasterizerSamples] =
    {
        VK_DEFAULT_SAMPLE_PATTERN_1X
    };

    static const Pal::Offset2d DefaultSamplePattern2x[Pal::MaxMsaaRasterizerSamples] =
    {
        VK_DEFAULT_SAMPLE_PATTERN_2X
    };

    static const Pal::Offset2d DefaultSamplePattern4x[Pal::MaxMsaaRasterizerSamples] =
    {
        VK_DEFAULT_SAMPLE_PATTERN_4X
    };

    static const Pal::Offset2d DefaultSamplePattern8x[Pal::MaxMsaaRasterizerSamples] =
    {
        VK_DEFAULT_SAMPLE_PATTERN_8X
    };

    static const Pal::Offset2d DefaultSamplePattern16x[Pal::MaxMsaaRasterizerSamples] =
    {
        VK_DEFAULT_SAMPLE_PATTERN_16X
    };

    static const Pal::Offset2d* DefaultSamplePatterns[] =
    {
        DefaultSamplePattern1x,
        DefaultSamplePattern2x,
        DefaultSamplePattern4x,
        DefaultSamplePattern8x,
        DefaultSamplePattern16x,
    };

    const uint32_t patternCount = sizeof(DefaultSamplePatterns) / sizeof(DefaultSamplePatterns[0]);
    for (uint32_t pattern = 0; pattern < patternCount; pattern++)
    {
        const Pal::Offset2d* pPattern = DefaultSamplePatterns[pattern];

        for (uint32_t entry = 0; entry < Pal::MaxMsaaRasterizerSamples; entry++)
        {
            // Convert each pair of sample positions to continuous coordinates (floating-point values), dividing
            // them by 16.
            const float oneSixteen = 1.f / 16;

            pSamplePos->x = static_cast<float>(pPattern[entry].x) * oneSixteen;
            pSamplePos->y = static_cast<float>(pPattern[entry].y) * oneSixteen;
            pSamplePos++;
        }
    }
}

// =====================================================================================================================
// Get the default Quad sample pattern based on the specified sample count.
const Pal::MsaaQuadSamplePattern* Device::GetDefaultQuadSamplePattern(
    uint32_t sampleCount) // Sample count
{
    const Pal::MsaaQuadSamplePattern* pQuadPattern = nullptr;

    // Default quad sample patterns
    static const Pal::MsaaQuadSamplePattern DefaultQuadSamplePattern1x =
    {
        { VK_DEFAULT_SAMPLE_PATTERN_1X },
        { VK_DEFAULT_SAMPLE_PATTERN_1X },
        { VK_DEFAULT_SAMPLE_PATTERN_1X },
        { VK_DEFAULT_SAMPLE_PATTERN_1X },
    };

    static const Pal::MsaaQuadSamplePattern DefaultQuadSamplePattern2x =
    {
        { VK_DEFAULT_SAMPLE_PATTERN_2X },
        { VK_DEFAULT_SAMPLE_PATTERN_2X },
        { VK_DEFAULT_SAMPLE_PATTERN_2X },
        { VK_DEFAULT_SAMPLE_PATTERN_2X },
    };

    static const Pal::MsaaQuadSamplePattern DefaultQuadSamplePattern4x =
    {
        { VK_DEFAULT_SAMPLE_PATTERN_4X },
        { VK_DEFAULT_SAMPLE_PATTERN_4X },
        { VK_DEFAULT_SAMPLE_PATTERN_4X },
        { VK_DEFAULT_SAMPLE_PATTERN_4X },
    };

    static const Pal::MsaaQuadSamplePattern DefaultQuadSamplePattern8x =
    {
        { VK_DEFAULT_SAMPLE_PATTERN_8X },
        { VK_DEFAULT_SAMPLE_PATTERN_8X },
        { VK_DEFAULT_SAMPLE_PATTERN_8X },
        { VK_DEFAULT_SAMPLE_PATTERN_8X },
    };

    static const Pal::MsaaQuadSamplePattern DefaultQuadSamplePattern16x =
    {
        { VK_DEFAULT_SAMPLE_PATTERN_16X },
        { VK_DEFAULT_SAMPLE_PATTERN_16X },
        { VK_DEFAULT_SAMPLE_PATTERN_16X },
        { VK_DEFAULT_SAMPLE_PATTERN_16X },
    };

    switch (sampleCount)
    {
    case 1:
        pQuadPattern = &DefaultQuadSamplePattern1x;
        break;
    case 2:
        pQuadPattern = &DefaultQuadSamplePattern2x;
        break;
    case 4:
        pQuadPattern = &DefaultQuadSamplePattern4x;
        break;
    case 8:
        pQuadPattern = &DefaultQuadSamplePattern8x;
        break;
    case 16:
        pQuadPattern = &DefaultQuadSamplePattern16x;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return pQuadPattern;
}

// =====================================================================================================================
// Get table index of the default sample pattern based on the specified sample count.
uint32_t Device::GetDefaultSamplePatternIndex(
    uint32_t sampleCount)   // Sample count
{
    uint32_t patternIndex = 0;

    // Table indices of default sample patterns
    static const uint32_t DefaultSamplePatternIdx1x  = 0;
    static const uint32_t DefaultSamplePatternIdx2x  = 1;
    static const uint32_t DefaultSamplePatternIdx4x  = 2;
    static const uint32_t DefaultSamplePatternIdx8x  = 3;
    static const uint32_t DefaultSamplePatternIdx16x = 4;

    switch (sampleCount)
    {
    case 1:
        patternIndex = DefaultSamplePatternIdx1x;
        break;
    case 2:
        patternIndex = DefaultSamplePatternIdx2x;
        break;
    case 4:
        patternIndex = DefaultSamplePatternIdx4x;
        break;
    case 8:
        patternIndex = DefaultSamplePatternIdx8x;
        break;
    case 16:
        patternIndex = DefaultSamplePatternIdx16x;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return patternIndex;
}

// =====================================================================================================================
// Destroy Vulkan device. Destroy underlying PAL device, call destructor and free memory.
VkResult Device::Destroy(const VkAllocationCallbacks* pAllocator)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    if (VkInstance()->GetDevModeMgr() != nullptr)
    {
        VkInstance()->GetDevModeMgr()->PreDeviceDestroy(this);
    }
#endif

    if (m_pSqttMgr != nullptr)
    {
        for (uint32_t i = 0; i < Queue::MaxQueueFamilies; ++i)
        {
            for (uint32_t j = 0; (j < Queue::MaxQueuesPerFamily) && (m_pQueues[i][j] != nullptr); ++j)
            {
                Util::Destructor((*m_pQueues[i][j])->GetSqttState());
            }
        }

        Util::Destructor(m_pSqttMgr);

        VkInstance()->FreeMem(m_pSqttMgr);
    }

    if (m_pBarrierFilterLayer != nullptr)
    {
        Util::Destructor(m_pBarrierFilterLayer);

        VkInstance()->FreeMem(m_pBarrierFilterLayer);
    }

    if (m_pAppOptLayer != nullptr)
    {
        Util::Destructor(m_pAppOptLayer);

        VkInstance()->FreeMem(m_pAppOptLayer);
    }

    if (m_pAsyncLayer != nullptr)
    {
        Util::Destructor(m_pAsyncLayer);

        VkInstance()->FreeMem(m_pAsyncLayer);
    }

    for (uint32_t i = 0; i < Queue::MaxQueueFamilies; ++i)
    {
        for (uint32_t j = 0; (j < Queue::MaxQueuesPerFamily) && (m_pQueues[i][j] != nullptr); ++j)
        {
            Util::Destructor(static_cast<Queue*>(*m_pQueues[i][j]));
        }
    }

    for (uint32_t i = 0; i < BltMsaaStateCount; ++i)
    {
        m_renderStateCache.DestroyMsaaState(&m_pBltMsaaState[i][0], nullptr);
    }

    DestroyInternalPipelines();

    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        if (m_perGpu[deviceIdx].pSharedPalCmdAllocator != nullptr)
        {
            m_perGpu[deviceIdx].pSharedPalCmdAllocator->Destroy();
        }
    }
    VkInstance()->FreeMem(m_perGpu[DefaultDeviceIndex].pSharedPalCmdAllocator);

    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        if (m_perGpu[deviceIdx].pSwCompositingMemory != nullptr)
        {
            if (m_perGpu[deviceIdx].pSwCompositingSemaphore != nullptr)
            {
                m_perGpu[deviceIdx].pSwCompositingSemaphore->Destroy();
            }

            if (m_perGpu[deviceIdx].pSwCompositingQueue != nullptr)
            {
                m_perGpu[deviceIdx].pSwCompositingQueue->Destroy();
            }

            VkInstance()->FreeMem(m_perGpu[deviceIdx].pSwCompositingMemory);
        }
    }

    m_renderStateCache.Destroy();

    Util::Destructor(this);

    VkInstance()->FreeMem(ApiDevice::FromObject(this));

    return VK_SUCCESS;
}

// =====================================================================================================================
Pal::QueueType Device::GetQueueFamilyPalQueueType(
    uint32_t queueFamilyIndex) const
{
    auto palQueueType = VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalQueueType(queueFamilyIndex);

    if ((palQueueType == Pal::QueueType::QueueTypeDma) && m_useComputeAsTransferQueue)
    {
        palQueueType = Pal::QueueType::QueueTypeCompute;
    }

    return palQueueType;
}

// =====================================================================================================================
Pal::EngineType Device::GetQueueFamilyPalEngineType(
    uint32_t queueFamilyIndex) const
{
    auto palEngineType = VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalEngineType(queueFamilyIndex);

    if ((palEngineType == Pal::EngineType::EngineTypeDma) && m_useComputeAsTransferQueue)
    {
        palEngineType = Pal::EngineType::EngineTypeCompute;
    }

    return palEngineType;
}

// =====================================================================================================================
VkResult Device::CreateInternalComputePipeline(
    size_t                           codeByteSize,
    const uint8_t*                   pCode,
    uint32_t                         numUserDataNodes,
    const Llpc::ResourceMappingNode* pUserDataNodes,
    InternalPipeline*                pInternalPipeline)
{
    VK_ASSERT(numUserDataNodes <= VK_ARRAY_SIZE(pInternalPipeline->userDataNodeOffsets));

    VkResult             result              = VK_SUCCESS;
    PipelineCompiler*    pCompiler           = GetCompiler(DefaultDeviceIndex);
    ShaderModuleHandle   shaderModule        = {};
    const void*          pPipelineBinary     = nullptr;
    size_t               pipelineBinarySize  = 0;

    void*                pPipelineMem        = nullptr;

    ComputePipelineCreateInfo pipelineBuildInfo = {};

    // Build shader module
    result = pCompiler->BuildShaderModule(
        this,
        0,
        codeByteSize,
        pCode,
        &shaderModule);

    if (result == VK_SUCCESS)
    {
        // Build pipeline binary
        auto pShaderInfo = &pipelineBuildInfo.pipelineInfo.cs;
        pipelineBuildInfo.compilerType = PipelineCompilerTypeLlpc;
        pShaderInfo->pModuleData         = shaderModule.pLlpcShaderModule;
        pShaderInfo->pSpecializationInfo = nullptr;
        pShaderInfo->pEntryTarget        = "main";
        pShaderInfo->entryStage          = Llpc::ShaderStageCompute;
        pShaderInfo->pUserDataNodes      = pUserDataNodes;
        pShaderInfo->userDataNodeCount   = numUserDataNodes;

        pCompiler->ApplyDefaultShaderOptions(ShaderStageCompute,
                                             &pShaderInfo->options
                                             );

        Util::MetroHash::Hash cacheId;
        result = pCompiler->CreateComputePipelineBinary(this,
                                                        0,
                                                        nullptr,
                                                        &pipelineBuildInfo,
                                                        &pipelineBinarySize,
                                                        &pPipelineBinary,
                                                        &cacheId);

        pipelineBuildInfo.pMappingBuffer = nullptr;
    }

    Pal::IPipeline*      pPipeline[MaxPalDevices] = {};
    if (result == VK_SUCCESS)
    {
        Pal::ComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.flags.clientInternal = true;
        pipelineInfo.pPipelineBinary      = pPipelineBinary;
        pipelineInfo.pipelineBinarySize   = pipelineBinarySize;

        const size_t pipelineSize = PalDevice(DefaultDeviceIndex)->GetComputePipelineSize(pipelineInfo, nullptr);

        pPipelineMem = VkInstance()->AllocMem(pipelineSize * NumPalDevices(), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pPipelineMem != nullptr)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
            {
                result = PalToVkResult(PalDevice(deviceIdx)->CreateComputePipeline(
                    pipelineInfo, Util::VoidPtrInc(pPipelineMem, pipelineSize * deviceIdx), &pPipeline[deviceIdx]));
            }
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Cleanup
    pCompiler->FreeShaderModule(&shaderModule);

    if ((pPipelineBinary) || (result != VK_SUCCESS))
    {
        pCompiler->FreeComputePipelineBinary(&pipelineBuildInfo, pPipelineBinary, pipelineBinarySize);
        pCompiler->FreeComputePipelineCreateInfo(&pipelineBuildInfo);
    }

    if (result == VK_SUCCESS)
    {
        VK_ASSERT(pPipeline[0] == pPipelineMem);

        for (uint32_t i = 0; i < numUserDataNodes; ++i)
        {
            pInternalPipeline->userDataNodeOffsets[i] = pUserDataNodes[i].offsetInDwords;
        }
        memcpy(pInternalPipeline->pPipeline, pPipeline, sizeof(pPipeline));
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
        {
            if (pPipeline[deviceIdx] != nullptr)
            {
                pPipeline[deviceIdx]->Destroy();
            }
        }

        VkInstance()->FreeMem(pPipelineMem);
    }

    return result;
}

// =====================================================================================================================
VkResult Device::CreateInternalPipelines()
{
    VkResult result = VK_SUCCESS;

    const bool useStridedShader = UseStridedCopyQueryResults();
    VK_ASSERT((m_properties.timestampQueryPoolSlotSize == 8) || (m_properties.timestampQueryPoolSlotSize == 32));

    // Create the compute pipeline to copy timestamp query pool results to a buffer
    static constexpr uint8_t CopyTimestampQueryPoolSpv[] =
    {
    #include "shaders/copy_timestamp_query_pool_spv.h"
    };

    static constexpr uint8_t CopyTimestampQueryPoolStridedSpv[] =
    {
    #include "shaders/copy_timestamp_query_pool_strided_spv.h"
    };

    const uint8_t* pSpvCode  = useStridedShader ? CopyTimestampQueryPoolStridedSpv : CopyTimestampQueryPoolSpv;
    const size_t spvCodeSize = useStridedShader ?
        sizeof(CopyTimestampQueryPoolStridedSpv) : sizeof(CopyTimestampQueryPoolSpv);

    Llpc::ResourceMappingNode userDataNodes[3] = {};

    const uint32_t uavViewSize = m_properties.descriptorSizes.bufferView / sizeof(uint32_t);

    // Timestamp counter storage view
    userDataNodes[0].type = useStridedShader ?
        Llpc::ResourceMappingNodeType::DescriptorBuffer : Llpc::ResourceMappingNodeType::DescriptorTexelBuffer;
    userDataNodes[0].offsetInDwords = 0;
    userDataNodes[0].sizeInDwords = uavViewSize;
    userDataNodes[0].srdRange.set = 0;
    userDataNodes[0].srdRange.binding = 0;

    // Copy destination storage view
    userDataNodes[1].type = Llpc::ResourceMappingNodeType::DescriptorBuffer;
    userDataNodes[1].offsetInDwords = uavViewSize;
    userDataNodes[1].sizeInDwords = uavViewSize;
    userDataNodes[1].srdRange.set = 0;
    userDataNodes[1].srdRange.binding = 1;

    // Inline constant data
    userDataNodes[2].type = Llpc::ResourceMappingNodeType::PushConst;
    userDataNodes[2].offsetInDwords = 2 * uavViewSize;
    userDataNodes[2].sizeInDwords = 4;
    userDataNodes[2].srdRange.set = Llpc::InternalDescriptorSetId;

    result = CreateInternalComputePipeline(
        spvCodeSize,
        pSpvCode,
        VK_ARRAY_SIZE(userDataNodes),
        userDataNodes,
        &m_timestampQueryCopyPipeline);

    return result;
}

// =====================================================================================================================
void Device::DestroyInternalPipeline(
    InternalPipeline* pPipeline)
{
    void* pAllocMem = pPipeline->pPipeline[0];

    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        if (pPipeline->pPipeline[deviceIdx] != nullptr)
        {
            pPipeline->pPipeline[deviceIdx]->Destroy();
            pPipeline->pPipeline[deviceIdx] = nullptr;
        }
    }

    VkInstance()->FreeMem(pAllocMem);
}

// =====================================================================================================================
void Device::DestroyInternalPipelines()
{
    DestroyInternalPipeline(&m_timestampQueryCopyPipeline);
}

// =====================================================================================================================
// Wait for device idle. Punts to PAL device.
VkResult Device::WaitIdle(void)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t i = 0; (i < Queue::MaxQueueFamilies) && (result == VK_SUCCESS); ++i)
    {
        for (uint32_t j = 0;
            (j < Queue::MaxQueuesPerFamily) && (m_pQueues[i][j] != nullptr) && (result == VK_SUCCESS);
            ++j)
        {
            result = (*m_pQueues[i][j])->WaitIdle();
        }
    }

    return result;
}

// =====================================================================================================================
// Creates a new GPU memory object
VkResult Device::AllocMemory(
    const VkMemoryAllocateInfo*     pAllocInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkDeviceMemory*                 pMemory)
{
    // Simply call the static memory object creation function
    return Memory::Create(this, pAllocInfo, pAllocator, pMemory);
}

// =====================================================================================================================
// Creates a new event object
VkResult Device::CreateEvent(
    const VkEventCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkEvent*                        pEvent)
{
    return Event::Create(this, pCreateInfo, pAllocator, pEvent);
}

// =====================================================================================================================
// Creates a new fence object
VkResult Device::CreateFence(
    const VkFenceCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkFence*                        pFence)
{
    return Fence::Create(this, pCreateInfo, pAllocator, pFence);
}

// =====================================================================================================================
VkResult Device::GetQueue(
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue)
{
    *pQueue = reinterpret_cast<VkQueue>(m_pQueues[queueFamilyIndex][queueIndex]);

    return VK_SUCCESS;
}
// =====================================================================================================================
Pal::PrtFeatureFlags Device::GetPrtFeatures() const
{
    const Pal::PrtFeatureFlags featureFlags = VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures();

    for (uint32_t deviceIdx = 1; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        VK_ASSERT(featureFlags == VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures());
    }

    return featureFlags;
}

// =====================================================================================================================
VkResult Device::WaitForFences(
    uint32_t       fenceCount,
    const VkFence* pFences,
    VkBool32       waitAll,
    uint64_t       timeout)
{
    Pal::Result palResult = Pal::Result::Success;

    Pal::IFence** ppPalFences = static_cast<Pal::IFence**>(VK_ALLOC_A(sizeof(Pal::IFence*) * fenceCount));

    if (IsMultiGpu() == false)
    {
        for (uint32_t i = 0; i < fenceCount; ++i)
        {
            ppPalFences[i] = Fence::ObjectFromHandle(pFences[i])->PalFence(DefaultDeviceIndex);
        }

        palResult = PalDevice(DefaultDeviceIndex)->WaitForFences(fenceCount, ppPalFences, waitAll != VK_FALSE, timeout);
    }
    else
    {
        for (uint32_t deviceIdx = 0;
             (deviceIdx < NumPalDevices()) && (palResult == Pal::Result::Success);
             deviceIdx++)
        {
            const uint32_t currentDeviceMask = 1 << deviceIdx;

            uint32_t perDeviceFenceCount = 0;
            for (uint32_t i = 0; i < fenceCount; ++i)
            {
                Fence* pFence = Fence::ObjectFromHandle(pFences[i]);

                // Some conformance tests will wait on fences that were never submitted, so use only the first device
                // for these cases.
                const bool forceWait = (pFence->GetActiveDeviceMask() == 0) && (deviceIdx == DefaultDeviceIndex);

                if (forceWait || ((currentDeviceMask & pFence->GetActiveDeviceMask()) != 0))
                {
                    ppPalFences[perDeviceFenceCount++] = pFence->PalFence(deviceIdx);
                }
            }

            if (perDeviceFenceCount > 0)
            {
                palResult = PalDevice(deviceIdx)->WaitForFences(perDeviceFenceCount,
                                                                ppPalFences,
                                                                waitAll != VK_FALSE,
                                                                timeout);
            }
        }
    }
    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Resets the specified fences.
VkResult Device::ResetFences(
    uint32_t       fenceCount,
    const VkFence* pFences)
{
    Pal::IFence** ppPalFences = static_cast<Pal::IFence**>(VK_ALLOC_A(sizeof(Pal::IFence*) * fenceCount));

    Pal::Result palResult = Pal::Result::Success;

    // Clear the wait masks for each fence
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        Fence::ObjectFromHandle(pFences[i])->ClearActiveDeviceMask();
        Fence::ObjectFromHandle(pFences[i])->RestoreFence(this);
    }

    for (uint32_t deviceIdx = 0;
        (deviceIdx < NumPalDevices()) && (palResult == Pal::Result::Success);
        deviceIdx++)
    {
        for (uint32_t i = 0; i < fenceCount; ++i)
        {
            Fence* pFence = Fence::ObjectFromHandle(pFences[i]);
            ppPalFences[i] = pFence->PalFence(deviceIdx);
        }

        palResult = PalDevice(deviceIdx)->ResetFences(fenceCount, ppPalFences);
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult Device::CreateDescriptorSetLayout(
    const VkDescriptorSetLayoutCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*                 pAllocator,
    VkDescriptorSetLayout*                       pSetLayout)
{
    return DescriptorSetLayout::Create(this, pCreateInfo, pAllocator, pSetLayout);
}

// =====================================================================================================================
VkResult Device::CreateDescriptorUpdateTemplate(
    const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorUpdateTemplate*                 pDescriptorUpdateTemplate)
{
    return DescriptorUpdateTemplate::Create(this, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

// =====================================================================================================================
VkResult Device::CreatePipelineLayout(
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineLayout*                           pPipelineLayout)
{
    return PipelineLayout::Create(this, pCreateInfo, pAllocator, pPipelineLayout);
}

// =====================================================================================================================
// Allocate one or more command buffers.
VkResult Device::AllocateCommandBuffers(
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer*                   pCommandBuffers)
{
    return CmdBuffer::Create(
        this,
        pAllocateInfo,
        pCommandBuffers);
}

// =====================================================================================================================
VkResult Device::CreateFramebuffer(
    const VkFramebufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkFramebuffer*                 pFramebuffer)
{
    return Framebuffer::Create(this, pCreateInfo, pAllocator, pFramebuffer);
}

// =====================================================================================================================
VkResult Device::CreateCommandPool(
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkCommandPool*                 pCmdPool)
{
    return CmdPool::Create(this, pCreateInfo, pAllocator, pCmdPool);
}

// =====================================================================================================================
VkResult Device::CreateShaderModule(
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkShaderModule*                 pShaderModule)
{
    return ShaderModule::Create(this, pCreateInfo, pAllocator, pShaderModule);
}

// =====================================================================================================================
VkResult Device::CreatePipelineCache(
    const VkPipelineCacheCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*     pAllocator,
    VkPipelineCache*                 pPipelineCache)
{
    return PipelineCache::Create(this, pCreateInfo, pAllocator, pPipelineCache);
}

// =====================================================================================================================
VkResult Device::CreateRenderPass(
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*  pAllocator,
    VkRenderPass*                 pRenderPass)
{
    return RenderPass::Create(this, pCreateInfo, pAllocator, pRenderPass);
}

// =====================================================================================================================
VkResult Device::CreateRenderPass2(
    const VkRenderPassCreateInfo2KHR*   pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkRenderPass*                       pRenderPass)
{
    return RenderPass::Create(this, pCreateInfo, pAllocator, pRenderPass);
}

// =====================================================================================================================
VkResult Device::CreateBuffer(
    const VkBufferCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBuffer*                                   pBuffer)
{
    return Buffer::Create(this, pCreateInfo, pAllocator, pBuffer);
}

// =====================================================================================================================
VkResult Device::CreateBufferView(
    const VkBufferViewCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*       pAllocator,
    VkBufferView*                      pView)
{
    return BufferView::Create(this, pCreateInfo, pAllocator, pView);
}

// =====================================================================================================================
VkResult Device::CreateImage(
    const VkImageCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImage*                                    pImage)
{
    return Image::Create(this, pCreateInfo, pAllocator, pImage);
}

// =====================================================================================================================
VkResult Device::CreateImageView(
    const VkImageViewCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*      pAllocator,
    VkImageView*                      pView)
{
    return ImageView::Create(this, pCreateInfo, pAllocator, 0, pView);
}

// =====================================================================================================================
VkResult Device::CreateGraphicsPipelines(
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    VkResult finalResult = VK_SUCCESS;
    PipelineCache* pPipelineCache = PipelineCache::ObjectFromHandle(pipelineCache);

    for (uint32_t i = 0; i < count; ++i)
    {
        VkResult result = GraphicsPipeline::Create(
            this,
            pPipelineCache,
            &pCreateInfos[i],
            pAllocator,
            &pPipelines[i]);

        if (result != VK_SUCCESS)
        {
            // We should return null handle in case of failure.
            pPipelines[i] = VK_NULL_HANDLE;
            finalResult = result;
        }
    }

    return finalResult;
}

// =====================================================================================================================
VkResult Device::CreateComputePipelines(
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    VkResult finalResult = VK_SUCCESS;
    PipelineCache* pPipelineCache = PipelineCache::ObjectFromHandle(pipelineCache);

    for (uint32_t i = 0; i < count; ++i)
    {
        VkResult result = ComputePipeline::Create(
            this,
            pPipelineCache,
            &pCreateInfos[i],
            pAllocator,
            &pPipelines[i]);

        if (result != VK_SUCCESS)
        {
            // We should return null handle in case of failure.
            pPipelines[i] = VK_NULL_HANDLE;
            finalResult = result;
        }
    }

    return finalResult;
}

// =====================================================================================================================
// Called in response to vkGetDeviceGroupPeerMemoryFeatures
void Device::GetDeviceGroupPeerMemoryFeatures(
    uint32_t                  heapIndex,
    uint32_t                  localDeviceIndex,
    uint32_t                  remoteDeviceIndex,
    VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) const
{
    uint32_t enabledFeatures = 0;

    if (localDeviceIndex != remoteDeviceIndex)
    {
        const Pal::GpuHeap palHeap = GetPalHeapFromVkTypeIndex(heapIndex);

        enabledFeatures |= VK_PEER_MEMORY_FEATURE_COPY_DST_BIT;

        switch (palHeap)
        {
            case Pal::GpuHeapLocal:
            case Pal::GpuHeapInvisible:
                break;
            case Pal::GpuHeapGartUswc:
            case Pal::GpuHeapGartCacheable:
                break;
            default:
                VK_NOT_IMPLEMENTED;
                break;
        };
    }

    *pPeerMemoryFeatures = enabledFeatures;
}

// =====================================================================================================================
VkResult Device::GetDeviceGroupPresentCapabilities(
    VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) const
{
    VK_ASSERT(pDeviceGroupPresentCapabilities->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR);

    GetDeviceGroupSurfacePresentModes(VK_NULL_HANDLE, &pDeviceGroupPresentCapabilities->modes);

    memset(pDeviceGroupPresentCapabilities->presentMask, 0, sizeof(pDeviceGroupPresentCapabilities->presentMask));
    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
         pDeviceGroupPresentCapabilities->presentMask[deviceIdx] = (1 << deviceIdx);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Device::GetDeviceGroupSurfacePresentModes(
    VkSurfaceKHR                      surface,
    VkDeviceGroupPresentModeFlagsKHR* pModes) const
{
    *pModes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;

        //      VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR;

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Device::BindBufferMemory(
    uint32_t                        bindInfoCount,
    const VkBindBufferMemoryInfo*   pBindInfos) const
{
    union
    {
        const VkStructHeader*                    pHeader;
        const VkBindBufferMemoryInfo*            pVkBindBufferMemoryInfo;
        const VkBindBufferMemoryDeviceGroupInfo* pVkBindBufferMemoryDeviceGroupInfo;
    };

    for (uint32_t bindIdx = 0; bindIdx < bindInfoCount; bindIdx++)
    {
        const VkBindBufferMemoryInfo& info = pBindInfos[bindIdx];

        uint32_t        deviceIndexCount = 0;
        const uint32_t* pDeviceIndices   = nullptr;

        for (pVkBindBufferMemoryInfo = &info; pHeader != nullptr; pHeader = pHeader->pNext)
        {
            switch (static_cast<uint32_t>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO:
            {
                VK_ASSERT(pVkBindBufferMemoryInfo == &info);
                break;
            }

            case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
            {
                deviceIndexCount = pVkBindBufferMemoryDeviceGroupInfo->deviceIndexCount;
                pDeviceIndices   = pVkBindBufferMemoryDeviceGroupInfo->pDeviceIndices;
                break;
            }

            default:
                VK_NOT_IMPLEMENTED;
                break;
            }
        }

        VK_ASSERT((deviceIndexCount == 0) || (deviceIndexCount == NumPalDevices()));

        Buffer::ObjectFromHandle(info.buffer)->BindMemory(this, info.memory, info.memoryOffset, pDeviceIndices);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Device::BindImageMemory(
    uint32_t                          bindInfoCount,
    const VkBindImageMemoryInfo*      pBindInfos) const
{
    union
    {
        const VkStructHeader*                        pHeader;
        const VkBindImageMemoryInfo*                 pVkBindImageMemoryInfo;
        const VkBindImageMemorySwapchainInfoKHR*     pVkBindImageMemorySwapchainInfo;
        const VkBindImageMemoryDeviceGroupInfo*      pVkBindImageMemoryDeviceGroupInfo;
    };

    for (uint32_t bindIdx = 0; bindIdx < bindInfoCount; bindIdx++)
    {
        uint32_t        deviceIndexCount    = 0;
        const uint32_t* pDeviceIndices      = nullptr;

        uint32_t        SFRRectCount        = 0;
        const VkRect2D* pSFRRects           = nullptr;

        uint32_t        swapChainImageIndex = 0;
        SwapChain*      pSwapchain          = nullptr;

        const VkBindImageMemoryInfo& info = pBindInfos[bindIdx];

        for (pVkBindImageMemoryInfo = &info; pHeader != nullptr; pHeader = pHeader->pNext)
        {
            switch (static_cast<uint32_t>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO:
            {
                VK_ASSERT(pVkBindImageMemoryInfo == &info);
                break;
            }

            case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
            {
                deviceIndexCount = pVkBindImageMemoryDeviceGroupInfo->deviceIndexCount;
                pDeviceIndices = pVkBindImageMemoryDeviceGroupInfo->pDeviceIndices;
                SFRRectCount = pVkBindImageMemoryDeviceGroupInfo->splitInstanceBindRegionCount;
                pSFRRects = pVkBindImageMemoryDeviceGroupInfo->pSplitInstanceBindRegions;
                break;
            }

            case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR:
            {
                pSwapchain = SwapChain::ObjectFromHandle(pVkBindImageMemorySwapchainInfo->swapchain);
                swapChainImageIndex = pVkBindImageMemorySwapchainInfo->imageIndex;
                break;
            }

            default:
                VK_NOT_IMPLEMENTED;
                break;
            }
        }

        VK_ASSERT((deviceIndexCount == 0) || (deviceIndexCount == NumPalDevices()));

        if (pSwapchain != nullptr)
        {
            Image::ObjectFromHandle(info.image)->BindSwapchainMemory(
                this,
                swapChainImageIndex,
                pSwapchain,
                deviceIndexCount,
                pDeviceIndices,
                SFRRectCount,
                pSFRRects);
        }
        else
        {
            Image::ObjectFromHandle(info.image)->BindMemory(
                this,
                info.memory,
                info.memoryOffset,
                deviceIndexCount,
                pDeviceIndices,
                SFRRectCount,
                pSFRRects);
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================

// =====================================================================================================================
VkResult Device::GetCalibratedTimestamps(
    uint32_t                            timestampCount,
    const VkCalibratedTimestampInfoEXT* pTimestampInfos,
    uint64_t*                           pTimestamps,
    uint64_t*                           pMaxDeviation)
{
    Pal::CalibratedTimestamps calibratedTimestamps = {};

    Pal::Result palResult = PalDevice(DefaultDeviceIndex)->GetCalibratedTimestamps(&calibratedTimestamps);
    VkResult result = PalToVkResult(palResult);

    if (result == VK_SUCCESS)
    {
        for (uint32_t i = 0; i < timestampCount; ++i)
        {
            switch (pTimestampInfos[i].timeDomain)
            {
            case VK_TIME_DOMAIN_DEVICE_EXT:
                pTimestamps[i] = calibratedTimestamps.gpuTimestamp;
                break;
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT:
                pTimestamps[i] = calibratedTimestamps.cpuClockMonotonicTimestamp;
                break;
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT:
                pTimestamps[i] = calibratedTimestamps.cpuClockMonotonicRawTimestamp;
                break;
            case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT:
                pTimestamps[i] = calibratedTimestamps.cpuQueryPerfCounterTimestamp;
                break;
            default:
                // An invalid time domain value was specified.  Return error.
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
                pTimestamps[i] = 0;
                VK_NEVER_CALLED();
                break;
            }
        }

        *pMaxDeviation = calibratedTimestamps.maxDeviation;
    }

    return result;
}

// =====================================================================================================================
VkResult Device::CreateSampler(
    const VkSamplerCreateInfo*                  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSampler*                                  pSampler)
{
    return Sampler::Create(this, pCreateInfo, pAllocator, pSampler);
}

// =====================================================================================================================
VkResult Device::CreateSemaphore(
    const VkSemaphoreCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSemaphore*                                pSemaphore)
{
    return Semaphore::Create(this, pCreateInfo, pAllocator, pSemaphore);
}

// =====================================================================================================================
VkResult Device::CreateQueryPool(
    const VkQueryPoolCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkQueryPool*                                pQueryPool)
{
    return QueryPool::Create(this, pCreateInfo, pAllocator, pQueryPool);
}

VkResult Device::ImportSemaphore(
    VkSemaphore                 semaphore,
    const ImportSemaphoreInfo&  importInfo)
{
    return Semaphore::ObjectFromHandle(semaphore)->ImportSemaphore(this, importInfo);
}

// =====================================================================================================================
VkResult Device::CreateSwapchain(
    const VkSwapchainCreateInfoKHR*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSwapchainKHR*                             pSwapChain)
{
    return SwapChain::Create(this, pCreateInfo, pAllocator, pSwapChain);
}

// =====================================================================================================================
// Adds an item to the residency list.
Pal::Result Device::AddMemReference(
    Pal::IDevice*    pPalDevice,
    Pal::IGpuMemory* pPalMemory,
    bool             readOnly)
{
    Pal::GpuMemoryRef memRef = {};

    memRef.pGpuMemory     = pPalMemory;
    memRef.flags.readOnly = readOnly;

    const Pal::GpuMemoryRefFlags memoryReferenceFlags = static_cast<Pal::GpuMemoryRefFlags>(0);

    return pPalDevice->AddGpuMemoryReferences(1, &memRef, nullptr, memoryReferenceFlags);
}

// =====================================================================================================================
// Removes an item from the residency list.
void Device::RemoveMemReference(
    Pal::IDevice*    pPalDevice,
    Pal::IGpuMemory* pPalMemory)
{
    pPalDevice->RemoveGpuMemoryReferences(1, &pPalMemory, nullptr);
}

// =====================================================================================================================
VkResult Device::CreateBltMsaaStates()
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t log2Samples = 0;
         (log2Samples < BltMsaaStateCount) && (palResult == Pal::Result::Success);
         ++log2Samples)
    {
        uint32_t samples = (1UL << log2Samples);

        Pal::MsaaStateCreateInfo info = {};

        info.coverageSamples         = samples;
        info.exposedSamples          = samples;
        info.pixelShaderSamples      = samples;
        info.depthStencilSamples     = samples;
        info.shaderExportMaskSamples = samples;
        info.sampleMask              = (1UL << samples) - 1;
        info.sampleClusters          = 0;
        info.alphaToCoverageSamples  = 0;
        info.occlusionQuerySamples   = samples;

        palResult = m_renderStateCache.CreateMsaaState(
            info, nullptr, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, &m_pBltMsaaState[log2Samples][0]);
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Individual VkMemory objects fit some GPU VA base address alignment guarantees.  Given a mask of memory type indices,
// this function will return the *smallest* possible alignment amongst those types.  Note that you can pass in a single
// memory type bit to get that type's exact base address alignment.
VkDeviceSize Device::GetMemoryBaseAddrAlignment(
    uint32_t memoryTypes
    ) const
{
    const RuntimeSettings& settings = GetRuntimeSettings();

    uint32_t minAlignment = 0;

    if (memoryTypes != 0)
    {
        minAlignment = settings.memoryBaseAddrAlignment;
    }

    return minAlignment;
}

// =====================================================================================================================
// Returns the memory types compatible with pinned system memory.
uint32_t Device::GetPinnedSystemMemoryTypes() const
{
    uint32_t memoryTypes = 0;
    uint32_t gartIndexBits;

    if (GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeapGartCacheable, &gartIndexBits))
    {
        memoryTypes |= gartIndexBits;
    }

    return memoryTypes;
}

uint32_t Device::GetPinnedHostMappedForeignMemoryTypes() const
{
    uint32_t memoryTypes = 0;
    uint32_t gartIndexBits;

    if (GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeapGartUswc, &gartIndexBits))
    {
        memoryTypes |= gartIndexBits;
    }

    return memoryTypes;
}
// =====================================================================================================================
// Returns the memory type bit-mask that is compatible to be used as pinned memory types for the given external
// host pointer
uint32_t Device::GetExternalHostMemoryTypes(
    VkExternalMemoryHandleTypeFlagBits handleType,
    const void*                        pExternalPtr
    ) const
{
    uint32_t memoryTypes = 0;

    if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
    {
        memoryTypes = GetPinnedSystemMemoryTypes();
    }
    else if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT)
    {
        memoryTypes = GetPinnedHostMappedForeignMemoryTypes();
    }
    return memoryTypes;
}

// =====================================================================================================================
// Checks to see if memory is available for device local allocations made by the application (externally) and
// reports OOM if necessary
VkResult Device::TryIncreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     deviceMask,
    uint32_t     heapIdx)
{
    VkResult           vkResult = VK_SUCCESS;
    utils::IterateMask deviceGroup(deviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        vkResult = m_perGpu[deviceIdx].pPhysicalDevice->TryIncreaseAllocatedMemorySize(allocationSize, heapIdx);

        if (vkResult != VK_SUCCESS)
        {
            break;
        }
    }

    return vkResult;
}

// =====================================================================================================================
// Increases the allocated memory size for device local allocations made by the application (externally) and
// reports OOM if necessary
void Device::IncreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     deviceMask,
    uint32_t     heapIdx)
{
    utils::IterateMask deviceGroup(deviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        m_perGpu[deviceIdx].pPhysicalDevice->IncreaseAllocatedMemorySize(allocationSize, heapIdx);
    }
}

// =====================================================================================================================
// Decreases the allocated memory size for device local allocations made by the application (externally)
void Device::DecreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     deviceMask,
    uint32_t     heapIdx)
{
    utils::IterateMask deviceGroup(deviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        m_perGpu[deviceIdx].pPhysicalDevice->DecreaseAllocatedMemorySize(allocationSize, heapIdx);
    }
}

// =====================================================================================================================
// One time setup for software compositing for this physical device
VkResult Device::InitSwCompositing(
    uint32_t deviceIdx)
{
    VkResult result = VK_SUCCESS;

    if (m_perGpu[deviceIdx].pSwCompositingMemory == nullptr)
    {
        Pal::QueueSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.maxCount = 1;

        Pal::Result palResult;

        size_t palSemaphoreSize = PalDevice(deviceIdx)->GetQueueSemaphoreSize(semaphoreCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        Pal::QueueCreateInfo queueCreateInfo = {};

        queueCreateInfo.engineIndex = 0;
        queueCreateInfo.engineType  = Pal::EngineType::EngineTypeDma;
        queueCreateInfo.queueType   = Pal::QueueType::QueueTypeDma;

        size_t palQueueSize = PalDevice(deviceIdx)->GetQueueSize(queueCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        m_perGpu[deviceIdx].pSwCompositingMemory = VkInstance()->AllocMem(
            (palQueueSize + palSemaphoreSize),
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (m_perGpu[deviceIdx].pSwCompositingMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            palResult = PalDevice(deviceIdx)->CreateQueueSemaphore(
                semaphoreCreateInfo,
                m_perGpu[deviceIdx].pSwCompositingMemory,
                &m_perGpu[deviceIdx].pSwCompositingSemaphore);

            if (palResult == Pal::Result::Success)
            {
                palResult = PalDevice(deviceIdx)->CreateQueue(
                    queueCreateInfo,
                    Util::VoidPtrInc(m_perGpu[deviceIdx].pSwCompositingMemory, palSemaphoreSize),
                    &m_perGpu[deviceIdx].pSwCompositingQueue);
            }

            // Clean up if any error is encountered
            if (palResult != Pal::Result::Success)
            {
                if (m_perGpu[deviceIdx].pSwCompositingSemaphore != nullptr)
                {
                    m_perGpu[deviceIdx].pSwCompositingSemaphore->Destroy();
                    m_perGpu[deviceIdx].pSwCompositingSemaphore = nullptr;
                }

                VkInstance()->FreeMem(m_perGpu[deviceIdx].pSwCompositingMemory);
                m_perGpu[deviceIdx].pSwCompositingMemory = nullptr;

                result = PalToVkError(palResult);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Issue the software compositing and/or synchronization with the updated presenting queue
Pal::IQueue* Device::PerformSwCompositing(
    uint32_t         deviceIdx,
    uint32_t         presentationDeviceIdx,
    Pal::ICmdBuffer* pCommandBuffer,
    Pal::QueueType   cmdBufferQueueType,
    const Queue*     pQueue)
{
    Pal::IQueue* pPresentQueue = nullptr;
    VkResult     result        = VK_SUCCESS;

    // Initialization will be performed on the first use for both the slave and master devices
    result = InitSwCompositing(deviceIdx);

    if (result == VK_SUCCESS)
    {
        result = InitSwCompositing(presentationDeviceIdx);
    }

    if (result == VK_SUCCESS)
    {
        Pal::IQueue* pRenderQueue      = pQueue->PalQueue(deviceIdx);
        Pal::IQueue* pCompositingQueue = pRenderQueue;

        // For SW composition, we must use a different present queue for slave devices. Otherwise, frame pacing
        // will block any rendering submitted to the master present queue. Using an internal master SDMA queue
        // for the presents of all devices is the simplest solution to this.
        pPresentQueue = m_perGpu[presentationDeviceIdx].pSwCompositingQueue;

        if (deviceIdx != presentationDeviceIdx)
        {
            Pal::CmdBufInfo cmdBufInfo = {};
            cmdBufInfo.isValid = true;
            cmdBufInfo.p2pCmd  = true;

            Pal::SubmitInfo submitInfo = {};
            submitInfo.cmdBufferCount  = 1;
            submitInfo.pCmdBufInfoList = &cmdBufInfo;
            submitInfo.ppCmdBuffers    = &pCommandBuffer;

            // Use the separate SDMA queue and synchronize the slave device's queue with the peer transfer
            if (cmdBufferQueueType == Pal::QueueType::QueueTypeDma)
            {
                pCompositingQueue = m_perGpu[deviceIdx].pSwCompositingQueue;

                pRenderQueue->SignalQueueSemaphore(m_perGpu[deviceIdx].pSwCompositingSemaphore);
                pCompositingQueue->WaitQueueSemaphore(m_perGpu[deviceIdx].pSwCompositingSemaphore);
            }

            VK_ASSERT(cmdBufferQueueType == pCompositingQueue->Type());
            pCompositingQueue->Submit(submitInfo);
        }

        pCompositingQueue->SignalQueueSemaphore(m_perGpu[deviceIdx].pSwCompositingSemaphore);
        pPresentQueue->WaitQueueSemaphore(m_perGpu[deviceIdx].pSwCompositingSemaphore);
    }

    return pPresentQueue;
}

// =====================================================================================================================
// Return true if Big Software Release 6.0 is supported.
bool Device::BigSW60Supported() const
{
    const Pal::DeviceProperties&       deviceProps      = VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
    const Pal::BigSoftwareReleaseInfo* pBigSwInfo       = &(deviceProps.bigSoftwareReleaseInfo);

    return ((pBigSwInfo->majorVersion > 2019) ||
           ((pBigSwInfo->majorVersion == 2019) && (pBigSwInfo->minorVersion >= 1)));
}

// =====================================================================================================================
// Update driver feature settings for this device based on an app profile and global settings.
void Device::UpdateFeatureSettings()
{
    ProfileSettings profileSettings = {};

    ReloadAppProfileSettings(m_pInstance, &profileSettings);

}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */
namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(
    VkDevice                                    device,
    const VkFenceCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkFence*                                    pFence)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateFence(pCreateInfo, pAllocCB, pFence);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences,
    VkBool32                                    waitAll,
    uint64_t                                    timeout)
{
     return ApiDevice::ObjectFromHandle(device)->WaitForFences(fenceCount,
                                                              pFences,
                                                              waitAll,
                                                              timeout);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences)
{
    return ApiDevice::ObjectFromHandle(device)->ResetFences(fenceCount, pFences);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
    ApiDevice::ObjectFromHandle(device)->GetQueue(queueFamilyIndex, queueIndex, pQueue);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue2(
    VkDevice                                    device,
    const VkDeviceQueueInfo2*                   pQueueInfo,
    VkQueue*                                    pQueue)
{
    // For now we assume we don't get any additional information here compared to vkGetDeviceQueue.
    // If that changes due to having new structs added to the chain, adding support for protected queues, or other
    // queue creation flags then this code needs to be updated.
    VK_ASSERT(pQueueInfo->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2);
    VK_ASSERT(pQueueInfo->pNext == nullptr);
    VK_ASSERT(pQueueInfo->flags != 0);  // by specs, the flags must not NOT be 0 when calling this function.
    VK_NOT_IMPLEMENTED;

    // Our driver currently doesn't support VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT, and this function should be
    // called with a non-zero queue creation flag, so the application has no valid way of calling it and we should
    // always return VK_NULL_HANDLE in pQueue.
    *pQueue = VK_NULL_HANDLE;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(
    VkDevice                                    device,
    const VkSemaphoreCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSemaphore*                                pSemaphore)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateSemaphore(pCreateInfo, pAllocCB, pSemaphore);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice                                    device,
    const VkAllocationCallbacks*                pAllocator)
{
    if (device != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        pDevice->Destroy(pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(
    VkDevice                                    device)
{
    return ApiDevice::ObjectFromHandle(device)->WaitIdle();
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateEvent(
    VkDevice                                    device,
    const VkEventCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkEvent*                                    pEvent)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateEvent(pCreateInfo, pAllocCB, pEvent);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateQueryPool(
    VkDevice                                    device,
    const VkQueryPoolCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkQueryPool*                                pQueryPool)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateQueryPool(pCreateInfo, pAllocCB, pQueryPool);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorSetLayout*                      pSetLayout)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateDescriptorSetLayout(pCreateInfo, pAllocCB, pSetLayout);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice                                    device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineLayout*                           pPipelineLayout)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreatePipelineLayout(pCreateInfo, pAllocCB, pPipelineLayout);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(
    VkDevice                                    device,
    const VkFramebufferCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkFramebuffer*                              pFramebuffer)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateFramebuffer(pCreateInfo, pAllocCB, pFramebuffer);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice                                    device,
    const VkRenderPassCreateInfo*               pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkRenderPass*                               pRenderPass)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateRenderPass(pCreateInfo, pAllocCB, pRenderPass);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(
    VkDevice                                    device,
    const VkRenderPassCreateInfo2KHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkRenderPass*                               pRenderPass)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateRenderPass2(pCreateInfo, pAllocCB, pRenderPass);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(
    VkDevice                                    device,
    const VkBufferCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBuffer*                                   pBuffer)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateBuffer(pCreateInfo, pAllocCB, pBuffer);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferView(
    VkDevice                                    device,
    const VkBufferViewCreateInfo*               pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBufferView*                               pView)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateBufferView(pCreateInfo, pAllocCB, pView);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImage*                                    pImage)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateImage(pCreateInfo, pAllocCB, pImage);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImageView*                                pView)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateImageView(pCreateInfo, pAllocCB, pView);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkShaderModule*                             pShaderModule)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateShaderModule(pCreateInfo, pAllocCB, pShaderModule);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineCache(
    VkDevice                                    device,
    const VkPipelineCacheCreateInfo*            pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineCache*                            pPipelineCache)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreatePipelineCache(pCreateInfo, pAllocCB, pPipelineCache);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateGraphicsPipelines(
        pipelineCache,
        createInfoCount,
        pCreateInfos,
        pAllocCB,
        pPipelines);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateComputePipelines(
        pipelineCache,
        createInfoCount,
        pCreateInfos,
        pAllocCB,
        pPipelines);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(
    VkDevice                                    device,
    const VkSamplerCreateInfo*                  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSampler*                                  pSampler)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateSampler(pCreateInfo, pAllocCB, pSampler);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSamplerYcbcrConversion(
    VkDevice                                    device,
    const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSamplerYcbcrConversion*                   pYcbcrConversion)
{
    VK_NOT_IMPLEMENTED;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversion(
    VkDevice                                    device,
    VkSamplerYcbcrConversion                    ycbcrConversion,
    const VkAllocationCallbacks*                pAllocator)
{
    VK_NOT_IMPLEMENTED;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice                                     device,
    const VkSwapchainCreateInfoKHR*              pCreateInfo,
    const VkAllocationCallbacks*                 pAllocator,
    VkSwapchainKHR*                              pSwapchain)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateSwapchain(pCreateInfo, pAllocCB, pSwapchain);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetRenderAreaGranularity(
    VkDevice                                    device,
    VkRenderPass                                renderPass,
    VkExtent2D*                                 pGranularity)
{
    pGranularity->width  = 1;
    pGranularity->height = 1;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice                                    device,
    const VkCommandBufferAllocateInfo*          pAllocateInfo,
    VkCommandBuffer*                            pCommandBuffers)
{
    return ApiDevice::ObjectFromHandle(device)->AllocateCommandBuffers(pAllocateInfo,
                                                                       pCommandBuffers);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice                                    device,
    const VkCommandPoolCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkCommandPool*                              pCommandPool)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateCommandPool(pCreateInfo, pAllocCB, pCommandPool);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice                                    device,
    const VkMemoryAllocateInfo*                 pAllocateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDeviceMemory*                             pMemory)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->AllocMemory(pAllocateInfo, pAllocCB, pMemory);
}

VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFdKHR(
    VkDevice device,
    const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo)
{
    ImportSemaphoreInfo importInfo = {};
    importInfo.handleType  = pImportSemaphoreFdInfo->handleType;
    importInfo.handle      = pImportSemaphoreFdInfo->fd;
    importInfo.importFlags = pImportSemaphoreFdInfo->flags;

    return ApiDevice::ObjectFromHandle(device)->ImportSemaphore(pImportSemaphoreFdInfo->semaphore, importInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2(
    VkDevice                                    device,
    uint32_t                                    bindInfoCount,
    const VkBindBufferMemoryInfo*               pBindInfos)
{
    return ApiDevice::ObjectFromHandle(device)->BindBufferMemory(bindInfoCount, pBindInfos);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2(
    VkDevice                                    device,
    uint32_t                                    bindInfoCount,
    const VkBindImageMemoryInfo*                pBindInfos)
{
    return ApiDevice::ObjectFromHandle(device)->BindImageMemory(bindInfoCount, pBindInfos);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplate(
    VkDevice                                        device,
    const VkDescriptorUpdateTemplateCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*                    pAllocator,
    VkDescriptorUpdateTemplate*                     pDescriptorUpdateTemplate)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateDescriptorUpdateTemplate(pCreateInfo, pAllocCB, pDescriptorUpdateTemplate);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceGroupPeerMemoryFeatures(
    VkDevice                                    device,
    uint32_t                                    heapIndex,
    uint32_t                                    localDeviceIndex,
    uint32_t                                    remoteDeviceIndex,
    VkPeerMemoryFeatureFlags*                   pPeerMemoryFeatures)
{
    ApiDevice::ObjectFromHandle(device)->GetDeviceGroupPeerMemoryFeatures(
        heapIndex,
        localDeviceIndex,
        remoteDeviceIndex,
        pPeerMemoryFeatures);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupPresentCapabilitiesKHR(
    VkDevice                                    device,
    VkDeviceGroupPresentCapabilitiesKHR*        pDeviceGroupPresentCapabilities)
{
    return ApiDevice::ObjectFromHandle(device)->GetDeviceGroupPresentCapabilities(pDeviceGroupPresentCapabilities);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupSurfacePresentModesKHR(
    VkDevice                                    device,
    VkSurfaceKHR                                surface,
    VkDeviceGroupPresentModeFlagsKHR*           pModes)
{
    return ApiDevice::ObjectFromHandle(device)->GetDeviceGroupSurfacePresentModes(surface, pModes);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectTagInfoEXT*        pTagInfo)
{
    // The SQTT layer shadows this extension's functions and contains extra code to make use of them.  This
    // extension is not enabled when the SQTT layer is not also enabled, so these functions are currently
    // just blank placeholder functions in case there will be a time where we need to do something with them
    // on this path also.

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectNameInfoEXT*       pNameInfo)
{
    // The SQTT layer shadows this extension's functions and contains extra code to make use of them.  This
    // extension is not enabled when the SQTT layer is not also enabled, so these functions are currently
    // just blank placeholder functions in case there will be a time where we need to do something with them
    // on this path also.

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectTagEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectTagInfoEXT*         pTagInfo)
{

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectNameInfoEXT*        pNameInfo)
{

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetGpaDeviceClockModeAMD(
    VkDevice                                    device,
    VkGpaDeviceClockModeInfoAMD*                pInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    Pal::SetClockModeInput input = {};

    input.clockMode = VkToPalDeviceClockMode(pInfo->clockMode);

    Pal::SetClockModeOutput output = {};
    Pal::Result palResult = Pal::Result::Success;

    // Set clock mode for all devices in the group unless we are querying
    if (input.clockMode != Pal::DeviceClockMode::Query)
    {
        for (uint32_t deviceIdx = 0;
            (deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
            ++deviceIdx)
        {
            palResult = pDevice->PalDevice(deviceIdx)->SetClockMode(input, &output);
        }
    }
    else
    {
        palResult = pDevice->PalDevice(DefaultDeviceIndex)->SetClockMode(input, &output);

        if (palResult == Pal::Result::Success)
        {
            pInfo->engineClockRatioToPeak = output.engineClockRatioToPeak;
            pInfo->memoryClockRatioToPeak = output.memoryClockRatioToPeak;
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupport(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    VkDescriptorSetLayoutSupport*               pSupport)
{
    VkStructHeaderNonConst* pHeader = reinterpret_cast<VkStructHeaderNonConst*>(pSupport);

    // No descriptor set layout validation is required beyond what is expressed with existing limits.
    VK_ASSERT(pSupport->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT);

    while (pHeader)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT:
            {
                pSupport->supported = VK_TRUE;
                break;
            }

            case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT:
            {
                VkDescriptorSetVariableDescriptorCountLayoutSupportEXT * pDescCountLayoutSupport =
                    reinterpret_cast<VkDescriptorSetVariableDescriptorCountLayoutSupportEXT *>(pHeader);

                pDescCountLayoutSupport->maxVariableDescriptorCount = UINT_MAX;

                break;
            }

            default:
                break;
        }

        pHeader = reinterpret_cast<VkStructHeaderNonConst*>(pHeader->pNext);
    }
}

// =====================================================================================================================

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetCalibratedTimestampsEXT(
    VkDevice                                    device,
    uint32_t                                    timestampCount,
    const VkCalibratedTimestampInfoEXT*         pTimestampInfos,
    uint64_t*                                   pTimestamps,
    uint64_t*                                   pMaxDeviation)
{
    return ApiDevice::ObjectFromHandle(device)->GetCalibratedTimestamps(
        timestampCount,
        pTimestampInfos,
        pTimestamps,
        pMaxDeviation);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryHostPointerPropertiesEXT(
    VkDevice                                    device,
    VkExternalMemoryHandleTypeFlagBits          handleType,
    const void*                                 pHostPointer,
    VkMemoryHostPointerPropertiesEXT*           pMemoryHostPointerProperties)
{
    VkResult result         = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const Device* pDevice   = ApiDevice::ObjectFromHandle(device);
    const uint32_t memTypes = pDevice->GetExternalHostMemoryTypes(handleType, pHostPointer);

    if (memTypes != 0)
    {
        pMemoryHostPointerProperties->memoryTypeBits = memTypes;

        result = VK_SUCCESS;
    }

    return result;
}

} // entry

} // vk
