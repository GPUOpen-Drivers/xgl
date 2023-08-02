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
 * @brief Contains dispatch table management for Vulkan, including interface to ICD loader.
 ***********************************************************************************************************************
 */

#include "include/khronos/vulkan.h"
#include "include/khronos/vk_icd.h"

#include "include/vk_buffer.h"
#include "include/vk_buffer_view.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_descriptor_buffer.h"
#include "include/vk_descriptor_pool.h"
#include "include/vk_descriptor_set.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_descriptor_update_template.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"
#include "include/vk_event.h"
#include "include/vk_extensions.h"
#include "include/vk_fence.h"
#include "include/vk_framebuffer.h"
#include "include/vk_gpa_session.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_query.h"
#include "include/vk_queue.h"
#include "include/vk_render_pass.h"
#include "include/vk_sampler.h"
#include "include/vk_sampler_ycbcr_conversion.h"
#include "include/vk_semaphore.h"
#include "include/vk_shader.h"
#include "include/vk_surface.h"
#include "include/vk_swapchain.h"
#include "include/vk_debug_report.h"

#include <cstring>

namespace vk
{

static EntryPoint::Metadata     g_EntryPointMetadataTable[VKI_ENTRY_POINT_COUNT];

const DispatchTable             g_GlobalDispatchTable(DispatchTable::Type::GLOBAL);

// Helper macro used to index the entry point metadata table
#define METADATA_TABLE(entry_name) g_EntryPointMetadataTable[entry_name##_index]

// Helper macro used to initialize the global dispatch table entries
#define INIT_DISPATCH_ALIAS(entry_name, func_name) \
    if (GetType() == Type::GLOBAL) \
    { \
        METADATA_TABLE(entry_name).pName            = vk::strings::entry::entry_name##_name; \
        METADATA_TABLE(entry_name).type             = entry_name##_type; \
    } \
    if (entry_name##_condition) \
    { \
        m_func.entry_name = vk::entry::func_name; \
    }

// Helper macro used to initialize non-aliased global dispatch table entries
#define INIT_DISPATCH_ENTRY(entry_name) INIT_DISPATCH_ALIAS(entry_name, entry_name)

// =====================================================================================================================
// Checks whether API version requirement is fulfilled.
bool DispatchTable::CheckAPIVersion(uint32_t apiVersion)
{
    switch (GetType())
    {
    case Type::GLOBAL:
        // The global dispatch table should not contain any entry points that depend on the API version.
        return false;

    case Type::INSTANCE:
        VK_ASSERT(m_pInstance != nullptr);
        return m_pInstance->GetAPIVersion() >= apiVersion;

    case Type::DEVICE:
        VK_ASSERT(m_pDevice != nullptr);
        return m_pDevice->VkInstance()->GetAPIVersion() >= apiVersion;

    default:
        VK_NEVER_CALLED();
        return false;
    }
}

// =====================================================================================================================
// Checks whether instance extension requirement is fulfilled.
bool DispatchTable::CheckInstanceExtension(InstanceExtensions::ExtensionId id)
{
    switch (GetType())
    {
    case Type::GLOBAL:
        // The global dispatch table should not contain any entry points that depend on instance extensions.
        return false;

    case Type::INSTANCE:
        VK_ASSERT(m_pInstance != nullptr);
        return m_pInstance->IsExtensionEnabled(id);

    case Type::DEVICE:
        VK_ASSERT(m_pDevice != nullptr);
        return m_pDevice->VkInstance()->IsExtensionEnabled(id);

    default:
        VK_NEVER_CALLED();
        return false;
    }
}

// =====================================================================================================================
// Checks whether device extension requirement is fulfilled.
bool DispatchTable::CheckDeviceExtension(DeviceExtensions::ExtensionId id)
{
    switch (GetType())
    {
    case Type::GLOBAL:
        // The global dispatch table should not contain any entry points that depend on device extensions.
        return false;

    case Type::INSTANCE:
        VK_ASSERT(m_pInstance != nullptr);
        return m_pInstance->IsDeviceExtensionAvailable(id);

    case Type::DEVICE:
        VK_ASSERT(m_pDevice != nullptr);
        return m_pDevice->IsExtensionEnabled(id);

    default:
        VK_NEVER_CALLED();
        return false;
    }
}

// =====================================================================================================================
// This constructor initializes the dispatch table based on its type.
// For the global dispatch table this constructor also initializes the entry point metadata table, thus this is expected
// to be only called once, implicitly, as part of constructing g_GlobalDispatchTable.
DispatchTable::DispatchTable(
    Type                type,
    const Instance*     pInstance,
    const Device*       pDevice)
  : m_type(type),
    m_pInstance(pInstance),
    m_pDevice(pDevice)
{
    // Clear dispatch table.
    memset(&m_func, 0, sizeof(m_func));

    // If this is the global dispatch table then clear the entry point metadata table.
    // The INIT_DISPATCH_ENTRY will also initialize the individual entry point metadata entries as well.
    if (GetType() == Type::GLOBAL)
    {
        VK_ASSERT(this == &g_GlobalDispatchTable);
        memset(&g_EntryPointMetadataTable[0], 0, sizeof(g_EntryPointMetadataTable));

        // The global dispatch table doesn't need a separate Init() call, it's automatically initialized.
        Init();
    }
}

// =====================================================================================================================
// This constructor initializes the dispatch table based on its type.
// For the global dispatch table this constructor also initializes the entry point metadata table, thus this is expected
// to be only called once, implicitly, as part of constructing g_GlobalDispatchTable.
void DispatchTable::Init()
{
    INIT_DISPATCH_ENTRY(vkGetInstanceProcAddr                           );

    INIT_DISPATCH_ENTRY(vkCreateInstance                                );
    INIT_DISPATCH_ENTRY(vkEnumerateInstanceExtensionProperties          );
    INIT_DISPATCH_ENTRY(vkEnumerateInstanceLayerProperties              );
    INIT_DISPATCH_ENTRY(vkEnumerateInstanceVersion                      );

    INIT_DISPATCH_ENTRY(vkGetDeviceProcAddr                             );
    INIT_DISPATCH_ENTRY(vkAcquireNextImageKHR                           );

    INIT_DISPATCH_ENTRY(vkAllocateDescriptorSets                        );
    INIT_DISPATCH_ENTRY(vkAllocateMemory                                );
    INIT_DISPATCH_ENTRY(vkBeginCommandBuffer                            );
    INIT_DISPATCH_ENTRY(vkBindBufferMemory                              );
    INIT_DISPATCH_ENTRY(vkBindImageMemory                               );
    INIT_DISPATCH_ENTRY(vkCmdBeginRenderPass                            );
    INIT_DISPATCH_ENTRY(vkCmdBeginQuery                                 );
    INIT_DISPATCH_ENTRY(vkCmdBindDescriptorSets                         );
    INIT_DISPATCH_ENTRY(vkCmdBindIndexBuffer                            );
    INIT_DISPATCH_ENTRY(vkCmdBindPipeline                               );
    INIT_DISPATCH_ENTRY(vkCmdBindVertexBuffers                          );
    INIT_DISPATCH_ENTRY(vkCmdBlitImage                                  );
    INIT_DISPATCH_ENTRY(vkCmdClearAttachments                           );
    INIT_DISPATCH_ENTRY(vkCmdClearColorImage                            );
    INIT_DISPATCH_ENTRY(vkCmdClearDepthStencilImage                     );
    INIT_DISPATCH_ENTRY(vkCmdCopyBuffer                                 );
    INIT_DISPATCH_ENTRY(vkCmdCopyBufferToImage                          );
    INIT_DISPATCH_ENTRY(vkCmdCopyImage                                  );
    INIT_DISPATCH_ENTRY(vkCmdCopyImageToBuffer                          );
    INIT_DISPATCH_ENTRY(vkCmdCopyQueryPoolResults                       );
    INIT_DISPATCH_ENTRY(vkCmdDraw                                       );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndexed                                );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndexedIndirect                        );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndirect                               );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndexedIndirectCount                   );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndirectCount                          );
    INIT_DISPATCH_ALIAS(vkCmdDrawIndexedIndirectCountKHR                ,
                        vkCmdDrawIndexedIndirectCount                   );
    INIT_DISPATCH_ALIAS(vkCmdDrawIndirectCountKHR                       ,
                        vkCmdDrawIndirectCount                          );
    INIT_DISPATCH_ALIAS(vkCmdDrawIndexedIndirectCountAMD                ,
                        vkCmdDrawIndexedIndirectCount                   );
    INIT_DISPATCH_ALIAS(vkCmdDrawIndirectCountAMD                       ,
                        vkCmdDrawIndirectCount                          );
    INIT_DISPATCH_ENTRY(vkCreateRenderPass2                             );
    INIT_DISPATCH_ENTRY(vkCmdBeginRenderPass2                           );
    INIT_DISPATCH_ENTRY(vkCmdNextSubpass2                               );
    INIT_DISPATCH_ENTRY(vkCmdEndRenderPass2                             );
    INIT_DISPATCH_ALIAS(vkCreateRenderPass2KHR                          ,
                        vkCreateRenderPass2                             );
    INIT_DISPATCH_ALIAS(vkCmdBeginRenderPass2KHR                        ,
                        vkCmdBeginRenderPass2                           );
    INIT_DISPATCH_ALIAS(vkCmdNextSubpass2KHR                            ,
                        vkCmdNextSubpass2                               );
    INIT_DISPATCH_ALIAS(vkCmdEndRenderPass2KHR                          ,
                        vkCmdEndRenderPass2                             );
    INIT_DISPATCH_ENTRY(vkCmdDispatch                                   );
    INIT_DISPATCH_ENTRY(vkCmdDispatchIndirect                           );
    INIT_DISPATCH_ENTRY(vkCmdEndRenderPass                              );
    INIT_DISPATCH_ENTRY(vkCmdEndQuery                                   );
    INIT_DISPATCH_ENTRY(vkCmdExecuteCommands                            );
    INIT_DISPATCH_ENTRY(vkCmdFillBuffer                                 );
    INIT_DISPATCH_ENTRY(vkCmdNextSubpass                                );
    INIT_DISPATCH_ENTRY(vkCmdPipelineBarrier                            );
    INIT_DISPATCH_ENTRY(vkCmdPushConstants                              );
    INIT_DISPATCH_ENTRY(vkCmdResetEvent                                 );
    INIT_DISPATCH_ENTRY(vkCmdResetQueryPool                             );
    INIT_DISPATCH_ENTRY(vkCmdResolveImage                               );

    INIT_DISPATCH_ENTRY(vkCmdSetBlendConstants                          );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthBias                               );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthBounds                             );

    INIT_DISPATCH_ENTRY(vkCmdSetEvent                                   );

    INIT_DISPATCH_ENTRY(vkCmdSetLineWidth                               );
    INIT_DISPATCH_ENTRY(vkCmdSetScissor                                 );
    INIT_DISPATCH_ENTRY(vkCmdSetStencilCompareMask                      );
    INIT_DISPATCH_ENTRY(vkCmdSetStencilReference                        );
    INIT_DISPATCH_ENTRY(vkCmdSetStencilWriteMask                        );
    INIT_DISPATCH_ENTRY(vkCmdSetViewport                                );

    INIT_DISPATCH_ENTRY(vkCmdUpdateBuffer                               );
    INIT_DISPATCH_ENTRY(vkCmdWaitEvents                                 );
    INIT_DISPATCH_ENTRY(vkCmdWriteTimestamp                             );

    INIT_DISPATCH_ENTRY(vkCreateBuffer                                  );
    INIT_DISPATCH_ENTRY(vkCreateBufferView                              );
    INIT_DISPATCH_ENTRY(vkAllocateCommandBuffers                        );
    INIT_DISPATCH_ENTRY(vkCreateCommandPool                             );
    INIT_DISPATCH_ENTRY(vkCreateComputePipelines                        );
    INIT_DISPATCH_ENTRY(vkCreateDescriptorPool                          );
    INIT_DISPATCH_ENTRY(vkCreateDescriptorSetLayout                     );
    INIT_DISPATCH_ENTRY(vkCreateDevice                                  );

    INIT_DISPATCH_ENTRY(vkCreateEvent                                   );
    INIT_DISPATCH_ENTRY(vkCreateFence                                   );
    INIT_DISPATCH_ENTRY(vkCreateFramebuffer                             );
    INIT_DISPATCH_ENTRY(vkCreateGraphicsPipelines                       );
    INIT_DISPATCH_ENTRY(vkCreateImage                                   );
    INIT_DISPATCH_ENTRY(vkCreateImageView                               );
    INIT_DISPATCH_ENTRY(vkCreatePipelineLayout                          );
    INIT_DISPATCH_ENTRY(vkCreatePipelineCache                           );
    INIT_DISPATCH_ENTRY(vkCreateQueryPool                               );
    INIT_DISPATCH_ENTRY(vkCreateRenderPass                              );
    INIT_DISPATCH_ENTRY(vkCreateSampler                                 );
    INIT_DISPATCH_ENTRY(vkCreateSemaphore                               );
    INIT_DISPATCH_ENTRY(vkCreateShaderModule                            );
    INIT_DISPATCH_ENTRY(vkCreateSwapchainKHR                            );
    INIT_DISPATCH_ENTRY(vkDestroySurfaceKHR                             );
