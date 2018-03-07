/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 **********************************************************************************************************************
 * @file  vk_khx_device_group.h
 * @brief
 **********************************************************************************************************************
 */
#ifndef VK_KHX_DEVICE_GROUP_H_
#define VK_KHX_DEVICE_GROUP_H_

#define VK_KHX_DEVICE_GROUP_SPEC_VERSION        2
#define VK_KHX_DEVICE_GROUP_EXTENSION_NAME      "VK_KHX_device_group"
#define VK_KHX_DEVICE_GROUP_EXTENSION_NUMBER    61
#define VK_MAX_DEVICE_GROUP_SIZE_KHX            32

#define VK_KHX_DEVICE_GROUP_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_KHX_DEVICE_GROUP_EXTENSION_NUMBER, type, offset)

#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHX                ((VkStructureType)1000060000)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO_KHX       ((VkStructureType)1000060003)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHX    ((VkStructureType)1000060004)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHX                  ((VkStructureType)1000060005)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO_KHX             ((VkStructureType)1000060006)
#define VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHX                   ((VkStructureType)1000060010)
#define VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHX      ((VkStructureType)1000060013)
#define VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHX       ((VkStructureType)1000060014)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHX         ((VkStructureType)1000060007)
#define VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHX               ((VkStructureType)1000060008)
#define VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHX          ((VkStructureType)1000060009)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHX                 ((VkStructureType)1000060011)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHX        ((VkStructureType)1000060012)

#define VK_IMAGE_CREATE_BIND_SFR_BIT_KHX                                ((VkImageCreateFlagBits)0x00000040)

#define VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT_KHX         ((VkPipelineCreateFlagBits)0x00000008)
#define VK_PIPELINE_CREATE_DISPATCH_BASE_KHX                            ((VkPipelineCreateFlagBits)0x00000010)

#define VK_DEPENDENCY_DEVICE_GROUP_BIT_KHX                              ((VkDependencyFlagBits)0x00000004)

#define VK_SWAPCHAIN_CREATE_BIND_SFR_BIT_KHX                            ((VkSwapchainCreateFlagBitsKHR)0x00000001)

typedef enum VkPeerMemoryFeatureFlagBitsKHX {
    VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT_KHX = 0x00000001,
    VK_PEER_MEMORY_FEATURE_COPY_DST_BIT_KHX = 0x00000002,
    VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT_KHX = 0x00000004,
    VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT_KHX = 0x00000008,
    VK_PEER_MEMORY_FEATURE_FLAG_BITS_MAX_ENUM_KHX = 0x7FFFFFFF
} VkPeerMemoryFeatureFlagBitsKHX;
typedef VkFlags VkPeerMemoryFeatureFlagsKHX;

typedef enum VkMemoryAllocateFlagBitsKHX {
    VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHX = 0x00000001,
    VK_MEMORY_ALLOCATE_FLAG_BITS_MAX_ENUM_KHX = 0x7FFFFFFF
} VkMemoryAllocateFlagBitsKHX;
typedef VkFlags VkMemoryAllocateFlagsKHX;

typedef enum VkDeviceGroupPresentModeFlagBitsKHX {
    VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHX = 0x00000001,
    VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHX = 0x00000002,
    VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHX = 0x00000004,
    VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHX = 0x00000008,
    VK_DEVICE_GROUP_PRESENT_MODE_FLAG_BITS_MAX_ENUM_KHX = 0x7FFFFFFF
} VkDeviceGroupPresentModeFlagBitsKHX;
typedef VkFlags VkDeviceGroupPresentModeFlagsKHX;

typedef struct VkMemoryAllocateFlagsInfoKHX {
    VkStructureType             sType;
    const void*                 pNext;
    VkMemoryAllocateFlagsKHX    flags;
    uint32_t                    deviceMask;
} VkMemoryAllocateFlagsInfoKHX;

typedef struct VkDeviceGroupRenderPassBeginInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           deviceMask;
    uint32_t           deviceRenderAreaCount;
    const VkRect2D*    pDeviceRenderAreas;
} VkDeviceGroupRenderPassBeginInfoKHX;

typedef struct VkDeviceGroupCommandBufferBeginInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           deviceMask;
} VkDeviceGroupCommandBufferBeginInfoKHX;

typedef struct VkDeviceGroupSubmitInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           waitSemaphoreCount;
    const uint32_t*    pWaitSemaphoreDeviceIndices;
    uint32_t           commandBufferCount;
    const uint32_t*    pCommandBufferDeviceMasks;
    uint32_t           signalSemaphoreCount;
    const uint32_t*    pSignalSemaphoreDeviceIndices;
} VkDeviceGroupSubmitInfoKHX;

typedef struct VkDeviceGroupBindSparseInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           resourceDeviceIndex;
    uint32_t           memoryDeviceIndex;
} VkDeviceGroupBindSparseInfoKHX;

typedef struct VkBindBufferMemoryDeviceGroupInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           deviceIndexCount;
    const uint32_t*    pDeviceIndices;
} VkBindBufferMemoryDeviceGroupInfoKHX;

typedef struct VkBindImageMemoryDeviceGroupInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    uint32_t           deviceIndexCount;
    const uint32_t*    pDeviceIndices;
    uint32_t           SFRRectCount;
    const VkRect2D*    pSFRRects;
} VkBindImageMemoryDeviceGroupInfoKHX;

typedef struct VkDeviceGroupPresentCapabilitiesKHX {
    VkStructureType                     sType;
    const void*                         pNext;
    uint32_t                            presentMask[VK_MAX_DEVICE_GROUP_SIZE_KHX];
    VkDeviceGroupPresentModeFlagsKHX    modes;
} VkDeviceGroupPresentCapabilitiesKHX;

typedef struct VkImageSwapchainCreateInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    VkSwapchainKHR     swapchain;
} VkImageSwapchainCreateInfoKHX;

typedef struct VkBindImageMemorySwapchainInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    VkSwapchainKHR     swapchain;
    uint32_t           imageIndex;
} VkBindImageMemorySwapchainInfoKHX;

typedef struct VkAcquireNextImageInfoKHX {
    VkStructureType    sType;
    const void*        pNext;
    VkSwapchainKHR     swapchain;
    uint64_t           timeout;
    VkSemaphore        semaphore;
    VkFence            fence;
    uint32_t           deviceMask;
} VkAcquireNextImageInfoKHX;

typedef struct VkDeviceGroupPresentInfoKHX {
    VkStructureType                        sType;
    const void*                            pNext;
    uint32_t                               swapchainCount;
    const uint32_t*                        pDeviceMasks;
    VkDeviceGroupPresentModeFlagBitsKHX    mode;
} VkDeviceGroupPresentInfoKHX;

typedef struct VkDeviceGroupSwapchainCreateInfoKHX {
    VkStructureType                     sType;
    const void*                         pNext;
    VkDeviceGroupPresentModeFlagsKHX    modes;
} VkDeviceGroupSwapchainCreateInfoKHX;

typedef void (VKAPI_PTR *PFN_vkGetDeviceGroupPeerMemoryFeaturesKHX)(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlagsKHX* pPeerMemoryFeatures);
typedef void (VKAPI_PTR *PFN_vkCmdSetDeviceMaskKHX)(VkCommandBuffer commandBuffer, uint32_t deviceMask);
typedef void (VKAPI_PTR *PFN_vkCmdDispatchBaseKHX)(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
typedef VkResult (VKAPI_PTR *PFN_vkGetDeviceGroupPresentCapabilitiesKHX)(VkDevice device, VkDeviceGroupPresentCapabilitiesKHX* pDeviceGroupPresentCapabilities);
typedef VkResult (VKAPI_PTR *PFN_vkGetDeviceGroupSurfacePresentModesKHX)(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHX* pModes);
typedef VkResult (VKAPI_PTR *PFN_vkGetPhysicalDevicePresentRectanglesKHX)(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects);
typedef VkResult (VKAPI_PTR *PFN_vkAcquireNextImage2KHX)(VkDevice device, const VkAcquireNextImageInfoKHX* pAcquireInfo, uint32_t* pImageIndex);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR void VKAPI_CALL vkGetDeviceGroupPeerMemoryFeaturesKHX(
    VkDevice                                    device,
    uint32_t                                    heapIndex,
    uint32_t                                    localDeviceIndex,
    uint32_t                                    remoteDeviceIndex,
    VkPeerMemoryFeatureFlagsKHX*                pPeerMemoryFeatures);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMaskKHX(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask);

VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBaseKHX(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    baseGroupX,
    uint32_t                                    baseGroupY,
    uint32_t                                    baseGroupZ,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupPresentCapabilitiesKHX(
    VkDevice                                    device,
    VkDeviceGroupPresentCapabilitiesKHX*        pDeviceGroupPresentCapabilities);

VKAPI_ATTR VkResult VKAPI_CALL vkGetDeviceGroupSurfacePresentModesKHX(
    VkDevice                                    device,
    VkSurfaceKHR                                surface,
    VkDeviceGroupPresentModeFlagsKHX*           pModes);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDevicePresentRectanglesKHX(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pRectCount,
    VkRect2D*                                   pRects);

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImage2KHX(
    VkDevice                                    device,
    const VkAcquireNextImageInfoKHX*            pAcquireInfo,
    uint32_t*                                   pImageIndex);
#endif

#endif /* VK_KHX_DEVICE_GROUP_H_ */
