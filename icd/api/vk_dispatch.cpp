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
#include "include/vk_semaphore.h"
#include "include/vk_shader.h"
#include "include/vk_surface.h"
#include "include/vk_swapchain.h"
#include "include/vk_debug_report.h"

#include <cstring>

namespace vk
{

// =====================================================================================================================
// Given one or more dispatch tables (ppTables), go through each one and look for the first dispatch table that
// matches the name and current entry point conditions.
//
// This implementation supports both secure and insecure strings.  If 'nameSecure' is true, then 'pName' is expected
// to be one of the vk::entry::secure string pointers (comparison happens by address).  If 'nameSecure' is false,
// 'pName' is expected to be a normal string.
void* GetIcdProcAddr(
    const Instance*            pInstance,
    const Device*              pDevice,
    uint32_t                   tableCount,
    const DispatchTableEntry** ppTables,
    const char*                pName)
{
    void* pFunc = nullptr;
    bool found = false;

    for (uint32_t tableIdx = 0; (found == false) && (tableIdx < tableCount); tableIdx++)
    {
        const DispatchTableEntry* pTable = ppTables[tableIdx];

        for (const DispatchTableEntry* pEntry = pTable; (found == false) && (pEntry->pName != 0); pEntry++)
        {
            if ((pName == pEntry->pName) || (strcmp(pName, pEntry->pName) == 0))
            {
                found = true;

                switch (pEntry->conditionType)
                {
                case vk::secure::entry::ENTRY_POINT_NONE:
                    {
                        // No further conditions, return the entry point.
                        pFunc = pEntry->pFunc;
                        break;
                    }
                case vk::secure::entry::ENTRY_POINT_CORE:
                    {
                        // Check version requested against the required version.
                        if ((pInstance != nullptr) && (pInstance->GetAPIVersion() >= pEntry->conditionValue))
                        {
                            pFunc = pEntry->pFunc;
                        }
                        break;
                    }
                case vk::secure::entry::ENTRY_POINT_INSTANCE_EXTENSION:
                    {
                        // Check instance extension support.
                        auto extension = static_cast<InstanceExtensions::ExtensionId>(pEntry->conditionValue);

                        if ((pInstance != nullptr) && pInstance->IsExtensionEnabled(extension))
                        {
                            pFunc = pEntry->pFunc;
                        }
                        break;
                    }
                case vk::secure::entry::ENTRY_POINT_DEVICE_EXTENSION:
                    {
                        // Check device extension support.
                        auto extension = static_cast<DeviceExtensions::ExtensionId>(pEntry->conditionValue);

                        if ((pDevice != nullptr) && pDevice->IsExtensionEnabled(extension))
                        {
                            pFunc = pEntry->pFunc;
                        }
                        else
                        {
                            // The loader-ICD interface allows querying "available" device extension commands using
                            // vk_icdGetInstanceProcAddr and vk_icdGetPhysicalDeviceProcAddr thus here we have to
                            // check whether any of the devices support the extension.
                            if ((pInstance != nullptr) && pInstance->IsDeviceExtensionAvailable(extension))
                            {
                                pFunc = pEntry->pFunc;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    return pFunc;
}

// =====================================================================================================================
// This is a catch-all implementation of all the public ways of resolving entry point names to function pointers e.g.
// vkGetInstanceProcAddr, vkGetDeviceProcAddr, vk_icdGetProcAddr, and so on.
static PFN_vkVoidFunction GetIcdProcAddr(
    VkInstance  instance,
    VkDevice    device,
    const char* pName)
{
    Instance* pInstance = nullptr;
    Device*   pDevice   = nullptr;

    if (instance != VK_NULL_HANDLE)
    {
        pInstance = Instance::ObjectFromHandle(instance);
    }
    else if (device != VK_NULL_HANDLE)
    {
        pDevice   = ApiDevice::ObjectFromHandle(device);
        pInstance = pDevice->VkInstance();
    }

    // Get the instance's dispatch tables if we have a valid instance handle.
    uint32_t tableCount = 0;
    const DispatchTableEntry* dispatchTables[Instance::MaxDispatchTables];

    if (pInstance != nullptr)
    {
        tableCount = pInstance->GetDispatchTables(dispatchTables);
    }
    else
    {
        // If this function is being called without a valid instance handle (which happens by the loader when it first
        // loads the ICD), use the global dispatch table which has the bare minimal plain entry points required by the
        // spec to create an instance and enumerate its properties.
        tableCount        = 1;
        dispatchTables[0] = vk::entry::g_GlobalDispatchTable;
    }

    return reinterpret_cast<PFN_vkVoidFunction>(GetIcdProcAddr(pInstance, pDevice, tableCount, dispatchTables, pName));
}

namespace entry
{

// Helper macro used to create an entry for the "primary" entry point implementation (i.e. the one that goes straight
// to the driver, unmodified.
#define PRIMARY_DISPATCH_ENTRY(entry_name) VK_DISPATCH_ENTRY(entry_name, vk::entry::entry_name)
#define PRIMARY_DISPATCH_ALIAS(alias_name, ext_suffix) \
    VK_DISPATCH_ALIAS(alias_name, alias_name##ext_suffix, vk::entry::alias_name##ext_suffix)

// These are the entry points that are legal to query from the driver with a NULL instance handle (See Table 3.1 of the
// Vulkan specification).  They are queried by the loader before creating any instances, and therefore we can not or
// should not specialize their function pointer based on any panel setting, etc..
const DispatchTableEntry g_GlobalDispatchTable[] =
{
    PRIMARY_DISPATCH_ENTRY(vkCreateInstance),
    PRIMARY_DISPATCH_ENTRY(vkEnumerateInstanceExtensionProperties),
    PRIMARY_DISPATCH_ENTRY(vkEnumerateInstanceLayerProperties),
#ifdef ICD_VULKAN_1_1
    PRIMARY_DISPATCH_ENTRY(vkEnumerateInstanceVersion),
#endif
    VK_DISPATCH_TABLE_END()
};

// These are the entries of the "standard" dispatch table.  They are the ones containing the real driver
// implementations running under "normal" driver behavior.  The GetProcAddr() function accesses the given VkInstance's
// dispatch table, and most VkInstances will return a dispatch table with just these entries.  When under specific
// panel or registry settings though, such as developer mode driver enabled, we may shadow some of these entry points
// with different implementations.
const DispatchTableEntry g_StandardDispatchTable[] =
{
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceProcAddr                             ),
    PRIMARY_DISPATCH_ENTRY( vkAcquireNextImageKHR                           ),
    PRIMARY_DISPATCH_ENTRY( vkAllocateDescriptorSets                        ),
    PRIMARY_DISPATCH_ENTRY( vkAllocateMemory                                ),
    PRIMARY_DISPATCH_ENTRY( vkBeginCommandBuffer                            ),
    PRIMARY_DISPATCH_ENTRY( vkBindBufferMemory                              ),
    PRIMARY_DISPATCH_ENTRY( vkBindImageMemory                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBeginRenderPass                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBeginQuery                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBindDescriptorSets                         ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBindIndexBuffer                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBindPipeline                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBindVertexBuffers                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBlitImage                                  ),
    PRIMARY_DISPATCH_ENTRY( vkCmdClearAttachments                           ),
    PRIMARY_DISPATCH_ENTRY( vkCmdClearColorImage                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdClearDepthStencilImage                     ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyBuffer                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyBufferToImage                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyImage                                  ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyImageToBuffer                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyQueryPoolResults                       ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDraw                                       ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDrawIndexed                                ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDrawIndexedIndirect                        ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDrawIndirect                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDrawIndexedIndirectCountAMD                ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDrawIndirectCountAMD                       ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDispatch                                   ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDispatchIndirect                           ),
    PRIMARY_DISPATCH_ENTRY( vkCmdEndRenderPass                              ),
    PRIMARY_DISPATCH_ENTRY( vkCmdEndQuery                                   ),
    PRIMARY_DISPATCH_ENTRY( vkCmdExecuteCommands                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdFillBuffer                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdNextSubpass                                ),
    PRIMARY_DISPATCH_ENTRY( vkCmdPipelineBarrier                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdPushConstants                              ),
    PRIMARY_DISPATCH_ENTRY( vkCmdResetEvent                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdResetQueryPool                             ),
    PRIMARY_DISPATCH_ENTRY( vkCmdResolveImage                               ),

    PRIMARY_DISPATCH_ENTRY( vkCmdSetBlendConstants                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetDepthBias                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetDepthBounds                             ),

    PRIMARY_DISPATCH_ENTRY( vkCmdSetEvent                                   ),

    PRIMARY_DISPATCH_ENTRY( vkCmdSetLineWidth                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetScissor                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetStencilCompareMask                      ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetStencilReference                        ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetStencilWriteMask                        ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetViewport                                ),

    PRIMARY_DISPATCH_ENTRY( vkCmdUpdateBuffer                               ),
    PRIMARY_DISPATCH_ENTRY( vkCmdWaitEvents                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCmdWriteTimestamp                             ),

    PRIMARY_DISPATCH_ENTRY( vkCreateBuffer                                  ),
    PRIMARY_DISPATCH_ENTRY( vkCreateBufferView                              ),
    PRIMARY_DISPATCH_ENTRY( vkAllocateCommandBuffers                        ),
    PRIMARY_DISPATCH_ENTRY( vkCreateCommandPool                             ),
    PRIMARY_DISPATCH_ENTRY( vkCreateComputePipelines                        ),
    PRIMARY_DISPATCH_ENTRY( vkCreateDescriptorPool                          ),
    PRIMARY_DISPATCH_ENTRY( vkCreateDescriptorSetLayout                     ),
    PRIMARY_DISPATCH_ENTRY( vkCreateDevice                                  ),

    PRIMARY_DISPATCH_ENTRY( vkCreateEvent                                   ),
    PRIMARY_DISPATCH_ENTRY( vkCreateFence                                   ),
    PRIMARY_DISPATCH_ENTRY( vkCreateFramebuffer                             ),
    PRIMARY_DISPATCH_ENTRY( vkCreateGraphicsPipelines                       ),
    PRIMARY_DISPATCH_ENTRY( vkCreateImage                                   ),
    PRIMARY_DISPATCH_ENTRY( vkCreateImageView                               ),
    PRIMARY_DISPATCH_ENTRY( vkCreateInstance                                ),
    PRIMARY_DISPATCH_ENTRY( vkCreatePipelineLayout                          ),
    PRIMARY_DISPATCH_ENTRY( vkCreatePipelineCache                           ),
    PRIMARY_DISPATCH_ENTRY( vkCreateQueryPool                               ),
    PRIMARY_DISPATCH_ENTRY( vkCreateRenderPass                              ),
    PRIMARY_DISPATCH_ENTRY( vkCreateSampler                                 ),
    PRIMARY_DISPATCH_ENTRY( vkCreateSemaphore                               ),
    PRIMARY_DISPATCH_ENTRY( vkCreateShaderModule                            ),
    PRIMARY_DISPATCH_ENTRY( vkCreateSwapchainKHR                            ),
    PRIMARY_DISPATCH_ENTRY( vkDestroySurfaceKHR                             ),
    PRIMARY_DISPATCH_ENTRY( vkCreateXcbSurfaceKHR                           ),
    PRIMARY_DISPATCH_ENTRY( vkCreateXlibSurfaceKHR                          ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceXcbPresentationSupportKHR    ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceXlibPresentationSupportKHR   ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyBuffer                                 ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyBufferView                             ),
    PRIMARY_DISPATCH_ENTRY( vkFreeCommandBuffers                            ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyCommandPool                            ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyDescriptorPool                         ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyDescriptorSetLayout                    ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyDevice                                 ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyEvent                                  ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyFence                                  ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyFramebuffer                            ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyImage                                  ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyImageView                              ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyInstance                               ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyPipeline                               ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyPipelineCache                          ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyPipelineLayout                         ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyQueryPool                              ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyRenderPass                             ),
    PRIMARY_DISPATCH_ENTRY( vkDestroySampler                                ),
    PRIMARY_DISPATCH_ENTRY( vkDestroySemaphore                              ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyShaderModule                           ),
    PRIMARY_DISPATCH_ENTRY( vkDestroySwapchainKHR                           ),

    PRIMARY_DISPATCH_ENTRY( vkDeviceWaitIdle                                ),
    PRIMARY_DISPATCH_ENTRY( vkEndCommandBuffer                              ),
    PRIMARY_DISPATCH_ENTRY( vkEnumeratePhysicalDevices                      ),
    PRIMARY_DISPATCH_ENTRY( vkFlushMappedMemoryRanges                       ),
    PRIMARY_DISPATCH_ENTRY( vkFreeDescriptorSets                            ),
    PRIMARY_DISPATCH_ENTRY( vkFreeMemory                                    ),
    PRIMARY_DISPATCH_ENTRY( vkGetBufferMemoryRequirements                   ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceMemoryCommitment                     ),
    PRIMARY_DISPATCH_ENTRY( vkGetInstanceProcAddr                           ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceProcAddr                             ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceQueue                                ),
    PRIMARY_DISPATCH_ENTRY( vkGetEventStatus                                ),
    PRIMARY_DISPATCH_ENTRY( vkGetFenceStatus                                ),

    PRIMARY_DISPATCH_ENTRY( vkEnumerateInstanceExtensionProperties          ),
    PRIMARY_DISPATCH_ENTRY( vkEnumerateInstanceLayerProperties              ),
    PRIMARY_DISPATCH_ENTRY( vkEnumerateDeviceExtensionProperties            ),
    PRIMARY_DISPATCH_ENTRY( vkEnumerateDeviceLayerProperties                ),

    PRIMARY_DISPATCH_ENTRY( vkGetImageMemoryRequirements                    ),
    PRIMARY_DISPATCH_ENTRY( vkGetImageSparseMemoryRequirements              ),
    PRIMARY_DISPATCH_ENTRY( vkGetImageSubresourceLayout                     ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceFeatures                     ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceFormatProperties             ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceImageFormatProperties        ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceMemoryProperties             ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceProperties                   ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceQueueFamilyProperties        ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSparseImageFormatProperties  ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfaceSupportKHR            ),
    PRIMARY_DISPATCH_ENTRY( vkGetPipelineCacheData                          ),
    PRIMARY_DISPATCH_ENTRY( vkGetQueryPoolResults                           ),
    PRIMARY_DISPATCH_ENTRY( vkGetRenderAreaGranularity                      ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfaceCapabilitiesKHR       ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfaceCapabilities2KHR      ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfaceFormatsKHR            ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfaceFormats2KHR           ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSurfacePresentModesKHR       ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDevicePresentRectanglesKHX         ),

    PRIMARY_DISPATCH_ENTRY( vkGetSwapchainImagesKHR                         ),

    PRIMARY_DISPATCH_ENTRY( vkInvalidateMappedMemoryRanges                  ),
    PRIMARY_DISPATCH_ENTRY( vkMapMemory                                     ),
    PRIMARY_DISPATCH_ENTRY( vkMergePipelineCaches                           ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceFeatures2KHR                 ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceProperties2KHR               ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceFormatProperties2KHR         ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceImageFormatProperties2KHR    ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceQueueFamilyProperties2KHR    ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceMemoryProperties2KHR         ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceSparseImageFormatProperties2KHR ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceExternalBufferPropertiesKHR  ),

    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceExternalSemaphorePropertiesKHR ),
    PRIMARY_DISPATCH_ENTRY( vkGetMemoryFdPropertiesKHR                      ),
    PRIMARY_DISPATCH_ENTRY( vkGetMemoryFdKHR                                ),
    PRIMARY_DISPATCH_ENTRY( vkImportSemaphoreFdKHR                          ),
    PRIMARY_DISPATCH_ENTRY( vkGetSemaphoreFdKHR                             ),
    PRIMARY_DISPATCH_ENTRY( vkGetFenceFdKHR                                 ),
    PRIMARY_DISPATCH_ENTRY( vkImportFenceFdKHR                              ),

    PRIMARY_DISPATCH_ENTRY( vkBindBufferMemory2KHR                          ),
    PRIMARY_DISPATCH_ENTRY( vkBindImageMemory2KHR                           ),

    PRIMARY_DISPATCH_ENTRY( vkCreateDescriptorUpdateTemplateKHR             ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyDescriptorUpdateTemplateKHR            ),
    PRIMARY_DISPATCH_ENTRY( vkUpdateDescriptorSetWithTemplateKHR            ),

    PRIMARY_DISPATCH_ENTRY( vkAcquireNextImage2KHX                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDispatchBaseKHX                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetDeviceMaskKHX                           ),
    PRIMARY_DISPATCH_ENTRY( vkEnumeratePhysicalDeviceGroupsKHX              ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupPeerMemoryFeaturesKHX           ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupPresentCapabilitiesKHX          ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupSurfacePresentModesKHX          ),

#ifdef ICD_VULKAN_1_1
    PRIMARY_DISPATCH_ENTRY( vkAcquireNextImage2KHR                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDispatchBaseKHR                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdSetDeviceMaskKHR                           ),
    PRIMARY_DISPATCH_ENTRY( vkEnumeratePhysicalDeviceGroupsKHR              ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupPeerMemoryFeaturesKHR           ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupPresentCapabilitiesKHR          ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceGroupSurfacePresentModesKHR          ),
#endif

    PRIMARY_DISPATCH_ENTRY( vkQueueBindSparse                               ),
    PRIMARY_DISPATCH_ENTRY( vkQueuePresentKHR                               ),
    PRIMARY_DISPATCH_ENTRY( vkQueueSubmit                                   ),
    PRIMARY_DISPATCH_ENTRY( vkQueueWaitIdle                                 ),
    PRIMARY_DISPATCH_ENTRY( vkResetCommandBuffer                            ),
    PRIMARY_DISPATCH_ENTRY( vkResetCommandPool                              ),
    PRIMARY_DISPATCH_ENTRY( vkResetDescriptorPool                           ),
    PRIMARY_DISPATCH_ENTRY( vkResetEvent                                    ),
    PRIMARY_DISPATCH_ENTRY( vkResetFences                                   ),
    PRIMARY_DISPATCH_ENTRY( vkSetEvent                                      ),
    PRIMARY_DISPATCH_ENTRY( vkTrimCommandPoolKHR                            ),
    PRIMARY_DISPATCH_ENTRY( vkUnmapMemory                                   ),
    PRIMARY_DISPATCH_ENTRY( vkUpdateDescriptorSets                          ),
    PRIMARY_DISPATCH_ENTRY( vkWaitForFences                                 ),
    PRIMARY_DISPATCH_ENTRY( vkGetShaderInfoAMD                              ),

    PRIMARY_DISPATCH_ENTRY( vkCmdDebugMarkerBeginEXT                        ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDebugMarkerEndEXT                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdDebugMarkerInsertEXT                       ),
    PRIMARY_DISPATCH_ENTRY( vkDebugMarkerSetObjectTagEXT                    ),
    PRIMARY_DISPATCH_ENTRY( vkDebugMarkerSetObjectNameEXT                   ),

    PRIMARY_DISPATCH_ENTRY( vkCreateGpaSessionAMD                           ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyGpaSessionAMD                          ),
    PRIMARY_DISPATCH_ENTRY( vkSetGpaDeviceClockModeAMD                      ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBeginGpaSessionAMD                         ),
    PRIMARY_DISPATCH_ENTRY( vkCmdEndGpaSessionAMD                           ),
    PRIMARY_DISPATCH_ENTRY( vkCmdBeginGpaSampleAMD                          ),
    PRIMARY_DISPATCH_ENTRY( vkCmdEndGpaSampleAMD                            ),
    PRIMARY_DISPATCH_ENTRY( vkGetGpaSessionStatusAMD                        ),
    PRIMARY_DISPATCH_ENTRY( vkGetGpaSessionResultsAMD                       ),
    PRIMARY_DISPATCH_ENTRY( vkResetGpaSessionAMD                            ),
    PRIMARY_DISPATCH_ENTRY( vkCmdCopyGpaSessionResultsAMD                   ),
    PRIMARY_DISPATCH_ENTRY( vkGetImageMemoryRequirements2KHR                ),
    PRIMARY_DISPATCH_ENTRY( vkGetBufferMemoryRequirements2KHR               ),
    PRIMARY_DISPATCH_ENTRY( vkGetImageSparseMemoryRequirements2KHR          ),

    PRIMARY_DISPATCH_ENTRY( vkCmdSetSampleLocationsEXT                      ),
    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceMultisamplePropertiesEXT     ),

#ifdef ICD_VULKAN_1_1
    PRIMARY_DISPATCH_ENTRY( vkGetDescriptorSetLayoutSupportKHR              ),
#endif

    PRIMARY_DISPATCH_ENTRY( vkGetPhysicalDeviceExternalFencePropertiesKHR   ),
#ifdef ICD_VULKAN_1_1
    PRIMARY_DISPATCH_ENTRY( vkEnumerateInstanceVersion                      ),

    PRIMARY_DISPATCH_ALIAS( vkBindBufferMemory2                             , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkBindImageMemory2                              , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkCmdSetDeviceMask                              , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkCmdDispatchBase                               , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkCreateDescriptorUpdateTemplate                , KHR ),
    PRIMARY_DISPATCH_ENTRY( vkCreateSamplerYcbcrConversion                  ),
    PRIMARY_DISPATCH_ALIAS( vkDestroyDescriptorUpdateTemplate               , KHR ),
    PRIMARY_DISPATCH_ENTRY( vkDestroySamplerYcbcrConversion                 ),
    PRIMARY_DISPATCH_ALIAS( vkEnumeratePhysicalDeviceGroups                 , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetBufferMemoryRequirements2                  , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetDescriptorSetLayoutSupport                 , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetDeviceGroupPeerMemoryFeatures              , KHR ),
    PRIMARY_DISPATCH_ENTRY( vkGetDeviceQueue2                               ),
    PRIMARY_DISPATCH_ALIAS( vkGetImageMemoryRequirements2                   , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetImageSparseMemoryRequirements2             , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceExternalBufferProperties     , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceExternalFenceProperties      , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceExternalSemaphoreProperties  , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceFeatures2                    , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceFormatProperties2            , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceImageFormatProperties2       , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceMemoryProperties2            , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceProperties2                  , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceQueueFamilyProperties2       , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkGetPhysicalDeviceSparseImageFormatProperties2 , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkTrimCommandPool                               , KHR ),
    PRIMARY_DISPATCH_ALIAS( vkUpdateDescriptorSetWithTemplate               , KHR ),
#endif
    PRIMARY_DISPATCH_ENTRY( vkCreateDebugReportCallbackEXT                  ),
    PRIMARY_DISPATCH_ENTRY( vkDestroyDebugReportCallbackEXT                 ),
    PRIMARY_DISPATCH_ENTRY( vkDebugReportMessageEXT                         ),
    PRIMARY_DISPATCH_ENTRY( vkCmdWriteBufferMarkerAMD                             ),
    PRIMARY_DISPATCH_ENTRY( vkGetMemoryHostPointerPropertiesEXT                   ),

    VK_DISPATCH_TABLE_END()
};

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::GetIcdProcAddr(instance, VK_NULL_HANDLE, pName);
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetPhysicalDeviceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::GetIcdProcAddr(instance, VK_NULL_HANDLE, pName);
}

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName)
{
    return vk::GetIcdProcAddr(VK_NULL_HANDLE, device, pName);
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
    static constexpr uint32_t MaxDriverSupportedInterfaceVersion = 5;

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

} // namespace vk

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
#include "strings/g_func_table.cpp"