#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
    INIT_DISPATCH_ENTRY(vkCreateXcbSurfaceKHR                           );
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    INIT_DISPATCH_ENTRY(vkCreateXlibSurfaceKHR                          );
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    INIT_DISPATCH_ENTRY(vkCreateWaylandSurfaceKHR                       );
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceXcbPresentationSupportKHR    );
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceXlibPresentationSupportKHR   );
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceWaylandPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    INIT_DISPATCH_ENTRY(vkAcquireXlibDisplayEXT                         );
    INIT_DISPATCH_ENTRY(vkGetRandROutputDisplayEXT                      );
#endif
    INIT_DISPATCH_ENTRY(vkReleaseDisplayEXT                             );
#endif
    INIT_DISPATCH_ENTRY(vkDestroyBuffer                                 );
    INIT_DISPATCH_ENTRY(vkDestroyBufferView                             );
    INIT_DISPATCH_ENTRY(vkFreeCommandBuffers                            );
    INIT_DISPATCH_ENTRY(vkDestroyCommandPool                            );
    INIT_DISPATCH_ENTRY(vkDestroyDescriptorPool                         );
    INIT_DISPATCH_ENTRY(vkDestroyDescriptorSetLayout                    );
    INIT_DISPATCH_ENTRY(vkDestroyDevice                                 );
    INIT_DISPATCH_ENTRY(vkDestroyEvent                                  );
    INIT_DISPATCH_ENTRY(vkDestroyFence                                  );
    INIT_DISPATCH_ENTRY(vkDestroyFramebuffer                            );
    INIT_DISPATCH_ENTRY(vkDestroyImage                                  );
    INIT_DISPATCH_ENTRY(vkDestroyImageView                              );
    INIT_DISPATCH_ENTRY(vkDestroyInstance                               );
    INIT_DISPATCH_ENTRY(vkDestroyPipeline                               );
    INIT_DISPATCH_ENTRY(vkDestroyPipelineCache                          );
    INIT_DISPATCH_ENTRY(vkDestroyPipelineLayout                         );
    INIT_DISPATCH_ENTRY(vkDestroyQueryPool                              );
    INIT_DISPATCH_ENTRY(vkDestroyRenderPass                             );
    INIT_DISPATCH_ENTRY(vkDestroySampler                                );
    INIT_DISPATCH_ENTRY(vkDestroySemaphore                              );
    INIT_DISPATCH_ENTRY(vkDestroyShaderModule                           );
    INIT_DISPATCH_ENTRY(vkDestroySwapchainKHR                           );

    INIT_DISPATCH_ENTRY(vkDeviceWaitIdle                                );
    INIT_DISPATCH_ENTRY(vkEndCommandBuffer                              );
    INIT_DISPATCH_ENTRY(vkEnumeratePhysicalDevices                      );
    INIT_DISPATCH_ENTRY(vkFlushMappedMemoryRanges                       );
    INIT_DISPATCH_ENTRY(vkFreeDescriptorSets                            );
    INIT_DISPATCH_ENTRY(vkFreeMemory                                    );
    INIT_DISPATCH_ENTRY(vkGetBufferMemoryRequirements                   );
    INIT_DISPATCH_ENTRY(vkGetDeviceMemoryCommitment                     );
    INIT_DISPATCH_ENTRY(vkGetDeviceQueue                                );
    INIT_DISPATCH_ENTRY(vkGetEventStatus                                );
    INIT_DISPATCH_ENTRY(vkGetFenceStatus                                );

    INIT_DISPATCH_ENTRY(vkEnumerateDeviceExtensionProperties            );
    INIT_DISPATCH_ENTRY(vkEnumerateDeviceLayerProperties                );

    INIT_DISPATCH_ENTRY(vkSetHdrMetadataEXT                             );

    INIT_DISPATCH_ENTRY(vkGetImageMemoryRequirements                    );
    INIT_DISPATCH_ENTRY(vkGetImageSparseMemoryRequirements              );
    INIT_DISPATCH_ENTRY(vkGetImageSubresourceLayout                     );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceFeatures                     );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceFormatProperties             );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceImageFormatProperties        );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceMemoryProperties             );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceProperties                   );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties        );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties  );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceSupportKHR            );
    INIT_DISPATCH_ENTRY(vkGetPipelineCacheData                          );
    INIT_DISPATCH_ENTRY(vkGetQueryPoolResults                           );
    INIT_DISPATCH_ENTRY(vkGetRenderAreaGranularity                      );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR       );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2KHR      );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR            );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceFormats2KHR           );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR       );

    INIT_DISPATCH_ENTRY(vkGetSwapchainImagesKHR                         );

    INIT_DISPATCH_ENTRY(vkInvalidateMappedMemoryRanges                  );
    INIT_DISPATCH_ENTRY(vkMapMemory                                     );
    INIT_DISPATCH_ENTRY(vkMergePipelineCaches                           );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceFeatures2KHR                 ,
                        vkGetPhysicalDeviceFeatures2                    );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceProperties2KHR               ,
                        vkGetPhysicalDeviceProperties2                  );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceFormatProperties2KHR         ,
                        vkGetPhysicalDeviceFormatProperties2            );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceImageFormatProperties2KHR    ,
                        vkGetPhysicalDeviceImageFormatProperties2       );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceQueueFamilyProperties2KHR    ,
                        vkGetPhysicalDeviceQueueFamilyProperties2       );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceMemoryProperties2KHR         ,
                        vkGetPhysicalDeviceMemoryProperties2            );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceSparseImageFormatProperties2KHR,
                        vkGetPhysicalDeviceSparseImageFormatProperties2 );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceExternalBufferPropertiesKHR  ,
                        vkGetPhysicalDeviceExternalBufferProperties     );

    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR,
                        vkGetPhysicalDeviceExternalSemaphoreProperties  );
