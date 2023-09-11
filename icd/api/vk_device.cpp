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
#include "include/vk_pipeline_layout.h"
#include "include/vk_physical_device.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_instance.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_event.h"
#include "include/vk_memory.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_query.h"
#include "include/vk_queue.h"
#include "include/vk_render_pass.h"
#include "include/vk_semaphore.h"
#include "include/vk_shader.h"
#include "include/vk_sampler.h"
#include "include/vk_sampler_ycbcr_conversion.h"
#include "include/vk_swapchain.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "include/cmd_buffer_ring.h"
#include "include/graphics_pipeline_common.h"
#include "include/internal_layer_hooks.h"

#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#include "raytrace/ray_tracing_util.h"
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/vk_acceleration_structure.h"
#endif

#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"
#include "sqtt/sqtt_rgp_annotations.h"

#if VKI_GPU_DECOMPRESS
#include "appopt/gpu_decode_layer.h"
#endif

#include "appopt/barrier_filter_layer.h"
#include "appopt/strange_brigade_layer.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

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
#include "palAutoBuffer.h"
#include "palBorderColorPalette.h"
#include "palVectorImpl.h"

#include <cmath>

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
static VkResult VerifyRequestedPhysicalDeviceFeatures(
    const PhysicalDevice* pPhysicalDevice,
    VirtualStackFrame*    pVirtStackFrame,
    const void*           pRequestedFeatures,
    bool                  isPhysicalDeviceFeature)
{
    size_t headerSize;
    size_t structSize;

    VkBool32* pSupportedFeatures = nullptr;
    VkStructHeaderNonConst structSizeFeaturesHeader = {};
    structSizeFeaturesHeader.sType = static_cast<const VkStructHeader*>(pRequestedFeatures)->sType;
    VkResult result = VK_SUCCESS;

    structSize = isPhysicalDeviceFeature ? pPhysicalDevice->GetFeatures(nullptr) :
                    pPhysicalDevice->GetFeatures2((&structSizeFeaturesHeader), false);

    // unsupported feature header structures will return structSize 0, ignore unkown structs.
    if (structSize != 0)
    {
        // allocate appropriate sized memory to pSupportedFeatures
        pSupportedFeatures = pVirtStackFrame->AllocArray<VkBool32>(structSize / sizeof(VkBool32));

        if (pSupportedFeatures != nullptr)
        {
            // Start by making a copy of the requested features all so that uninitialized struct padding always matches below.
            memcpy(pSupportedFeatures, pRequestedFeatures, structSize);

            if (isPhysicalDeviceFeature)
            {
                VkPhysicalDeviceFeatures* pPhysicalDeviceFeatures =
                    reinterpret_cast<VkPhysicalDeviceFeatures*>(pSupportedFeatures);

                pPhysicalDevice->GetFeatures(pPhysicalDeviceFeatures);

                // The original VkPhysicalDeviceFeatures struct doesn't contain a VkStructHeader
                headerSize = 0;
            }
            else
            {
                VkStructHeaderNonConst* pHeader = reinterpret_cast<VkStructHeaderNonConst*>(pSupportedFeatures);

                pHeader->pNext = nullptr; // Make sure GetFeatures2 doesn't clobber the original requested features chain

                // update the features using default true
                pPhysicalDevice->GetFeatures2(pHeader, true);

                headerSize = offsetof(VkPhysicalDeviceFeatures2, features);
            }

            // Struct padding may give us extra features
            const size_t numFeatures = (structSize - headerSize) / sizeof(VkBool32);
            const auto   supported = static_cast<const VkBool32*>(Util::VoidPtrInc(pSupportedFeatures, headerSize));
            const auto   requested = static_cast<const VkBool32*>(Util::VoidPtrInc(pRequestedFeatures, headerSize));

            for (size_t featureNdx = 0; featureNdx < numFeatures; ++featureNdx)
            {
                if (requested[featureNdx] && !supported[featureNdx])
                {
                    result = VK_ERROR_FEATURE_NOT_PRESENT;

                    break;
                }
            }

            pVirtStackFrame->FreeArray(pSupportedFeatures);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return result;
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
    const VkDeviceCreateInfo*           pCreateInfo,
    const DeviceExtensions::Enabled&    enabledExtensions,
    const VkPhysicalDeviceFeatures*     pFeatures,
    bool                                useComputeAsTransferQueue,
    uint32                              privateDataSlotRequestCount,
    size_t                              privateDataSize,
    const DeviceFeatures&               deviceFeatures)
    :
    m_pInstance(pPhysicalDevices[DefaultDeviceIndex]->VkInstance()),
    m_settings(pPhysicalDevices[DefaultDeviceIndex]->GetRuntimeSettings()),
    m_palDeviceCount(palDeviceCount),
    m_internalMemMgr(this, pPhysicalDevices[DefaultDeviceIndex]->VkInstance()),
    m_shaderOptimizer(this, pPhysicalDevices[DefaultDeviceIndex]),
    m_pipelineBinningMode(m_settings.pipelineBinningMode),
    m_resourceOptimizer(this, pPhysicalDevices[DefaultDeviceIndex]),
    m_renderStateCache(this),
    m_barrierPolicy(
        pPhysicalDevices[DefaultDeviceIndex],
        pCreateInfo,
        enabledExtensions),
    m_enabledExtensions(enabledExtensions),
    m_dispatchTable(DispatchTable::Type::DEVICE, m_pInstance, this),
    m_pSqttMgr(nullptr),
    m_pAppOptLayer(nullptr),
    m_pBarrierFilterLayer(nullptr),
#if VKI_GPU_DECOMPRESS
    m_pGpuDecoderLayer(nullptr),
#endif
    m_enabledFeatures(deviceFeatures),
    m_allocationSizeTracking(true),
    m_useComputeAsTransferQueue(useComputeAsTransferQueue),
    m_useUniversalAsComputeQueue(pPhysicalDevices[DefaultDeviceIndex]->GetRuntimeSettings().useUniversalAsComputeQueue),
    m_useGlobalGpuVa(false),
#if VKI_RAY_TRACING
    m_pRayTrace(nullptr),
#endif
    m_pBorderColorUsedIndexes(nullptr),
    m_retrievedFaultData(false),
    m_pNullPipelineLayout(nullptr)
{
    memset(m_pBltMsaaState, 0, sizeof(m_pBltMsaaState));

    memset(m_pQueues, 0, sizeof(m_pQueues));

    m_maxVrsShadingRate = {0, 0};

    for (uint32_t deviceIdx = 0; deviceIdx < palDeviceCount; ++deviceIdx)
    {
        m_perGpu[deviceIdx].pPhysicalDevice = pPhysicalDevices[deviceIdx];
        m_perGpu[deviceIdx].pPalDevice      = pPalDevices[deviceIdx];

        m_perGpu[deviceIdx].pSharedPalCmdAllocator = nullptr;

        m_perGpu[deviceIdx].pSwCompositingMemory    = nullptr;
        m_perGpu[deviceIdx].pSwCompositingQueue     = nullptr;
        m_perGpu[deviceIdx].pSwCompositingSemaphore = nullptr;
        m_perGpu[deviceIdx].pSwCompositingCmdBuffer = nullptr;
        m_perGpu[deviceIdx].pPalBorderColorPalette  = nullptr;
    }

#if VKI_RAY_TRACING
#endif

    m_shaderOptimizer.Init();
    m_resourceOptimizer.Init();

    memset(m_overallocationRequestedForPalHeap, 0, sizeof(m_overallocationRequestedForPalHeap));

    m_nextPrivateDataSlot = 0;
    m_privateDataSize = privateDataSize;
    m_privateDataSlotRequestCount = privateDataSlotRequestCount;
}

// =====================================================================================================================
// Creates a new Vulkan API device object
VkResult Device::Create(
    PhysicalDevice*                 pPhysicalDevice,
    const VkDeviceCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    ApiDevice**                     ppDevice)
{
    Pal::Result    palResult                            = Pal::Result::Success;
    DeviceFeatures deviceFeatures                       = {};
    uint32_t       queueCounts[Queue::MaxQueueFamilies] = {};
    uint32_t       queueFlags[Queue::MaxQueueFamilies]  = {};

    // initialize queue priority with the default value VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR.
    VkQueueGlobalPriorityKHR queuePriority[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily] = {};
    for (uint32_t familyId = 0; familyId < Queue::MaxQueueFamilies; familyId++)
    {
        for (uint32_t queueId = 0; queueId < Queue::MaxQueuesPerFamily; queueId++)
        {
            queuePriority[familyId][queueId] = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR;
        }
    }

    // Dedicated Compute Units
    static constexpr uint32_t MaxEngineCount = 8;
    uint32_t dedicatedComputeUnits[Queue::MaxQueueFamilies][MaxEngineCount] = {};

    VkResult vkResult = VK_SUCCESS;
    void*    pMemory  = nullptr;

    DeviceExtensions::Enabled enabledDeviceExtensions;

    VK_ASSERT(pCreateInfo != nullptr);

    // Make sure the caller only requests extensions we actually support.
    if (pCreateInfo->enabledExtensionCount > 0)
    {
        if (!DeviceExtensions::EnableExtensions(pCreateInfo->ppEnabledExtensionNames,
                                                pCreateInfo->enabledExtensionCount,
                                                pPhysicalDevice->GetAllowedExtensions(),
                                                pPhysicalDevice->GetIgnoredExtensions(),
                                                &enabledDeviceExtensions))
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
    bool                              pageableDeviceLocalMemory       = false;

    uint32                            privateDataSlotRequestCount           = 0;
    bool                              privateDataEnabled                    = false;
    size_t                            privateDataSize                       = 0;
    bool                              bufferDeviceAddressMultiDeviceEnabled = false;
    bool                              maintenance4Enabled                   = false;
    bool                              globalPriorityQueryEnabled            = false;

    const VkStructHeader* pHeader = nullptr;

    VirtualStackAllocator* pStackAllocator = nullptr;

    palResult = pInstance->StackMgr()->AcquireAllocator(&pStackAllocator);

    if (palResult == Pal::Result::Success)
    {
        VirtualStackFrame virtStackFrame(pStackAllocator);

        for (pHeader = static_cast<const VkStructHeader*>(pCreateInfo->pNext);
            ((pHeader != nullptr) && (vkResult == VK_SUCCESS));
            pHeader = pHeader->pNext)
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
                VK_ASSERT(pCreateInfo->pEnabledFeatures == nullptr);

                // If present, VkPhysicalDeviceFeatures2 controls which features are enabled
                // instead of pEnabledFeatures.
                pEnabledFeatures = &reinterpret_cast<const VkPhysicalDeviceFeatures2*>(pHeader)->features;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
            {
                if (reinterpret_cast<const VkPhysicalDeviceScalarBlockLayoutFeatures*>(pHeader)->scalarBlockLayout)
                {
                    deviceFeatures.scalarBlockLayout = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            {
                if (reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(
                    pHeader)->bufferDeviceAddressMultiDevice)
                {
                    bufferDeviceAddressMultiDeviceEnabled = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT:
            {
                if (reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeaturesEXT*>(
                    pHeader)->bufferDeviceAddressMultiDevice)
                {
                    bufferDeviceAddressMultiDeviceEnabled = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            {

                break;
            }

            case VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD:
            {
                const VkDeviceMemoryOverallocationCreateInfoAMD* pMemoryOverallocationCreateInfo =
                    reinterpret_cast<const VkDeviceMemoryOverallocationCreateInfoAMD*>(pHeader);

                overallocationBehavior = pMemoryOverallocationCreateInfo->overallocationBehavior;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD:
            {

                deviceFeatures.deviceCoherentMemory = enabledDeviceExtensions.IsExtensionEnabled(
                    DeviceExtensions::AMD_DEVICE_COHERENT_MEMORY) &&
                    reinterpret_cast<const VkPhysicalDeviceCoherentMemoryFeaturesAMD *>(pHeader)->deviceCoherentMemory;

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            {

                if (reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(pHeader)->scalarBlockLayout)
                {
                    deviceFeatures.scalarBlockLayout = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
            {
                if (reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(pHeader)->maintenance4)
                {
                    maintenance4Enabled = true;
                }

                if (reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(pHeader)->privateData)
                {
                    privateDataEnabled = true;
                }

                if (reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(pHeader)->robustImageAccess)
                {
                    deviceFeatures.robustImageAccessExtended = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR:
            {

                if (reinterpret_cast<const VkPhysicalDeviceFragmentShadingRateFeaturesKHR*>(
                    pHeader)->attachmentFragmentShadingRate)
                {
                    deviceFeatures.attachmentFragmentShadingRate = true;
                }
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT:
            {

                if (reinterpret_cast<const VkPhysicalDeviceRobustness2FeaturesEXT*>(pHeader)->robustBufferAccess2)
                {
                    deviceFeatures.robustBufferAccessExtended = true;
                }

                if (reinterpret_cast<const VkPhysicalDeviceRobustness2FeaturesEXT*>(pHeader)->robustImageAccess2)
                {
                    deviceFeatures.robustImageAccessExtended= true;
                }

                if (reinterpret_cast<const VkPhysicalDeviceRobustness2FeaturesEXT*>(pHeader)->nullDescriptor)
                {
                    deviceFeatures.nullDescriptorExtended = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT:
            {
                if (reinterpret_cast<const VkPhysicalDevicePrivateDataFeaturesEXT*>(pHeader)->privateData)
                {
                    privateDataEnabled = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT:
            {
                if (reinterpret_cast<const VkPhysicalDeviceImageRobustnessFeaturesEXT*>(pHeader)->robustImageAccess)
                {
                    deviceFeatures.robustImageAccessExtended= true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT:
            {
               if (reinterpret_cast<const VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT*>(
                   pHeader)->pageableDeviceLocalMemory)
               {
                   pageableDeviceLocalMemory = true;
               }

                if (enabledDeviceExtensions.IsExtensionEnabled(DeviceExtensions::EXT_MEMORY_PRIORITY) ||
                   (enabledDeviceExtensions.IsExtensionEnabled(DeviceExtensions::EXT_PAGEABLE_DEVICE_LOCAL_MEMORY) &&
                    pageableDeviceLocalMemory))
               {
                  deviceFeatures.appControlledMemPriority = true;
               }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR:
            {
               if (reinterpret_cast<const VkPhysicalDeviceMaintenance4FeaturesKHR*>(
                   pHeader)->maintenance4)
               {
                   maintenance4Enabled = true;
               }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT:
            {
                if (reinterpret_cast<const VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT*>(
                    pHeader)->globalPriorityQuery)
                {
                    globalPriorityQueryEnabled = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT:
            {
                if (reinterpret_cast<const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT*>(
                    pHeader)->graphicsPipelineLibrary)
                {
                    deviceFeatures.graphicsPipelineLibrary = true;
                }

                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT:
            {
                const VkPhysicalDeviceAddressBindingReportFeaturesEXT* pAddressBindingReportFeaturesEXT =
                    reinterpret_cast<const VkPhysicalDeviceAddressBindingReportFeaturesEXT*>(pHeader);

                if (pAddressBindingReportFeaturesEXT->reportAddressBinding)
                {
                    deviceFeatures.deviceAddressBindingReport = true;
                    deviceFeatures.gpuMemoryEventHandler      = true;

                    uint32 enabledCallbacks = pInstance->PalPlatform()->GetEnabledCallbackTypes();

                    enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::AllocGpuMemory);
                    enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::FreeGpuMemory);
                    enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubAllocGpuMemory);
                    enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubFreeGpuMemory);
                    enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::BindGpuMemory);

                    pInstance->PalPlatform()->SetEnabledCallbackTypes(enabledCallbacks);
                }

                break;
            }

            default:
                break;
            }

            vkResult = VerifyRequestedPhysicalDeviceFeatures(pPhysicalDevice, &virtStackFrame, pHeader, false);
        }

        // If the pNext chain includes a VkPhysicalDeviceFeatures2 structure, then pEnabledFeatures must be NULL.
        if ((vkResult == VK_SUCCESS) && (pCreateInfo->pEnabledFeatures != nullptr))
        {
            vkResult = VerifyRequestedPhysicalDeviceFeatures(
                pPhysicalDevice,
                &virtStackFrame,
                pCreateInfo->pEnabledFeatures,
                true);
        }

        if (pEnabledFeatures != nullptr)
        {
            if (pEnabledFeatures->robustBufferAccess)
            {
                deviceFeatures.robustBufferAccess = true;
            }

            if (pEnabledFeatures->sparseBinding)
            {
                deviceFeatures.sparseBinding = true;
            }
        }

        if (pPhysicalDevice->GetRuntimeSettings().robustBufferAccess == FeatureForceEnable)
        {
            deviceFeatures.robustBufferAccess = true;
        }
        else if (pPhysicalDevice->GetRuntimeSettings().robustBufferAccess == FeatureForceDisable)
        {
            deviceFeatures.robustBufferAccess = false;
        }

        if (pPhysicalDevice->GetRuntimeSettings().enableRelocatableShaders)
        {
            deviceFeatures.mustWriteImmutableSamplers = true;
        }
        else
        {
            deviceFeatures.mustWriteImmutableSamplers = false;
        }

        if (enabledDeviceExtensions.IsExtensionEnabled(DeviceExtensions::EXT_EXTENDED_DYNAMIC_STATE3))
        {
            if (pPhysicalDevice->GetRuntimeSettings().dynamicPrimitiveTopologyUnrestricted)
            {
                deviceFeatures.dynamicPrimitiveTopologyUnrestricted = true;
                deviceFeatures.assumeDynamicTopologyInLibs = deviceFeatures.graphicsPipelineLibrary;
            }
        }

        if ((pPhysicalDevice->GetRuntimeSettings().strictImageSizeRequirements == StrictImageSizeOn) ||
            ((pPhysicalDevice->GetRuntimeSettings().strictImageSizeRequirements == StrictImageSizeAppControlled) &&
             maintenance4Enabled))
        {
            deviceFeatures.strictImageSizeRequirements = true;
        }
        else
        {
            deviceFeatures.strictImageSizeRequirements = false;
        }
    }

    if (palResult == Pal::Result::Success)
    {
        // Don't free pStackAllocator before virtStackFrame's destructor
        // Otherwise memory corruption when multi-threads are using same linear allocator
        pInstance->StackMgr()->ReleaseAllocator(pStackAllocator);
    }
    else
    {
        vkResult = PalToVkResult(palResult);
    }

    for (uint32 i = 0; ((i < pCreateInfo->queueCreateInfoCount) && (vkResult == VK_SUCCESS)); ++i)
    {
        const VkDeviceQueueCreateInfo* pQueueInfo = &pCreateInfo->pQueueCreateInfos[i];

        if (pQueueInfo->queueCount > pPhysicalDevice->GetQueueFamilyProperties(pQueueInfo->queueFamilyIndex).queueCount)
        {
            vkResult = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    uint32 totalQueues = 0;
    const Pal::DeviceProperties& properties = pPhysicalDevice->PalProperties();

    for (uint32 i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
    {
        const VkDeviceQueueCreateInfo* pQueueInfo = &pCreateInfo->pQueueCreateInfos[i];

        queueCounts[pQueueInfo->queueFamilyIndex] = pQueueInfo->queueCount;
        totalQueues += pQueueInfo->queueCount;

        queueFlags[pQueueInfo->queueFamilyIndex]  = pQueueInfo->flags;

        // handle global priority
        for (const VkStructHeader* pSubHeader = static_cast<const VkStructHeader*>(pQueueInfo->pNext);
             pSubHeader != nullptr;
             pSubHeader = pSubHeader->pNext)
        {

            switch (static_cast<uint32>(pSubHeader->sType))
            {
            case VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR:
            {
                const auto* pPriorityInfo =
                    reinterpret_cast<const VkDeviceQueueGlobalPriorityCreateInfoEXT*>(pSubHeader);

                for (uint32 queueId = 0; queueId < pQueueInfo->queueCount; queueId++)
                {
                    queuePriority[pQueueInfo->queueFamilyIndex][queueId] = pPriorityInfo->globalPriority;
                }

                const auto palEngineType = pPhysicalDevice->GetQueueFamilyPalEngineType(pQueueInfo->queueFamilyIndex);
                uint32 queuePrioritySupportMask = 0;
                for (uint32 engineNdx = 0u;
                     engineNdx < properties.engineProperties[palEngineType].engineCount;
                     ++engineNdx)
                {
                    const auto& engineCapabilities = properties.engineProperties[palEngineType].capabilities[engineNdx];

                    // Leave out High Priority for Universal Queue
                    if ((palEngineType != Pal::EngineTypeUniversal) || IsNormalQueue(engineCapabilities))
                    {
                        queuePrioritySupportMask |= engineCapabilities.queuePrioritySupport;
                    }
                }

                Pal::QueuePrioritySupport palQueuePriority = VkToPalGlobaPrioritySupport(pPriorityInfo->globalPriority);
                if (((palQueuePriority & queuePrioritySupportMask) == false) &&
                     globalPriorityQueryEnabled &&
                     (pPhysicalDevice->GetRuntimeSettings().ignoreDeviceQueuePriorityFailures == false))
                {
                    vkResult = VK_ERROR_INITIALIZATION_FAILED;
                }
            }
            break;
            default:
                // Skip any unknown extension structures
                break;
            }
        }
    }

    GpuMemoryEventHandler::DeviceMemoryReportCallbacks deviceMemoryReportCallbacks {nullptr};

    for (pHeader = static_cast<const VkStructHeader*>(pCreateInfo->pNext); pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO_EXT:
            {
                const auto* pPrivateDataInfo = reinterpret_cast<const VkDevicePrivateDataCreateInfoEXT*>(pHeader);

                privateDataSlotRequestCount += pPrivateDataInfo->privateDataSlotRequestCount;
                break;
            }

            case VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT:
            {
                deviceFeatures.deviceMemoryReport    = true;
                deviceFeatures.gpuMemoryEventHandler = true;

                uint32 enabledCallbacks = pInstance->PalPlatform()->GetEnabledCallbackTypes();

                enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::AllocGpuMemory);
                enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::FreeGpuMemory);
                enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubAllocGpuMemory);
                enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubFreeGpuMemory);

                pInstance->PalPlatform()->SetEnabledCallbackTypes(enabledCallbacks);

                const auto* pDeviceMemoryReportInfo =
                    reinterpret_cast<const VkDeviceDeviceMemoryReportCreateInfoEXT*>(pHeader);

                GpuMemoryEventHandler::DeviceMemoryReportCallback callback =
                    {pDeviceMemoryReportInfo->pfnUserCallback, pDeviceMemoryReportInfo->pUserData, nullptr};

                deviceMemoryReportCallbacks.PushBack(callback);
                break;
            }

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
                // The implementation of multiview does not require special handling,
                // therefore the following multview features can be ignored.
                //    - pMultiviewFeatures->multiview
                //    - pMultiviewFeatures->multiviewGeometryShader
                //    - pMultiviewFeatures->multiviewTessellationShader
                //
                // Fall through to default.

            default:
                // Skip any unknown extension structures
                break;
        }
    }

    bool useComputeAsTransferQueue = false;

    // If the app requested any sparse residency features, but we can't reliably support PRT on transfer queues,
    // we have to fall-back on a compute queue instead and avoid using DMA.
    if ((pEnabledFeatures != nullptr) &&
        pPhysicalDevices[DefaultDeviceIndex]->IsComputeEngineSupported() &&
        (pPhysicalDevices[DefaultDeviceIndex]->IsPrtSupportedOnDmaEngine() == false))
    {
        useComputeAsTransferQueue = (pEnabledFeatures->sparseResidencyBuffer  ||
                                     pEnabledFeatures->sparseResidencyImage2D ||
                                     pEnabledFeatures->sparseResidencyImage3D);
    }

    // Create the queues for the device up-front and hand them to the new device object.
    size_t apiDeviceSize = ObjectSize(sizeof(ApiDevice), numDevices);
    size_t apiQueueSize  = sizeof(ApiQueue);

    // Compute the amount of memory required for each queue type.
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;

    if (privateDataEnabled)
    {

        privateDataSlotRequestCount = (privateDataSlotRequestCount > 0) ? privateDataSlotRequestCount : 1;

        privateDataSize = Util::Pow2Align(
            sizeof(PrivateDataStorage) + sizeof(uint64_t) * (privateDataSlotRequestCount - 1),
            VK_DEFAULT_MEM_ALIGN);
    }

    const RuntimeSettings& settings = pPhysicalDevice->GetRuntimeSettings();

    if (vkResult == VK_SUCCESS)
    {
        pMemory = pInstance->AllocMem(
            privateDataSize + apiDeviceSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (privateDataEnabled && (pMemory != nullptr))
        {
             memset(pMemory, 0, privateDataSize);
             pMemory = Util::VoidPtrInc(pMemory, privateDataSize);
        }

        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if ((pCreateInfo != nullptr) && (pMemory != nullptr))
    {
        vkResult = VK_SUCCESS;

        // Finalize the physical device settings before they are cached in the device
        pPhysicalDevices[DefaultDeviceIndex]->GetSettingsLoader()->FinalizeSettings(enabledDeviceExtensions);

        if (settings.simulateExtDeviceMemoryReport)
        {
            deviceFeatures.deviceMemoryReport    = true;
            deviceFeatures.gpuMemoryEventHandler = true;
        }

        if (settings.simulateExtDeviceAddressBindingReport)
        {
            deviceFeatures.deviceAddressBindingReport = true;
            deviceFeatures.gpuMemoryEventHandler      = true;
        }

        // Construct API device object.
        VK_INIT_DISPATCHABLE(Device, pMemory, (
            numDevices,
            pPhysicalDevices,
            pPalDevices,
            pCreateInfo,
            enabledDeviceExtensions,
            pEnabledFeatures,
            useComputeAsTransferQueue,
            privateDataSlotRequestCount,
            privateDataSize,
            deviceFeatures));

        ApiDevice* pDispatchableDevice = static_cast<ApiDevice*>(pMemory);
        ApiQueue*  pDispatchableQueues[Queue::MaxQueueFamilies][Queue::MaxQueuesPerFamily] = {};

        auto gpuMemoryEventHandler = pInstance->GetGpuMemoryEventHandler();
        Device* pDevice            = ApiDevice::ObjectFromHandle(reinterpret_cast<VkDevice>(pDispatchableDevice));

        if (settings.simulateExtDeviceMemoryReport)
        {
            uint32 enabledCallbacks = pInstance->PalPlatform()->GetEnabledCallbackTypes();

            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::AllocGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::FreeGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubAllocGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubFreeGpuMemory);

            pInstance->PalPlatform()->SetEnabledCallbackTypes(enabledCallbacks);
        }

        if (settings.simulateExtDeviceAddressBindingReport)
        {
            uint32 enabledCallbacks = pInstance->PalPlatform()->GetEnabledCallbackTypes();

            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::AllocGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::FreeGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubAllocGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::SubFreeGpuMemory);
            enabledCallbacks |= 1 << static_cast<uint32>(Pal::Developer::CallbackType::BindGpuMemory);

            pInstance->PalPlatform()->SetEnabledCallbackTypes(enabledCallbacks);
        }

        for (auto iter = deviceMemoryReportCallbacks.Begin(); iter.IsValid(); iter.Next())
        {
            iter.Get().pDevice = pDevice;

            gpuMemoryEventHandler->RegisterDeviceMemoryReportCallback(iter.Get());
        }

        if (deviceFeatures.gpuMemoryEventHandler)
        {
            gpuMemoryEventHandler->EnableGpuMemoryEvents(pDevice);
        }

        size_t       palQueueMemoryOffset = 0;
        uint32       tmzQueueIndex        = 0;

        for (queueFamilyIndex = 0;
             (queueFamilyIndex < Queue::MaxQueueFamilies) && (vkResult == VK_SUCCESS);
             queueFamilyIndex++)
        {
            for (queueIndex = 0;
                 (queueIndex < queueCounts[queueFamilyIndex]) && (vkResult == VK_SUCCESS);
                 queueIndex++)
            {
                VkQueue vkQueue;
                vkResult = Queue::Create(*pDispatchableDevice,
                              pAllocator,
                              queueFlags[queueFamilyIndex],
                              queueFamilyIndex,
                              queueIndex,
                              queuePriority[queueFamilyIndex][queueIndex],
                              dedicatedComputeUnits[queueFamilyIndex][queueIndex],
                              false,
                              &vkQueue);

                if (vkResult == VK_SUCCESS)
                {
                    pDispatchableQueues[queueFamilyIndex][queueIndex] = reinterpret_cast<ApiQueue*>(vkQueue);
                }
            }
        }

        if (vkResult != VK_SUCCESS)
        {
            // Cleanup any successfully created queues before failure
            for (queueFamilyIndex = 0; queueFamilyIndex < Queue::MaxQueueFamilies; queueFamilyIndex++)
            {
                for (queueIndex = 0; queueIndex < queueCounts[queueFamilyIndex]; queueIndex++)
                {
                    if (pDispatchableQueues[queueFamilyIndex][queueIndex] != nullptr)
                    {
                        Queue* pQueue = static_cast<Queue*>(*(pDispatchableQueues[queueFamilyIndex][queueIndex]));
                        pQueue->Destroy(*pDispatchableDevice, pAllocator);
                    }
                }
            }

            (*pDispatchableDevice)->Destroy(pAllocator);
        }
        else
        {
            vkResult = (*pDispatchableDevice)->Initialize(
                pPhysicalDevice,
                &pDispatchableQueues[0][0],
                enabledDeviceExtensions,
                overallocationBehavior,
                bufferDeviceAddressMultiDeviceEnabled,
                pageableDeviceLocalMemory,
                pAllocator);

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

#if VKI_RAY_TRACING
// =====================================================================================================================
VkResult Device::CreateRayTraceState()
{
    VkResult result = VK_SUCCESS;

    const RuntimeSettings& settings = GetRuntimeSettings();

    void* pMemory = m_pInstance->AllocMem(
            sizeof(RayTracingDevice),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        memset(pMemory, 0, sizeof(RayTracingDevice));

        m_pRayTrace = VK_PLACEMENT_NEW(pMemory) RayTracingDevice(this);

        result = m_pRayTrace->Init();
    }

    return result;
}
#endif

// =====================================================================================================================
// Bring up the Vulkan device.
VkResult Device::Initialize(
    PhysicalDevice*                         pPhysicalDevice,
    ApiQueue**                              pQueues,
    const DeviceExtensions::Enabled&        enabled,
    const VkMemoryOverallocationBehaviorAMD overallocationBehavior,
    bool                                    bufferDeviceAddressMultiDeviceEnabled,
    bool                                    pageableDeviceLocalMemory,
    const VkAllocationCallbacks*            pAllocator)
{
    // Initialize the internal memory manager
    VkResult result = m_internalMemMgr.Init();

    // Initialize the render state cache
    if (result == VK_SUCCESS)
    {
        result = m_renderStateCache.Init();
    }

    memcpy(&m_pQueues, pQueues, sizeof(m_pQueues));
    Pal::DeviceProperties deviceProps = {};
    result = PalToVkResult(PalDevice(DefaultDeviceIndex)->GetProperties(&deviceProps));

    if (result == VK_SUCCESS)
    {
        result = CreateSharedPalCmdAllocator(
        );
    }

    if (m_enabledFeatures.graphicsPipelineLibrary)
    {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {};
        pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeLayoutCreateInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
        result = PipelineLayout::Create(this, &pipeLayoutCreateInfo, pAllocator, &pipelineLayout);
        if (result == VK_SUCCESS)
        {
            m_pNullPipelineLayout = PipelineLayout::ObjectFromHandle(pipelineLayout);
        }
    }

#if VKI_RAY_TRACING
    m_properties.rayTracingIpLevel          = deviceProps.gfxipProperties.rayTracingIp;
#endif
    m_properties.virtualMemAllocGranularity = deviceProps.gpuMemoryProperties.virtualMemAllocGranularity;
    m_properties.virtualMemPageSize         = deviceProps.gpuMemoryProperties.virtualMemPageSize;
    m_properties.descriptorSizes.bufferView = deviceProps.gfxipProperties.srdSizes.bufferView;
    m_properties.descriptorSizes.imageView  = deviceProps.gfxipProperties.srdSizes.imageView;
    m_properties.descriptorSizes.fmaskView  = deviceProps.gfxipProperties.srdSizes.fmaskView;
    m_properties.descriptorSizes.sampler    = deviceProps.gfxipProperties.srdSizes.sampler;
    m_properties.descriptorSizes.bvh        = deviceProps.gfxipProperties.srdSizes.bvh;
    // Size of combined image samplers is the sum of the image and sampler SRD sizes (8DW + 4DW)
    m_properties.descriptorSizes.combinedImageSampler =
        m_properties.descriptorSizes.imageView +
        m_properties.descriptorSizes.sampler;

    // The worst case alignment requirement of descriptors is always 2DWs. There's no way to query this from PAL yet,
    // but for now a hard coded value will do the job.
    m_properties.descriptorSizes.alignmentInDwords = 2;

    m_properties.palSizes.colorTargetView  = PalDevice(DefaultDeviceIndex)->GetColorTargetViewSize(nullptr);
    m_properties.palSizes.depthStencilView = PalDevice(DefaultDeviceIndex)->GetDepthStencilViewSize(nullptr);

    m_properties.connectThroughThunderBolt = (deviceProps.pciProperties.flags.gpuConnectedViaThunderbolt) ? true : false;

    m_properties.timestampQueryPoolSlotSize =
        deviceProps.engineProperties[Pal::EngineTypeDma].minTimestampAlignment > 0 ?
        deviceProps.engineProperties[Pal::EngineTypeDma].minTimestampAlignment :
        deviceProps.engineProperties[Pal::EngineTypeUniversal].minTimestampAlignment;

    // If VkPhysicalDeviceBufferDeviceAddressFeaturesEXT.bufferDeviceAddressMultiDevice is enabled
    // and if globalGpuVaSupport is supported and if multiple devices are used set the global GpuVa.
    m_useGlobalGpuVa = (bufferDeviceAddressMultiDeviceEnabled                    &&
                        deviceProps.gpuMemoryProperties.flags.globalGpuVaSupport &&
                        IsMultiGpu());

    Pal::VrsShadingRate maxPalVrsShadingRate;
    bool vrsMaskIsValid = Util::BitMaskScanReverse(reinterpret_cast<uint32*>(&maxPalVrsShadingRate),
                                                   deviceProps.gfxipProperties.supportedVrsRates);
    if (vrsMaskIsValid)
    {
        m_maxVrsShadingRate = PalToVkShadingSize(maxPalVrsShadingRate);
    }

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

#if VKI_GPU_DECOMPRESS
    if ((result == VK_SUCCESS) && (m_settings.enableShaderDecode))
    {
        void* pGpuDecoderLayer = nullptr;

        pGpuDecoderLayer = VkInstance()->AllocMem(sizeof(GpuDecoderLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pGpuDecoderLayer != nullptr)
        {
            m_pGpuDecoderLayer = VK_PLACEMENT_NEW(pGpuDecoderLayer) GpuDecoderLayer(this);
            result = m_pGpuDecoderLayer->Init(this);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
#endif

    if (result == VK_SUCCESS)
    {
        // Allow overallocation by disabling tracking as requested by the setting.
        if (m_settings.memoryDeviceOverallocationAllowed)
        {
            m_allocationSizeTracking = false;
        }

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
            case AppProfile::Rage2:
                m_allocationSizeTracking = false;
                break;
            default:
                break;
            }

            if (enabled.IsExtensionEnabled(DeviceExtensions::ExtensionId::AMD_MEMORY_OVERALLOCATION_BEHAVIOR))
            {
                // This setting is for app detects only and violates AMD_MEMORY_OVERALLOCATION_BEHAVIOR rules.
                VK_ASSERT(m_settings.overrideHeapChoiceToLocal == 0);

                switch (overallocationBehavior)
                {
                case VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD:
                    m_allocationSizeTracking = false;
                    break;
                case VK_MEMORY_OVERALLOCATION_BEHAVIOR_DISALLOWED_AMD:
                    m_allocationSizeTracking = true;
                    break;
                default:
                    break;
                }
            }
            else if (enabled.IsExtensionEnabled(DeviceExtensions::ExtensionId::EXT_PAGEABLE_DEVICE_LOCAL_MEMORY) &&
                     pageableDeviceLocalMemory)
            {
                m_allocationSizeTracking = false;
            }
            else if (m_settings.overrideHeapChoiceToLocal != 0)
            {
                // This setting utilizes overallocation behavior's heap size tracking. Overallocation to the local
                // visible heap is not desired but permitted as a precaution.
                m_allocationSizeTracking = true;
                m_overallocationRequestedForPalHeap[Pal::GpuHeap::GpuHeapLocal] = true;
            }
        }

        if (m_allocationSizeTracking == false)
        {
            // Add back-up heaps for device-local heaps
            m_overallocationRequestedForPalHeap[Pal::GpuHeap::GpuHeapInvisible] = true;
            m_overallocationRequestedForPalHeap[Pal::GpuHeap::GpuHeapLocal]     = true;
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
        // Get the current values of driver features, from an app profile or global settings.
        UpdateFeatureSettings();
    }

#if VKI_RAY_TRACING
    if ((result == VK_SUCCESS) &&
        (enabled.IsExtensionEnabled(DeviceExtensions::KHR_RAY_TRACING_PIPELINE) ||
         enabled.IsExtensionEnabled(DeviceExtensions::KHR_RAY_QUERY)))
    {
        result = CreateRayTraceState();
    }
#endif

    if (result == VK_SUCCESS)
    {
        InitDispatchTable();
    }

    if ((result == VK_SUCCESS) &&
        IsExtensionEnabled(DeviceExtensions::EXT_CUSTOM_BORDER_COLOR))
    {
        result = AllocBorderColorPalette();
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

    if (m_enabledExtensions.IsExtensionEnabled(DeviceExtensions::KHR_PUSH_DESCRIPTOR))
    {
        ep->vkCmdPushDescriptorSetKHR             = CmdBuffer::GetCmdPushDescriptorSetKHRFunc(this);
        ep->vkCmdPushDescriptorSetWithTemplateKHR = CmdBuffer::GetCmdPushDescriptorSetWithTemplateKHRFunc(this);
    }

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

#if VKI_GPU_DECOMPRESS
    if (m_pGpuDecoderLayer != nullptr)
    {
        m_pGpuDecoderLayer->OverrideDispatchTable(&m_dispatchTable);
    }
#endif

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

#if VKI_RAY_TRACING
    if (m_pRayTrace != nullptr)
    {
        m_pRayTrace->Destroy();
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

    if (m_pNullPipelineLayout != nullptr)
    {
        m_pNullPipelineLayout->Destroy(this, pAllocator);
    }

#if VKI_GPU_DECOMPRESS
    if (m_pGpuDecoderLayer != nullptr)
    {
        Util::Destructor(m_pGpuDecoderLayer);
        VkInstance()->FreeMem(m_pGpuDecoderLayer);
    }
#endif

    for (uint32_t i = 0; i < Queue::MaxQueueFamilies; ++i)
    {
        for (uint32_t j = 0; (j < Queue::MaxQueuesPerFamily) && (m_pQueues[i][j] != nullptr); ++j)
        {
            Queue *pQueue = static_cast<Queue*>(*m_pQueues[i][j]);
            pQueue->Destroy(this, pAllocator);
        }
    }

    for (uint32_t i = 0; i < BltMsaaStateCount; ++i)
    {
        m_renderStateCache.DestroyMsaaState(&m_pBltMsaaState[i][0], nullptr);
    }

    DestroyInternalPipelines();

    DestroySharedPalCmdAllocator();

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

            if (m_perGpu[deviceIdx].pSwCompositingCmdBuffer != nullptr)
            {
                m_perGpu[deviceIdx].pSwCompositingCmdBuffer->Destroy();
            }

            VkInstance()->FreeMem(m_perGpu[deviceIdx].pSwCompositingMemory);
        }
    }

    DestroyBorderColorPalette();

    m_renderStateCache.Destroy();

    Util::Destructor(this);

    if (m_enabledFeatures.deviceMemoryReport)
    {
        VkInstance()->GetGpuMemoryEventHandler()->UnregisterDeviceMemoryReportCallbacks(this);
    }

    if (m_enabledFeatures.gpuMemoryEventHandler)
    {
        VkInstance()->GetGpuMemoryEventHandler()->DisableGpuMemoryEvents(this);
    }

    FreeApiObject(VkInstance()->GetAllocCallbacks(), ApiDevice::FromObject(this));

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

    else if ((palQueueType == Pal::QueueType::QueueTypeCompute) && m_useUniversalAsComputeQueue)
    {
        palQueueType = Pal::QueueType::QueueTypeUniversal;
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

    else if ((palEngineType == Pal::EngineType::EngineTypeCompute) && m_useUniversalAsComputeQueue)
    {
        palEngineType = Pal::EngineType::EngineTypeUniversal;
    }

    return palEngineType;
}

// =====================================================================================================================
VkResult Device::CreateInternalComputePipeline(
    size_t                         codeByteSize,
    const uint8_t*                 pCode,
    uint32_t                       numUserDataNodes,
    Vkgc::ResourceMappingRootNode* pUserDataNodes,
    VkShaderModuleCreateFlags      internalShaderFlags,
    bool                           forceWave64,
    const VkSpecializationInfo*    pSpecializationInfo,
    InternalPipeline*              pInternalPipeline)
{
    VK_ASSERT(numUserDataNodes <= VK_ARRAY_SIZE(pInternalPipeline->userDataNodeOffsets));

    VkResult             result               = VK_SUCCESS;
    PipelineCompiler*    pCompiler            = GetCompiler(DefaultDeviceIndex);
    ShaderModuleHandle   shaderModule         = {};
    const void*          pPipelineBinary      = nullptr;
    size_t               pipelineBinarySize   = 0;
    ShaderOptimizerKey   shaderOptimzierKey   = {};
    PipelineOptimizerKey pipelineOptimizerKey = {};
    PipelineMetadata     binaryMetadata       = {};

    void*                pPipelineMem        = nullptr;

    ComputePipelineBinaryCreateInfo pipelineBuildInfo = {};

    pCompiler->ApplyPipelineOptions(this, 0, &pipelineBuildInfo.pipelineInfo.options
    );

    shaderOptimzierKey.stage              = ShaderStage::ShaderStageCompute;
    pipelineOptimizerKey.shaderCount      = 1;
    pipelineOptimizerKey.pShaders         = &shaderOptimzierKey;
    pipelineBuildInfo.pPipelineProfileKey = &pipelineOptimizerKey;
    pipelineBuildInfo.pBinaryMetadata     = &binaryMetadata;

    // Build shader module
    result = pCompiler->BuildShaderModule(
        this,
        0,
        internalShaderFlags,
        codeByteSize,
        pCode,
        false,
        true,
        nullptr,
        nullptr,
        &shaderModule);

    Vkgc::BinaryData spvBin = { codeByteSize, pCode };

    if (result == VK_SUCCESS)
    {
        // Build pipeline binary
        auto pShaderInfo = &pipelineBuildInfo.pipelineInfo.cs;
        pipelineBuildInfo.compilerType   = PipelineCompilerTypeLlpc;
        pShaderInfo->pModuleData         = shaderModule.pLlpcShaderModule;
        pShaderInfo->pSpecializationInfo = pSpecializationInfo;
        pShaderInfo->pEntryTarget        = Vkgc::IUtil::GetEntryPointNameFromSpirvBinary(&spvBin);
        pShaderInfo->entryStage          = Vkgc::ShaderStageCompute;

        pipelineBuildInfo.pipelineInfo.resourceMapping.pUserDataNodes = pUserDataNodes;
        pipelineBuildInfo.pipelineInfo.resourceMapping.userDataNodeCount = numUserDataNodes;

        pCompiler->ApplyDefaultShaderOptions(ShaderStage::ShaderStageCompute, 0, &pShaderInfo->options);

        if (forceWave64)
        {
            pShaderInfo->options.waveSize = 64;
        }

        Pal::ShaderHash codeHash = ShaderModule::GetCodeHash(
            ShaderModule::BuildCodeHash(
                pCode,
                codeByteSize),
            pShaderInfo->pEntryTarget);

        GetShaderOptimizer()->CreateShaderOptimizerKey(
            pShaderInfo->pModuleData,
            codeHash,
            Vkgc::ShaderStage::ShaderStageCompute,
            codeByteSize,
            &shaderOptimzierKey);

        PipelineShaderOptionsPtr options = {};
        options.pPipelineOptions = &pipelineBuildInfo.pipelineInfo.options;
        options.pOptions = &pShaderInfo->options;

        // Override the compile parameters based on any app profile
        GetShaderOptimizer()->OverrideShaderCreateInfo(
            pipelineOptimizerKey,
            0,
            options);

        // PAL Pipeline caching
        Util::Result          cacheResult = Util::Result::NotFound;
        Util::MetroHash::Hash cacheId     = {};

        bool isUserCacheHit     = false;
        bool isInternalCacheHit = false;

        if (pCompiler->GetBinaryCache() != nullptr)
        {
            pCompiler->GetComputePipelineCacheId(
                DefaultDeviceIndex,
                &pipelineBuildInfo,
                Vkgc::IPipelineDumper::GetPipelineHash(&pipelineBuildInfo.pipelineInfo),
                VkPhysicalDevice(DefaultDeviceIndex)->GetSettingsLoader()->GetSettingsHash(),
                &cacheId);

            cacheResult = pCompiler->GetCachedPipelineBinary(
                &cacheId,
                nullptr,
                &pipelineBinarySize,
                &pPipelineBinary,
                &isUserCacheHit,
                &isInternalCacheHit,
                &pipelineBuildInfo.freeCompilerBinary,
                &pipelineBuildInfo.pipelineFeedback);
        }

        if (cacheResult != Util::Result::Success)
        {
            result = pCompiler->CreateComputePipelineBinary(
                this,
                DefaultDeviceIndex,
                nullptr,
                &pipelineBuildInfo,
                &pipelineBinarySize,
                &pPipelineBinary,
                &cacheId);

            if (result == VK_SUCCESS)
            {
                pCompiler->CachePipelineBinary(
                    &cacheId,
                    nullptr,
                    pipelineBinarySize,
                    pPipelineBinary,
                    isUserCacheHit,
                    isInternalCacheHit);
            }
        }

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
            pInternalPipeline->userDataNodeOffsets[i] = pUserDataNodes[i].node.offsetInDwords;
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

    static constexpr uint8_t AccelerationStructureQueryPoolSpv[] =
    {
    #include "shaders/copy_acceleration_structure_query_pool_spv.h"
    };

    const uint8_t* pSpvCode = useStridedShader ? CopyTimestampQueryPoolStridedSpv : CopyTimestampQueryPoolSpv;
    const size_t spvCodeSize = useStridedShader ?
        sizeof(CopyTimestampQueryPoolStridedSpv) : sizeof(CopyTimestampQueryPoolSpv);

    Vkgc::ResourceMappingRootNode userDataNodes[3] = {};

    const uint32_t uavViewSize = m_properties.descriptorSizes.bufferView / sizeof(uint32_t);

    // Timestamp counter storage view
    userDataNodes[0].node.type = useStridedShader ?
        Vkgc::ResourceMappingNodeType::DescriptorBuffer : Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
    userDataNodes[0].node.offsetInDwords = 0;
    userDataNodes[0].node.sizeInDwords = uavViewSize;
    userDataNodes[0].node.srdRange.set = 0;
    userDataNodes[0].node.srdRange.binding = 0;
    userDataNodes[0].node.srdRange.strideInDwords = 0;
    userDataNodes[0].visibility = Vkgc::ShaderStageComputeBit;

    // Copy destination storage view
    userDataNodes[1].node.type = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
    userDataNodes[1].node.offsetInDwords = uavViewSize;
    userDataNodes[1].node.sizeInDwords = uavViewSize;
    userDataNodes[1].node.srdRange.set = 0;
    userDataNodes[1].node.srdRange.binding = 1;
    userDataNodes[1].node.srdRange.strideInDwords = 0;
    userDataNodes[1].visibility = Vkgc::ShaderStageComputeBit;

    // Inline constant data
    userDataNodes[2].node.type = Vkgc::ResourceMappingNodeType::PushConst;
    userDataNodes[2].node.offsetInDwords = 2 * uavViewSize;
    userDataNodes[2].node.sizeInDwords = 4;
    userDataNodes[2].node.srdRange.set = Vkgc::InternalDescriptorSetId;
    userDataNodes[2].node.srdRange.strideInDwords = 0;
    userDataNodes[2].visibility = Vkgc::ShaderStageComputeBit;

    result = CreateInternalComputePipeline(
        spvCodeSize,
        pSpvCode,
        VK_ARRAY_SIZE(userDataNodes),
        userDataNodes,
        0,
        false,
        nullptr,
        &m_timestampQueryCopyPipeline);

#if VKI_RAY_TRACING
    if (result == VK_SUCCESS)
    {
        userDataNodes[2].node.sizeInDwords = 8;

        result = CreateInternalComputePipeline(
            sizeof(AccelerationStructureQueryPoolSpv),
            AccelerationStructureQueryPoolSpv,
            VK_ARRAY_SIZE(userDataNodes),
            userDataNodes,
            0,
            false,
            nullptr,
            &m_accelerationStructureQueryCopyPipeline);
    }
#endif

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
#if VKI_RAY_TRACING
    DestroyInternalPipeline(&m_accelerationStructureQueryCopyPipeline);
#endif
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
VkQueue Device::GetQueue(
    Pal::EngineType engineType,
    Pal::QueueType  queueType)
{
    VkQueue queueHandle = VK_NULL_HANDLE;

    for (uint32_t familyIdx = 0; familyIdx < Queue::MaxQueueFamilies; ++familyIdx)
    {
        if ((GetQueueFamilyPalQueueType(familyIdx) == queueType) &&
            (GetQueueFamilyPalEngineType(familyIdx) == engineType))
        {
            GetQueue(familyIdx, 0, &queueHandle);

            if (queueHandle != VK_NULL_HANDLE)
            {
                break;
            }
        }
    }

    return queueHandle;
}

// =====================================================================================================================
void Device::GetQueue(
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue)
{
    PhysicalDevice* pPhysicalDevice = VkPhysicalDevice(DefaultDeviceIndex);

    if (queueIndex < pPhysicalDevice->GetQueueFamilyProperties(queueFamilyIndex).queueCount)
    {
        *pQueue = reinterpret_cast<VkQueue>(m_pQueues[queueFamilyIndex][queueIndex]);
    }
    else
    {
        *pQueue = VK_NULL_HANDLE;
    }
}
// =====================================================================================================================
void Device::GetQueue2(
    const VkDeviceQueueInfo2* pQueueInfo,
    VkQueue*                  pQueue)
{
    VK_ASSERT(pQueueInfo->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2);

    VkDeviceQueueCreateFlags  flags             = pQueueInfo->flags;;
    uint32                    queueFamilyIndex  = pQueueInfo->queueFamilyIndex;
    uint32                    queueIndex        = pQueueInfo->queueIndex;
    const void*               pNext             = pQueueInfo->pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        default:
            VK_NOT_IMPLEMENTED;
            break;
        };

        pNext = pHeader->pNext;
    }

    *pQueue = VK_NULL_HANDLE;

    uint32 queueCount = VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyProperties(queueFamilyIndex).queueCount;

    // Queues with flags will be indexed separately to queues without flags
    // Consider only those queues with matching flags
    uint32 testIndex = 0;
    for (uint32 i = 0; i < queueCount; i++)
    {
        ApiQueue* pFoundQueue = m_pQueues[queueFamilyIndex][i];

        if ((pFoundQueue != nullptr) && ((*pFoundQueue)->GetFlags() == flags))
        {
            if (testIndex == queueIndex)
            {
                *pQueue = reinterpret_cast<VkQueue>(pFoundQueue);
                break;
            }
            testIndex++;
        }
    }
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
    const VkRenderPassCreateInfo2*      pCreateInfo,
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
    VkResult result = Image::Create(this, pCreateInfo, pAllocator, pImage);

    if (result == VK_SUCCESS)
    {
        Image*               pCreatedImage      = Image::ObjectFromHandle(*pImage);
        VkMemoryRequirements memoryRequirements = {};

        pCreatedImage->CalculateMemoryRequirementsAtCreate(
            this,
            pCreateInfo,
            &memoryRequirements);

        if (m_enabledFeatures.strictImageSizeRequirements &&
            Formats::IsDepthStencilFormat(pCreateInfo->format) &&
            (pCreatedImage->IsSparse() == false))
        {
            Image::CalculateAlignedMemoryRequirements(this, pCreateInfo, &memoryRequirements);
        }

        pCreatedImage->SetMemoryRequirements(memoryRequirements);
    }

    return result;
}

// =====================================================================================================================
VkResult Device::CreateImageView(
    const VkImageViewCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*      pAllocator,
    VkImageView*                      pView)
{
    return ImageView::Create(this, pCreateInfo, pAllocator, pView);
}

// =====================================================================================================================
VkResult Device::CreateGraphicsPipelines(
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    VK_ASSERT(pCreateInfos != nullptr);
    VK_ASSERT(pPipelines != nullptr);

    VkResult finalResult = VK_SUCCESS;
    PipelineCache* pPipelineCache = PipelineCache::ObjectFromHandle(pipelineCache);

    // Initialize output array to VK_NULL_HANDLE
    for (uint32_t i = 0; i < count; ++i)
    {
        pPipelines[i] = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const VkGraphicsPipelineCreateInfo* pCreateInfo = &pCreateInfos[i];
        PipelineCreateFlags flags = GetPipelineCreateFlags(pCreateInfo);

        VkResult result = GraphicsPipelineCommon::Create(
            this,
            pPipelineCache,
            pCreateInfo,
            flags,
            pAllocator,
            &pPipelines[i]);

        if (result != VK_SUCCESS)
        {
            // In case of failure, VK_NULL_HANDLE must be set
            VK_ASSERT(pPipelines[i] == VK_NULL_HANDLE);

            // Capture the first failure result and save it to be returned
            finalResult = (finalResult != VK_SUCCESS) ? finalResult : result;

            if (flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
            {
                break;
            }
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
    VK_ASSERT(pCreateInfos != nullptr);
    VK_ASSERT(pPipelines != nullptr);

    VkResult finalResult = VK_SUCCESS;
    PipelineCache* pPipelineCache = PipelineCache::ObjectFromHandle(pipelineCache);

    // Initialize output array to VK_NULL_HANDLE
    for (uint32_t i = 0; i < count; ++i)
    {
        pPipelines[i] = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const VkComputePipelineCreateInfo* pCreateInfo = &pCreateInfos[i];
        PipelineCreateFlags flags = GetPipelineCreateFlags(pCreateInfo);
        VkResult result = VK_SUCCESS;

            result = ComputePipeline::Create(
                this,
                pPipelineCache,
                pCreateInfo,
                flags,
                pAllocator,
                &pPipelines[i]);

        if (result != VK_SUCCESS)
        {
            // In case of failure, VK_NULL_HANDLE must be set
            VK_ASSERT(pPipelines[i] == VK_NULL_HANDLE);

            // Capture the first failure result and save it to be returned
            finalResult = (finalResult != VK_SUCCESS) ? finalResult : result;

            if (flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
            {
                break;
            }
        }
    }

    return finalResult;
}

#if VKI_RAY_TRACING
//=====================================================================================================================
// Clamps LDS traversal stack size to a power of two value, based on the thread group size.
uint32_t Device::ClampLdsStackSizeFromThreadGroupSize(
    uint32_t ldsStackSize // LDS traversal stack size in DWORDs
    ) const
{
    const Pal::DeviceProperties& props      = GetPalProperties();
    const uint32_t threadGroupSize          = GetRuntimeSettings().dispatchRaysThreadGroupSize;
    const uint32_t maxLdsStackSizeInBytes   = props.gfxipProperties.shaderCore.ldsSizePerThreadGroup;
    const uint32_t ldsStackSizeInBytes      = threadGroupSize * ldsStackSize * sizeof(uint32_t);

    if (ldsStackSizeInBytes > maxLdsStackSizeInBytes)
    {
        // 64 KB is the max LDS available so we need to clamp the number of stack entries per thread
        // to a lower power of two value.
        ldsStackSize = (1 << Util::Log2(maxLdsStackSizeInBytes / (threadGroupSize * sizeof(uint32_t))));
    }

    return ldsStackSize;
}

//=====================================================================================================================
// Gets LDS size per thread in DWORDs
// This includes the space for both the traversal stack and LDS spilling
uint32_t Device::GetDefaultLdsSizePerThread(
    bool isIndirect
    ) const
{
    const RuntimeSettings& settings = GetRuntimeSettings();

    // Initialise to default setting
    uint32_t ldsStackSize = settings.ldsStackSize;

    // Optionally resize based on target occupancy setting for indirect pipelines
    if (isIndirect && settings.enableOptimalLdsStackSizeForIndirect)
    {
        ldsStackSize = GetMaxLdsForTargetOccupancy(settings.indirectCallTargetOccupancyPerSimd);
    }

    // Clamp LDS size to maximum available based on thread group size
    uint32_t ldsSizePerTread = ClampLdsStackSizeFromThreadGroupSize(ldsStackSize);

    return ldsSizePerTread;
}

//=====================================================================================================================
// Gets LDS traversal stack size per thread in DWORDs
uint32_t Device::GetDefaultLdsTraversalStackSize(
    bool isIndirect
    ) const
{
    uint32_t ldsSizePerTread = GetDefaultLdsSizePerThread(isIndirect);

    // Clamp to power of 2
    uint32_t ldsStackSize = 1 << Util::Log2(ldsSizePerTread);

    return ldsStackSize;
}

//=====================================================================================================================
// Calculates max LDS per thread in DWORDS that can be used for given target occupancy
uint32_t Device::GetMaxLdsForTargetOccupancy(
    float targetOccupancyPerSimd
    ) const
{
    const RuntimeSettings& settings         = GetRuntimeSettings();
    const auto& props                       = GetPalProperties().gfxipProperties.shaderCore;

    const uint32_t targetNumWavesPerSimd    = Util::Max(1U, static_cast<uint32_t>(round(targetOccupancyPerSimd *
        props.numWavefrontsPerSimd)));
    const uint32_t targetNumWavesPerCu      = props.numSimdsPerCu * targetNumWavesPerSimd;
    const uint32_t threadGroupSize          = settings.dispatchRaysThreadGroupSize;
    const uint32_t maxLdsPerTg              = ((props.ldsSizePerCu * threadGroupSize) /
        (targetNumWavesPerCu * props.nativeWavefrontSize));
    const uint32_t clampedLdsPerTg          = Util::RoundDownToMultiple(maxLdsPerTg, props.ldsGranularity);
    const uint32_t clampedLdsPerThread      = clampedLdsPerTg / (threadGroupSize * sizeof(uint32_t));

    return clampedLdsPerThread;
}
#endif

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
                {
                    Pal::GpuCompatibilityInfo compatInfo       = {};
                    Pal::IDevice*             pLocalPalDevice  = VkPhysicalDevice(localDeviceIndex)->PalDevice();
                    Pal::IDevice*             pRemotePalDevice = VkPhysicalDevice(remoteDeviceIndex)->PalDevice();

                    Pal::Result result = pLocalPalDevice->GetMultiGpuCompatibility(*pRemotePalDevice, &compatInfo);

                    if ((result == Pal::Result::Success) && compatInfo.flags.peerTransferRead)
                    {
                        // Expose peer reads for XGMI
                        enabledFeatures |= VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT;
                        enabledFeatures |= VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT;

                        // Peer writes are always supported but not expected to be performant without XGMI
                        VK_ASSERT(compatInfo.flags.peerTransferWrite);
                        enabledFeatures |= VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT;
                    }
                }
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
    for (uint32 bindIdx = 0; bindIdx < bindInfoCount; bindIdx++)
    {
        uint32                        deviceIndexCount  = 0;
        const uint32*                 pDeviceIndices    = nullptr;
        const VkBindBufferMemoryInfo& info              = pBindInfos[bindIdx];

        VK_ASSERT(info.sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO);

        const void* pNext = info.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
            {
                const auto* pExtInfo = static_cast<const VkBindBufferMemoryDeviceGroupInfo*>(pNext);

                deviceIndexCount = pExtInfo->deviceIndexCount;
                pDeviceIndices   = pExtInfo->pDeviceIndices;
                break;
            }

            default:
                VK_NOT_IMPLEMENTED;
                break;
            }

            pNext = pHeader->pNext;
        }

        VK_ASSERT((deviceIndexCount == 0) || (deviceIndexCount == NumPalDevices()));

        Buffer::ObjectFromHandle(info.buffer)->BindMemory(this, info.memory, info.memoryOffset, pDeviceIndices);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Device::BindImageMemory(
    uint32_t                          bindInfoCount,
    const VkBindImageMemoryInfo*      pBindInfos)
{
    for (uint32 bindIdx = 0; bindIdx < bindInfoCount; bindIdx++)
    {
        uint32          deviceIndexCount    = 0;
        const uint32*   pDeviceIndices      = nullptr;

        uint32          SFRRectCount        = 0;
        const VkRect2D* pSFRRects           = nullptr;

        uint32          swapChainImageIndex = 0;
        SwapChain*      pSwapchain          = nullptr;

        const VkBindImageMemoryInfo& info = pBindInfos[bindIdx];

        VK_ASSERT(info.sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO);

        const void* pNext = info.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
            {
                const auto* pExtInfo = static_cast<const VkBindImageMemoryDeviceGroupInfo*>(pNext);

                deviceIndexCount = pExtInfo->deviceIndexCount;
                pDeviceIndices   = pExtInfo->pDeviceIndices;
                SFRRectCount     = pExtInfo->splitInstanceBindRegionCount;
                pSFRRects        = pExtInfo->pSplitInstanceBindRegions;
                break;
            }

            case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR:
            {
                const auto* pExtInfo = static_cast<const VkBindImageMemorySwapchainInfoKHR*>(pNext);

                pSwapchain = SwapChain::ObjectFromHandle(pExtInfo->swapchain);
                swapChainImageIndex = pExtInfo->imageIndex;
                break;
            }

            default:
                VK_NOT_IMPLEMENTED;
                break;
            }

            pNext = pHeader->pNext;
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
                result = VK_ERROR_UNKNOWN;
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
VkResult Device::CreateSamplerYcbcrConversion(
    const VkSamplerYcbcrConversionCreateInfo*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSamplerYcbcrConversion*                   pYcbcrConversion)
{
    return SamplerYcbcrConversion::Create(this, pCreateInfo, pAllocator, pYcbcrConversion);
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

// =====================================================================================================================
VkResult Device::GetSemaphoreCounterValue(
    VkSemaphore                                 semaphore,
    uint64_t*                                   pValue)
{
    Semaphore* pSemaphore = Semaphore::ObjectFromHandle(semaphore);

    return pSemaphore->GetSemaphoreCounterValue(this, pSemaphore, pValue);
}

// =====================================================================================================================
VkResult Device::WaitSemaphores(
    const VkSemaphoreWaitInfo*                  pWaitInfo,
    uint64_t                                    timeout)
{
    Pal::Result palResult = Pal::Result::Success;
    uint32_t flags = 0;

    Pal::IQueueSemaphore** ppPalSemaphores = static_cast<Pal::IQueueSemaphore**>(VK_ALLOC_A(
                sizeof(Pal::IQueueSemaphore*) * pWaitInfo->semaphoreCount));

    for (uint32_t i = 0; i < pWaitInfo->semaphoreCount; ++i)
    {
        Semaphore* currentSemaphore = Semaphore::ObjectFromHandle(pWaitInfo->pSemaphores[i]);
        ppPalSemaphores[i] = currentSemaphore->PalSemaphore(DefaultDeviceIndex);
        currentSemaphore->RestoreSemaphore();
    }

    if (pWaitInfo->flags == VK_SEMAPHORE_WAIT_ANY_BIT)
    {
        flags |= Pal::HostWaitFlags::HostWaitAny;
    }
    palResult = PalDevice(DefaultDeviceIndex)->WaitForSemaphores(pWaitInfo->semaphoreCount, ppPalSemaphores,
            pWaitInfo->pValues, flags, timeout);

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult Device::SignalSemaphore(
    VkSemaphore                                 semaphore,
    uint64_t                                    value)
{
    Semaphore* pSemaphore = Semaphore::ObjectFromHandle(semaphore);

    return pSemaphore->SignalSemaphoreValue(this, pSemaphore, value);
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

    if (GetVkTypeIndexBitsFromPalHeap(Pal::GpuHeapGartUswc, &gartIndexBits))
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

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        vkResult = m_perGpu[deviceIdx].pPhysicalDevice->TryIncreaseAllocatedMemorySize(allocationSize, heapIdx);

        if (vkResult != VK_SUCCESS)
        {
            break;
        }
    }
    while (deviceGroup.IterateNext());

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

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        m_perGpu[deviceIdx].pPhysicalDevice->IncreaseAllocatedMemorySize(allocationSize, heapIdx);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Decreases the allocated memory size for device local allocations made by the application (externally)
void Device::DecreaseAllocatedMemorySize(
    Pal::gpusize allocationSize,
    uint32_t     deviceMask,
    uint32_t     heapIdx)
{
    utils::IterateMask deviceGroup(deviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        m_perGpu[deviceIdx].pPhysicalDevice->DecreaseAllocatedMemorySize(allocationSize, heapIdx);
    }
    while (deviceGroup.IterateNext());
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

        Pal::CmdBufferCreateInfo cmdBufferCreateInfo = {};

        cmdBufferCreateInfo.pCmdAllocator = GetSharedCmdAllocator(deviceIdx);
        cmdBufferCreateInfo.engineType    = Pal::EngineType::EngineTypeDma;
        cmdBufferCreateInfo.queueType     = Pal::QueueType::QueueTypeDma;

        size_t cmdBufferSize = PalDevice(deviceIdx)->GetCmdBufferSize(cmdBufferCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        m_perGpu[deviceIdx].pSwCompositingMemory = VkInstance()->AllocMem(
            (palQueueSize + palSemaphoreSize + cmdBufferSize),
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

            if (palResult == Pal::Result::Success)
            {
                palResult = PalDevice(deviceIdx)->CreateCmdBuffer(
                    cmdBufferCreateInfo,
                    Util::VoidPtrInc(m_perGpu[deviceIdx].pSwCompositingMemory, (palSemaphoreSize + palQueueSize)),
                    &(m_perGpu[deviceIdx].pSwCompositingCmdBuffer));

                if (palResult == Pal::Result::Success)
                {
                    Pal::CmdBufferBuildInfo buildInfo = {};
                    palResult = m_perGpu[deviceIdx].pSwCompositingCmdBuffer->Begin(buildInfo);
                    if (palResult == Pal::Result::Success)
                    {
                        palResult = m_perGpu[deviceIdx].pSwCompositingCmdBuffer->End();
                    }
                }
            }

            // Clean up if any error is encountered
            if (palResult != Pal::Result::Success)
            {
                if (m_perGpu[deviceIdx].pSwCompositingSemaphore != nullptr)
                {
                    m_perGpu[deviceIdx].pSwCompositingSemaphore->Destroy();
                    m_perGpu[deviceIdx].pSwCompositingSemaphore = nullptr;
                }

                if (m_perGpu[deviceIdx].pSwCompositingQueue != nullptr)
                {
                    m_perGpu[deviceIdx].pSwCompositingQueue->Destroy();
                }

                if (m_perGpu[deviceIdx].pSwCompositingCmdBuffer != nullptr)
                {
                    m_perGpu[deviceIdx].pSwCompositingCmdBuffer->Destroy();
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
// Create a common CmdAllocator for internal use. For the driver setting, useSharedCmdAllocator, this CmdAllocator will
// be used by all command buffers created by this device. It must be thread safe because two threads could modify two
// command buffers at once which may cause those command buffers to access the allocator simultaneously.
VkResult Device::CreateSharedPalCmdAllocator(
    )
{
    VkResult result = VK_SUCCESS;

    Pal::CmdAllocatorCreateInfo createInfo = {};

    createInfo.flags.threadSafe               = 1;
    createInfo.flags.autoMemoryReuse          = 1;
    createInfo.flags.disableBusyChunkTracking = 1;

    // Initialize command data chunk allocation size
    createInfo.allocInfo[Pal::CommandDataAlloc].allocHeap    = m_settings.cmdAllocatorDataHeap;
    createInfo.allocInfo[Pal::CommandDataAlloc].allocSize    = m_settings.cmdAllocatorDataAllocSize;
    createInfo.allocInfo[Pal::CommandDataAlloc].suballocSize = m_settings.cmdAllocatorDataSubAllocSize;

    // Initialize embedded data chunk allocation size
    createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocHeap    = m_settings.cmdAllocatorEmbeddedHeap;
    createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocSize    = m_settings.cmdAllocatorEmbeddedAllocSize;
    createInfo.allocInfo[Pal::EmbeddedDataAlloc].suballocSize = m_settings.cmdAllocatorEmbeddedSubAllocSize;

    // Initialize large embedded data chunk allocation size
    createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocHeap    = m_settings.cmdAllocatorEmbeddedHeap;
    createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocSize    = m_settings.cmdAllocatorLargeEmbeddedAllocSize;
    createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].suballocSize = m_settings.cmdAllocatorLargeEmbeddedSubAllocSize;

    // Initialize GPU scratch memory chunk allocation size
    createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocHeap    = m_settings.cmdAllocatorScratchHeap;
    createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocSize    = m_settings.cmdAllocatorScratchAllocSize;
    createInfo.allocInfo[Pal::GpuScratchMemAlloc].suballocSize = m_settings.cmdAllocatorScratchSubAllocSize;

    Pal::Result  palResult;
    const size_t allocatorSize = PalDevice(DefaultDeviceIndex)->GetCmdAllocatorSize(createInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    void* pAllocatorMem = m_pInstance->AllocMem(
        allocatorSize * NumPalDevices(),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pAllocatorMem != NULL)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); ++deviceIdx)
        {
            VK_ASSERT(allocatorSize == PalDevice(deviceIdx)->GetCmdAllocatorSize(createInfo, &palResult));

            palResult = PalDevice(deviceIdx)->CreateCmdAllocator(
                createInfo,
                Util::VoidPtrInc(pAllocatorMem, allocatorSize * deviceIdx),
                &m_perGpu[deviceIdx].pSharedPalCmdAllocator);

            if (palResult != Pal::Result::Success)
            {
                if (m_perGpu[DefaultDeviceIndex].pSharedPalCmdAllocator != nullptr)
                {
                    DestroySharedPalCmdAllocator();
                }
                else
                {
                    m_pInstance->FreeMem(pAllocatorMem);
                }

                result = PalToVkError(palResult);
                break;
            }
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
void Device::DestroySharedPalCmdAllocator()
{
    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        if (m_perGpu[deviceIdx].pSharedPalCmdAllocator != nullptr)
        {
            m_perGpu[deviceIdx].pSharedPalCmdAllocator->Destroy();
        }
    }

    m_pInstance->FreeMem(m_perGpu[DefaultDeviceIndex].pSharedPalCmdAllocator);
}

// =====================================================================================================================
VkResult Device::AllocBorderColorPalette()
{
    VkResult result = VK_SUCCESS;

    Pal::BorderColorPaletteCreateInfo createInfo = { };
    createInfo.paletteSize = MaxBorderColorPaletteSize;

    Pal::Result palResult = Pal::Result::Success;
    const size_t palSize = PalDevice(DefaultDeviceIndex)->GetBorderColorPaletteSize(createInfo, &palResult);

    result = PalToVkResult(palResult);

    void* pSystemMem = nullptr;

    if (result == VK_SUCCESS)
    {
        const size_t memSize = (palSize * NumPalDevices()) +
                               (sizeof(bool)* MaxBorderColorPaletteSize);

        pSystemMem = VkInstance()->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            memset(pSystemMem, 0, memSize);
        }
    }

    for (uint32_t deviceIdx = 0; (deviceIdx < NumPalDevices()) && (result == VK_SUCCESS); ++deviceIdx)
    {
        result = PalToVkResult(PalDevice(deviceIdx)->CreateBorderColorPalette(
                                createInfo, pSystemMem, &m_perGpu[deviceIdx].pPalBorderColorPalette));

        pSystemMem = Util::VoidPtrInc(pSystemMem, palSize);
    }

    if (result == VK_SUCCESS)
    {
        Pal::IBorderColorPalette* pPalBorderColorPalette[MaxPalDevices] = {};

        for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); ++deviceIdx)
        {
            pPalBorderColorPalette[deviceIdx] = m_perGpu[deviceIdx].pPalBorderColorPalette;
        }

        result = MemMgr()->AllocAndBindGpuMem(
            NumPalDevices(),
            reinterpret_cast<Pal::IGpuMemoryBindable**>(pPalBorderColorPalette),
            false,
            &m_memoryPalBorderColorPalette,
            GetPalDeviceMask(),
            true,
            false,
            VK_OBJECT_TYPE_DEVICE,
            ApiDevice::IntValueFromHandle(ApiDevice::FromObject(this)));
    }

    if (result == VK_SUCCESS)
    {
        m_pBorderColorUsedIndexes = static_cast<bool*>(pSystemMem);
    }
    else if (m_perGpu[0].pPalBorderColorPalette != nullptr)
    {
        VkInstance()->FreeMem(m_perGpu[0].pPalBorderColorPalette);
        m_perGpu[0].pPalBorderColorPalette = nullptr;
    }

    return result;
}

// =====================================================================================================================
void Device::DestroyBorderColorPalette()
{
    if (m_perGpu[0].pPalBorderColorPalette != nullptr)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); ++deviceIdx)
        {
            m_perGpu[deviceIdx].pPalBorderColorPalette->Destroy();
        }

        VkInstance()->FreeMem(m_perGpu[0].pPalBorderColorPalette);
        m_perGpu[0].pPalBorderColorPalette = nullptr;
        MemMgr()->FreeGpuMem(&m_memoryPalBorderColorPalette);
    }
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

            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount  = 1;
            perSubQueueInfo.ppCmdBuffers    = &pCommandBuffer;
            perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

            Pal::SubmitInfo submitInfo = {};
            submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
            submitInfo.perSubQueueInfoCount = 1;

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
// Issue the flip metadata on the software compositing internal present queue.
VkResult Device::SwCompositingNotifyFlipMetadata(
    Pal::IQueue*            pPresentQueue,
    const Pal::CmdBufInfo&  cmdBufInfo)
{
    Pal::Result palResult = Pal::Result::ErrorUnknown;

    for (uint32_t presentationDeviceIdx = 0; presentationDeviceIdx < NumPalDevices(); presentationDeviceIdx++)
    {
        if (pPresentQueue == m_perGpu[presentationDeviceIdx].pSwCompositingQueue)
        {
            // Found the master device index for the correct dummy command buffer to use.
            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount  = 1;
            perSubQueueInfo.ppCmdBuffers    = &m_perGpu[presentationDeviceIdx].pSwCompositingCmdBuffer;
            perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

            Pal::SubmitInfo submitInfo = {};
            submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
            submitInfo.perSubQueueInfoCount = 1;

            palResult = pPresentQueue->Submit(submitInfo);
            break;
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Return true if Big Software Release 6.0 is supported.
bool Device::BigSW60Supported() const
{
    return utils::BigSW60Supported(VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().bigSoftwareReleaseInfo);
}

// =====================================================================================================================
// Update driver feature settings for this device based on an app profile and global settings.
void Device::UpdateFeatureSettings()
{
    ProfileSettings profileSettings = {};

    ReloadAppProfileSettings(VkPhysicalDevice(DefaultDeviceIndex), m_pInstance, &profileSettings);

#if VKI_RAY_TRACING
#endif
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Creates deferred operation
VkResult Device::CreateDeferredOperation(
    const VkAllocationCallbacks* pAllocator,
    VkDeferredOperationKHR*      pDeferredOperation)
{
    return DeferredHostOperation::Create(this, pAllocator, pDeferredOperation);
}

// =====================================================================================================================
// Creates ray tracing pipeline objects
VkResult Device::CreateRayTracingPipelines(
    VkDeferredOperationKHR                      deferredOperation,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    VK_ASSERT(pCreateInfos != nullptr);
    VK_ASSERT(pPipelines != nullptr);

    DeferredHostOperation* pDeferredOperation = DeferredHostOperation::ObjectFromHandle(deferredOperation);
    PipelineCache* pPipelineCache = PipelineCache::ObjectFromHandle(pipelineCache);

    for (uint32_t i = 0; i < count; ++i)
    {
        // Initialize output array to VK_NULL_HANDLE
        pPipelines[i] = VK_NULL_HANDLE;
    }

    return RayTracingPipeline::Create(
        this,
        pDeferredOperation,
        pPipelineCache,
        count,
        pCreateInfos,
        pAllocator,
        pPipelines);
}

// =====================================================================================================================
VkResult Device::CopyAccelerationStructure(
    VkDeferredOperationKHR                    deferredOperation,
    const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
VkResult Device::CopyAccelerationStructureToMemory(
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
VkResult Device::CopyMemoryToAccelerationStructure(
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
void Device::GetDeviceAccelerationStructureCompatibility(
    const uint8_t*                              pData,
    VkAccelerationStructureCompatibilityKHR*    pCompatibility)
{
    if (m_settings.disableASCompatibilityCheck)
    {
        *pCompatibility = VkAccelerationStructureCompatibilityKHR::VK_ACCELERATION_STRUCTURE_COMPATIBILITY_COMPATIBLE_KHR;
    }
    else
    {
        uint64_t asVersion;
        RayTrace()->GpuRt(DefaultDeviceIndex)->GetSerializedAccelStructVersion(static_cast<const void*>(pData), &asVersion);

        const uint64_t currVersion = RayTrace()->DeviceSettings().accelerationStructureUUID;

        *pCompatibility = (asVersion == currVersion) ?
            VkAccelerationStructureCompatibilityKHR::VK_ACCELERATION_STRUCTURE_COMPATIBILITY_COMPATIBLE_KHR :
            VkAccelerationStructureCompatibilityKHR::VK_ACCELERATION_STRUCTURE_COMPATIBILITY_INCOMPATIBLE_KHR;
    }
}

// =====================================================================================================================
VkResult Device::WriteAccelerationStructuresProperties(
    uint32_t                                    accelerationStructureCount,
    const VkAccelerationStructureKHR*           pAccelerationStructures,
    VkQueryType                                 queryType,
    size_t                                      dataSize,
    void*                                       pData,
    size_t                                      stride)
{
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
VkResult Device::CreateAccelerationStructureKHR(
    const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkAccelerationStructureKHR*                 pAccelerationStructure)
{
    return AccelerationStructure::CreateKHR(this, pCreateInfo, pAllocator, pAccelerationStructure);
}

// =====================================================================================================================
VkResult Device::BuildAccelerationStructure(
    VkDeferredOperationKHR                                   deferredOperation,
    uint32_t                                                 infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*       pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*   ppBuildRangeInfos)
{
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

// =====================================================================================================================
void Device::GetAccelerationStructureBuildSizesKHR(
    VkAccelerationStructureBuildTypeKHR                  buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR*   pBuildInfo,
    const uint32_t*                                      pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR*            pSizeInfo)
{
    GeometryConvertHelper         helper = {};
    GpuRt::AccelStructBuildInputs inputs = {};

    VkResult result = AccelerationStructure::ConvertBuildSizeInputs(
        DefaultDeviceIndex,
        *pBuildInfo,
        pMaxPrimitiveCounts,
        &helper,
        &inputs);

    GpuRt::AccelStructPrebuildInfo prebuild = {};

    RayTrace()->GpuRt(DefaultDeviceIndex)->GetAccelStructPrebuildInfo(inputs, &prebuild);

    pSizeInfo->accelerationStructureSize = prebuild.resultDataMaxSizeInBytes;

    switch (buildType)
    {
    case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR:
        pSizeInfo->buildScratchSize  = 0;
        pSizeInfo->updateScratchSize = 0;
        VK_NEVER_CALLED();
        break;
    case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR:
    case VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_OR_DEVICE_KHR:
        pSizeInfo->buildScratchSize  = prebuild.scratchDataSizeInBytes;
        pSizeInfo->updateScratchSize = prebuild.updateScratchDataSizeInBytes;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }
}
#endif

// =====================================================================================================================
// Handles logging of debug names from the DebugUtils extension
VkResult Device::SetDebugUtilsObjectName(
    const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
    // Log the object name if it is an object type that RMV cares about.
    if ((pNameInfo->objectType == VkObjectType::VK_OBJECT_TYPE_BUFFER) ||
        (pNameInfo->objectType == VkObjectType::VK_OBJECT_TYPE_IMAGE) ||
        (pNameInfo->objectType == VkObjectType::VK_OBJECT_TYPE_PIPELINE))
    {
        // For buffers, the RMV resource handle is the same as the handle passed in so we can log that directly,
        // but for image and pipeline the RMV resource handle comes from the Pal object.
        Pal::DebugNameEventData nameData = {};
        if (pNameInfo->objectType == VkObjectType::VK_OBJECT_TYPE_BUFFER)
        {
            nameData.pObj = reinterpret_cast<const void*>(pNameInfo->objectHandle);
        }
        else if (pNameInfo->objectType == VkObjectType::VK_OBJECT_TYPE_IMAGE)
        {
            Image* pImage = reinterpret_cast<Image*>(pNameInfo->objectHandle);
            nameData.pObj = pImage->PalImage(DefaultDeviceIndex);
        }
        else // Pipeline
        {
            Pipeline* pPipeline = reinterpret_cast<Pipeline*>(pNameInfo->objectHandle);
            nameData.pObj = pPipeline->PalPipeline(DefaultDeviceIndex);
        }
        nameData.pDebugName = pNameInfo->pObjectName;
        VkInstance()->PalPlatform()->LogEvent(Pal::PalEvent::DebugName, &nameData, sizeof(nameData));
    }

    return VkResult::VK_SUCCESS;
}

// =====================================================================================================================
uint32_t Device::GetBorderColorIndex(
        const float*                 pBorderColor)
{
    uint32_t borderColorIndex = MaxBorderColorPaletteSize;
    bool     indexWasFound    = false;

    MutexAuto lock(&m_borderColorMutex);

    for (uint32_t index = 0; (index < MaxBorderColorPaletteSize) && (indexWasFound == false); ++index)
    {
        if (m_pBorderColorUsedIndexes[index] == false)
        {
            borderColorIndex = index;
            m_pBorderColorUsedIndexes[index] = true;

            for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
            {
                // Update border color entry
                m_perGpu[deviceIdx].pPalBorderColorPalette->Update(borderColorIndex, 1, pBorderColor);
            }

            indexWasFound = true;
        }
    }

    VK_ASSERT(indexWasFound);
    return borderColorIndex;
}

// =====================================================================================================================
void Device::ReleaseBorderColorIndex(
        uint32_t                     borderColorIndex)
{
    MutexAuto lock(&m_borderColorMutex);
    m_pBorderColorUsedIndexes[borderColorIndex] = false;
}

// =====================================================================================================================
void Device::ReserveBorderColorIndex(
    uint32                   borderColorIndex,
    const float*             pBorderColor)
{
    VK_ASSERT(m_pBorderColorUsedIndexes[borderColorIndex] == false);

    for (uint32_t deviceIdx = 0; deviceIdx < NumPalDevices(); deviceIdx++)
    {
        m_perGpu[deviceIdx].pPalBorderColorPalette->Update(borderColorIndex, 1, pBorderColor);
    }

    m_pBorderColorUsedIndexes[borderColorIndex] = true;
}

// =====================================================================================================================
bool Device::ReserveFastPrivateDataSlot(
        uint64*                         pIndex)
{
    Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> lock(&m_privateDataRWLock);
    *pIndex = ++m_nextPrivateDataSlot - 1;

    return ((*pIndex) < m_privateDataSlotRequestCount) ? true : false;
}

// =====================================================================================================================
// for extension private_data
void* Device::AllocApiObject(
        const VkAllocationCallbacks*    pAllocator,
        const size_t                    totalObjectSize) const
{
    VK_ASSERT(pAllocator != nullptr);

    size_t actualObjectSize = totalObjectSize + m_privateDataSize;

    void* pMemory = pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    actualObjectSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if ((m_privateDataSize > 0) && (pMemory != nullptr))
    {
        memset(pMemory, 0, m_privateDataSize);
        pMemory = Util::VoidPtrInc(pMemory, m_privateDataSize);
    }

    return pMemory;
}

// =====================================================================================================================
// for extension private_data
void Device::FreeApiObject(
        const VkAllocationCallbacks*    pAllocator,
        void*                           pMemory) const
{
    VK_ASSERT(pAllocator != nullptr);

    void* pActualMemory = pMemory;

    if ((m_privateDataSize > 0) && (pMemory != nullptr))
    {
        pActualMemory = Util::VoidPtrDec(pActualMemory, m_privateDataSize);
        FreeUnreservedPrivateData(pActualMemory);
    }

    pAllocator->pfnFree(pAllocator->pUserData, pActualMemory);
}

// =====================================================================================================================
void Device::FreeUnreservedPrivateData(
        void*                           pMemory) const
{
    PrivateDataStorage* pPrivateDataStorage = static_cast<PrivateDataStorage*>(pMemory);

    if (pPrivateDataStorage->pUnreserved != nullptr)
    {
        Util::Destructor(pPrivateDataStorage->pUnreserved);
        VkInstance()->FreeMem(pPrivateDataStorage->pUnreserved);

        pPrivateDataStorage->pUnreserved = nullptr;
    }
}

// =====================================================================================================================
Pal::TilingOptMode Device::GetTilingOptMode() const
{
    return m_enabledFeatures.strictImageSizeRequirements ?
        Pal::TilingOptMode::OptForSpace :
        m_settings.imageTilingOptMode;
}

// =====================================================================================================================
VkResult Device::GetDeviceFaultInfoEXT(
    VkDeviceFaultCountsEXT* pFaultCounts,
    VkDeviceFaultInfoEXT*   pFaultInfo)
{
    VkResult result = VK_SUCCESS;

    // subsequent calls need to return the same data
    if (m_retrievedFaultData == false)
    {
        m_pageFaultStatus.flags.pageFault = false;
        Pal::Result stateResult = PalDevice(DefaultDeviceIndex)->CheckExecutionState(&m_pageFaultStatus);

        if ((stateResult == Pal::Result::ErrorGpuPageFaultDetected) && m_pageFaultStatus.flags.pageFault == true)
        {
            m_retrievedFaultData = true;
        }
    }

    if (m_pageFaultStatus.flags.pageFault == false)
    {
        pFaultCounts->addressInfoCount = 0;
        pFaultCounts->vendorInfoCount  = 0;
        pFaultCounts->vendorBinarySize = 0;

        if (pFaultInfo != nullptr)
        {
            strcpy(pFaultInfo->description, "No fault detected");
        }
    }
    else if (pFaultInfo == nullptr)
    {
        pFaultCounts->addressInfoCount  = 1;
        pFaultCounts->vendorInfoCount   = 0;
        pFaultCounts->vendorBinarySize  = 0;
    }
    else if (pFaultCounts->addressInfoCount < 1)
    {
        pFaultCounts->addressInfoCount  = 0;
        pFaultCounts->vendorInfoCount   = 0;
        pFaultCounts->vendorBinarySize  = 0;

        result = VK_INCOMPLETE;
    }
    else
    {
        pFaultCounts->addressInfoCount  = 1;
        pFaultCounts->vendorInfoCount   = 0;
        pFaultCounts->vendorBinarySize  = 0;
        {
            strcpy(pFaultInfo->description, "GPU fault");
        }

        VkDeviceFaultAddressInfoEXT* pAddressInfo = pFaultInfo->pAddressInfos;

        pAddressInfo->addressType = m_pageFaultStatus.flags.readFault ? VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT :
                                                                        VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT;
        pAddressInfo->reportedAddress   = static_cast<VkDeviceAddress>(m_pageFaultStatus.faultAddress);
        pAddressInfo->addressPrecision  = 4096;
    }

    return result;
}

// =================================================================================================================
template<typename CreateInfo>
PipelineCreateFlags Device::GetPipelineCreateFlags(
    const CreateInfo* pCreateInfo)
{
    PipelineCreateFlags flags = pCreateInfo->flags;

    return flags;
}

// =================================================================================================================
BufferUsageFlagBits Device::GetBufferUsageFlagBits(
    const VkBufferCreateInfo* pCreateInfo)
{
    BufferUsageFlagBits usage = static_cast<BufferUsageFlagBits>(pCreateInfo->usage);

    return usage;
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
    ApiDevice::ObjectFromHandle(device)->GetQueue2(pQueueInfo, pQueue);
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
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2(
    VkDevice                                    device,
    const VkRenderPassCreateInfo2*              pCreateInfo,
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
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateSamplerYcbcrConversion(pCreateInfo, pAllocCB, pYcbcrConversion);
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
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    const VkAllocationCallbacks* pAllocCB = (pAllocator != nullptr)
                                            ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->AllocMemory(pAllocateInfo, pAllocCB, pMemory);
}

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFdKHR(
    VkDevice device,
    const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo)
{
    ImportSemaphoreInfo importInfo = {};
    importInfo.handleType       = pImportSemaphoreFdInfo->handleType;
    importInfo.handle           = pImportSemaphoreFdInfo->fd;
    importInfo.importFlags      = pImportSemaphoreFdInfo->flags;
    importInfo.crossProcess     = true;

    return ApiDevice::ObjectFromHandle(device)->ImportSemaphore(pImportSemaphoreFdInfo->semaphore, importInfo);
}
#endif

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
    return ApiDevice::ObjectFromHandle(device)->SetDebugUtilsObjectName(pNameInfo);
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
            const Pal::DeviceProperties& properties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

            pInfo->engineClockRatioToPeak = static_cast<float>(output.engineClockFrequency) /
                                            properties.gfxipProperties.performance.maxGpuClock;
            pInfo->memoryClockRatioToPeak = static_cast<float>(output.memoryClockFrequency) /
                                            properties.gpuMemoryProperties.performance.maxMemClock;
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
    // Validate mutable descriptor support and variable descriptor count limits
    const VkMutableDescriptorTypeCreateInfoEXT* pMutableDescriptorTypeCreateInfoEXT = nullptr;
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pCreateInfo->pNext);

        while (pHeader != nullptr)
        {
            switch (static_cast<uint32_t>(pHeader->sType))
            {
                case VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT:
                {
                    pMutableDescriptorTypeCreateInfoEXT =
                        reinterpret_cast<const VkMutableDescriptorTypeCreateInfoEXT*>(pHeader);

                    break;
                }

                default:
                    break;
            }

            pHeader = reinterpret_cast<const VkStructHeader*>(pHeader->pNext);
        }
    }

    {
        VkStructHeaderNonConst* pHeader = static_cast<VkStructHeaderNonConst*>(pSupport->pNext);

        while (pHeader != nullptr)
        {
            switch (static_cast<uint32_t>(pHeader->sType))
            {
                case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT:
                {
                    VkDescriptorSetVariableDescriptorCountLayoutSupportEXT * pDescCountLayoutSupport =
                        reinterpret_cast<VkDescriptorSetVariableDescriptorCountLayoutSupportEXT*>(pHeader);

                    pDescCountLayoutSupport->maxVariableDescriptorCount = UINT_MAX;

                    break;
                }

                default:
                    break;
            }

            pHeader = reinterpret_cast<VkStructHeaderNonConst*>(pHeader->pNext);
        }
    }

    pSupport->supported = VK_TRUE;

    for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
    {
        if (pCreateInfo->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
            if (pMutableDescriptorTypeCreateInfoEXT != nullptr)
            {
                const VkMutableDescriptorTypeListEXT& list =
                    pMutableDescriptorTypeCreateInfoEXT->pMutableDescriptorTypeLists[i];

                for (uint32_t j = 0; j < list.descriptorTypeCount; ++j)
                {
                    switch(list.pDescriptorTypes[j])
                    {
                        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                        case VK_DESCRIPTOR_TYPE_SAMPLER:
                        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                            break;
                        default:
                            pSupport->supported = VK_FALSE;
                            break;
                    }
                }
            }

            if (pSupport->supported == VK_FALSE)
            {
                break;
            }
        }
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

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreCounterValue(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    uint64_t*                                   pValue)
{
    return ApiDevice::ObjectFromHandle(device)->GetSemaphoreCounterValue(
        semaphore,
        pValue);
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(
    VkDevice                                    device,
    const VkSemaphoreWaitInfo*                  pWaitInfo,
    uint64_t                                    timeout)
{
    return ApiDevice::ObjectFromHandle(device)->WaitSemaphores(
        pWaitInfo,
        timeout);
}

VKAPI_ATTR VkResult VKAPI_CALL vkSignalSemaphore(
    VkDevice                                    device,
    const VkSemaphoreSignalInfo*                pSignalInfo)
{
    return ApiDevice::ObjectFromHandle(device)->SignalSemaphore(
        pSignalInfo->semaphore,
        pSignalInfo->value);
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

#if VKI_RAY_TRACING
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateAccelerationStructureKHR(
    VkDevice                                    device,
    const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkAccelerationStructureKHR*                 pAccelerationStructure)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateAccelerationStructureKHR(pCreateInfo, pAllocCB, pAccelerationStructure);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      deferredOperation,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateRayTracingPipelines(
        deferredOperation,
        pipelineCache,
        createInfoCount,
        pCreateInfos,
        pAllocCB,
        pPipelines);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBuildAccelerationStructuresKHR(
    VkDevice                                                device,
    VkDeferredOperationKHR                                  deferredOperation,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->BuildAccelerationStructure(
        deferredOperation,
        infoCount,
        pInfos,
        ppBuildRangeInfos);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      deferredOperation,
    const VkCopyAccelerationStructureInfoKHR*   pInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->CopyAccelerationStructure(deferredOperation, pInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureToMemoryKHR(
    VkDevice                                          device,
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->CopyAccelerationStructureToMemory(deferredOperation, pInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCopyMemoryToAccelerationStructureKHR(
    VkDevice                                          device,
    VkDeferredOperationKHR                            deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->CopyMemoryToAccelerationStructure(deferredOperation, pInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkWriteAccelerationStructuresPropertiesKHR(
    VkDevice                                    device,
    uint32_t                                    accelerationStructureCount,
    const VkAccelerationStructureKHR*           pAccelerationStructures,
    VkQueryType                                 queryType,
    size_t                                      dataSize,
    void*                                       pData,
    size_t                                      stride)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->WriteAccelerationStructuresProperties(
        accelerationStructureCount,
        pAccelerationStructures,
        queryType,
        dataSize,
        pData,
        stride);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    uint32_t                                    firstGroup,
    uint32_t                                    groupCount,
    size_t                                      dataSize,
    void*                                       pData)
{
    // Do exactly the same as vkGetRayTracingShaderGroupHandlesKHR. It is expected that these handles show up in
    // VkRayTracingPipelineCreateInfoKHR::VkRayTracingShaderGroupCreateInfoKHR::pShaderGroupCaptureReplayHandle when
    // replaying and we will make use of them in vkCreateRayTracingPipelinesKHR.
    RayTracingPipeline* pPipeline = RayTracingPipeline::ObjectFromHandle(pipeline);

    pPipeline->GetRayTracingShaderGroupHandles(DefaultDeviceIndex, firstGroup, groupCount, dataSize, pData);

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceAccelerationStructureCompatibilityKHR(
    VkDevice                                     device,
    const VkAccelerationStructureVersionInfoKHR* pVersionInfo,
    VkAccelerationStructureCompatibilityKHR*     pCompatibility)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    pDevice->GetDeviceAccelerationStructureCompatibility(
        pVersionInfo->pVersionData,
        pCompatibility);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetAccelerationStructureBuildSizesKHR(
    VkDevice                                             device,
    VkAccelerationStructureBuildTypeKHR                  buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR*   pBuildInfo,
    const uint32_t*                                      pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR*            pSizeInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);

    pDevice->GetAccelerationStructureBuildSizesKHR(
        buildType,
        pBuildInfo,
        pMaxPrimitiveCounts,
        pSizeInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDeferredOperationKHR(
    VkDevice                                    device,
    const VkAllocationCallbacks*                pAllocator,
    VkDeferredOperationKHR*                     pDeferredOperation)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return pDevice->CreateDeferredOperation(pAllocCB, pDeferredOperation);
}
#endif

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceBufferMemoryRequirements(
    VkDevice                                   device,
    const VkDeviceBufferMemoryRequirementsKHR* pInfo,
    VkMemoryRequirements2*                     pMemoryRequirements)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);
    Buffer::CalculateMemoryRequirements(pDevice,
                                        pInfo,
                                        pMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirements(
    VkDevice                                  device,
    const VkDeviceImageMemoryRequirementsKHR* pInfo,
    VkMemoryRequirements2*                    pMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    Image::CalculateMemoryRequirements(pDevice,
                                       pInfo,
                                       pMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageSparseMemoryRequirements(
    VkDevice                                  device,
    const VkDeviceImageMemoryRequirementsKHR* pInfo,
    uint32_t*                                 pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2*         pSparseMemoryRequirements)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    Image::CalculateSparseMemoryRequirements(pDevice,
                                             pInfo,
                                             pSparseMemoryRequirementCount,
                                             pSparseMemoryRequirements);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkSetDeviceMemoryPriorityEXT(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    float                                       priority)
{
    Memory* pMemory = Memory::ObjectFromHandle(memory);
    pMemory->SetPriority(MemoryPriority::FromVkMemoryPriority(priority), false);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceFaultInfoEXT(
    VkDevice                                    device,
    VkDeviceFaultCountsEXT*                     pFaultCounts,
    VkDeviceFaultInfoEXT*                       pFaultInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    return pDevice->GetDeviceFaultInfoEXT(pFaultCounts, pFaultInfo);
}

} // entry

} // vk

#if VKI_RAY_TRACING
template
vk::PipelineCreateFlags vk::Device::GetPipelineCreateFlags<VkRayTracingPipelineCreateInfoKHR>(
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfo);
#endif