#if defined(__unix__)
    INIT_DISPATCH_ENTRY(vkGetMemoryFdPropertiesKHR                      );
    INIT_DISPATCH_ENTRY(vkGetMemoryFdKHR                                );
    INIT_DISPATCH_ENTRY(vkImportSemaphoreFdKHR                          );
    INIT_DISPATCH_ENTRY(vkGetSemaphoreFdKHR                             );
    INIT_DISPATCH_ENTRY(vkGetFenceFdKHR                                 );
    INIT_DISPATCH_ENTRY(vkImportFenceFdKHR                              );
#endif

    INIT_DISPATCH_ALIAS(vkBindBufferMemory2KHR                          ,
                        vkBindBufferMemory2                             );
    INIT_DISPATCH_ALIAS(vkBindImageMemory2KHR                           ,
                        vkBindImageMemory2                              );

    INIT_DISPATCH_ALIAS(vkCreateDescriptorUpdateTemplateKHR             ,
                        vkCreateDescriptorUpdateTemplate                );
    INIT_DISPATCH_ALIAS(vkDestroyDescriptorUpdateTemplateKHR            ,
                        vkDestroyDescriptorUpdateTemplate               );
    INIT_DISPATCH_ALIAS(vkUpdateDescriptorSetWithTemplateKHR            ,
                        vkUpdateDescriptorSetWithTemplate               );

    INIT_DISPATCH_ENTRY(vkAcquireNextImage2KHR                          );
    INIT_DISPATCH_ALIAS(vkCmdDispatchBaseKHR                            ,
                        vkCmdDispatchBase                               );
    INIT_DISPATCH_ALIAS(vkCmdSetDeviceMaskKHR                           ,
                        vkCmdSetDeviceMask                              );
    INIT_DISPATCH_ALIAS(vkEnumeratePhysicalDeviceGroupsKHR              ,
                        vkEnumeratePhysicalDeviceGroups                 );
    INIT_DISPATCH_ALIAS(vkGetDeviceGroupPeerMemoryFeaturesKHR           ,
                        vkGetDeviceGroupPeerMemoryFeatures              );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDevicePresentRectanglesKHR         );
    INIT_DISPATCH_ENTRY(vkGetDeviceGroupPresentCapabilitiesKHR          );
    INIT_DISPATCH_ENTRY(vkGetDeviceGroupSurfacePresentModesKHR          );

    INIT_DISPATCH_ENTRY(vkQueueBindSparse                               );
    INIT_DISPATCH_ENTRY(vkQueuePresentKHR                               );
    INIT_DISPATCH_ENTRY(vkQueueSubmit                                   );
    INIT_DISPATCH_ENTRY(vkQueueWaitIdle                                 );
    INIT_DISPATCH_ENTRY(vkResetCommandBuffer                            );
    INIT_DISPATCH_ENTRY(vkResetCommandPool                              );
    INIT_DISPATCH_ENTRY(vkResetDescriptorPool                           );
    INIT_DISPATCH_ENTRY(vkResetEvent                                    );
    INIT_DISPATCH_ENTRY(vkResetFences                                   );
    INIT_DISPATCH_ENTRY(vkSetEvent                                      );
    INIT_DISPATCH_ALIAS(vkTrimCommandPoolKHR                            ,
                        vkTrimCommandPool                               );
    INIT_DISPATCH_ENTRY(vkUnmapMemory                                   );
    INIT_DISPATCH_ENTRY(vkUpdateDescriptorSets                          );
    INIT_DISPATCH_ENTRY(vkWaitForFences                                 );
    INIT_DISPATCH_ENTRY(vkGetShaderInfoAMD                              );

    INIT_DISPATCH_ENTRY(vkCmdDebugMarkerBeginEXT                        );
    INIT_DISPATCH_ENTRY(vkCmdDebugMarkerEndEXT                          );
    INIT_DISPATCH_ENTRY(vkCmdDebugMarkerInsertEXT                       );
    INIT_DISPATCH_ENTRY(vkDebugMarkerSetObjectTagEXT                    );
    INIT_DISPATCH_ENTRY(vkDebugMarkerSetObjectNameEXT                   );

    INIT_DISPATCH_ENTRY(vkCreateGpaSessionAMD                           );
    INIT_DISPATCH_ENTRY(vkDestroyGpaSessionAMD                          );
    INIT_DISPATCH_ENTRY(vkSetGpaDeviceClockModeAMD                      );
    INIT_DISPATCH_ENTRY(vkCmdBeginGpaSessionAMD                         );
    INIT_DISPATCH_ENTRY(vkCmdEndGpaSessionAMD                           );
    INIT_DISPATCH_ENTRY(vkCmdBeginGpaSampleAMD                          );
    INIT_DISPATCH_ENTRY(vkCmdEndGpaSampleAMD                            );
    INIT_DISPATCH_ENTRY(vkGetGpaSessionStatusAMD                        );
    INIT_DISPATCH_ENTRY(vkGetGpaSessionResultsAMD                       );
    INIT_DISPATCH_ENTRY(vkResetGpaSessionAMD                            );
    INIT_DISPATCH_ENTRY(vkCmdCopyGpaSessionResultsAMD                   );
    INIT_DISPATCH_ALIAS(vkGetImageMemoryRequirements2KHR                ,
                        vkGetImageMemoryRequirements2                   );
    INIT_DISPATCH_ALIAS(vkGetBufferMemoryRequirements2KHR               ,
                        vkGetBufferMemoryRequirements2                  );
    INIT_DISPATCH_ALIAS(vkGetImageSparseMemoryRequirements2KHR          ,
                        vkGetImageSparseMemoryRequirements2             );

    INIT_DISPATCH_ENTRY(vkCmdSetSampleLocationsEXT                      );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceMultisamplePropertiesEXT     );

    INIT_DISPATCH_ALIAS(vkGetDescriptorSetLayoutSupportKHR              ,
                        vkGetDescriptorSetLayoutSupport                 );

    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceExternalFencePropertiesKHR   ,
                        vkGetPhysicalDeviceExternalFenceProperties      );
    INIT_DISPATCH_ENTRY(vkBindBufferMemory2                             );
    INIT_DISPATCH_ENTRY(vkBindImageMemory2                              );
    INIT_DISPATCH_ENTRY(vkCmdSetDeviceMask                              );
    INIT_DISPATCH_ENTRY(vkCmdDispatchBase                               );
    INIT_DISPATCH_ENTRY(vkCreateDescriptorUpdateTemplate                );
    INIT_DISPATCH_ENTRY(vkCreateSamplerYcbcrConversion                  );
    INIT_DISPATCH_ALIAS(vkCreateSamplerYcbcrConversionKHR               ,
                        vkCreateSamplerYcbcrConversion                  );
    INIT_DISPATCH_ENTRY(vkDestroyDescriptorUpdateTemplate               );
    INIT_DISPATCH_ENTRY(vkDestroySamplerYcbcrConversion                 );
    INIT_DISPATCH_ALIAS(vkDestroySamplerYcbcrConversionKHR              ,
                        vkDestroySamplerYcbcrConversion                 );
    INIT_DISPATCH_ENTRY(vkEnumeratePhysicalDeviceGroups                 );
    INIT_DISPATCH_ENTRY(vkGetBufferMemoryRequirements2                  );
    INIT_DISPATCH_ENTRY(vkGetDescriptorSetLayoutSupport                 );
    INIT_DISPATCH_ENTRY(vkGetDeviceGroupPeerMemoryFeatures              );
    INIT_DISPATCH_ENTRY(vkGetDeviceQueue2                               );
    INIT_DISPATCH_ENTRY(vkGetImageMemoryRequirements2                   );
    INIT_DISPATCH_ENTRY(vkGetImageSparseMemoryRequirements2             );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceExternalBufferProperties     );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceExternalFenceProperties      );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceExternalSemaphoreProperties  );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceFeatures2                    );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceFormatProperties2            );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceImageFormatProperties2       );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceMemoryProperties2            );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceProperties2                  );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceQueueFamilyProperties2       );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSparseImageFormatProperties2 );
    INIT_DISPATCH_ENTRY(vkTrimCommandPool                               );
    INIT_DISPATCH_ENTRY(vkUpdateDescriptorSetWithTemplate               );
    INIT_DISPATCH_ENTRY(vkCreateDebugReportCallbackEXT                  );
    INIT_DISPATCH_ENTRY(vkDestroyDebugReportCallbackEXT                 );
    INIT_DISPATCH_ENTRY(vkDebugReportMessageEXT                         );
    INIT_DISPATCH_ENTRY(vkGetSemaphoreCounterValue                      );
    INIT_DISPATCH_ENTRY(vkWaitSemaphores                                );
    INIT_DISPATCH_ENTRY(vkSignalSemaphore                               );
    INIT_DISPATCH_ALIAS(vkGetSemaphoreCounterValueKHR                   ,
                        vkGetSemaphoreCounterValue                      );
    INIT_DISPATCH_ALIAS(vkWaitSemaphoresKHR                             ,
                        vkWaitSemaphores                                );
    INIT_DISPATCH_ALIAS(vkSignalSemaphoreKHR                            ,
                        vkSignalSemaphore                               );
    INIT_DISPATCH_ENTRY(vkCmdWriteBufferMarkerAMD                       );
    INIT_DISPATCH_ENTRY(vkGetMemoryHostPointerPropertiesEXT             );
#if defined(__unix__)
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceDisplayPropertiesKHR         );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceDisplayPlanePropertiesKHR    );
    INIT_DISPATCH_ENTRY(vkGetDisplayPlaneSupportedDisplaysKHR           );
    INIT_DISPATCH_ENTRY(vkGetDisplayModePropertiesKHR                   );
    INIT_DISPATCH_ENTRY(vkCreateDisplayModeKHR                          );
    INIT_DISPATCH_ENTRY(vkGetDisplayPlaneCapabilitiesKHR                );
    INIT_DISPATCH_ENTRY(vkCreateDisplayPlaneSurfaceKHR                  );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceDisplayProperties2KHR        );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceDisplayPlaneProperties2KHR   );
    INIT_DISPATCH_ENTRY(vkGetDisplayModeProperties2KHR                  );
    INIT_DISPATCH_ENTRY(vkGetDisplayPlaneCapabilities2KHR               );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceSurfaceCapabilities2EXT      );
#endif
    INIT_DISPATCH_ENTRY(vkCmdBindTransformFeedbackBuffersEXT            );
    INIT_DISPATCH_ENTRY(vkCmdBeginTransformFeedbackEXT                  );
    INIT_DISPATCH_ENTRY(vkCmdEndTransformFeedbackEXT                    );
    INIT_DISPATCH_ENTRY(vkCmdBeginQueryIndexedEXT                       );
    INIT_DISPATCH_ENTRY(vkCmdEndQueryIndexedEXT                         );
    INIT_DISPATCH_ENTRY(vkCmdDrawIndirectByteCountEXT                   );
    INIT_DISPATCH_ENTRY(vkSetDebugUtilsObjectNameEXT                    );
    INIT_DISPATCH_ENTRY(vkSetDebugUtilsObjectTagEXT                     );
    INIT_DISPATCH_ENTRY(vkQueueBeginDebugUtilsLabelEXT                  );
    INIT_DISPATCH_ENTRY(vkQueueEndDebugUtilsLabelEXT                    );
    INIT_DISPATCH_ENTRY(vkQueueInsertDebugUtilsLabelEXT                 );
    INIT_DISPATCH_ENTRY(vkCmdBeginDebugUtilsLabelEXT                    );
    INIT_DISPATCH_ENTRY(vkCmdEndDebugUtilsLabelEXT                      );
    INIT_DISPATCH_ENTRY(vkCmdInsertDebugUtilsLabelEXT                   );
    INIT_DISPATCH_ENTRY(vkCreateDebugUtilsMessengerEXT                  );
    INIT_DISPATCH_ENTRY(vkDestroyDebugUtilsMessengerEXT                 );
    INIT_DISPATCH_ENTRY(vkSubmitDebugUtilsMessageEXT                    );
#if VKI_RAY_TRACING
    INIT_DISPATCH_ENTRY(vkCreateDeferredOperationKHR);
    INIT_DISPATCH_ENTRY(vkDestroyDeferredOperationKHR);
    INIT_DISPATCH_ENTRY(vkGetDeferredOperationResultKHR);
    INIT_DISPATCH_ENTRY(vkGetDeferredOperationMaxConcurrencyKHR);
    INIT_DISPATCH_ENTRY(vkDeferredOperationJoinKHR);
    INIT_DISPATCH_ENTRY(vkCreateAccelerationStructureKHR                 );
    INIT_DISPATCH_ENTRY(vkDestroyAccelerationStructureKHR                );
    INIT_DISPATCH_ENTRY(vkCmdBuildAccelerationStructuresKHR              );
    INIT_DISPATCH_ENTRY(vkCmdBuildAccelerationStructuresIndirectKHR      );
    INIT_DISPATCH_ENTRY(vkBuildAccelerationStructuresKHR                 );
    INIT_DISPATCH_ENTRY(vkCopyAccelerationStructureToMemoryKHR           );
    INIT_DISPATCH_ENTRY(vkCopyAccelerationStructureKHR                   );
    INIT_DISPATCH_ENTRY(vkCopyMemoryToAccelerationStructureKHR           );
    INIT_DISPATCH_ENTRY(vkWriteAccelerationStructuresPropertiesKHR       );
    INIT_DISPATCH_ENTRY(vkCmdCopyAccelerationStructureKHR                );
    INIT_DISPATCH_ENTRY(vkCmdCopyAccelerationStructureToMemoryKHR        );
    INIT_DISPATCH_ENTRY(vkCmdCopyMemoryToAccelerationStructureKHR        );
    INIT_DISPATCH_ENTRY(vkCmdTraceRaysKHR                                );
    INIT_DISPATCH_ENTRY(vkCreateRayTracingPipelinesKHR                   );
    INIT_DISPATCH_ENTRY(vkGetRayTracingShaderGroupHandlesKHR             );
    INIT_DISPATCH_ENTRY(vkGetAccelerationStructureDeviceAddressKHR       );
    INIT_DISPATCH_ENTRY(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR);
    INIT_DISPATCH_ENTRY(vkCmdWriteAccelerationStructuresPropertiesKHR    );
    INIT_DISPATCH_ENTRY(vkCmdTraceRaysIndirectKHR                        );
    INIT_DISPATCH_ENTRY(vkGetDeviceAccelerationStructureCompatibilityKHR );
    INIT_DISPATCH_ENTRY(vkGetRayTracingShaderGroupStackSizeKHR           );
    INIT_DISPATCH_ENTRY(vkCmdSetRayTracingPipelineStackSizeKHR           );
    INIT_DISPATCH_ENTRY(vkGetAccelerationStructureBuildSizesKHR          );
    INIT_DISPATCH_ENTRY(vkCmdTraceRaysIndirect2KHR                       );
#endif
    INIT_DISPATCH_ENTRY(vkGetPipelineExecutablePropertiesKHR             );
    INIT_DISPATCH_ENTRY(vkGetPipelineExecutableStatisticsKHR             );
    INIT_DISPATCH_ENTRY(vkGetPipelineExecutableInternalRepresentationsKHR);
    INIT_DISPATCH_ENTRY(vkGetBufferDeviceAddress                        );
    INIT_DISPATCH_ENTRY(vkGetBufferOpaqueCaptureAddress                 );
    INIT_DISPATCH_ENTRY(vkGetDeviceMemoryOpaqueCaptureAddress           );
    INIT_DISPATCH_ALIAS(vkGetBufferDeviceAddressKHR                     ,
                        vkGetBufferDeviceAddress                        );
    INIT_DISPATCH_ALIAS(vkGetBufferOpaqueCaptureAddressKHR              ,
                        vkGetBufferOpaqueCaptureAddress                 );
    INIT_DISPATCH_ALIAS(vkGetDeviceMemoryOpaqueCaptureAddressKHR        ,
                        vkGetDeviceMemoryOpaqueCaptureAddress           );
    INIT_DISPATCH_ENTRY(vkResetQueryPool                                );
    INIT_DISPATCH_ALIAS(vkResetQueryPoolEXT                             ,
                        vkResetQueryPool                                );
    INIT_DISPATCH_ENTRY(vkCmdSetLineStippleEXT                          );

    INIT_DISPATCH_ENTRY(vkSetDeviceMemoryPriorityEXT                    );
    INIT_DISPATCH_ENTRY(vkGetDeviceFaultInfoEXT                         );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT  );
    INIT_DISPATCH_ENTRY(vkGetCalibratedTimestampsEXT                    );

    INIT_DISPATCH_ENTRY(vkCmdBeginConditionalRenderingEXT               );
    INIT_DISPATCH_ENTRY(vkCmdEndConditionalRenderingEXT                 );

    INIT_DISPATCH_ENTRY(vkCmdSetEvent2                                  );
    INIT_DISPATCH_ENTRY(vkCmdResetEvent2                                );
    INIT_DISPATCH_ENTRY(vkCmdWaitEvents2                                );
    INIT_DISPATCH_ENTRY(vkCmdPipelineBarrier2                           );
    INIT_DISPATCH_ENTRY(vkCmdWriteTimestamp2                            );
    INIT_DISPATCH_ENTRY(vkCmdWriteBufferMarker2AMD                      );
    INIT_DISPATCH_ENTRY(vkQueueSubmit2                                  );
    INIT_DISPATCH_ALIAS(vkCmdSetEvent2KHR,
                        vkCmdSetEvent2                                  );
    INIT_DISPATCH_ALIAS(vkCmdResetEvent2KHR,
                        vkCmdResetEvent2                                );
    INIT_DISPATCH_ALIAS(vkCmdWaitEvents2KHR,
                        vkCmdWaitEvents2                                );
    INIT_DISPATCH_ALIAS(vkCmdPipelineBarrier2KHR,
                        vkCmdPipelineBarrier2                           );
    INIT_DISPATCH_ALIAS(vkCmdWriteTimestamp2KHR,
                        vkCmdWriteTimestamp2                            );
    INIT_DISPATCH_ALIAS(vkQueueSubmit2KHR,
                        vkQueueSubmit2                                  );

    INIT_DISPATCH_ENTRY(vkCmdBeginRendering                             );
    INIT_DISPATCH_ENTRY(vkCmdEndRendering                               );
    INIT_DISPATCH_ALIAS(vkCmdBeginRenderingKHR,
                        vkCmdBeginRendering                             );
    INIT_DISPATCH_ALIAS(vkCmdEndRenderingKHR,
                        vkCmdEndRendering                               );

    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceToolProperties               );
    INIT_DISPATCH_ALIAS(vkGetPhysicalDeviceToolPropertiesEXT,
                        vkGetPhysicalDeviceToolProperties               );

    INIT_DISPATCH_ENTRY(vkCmdSetCullMode                                );
    INIT_DISPATCH_ENTRY(vkCmdSetFrontFace                               );
    INIT_DISPATCH_ENTRY(vkCmdSetPrimitiveTopology                       );
    INIT_DISPATCH_ENTRY(vkCmdSetViewportWithCount                       );
    INIT_DISPATCH_ENTRY(vkCmdSetScissorWithCount                        );
    INIT_DISPATCH_ENTRY(vkCmdBindVertexBuffers2                         );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthTestEnable                         );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthWriteEnable                        );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthCompareOp                          );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthBoundsTestEnable                   );
    INIT_DISPATCH_ENTRY(vkCmdSetStencilTestEnable                       );
    INIT_DISPATCH_ENTRY(vkCmdSetStencilOp                               );
    INIT_DISPATCH_ALIAS(vkCmdSetCullModeEXT,
                        vkCmdSetCullMode                                );
    INIT_DISPATCH_ALIAS(vkCmdSetFrontFaceEXT,
                        vkCmdSetFrontFace                               );
    INIT_DISPATCH_ALIAS(vkCmdSetPrimitiveTopologyEXT,
                        vkCmdSetPrimitiveTopology                       );
    INIT_DISPATCH_ALIAS(vkCmdSetViewportWithCountEXT,
                        vkCmdSetViewportWithCount                       );
    INIT_DISPATCH_ALIAS(vkCmdSetScissorWithCountEXT,
                        vkCmdSetScissorWithCount                        );
    INIT_DISPATCH_ALIAS(vkCmdBindVertexBuffers2EXT,
                        vkCmdBindVertexBuffers2                         );
    INIT_DISPATCH_ALIAS(vkCmdSetDepthTestEnableEXT,
                        vkCmdSetDepthTestEnable                         );
    INIT_DISPATCH_ALIAS(vkCmdSetDepthWriteEnableEXT,
                        vkCmdSetDepthWriteEnable                        );
    INIT_DISPATCH_ALIAS(vkCmdSetDepthCompareOpEXT,
                        vkCmdSetDepthCompareOp                          );
    INIT_DISPATCH_ALIAS(vkCmdSetDepthBoundsTestEnableEXT,
                        vkCmdSetDepthBoundsTestEnable                   );
    INIT_DISPATCH_ALIAS(vkCmdSetStencilTestEnableEXT,
                        vkCmdSetStencilTestEnable                       );
    INIT_DISPATCH_ALIAS(vkCmdSetStencilOpEXT,
                        vkCmdSetStencilOp                               );

    INIT_DISPATCH_ENTRY(vkCmdSetFragmentShadingRateKHR                  );
    INIT_DISPATCH_ENTRY(vkGetPhysicalDeviceFragmentShadingRatesKHR      );

    INIT_DISPATCH_ENTRY(vkCreatePrivateDataSlot                         );
    INIT_DISPATCH_ENTRY(vkDestroyPrivateDataSlot                        );
    INIT_DISPATCH_ENTRY(vkSetPrivateData                                );
    INIT_DISPATCH_ENTRY(vkGetPrivateData                                );
    INIT_DISPATCH_ALIAS(vkCreatePrivateDataSlotEXT,
                        vkCreatePrivateDataSlot                         );
    INIT_DISPATCH_ALIAS(vkDestroyPrivateDataSlotEXT,
                        vkDestroyPrivateDataSlot                        );
    INIT_DISPATCH_ALIAS(vkSetPrivateDataEXT,
                        vkSetPrivateData                                );
    INIT_DISPATCH_ALIAS(vkGetPrivateDataEXT,
                        vkGetPrivateData                                );

    INIT_DISPATCH_ENTRY(vkGetDescriptorSetLayoutSizeEXT                 );
    INIT_DISPATCH_ENTRY(vkGetDescriptorSetLayoutBindingOffsetEXT        );
    INIT_DISPATCH_ENTRY(vkGetDescriptorEXT                              );
    INIT_DISPATCH_ENTRY(vkGetBufferOpaqueCaptureDescriptorDataEXT       );
    INIT_DISPATCH_ENTRY(vkGetImageOpaqueCaptureDescriptorDataEXT        );
    INIT_DISPATCH_ENTRY(vkGetImageViewOpaqueCaptureDescriptorDataEXT    );
    INIT_DISPATCH_ENTRY(vkGetSamplerOpaqueCaptureDescriptorDataEXT      );
    INIT_DISPATCH_ENTRY(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT);
    INIT_DISPATCH_ENTRY(vkCmdBindDescriptorBuffersEXT                   );
    INIT_DISPATCH_ENTRY(vkCmdSetDescriptorBufferOffsetsEXT              );
    INIT_DISPATCH_ENTRY(vkCmdBindDescriptorBufferEmbeddedSamplersEXT    );

    INIT_DISPATCH_ENTRY(vkCmdSetColorWriteEnableEXT                     );

    INIT_DISPATCH_ENTRY(vkCmdSetRasterizerDiscardEnable                 );
    INIT_DISPATCH_ENTRY(vkCmdSetPrimitiveRestartEnable                  );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthBiasEnable                         );
    INIT_DISPATCH_ENTRY(vkCmdSetLogicOpEXT                              );
    INIT_DISPATCH_ENTRY(vkCmdSetPatchControlPointsEXT                   );
    INIT_DISPATCH_ALIAS(vkCmdSetRasterizerDiscardEnableEXT,
                        vkCmdSetRasterizerDiscardEnable                 );
    INIT_DISPATCH_ALIAS(vkCmdSetPrimitiveRestartEnableEXT,
                        vkCmdSetPrimitiveRestartEnable                  );
    INIT_DISPATCH_ALIAS(vkCmdSetDepthBiasEnableEXT,
                        vkCmdSetDepthBiasEnable                         );

    INIT_DISPATCH_ENTRY(vkCmdBlitImage2                                 );
    INIT_DISPATCH_ENTRY(vkCmdCopyBuffer2                                );
    INIT_DISPATCH_ENTRY(vkCmdCopyBufferToImage2                         );
    INIT_DISPATCH_ENTRY(vkCmdCopyImage2                                 );
    INIT_DISPATCH_ENTRY(vkCmdCopyImageToBuffer2                         );
    INIT_DISPATCH_ENTRY(vkCmdResolveImage2                              );
    INIT_DISPATCH_ALIAS(vkCmdBlitImage2KHR,
                        vkCmdBlitImage2                                 );
    INIT_DISPATCH_ALIAS(vkCmdCopyBuffer2KHR,
                        vkCmdCopyBuffer2                                );
    INIT_DISPATCH_ALIAS(vkCmdCopyBufferToImage2KHR,
                        vkCmdCopyBufferToImage2                         );
    INIT_DISPATCH_ALIAS(vkCmdCopyImage2KHR,
                        vkCmdCopyImage2                                 );
    INIT_DISPATCH_ALIAS(vkCmdCopyImageToBuffer2KHR,
                        vkCmdCopyImageToBuffer2                         );
    INIT_DISPATCH_ALIAS(vkCmdResolveImage2KHR,
                        vkCmdResolveImage2                              );
    INIT_DISPATCH_ENTRY(vkCmdPushDescriptorSetKHR                       );
    INIT_DISPATCH_ENTRY(vkCmdPushDescriptorSetWithTemplateKHR           );
    INIT_DISPATCH_ENTRY(vkGetDeviceBufferMemoryRequirements             );
    INIT_DISPATCH_ENTRY(vkGetDeviceImageMemoryRequirements              );
    INIT_DISPATCH_ENTRY(vkGetDeviceImageSparseMemoryRequirements        );
    INIT_DISPATCH_ALIAS(vkGetDeviceBufferMemoryRequirementsKHR,
                        vkGetDeviceBufferMemoryRequirements             );
    INIT_DISPATCH_ALIAS(vkGetDeviceImageMemoryRequirementsKHR,
                        vkGetDeviceImageMemoryRequirements              );
    INIT_DISPATCH_ALIAS(vkGetDeviceImageSparseMemoryRequirementsKHR,
                        vkGetDeviceImageSparseMemoryRequirements        );

    INIT_DISPATCH_ENTRY(vkCmdDrawMeshTasksEXT                           );
    INIT_DISPATCH_ENTRY(vkCmdDrawMeshTasksIndirectEXT                   );
    INIT_DISPATCH_ENTRY(vkCmdDrawMeshTasksIndirectCountEXT              );

    INIT_DISPATCH_ENTRY(vkMapMemory2KHR                                 );
    INIT_DISPATCH_ENTRY(vkUnmapMemory2KHR                               );

    INIT_DISPATCH_ENTRY(vkGetShaderModuleIdentifierEXT                  );
    INIT_DISPATCH_ENTRY(vkGetShaderModuleCreateInfoIdentifierEXT        );

    INIT_DISPATCH_ENTRY(vkCmdSetTessellationDomainOriginEXT             );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthClampEnableEXT                     );
    INIT_DISPATCH_ENTRY(vkCmdSetPolygonModeEXT                          );
    INIT_DISPATCH_ENTRY(vkCmdSetRasterizationSamplesEXT                 );
    INIT_DISPATCH_ENTRY(vkCmdSetSampleMaskEXT                           );
    INIT_DISPATCH_ENTRY(vkCmdSetAlphaToCoverageEnableEXT                );
    INIT_DISPATCH_ENTRY(vkCmdSetAlphaToOneEnableEXT                     );
    INIT_DISPATCH_ENTRY(vkCmdSetLogicOpEnableEXT                        );
    INIT_DISPATCH_ENTRY(vkCmdSetColorBlendEnableEXT                     );
    INIT_DISPATCH_ENTRY(vkCmdSetColorBlendEquationEXT                   );
    INIT_DISPATCH_ENTRY(vkCmdSetColorWriteMaskEXT                       );
    INIT_DISPATCH_ENTRY(vkCmdSetRasterizationStreamEXT                  );
    INIT_DISPATCH_ENTRY(vkCmdSetConservativeRasterizationModeEXT        );
    INIT_DISPATCH_ENTRY(vkCmdSetExtraPrimitiveOverestimationSizeEXT     );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthClipEnableEXT                      );
    INIT_DISPATCH_ENTRY(vkCmdSetSampleLocationsEnableEXT                );
    INIT_DISPATCH_ENTRY(vkCmdSetColorBlendAdvancedEXT                   );
    INIT_DISPATCH_ENTRY(vkCmdSetProvokingVertexModeEXT                  );
    INIT_DISPATCH_ENTRY(vkCmdSetLineRasterizationModeEXT                );
    INIT_DISPATCH_ENTRY(vkCmdSetLineStippleEnableEXT                    );
    INIT_DISPATCH_ENTRY(vkCmdSetDepthClipNegativeOneToOneEXT            );
    INIT_DISPATCH_ENTRY(vkCmdSetVertexInputEXT);

    INIT_DISPATCH_ENTRY(vkGetShaderModuleIdentifierEXT                  );
    INIT_DISPATCH_ENTRY(vkGetShaderModuleCreateInfoIdentifierEXT        );

#if defined(__unix__)
    INIT_DISPATCH_ENTRY(vkGetImageDrmFormatModifierPropertiesEXT        );
#endif

}

// =====================================================================================================================
// Call this function to get the entry point corresponding to an entry point name from the dispatch table.
// Depending on the dispatch table type the following behavior is expected:
// * GLOBAL - Only entry points queriable using vkGetInstanceProcAddr with an instance parameter of VK_NULL_HANDLE
//   are returned. This means only global entry points can be queried this way.
// * INSTANCE/PHYSDEVICE - Only entry points queriable using vkGetInstanceProcAddr with an instance parameter different
//   than VK_NULL_HANDLE are returned. This means instance- and device-level entry points can be queried this way and
//   for device entry points an appropriate trampoline is returned if applicable. Core API version, instance
//   extension enablement, and device extension availability are a prerequisite.
// * DEVICE - Only entry points queriable using vkGetDeviceProcAddr are returned. This means only device-level
//   entry points can be queried this way. Core API version and device extension enablement are a prerequisite.
PFN_vkVoidFunction DispatchTable::GetEntryPoint(const char* pName) const
{
    PFN_vkVoidFunction pFunc = nullptr;
    bool found = false;

    for (uint32_t epIdx = 0; (found == false) && (epIdx < VKI_ENTRY_POINT_COUNT); epIdx++)
    {
        const EntryPoint::Metadata& metadata = g_EntryPointMetadataTable[epIdx];

        if ((metadata.pName != nullptr) &&
            (strcmp(pName, metadata.pName) == 0))
        {
            found = true;

            switch (metadata.type)
            {
            case EntryPoint::Type::GLOBAL:
                // Only return global entry points if this is the global dispatch table or an instance
                // dispatch table.
                if ((GetType() == Type::GLOBAL) || (GetType() == Type::INSTANCE))
                {
                    pFunc = m_table[epIdx];
                }
                break;

            case EntryPoint::Type::PHYSDEVICE:
            case EntryPoint::Type::INSTANCE:
                // Only return physical-device and instance-level entry points if this is an instance dispatch table.
                if (GetType() == Type::INSTANCE)
                {
                    pFunc = m_table[epIdx];
                }

                // Allows physical-device and instance-level functions to be queried with vkGetDeviceProcAddr for
                // special cases.
                if (m_pDevice != nullptr)
                {
                    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
                    if (settings.lenientInstanceFuncQuery)
                    {
                        pFunc = m_table[epIdx];
                    }
                }
                break;

            case EntryPoint::Type::DEVICE:
                // Only return device-level entry points if this is an instance or device dispatch table.
                if ((GetType() == Type::INSTANCE) || (GetType() == Type::DEVICE))
                {
                    pFunc = m_table[epIdx];
                }
                break;
            }
        }
    }

    return pFunc;
}

// =====================================================================================================================
// Call this function to get the entry point corresponding to physical device entry point name from the instance
// dispatch table and return NULL addresses for non physical device entry points.(requirement of the
// vkGetPhysicalDeviceProcAddr function).
PFN_vkVoidFunction DispatchTable::GetPhysicalDeviceEntryPoint(const char* pName) const
{
    PFN_vkVoidFunction pFunc = nullptr;

    for (uint32_t epIdx = 0; epIdx < VKI_ENTRY_POINT_COUNT; epIdx++)
    {
        const EntryPoint::Metadata& metadata = g_EntryPointMetadataTable[epIdx];

        if ((metadata.pName != nullptr) &&
            (strcmp(pName, metadata.pName) == 0))
        {
            if (metadata.type == EntryPoint::Type::PHYSDEVICE)
            {
                pFunc = m_table[epIdx];
            }

            break;
        }
    }

    return pFunc;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    if (instance == VK_NULL_HANDLE)
    {
        return g_GlobalDispatchTable.GetEntryPoint(pName);
    }
    else
    {
        return Instance::ObjectFromHandle(instance)->GetDispatchTable().GetEntryPoint(pName);
    }
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetPhysicalDeviceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return Instance::ObjectFromHandle(instance)->GetDispatchTable().GetPhysicalDeviceEntryPoint(pName);
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName)
{
    return ApiDevice::ObjectFromHandle(device)->GetDispatchTable().GetEntryPoint(pName);
}

} // namespace entry

} // namespace vk

extern "C"
{

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::entry::vkGetInstanceProcAddr(instance, pName);
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::entry::vkGetInstanceProcAddr(instance, pName);
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetPhysicalDeviceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::entry::vkGetPhysicalDeviceProcAddr(instance, pName);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(
    uint32_t *pVersion)
{
    // InterfaceVersion 3 - was introduced at 1.0.30, therefore we reject all older versions of the loader.

    static constexpr uint32_t MinDriverSupportedInterfaceVersion = 3;
    static constexpr uint32_t MaxDriverSupportedInterfaceVersion = 6;

    if (*pVersion < MinDriverSupportedInterfaceVersion)
    {
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    // We can use 'usedInterfaceVersion' in the driver to implement special behavior based
    // on a particular version of the loader if necessary.
    const uint32_t usedInterfaceVersion = Util::Min(*pVersion, MaxDriverSupportedInterfaceVersion);

    *pVersion = usedInterfaceVersion;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName)
{
    return vk::entry::vkGetDeviceProcAddr(device, pName);
}

} // extern "C"

struct VK_LAYER_DISPATCH_TABLE
{
    PFN_vkGetInstanceProcAddr   pGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr     pGetDeviceProcAddr;
};

extern "C" const VK_LAYER_DISPATCH_TABLE dispatch_table =
{
    vk::entry::vkGetInstanceProcAddr,
    vk::entry::vkGetDeviceProcAddr,
};
