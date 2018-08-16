/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_layer_switchable_graphics.cpp
 * @brief Contains implementation of Vulkan switchable graphics layer instance interfaces.
 ***********************************************************************************************************************
 */

#include <string.h>
#include "include/vk_alloccb.h"
#include "include/vk_object.h"
#include "include/vk_layer_switchable_graphics.h"
#include "include/query_dlist.h"

namespace vk
{

NextLinkFuncPointers g_nextLinkFuncs;

// =====================================================================================================================
// Implement vkGetInstanceProcAddr for implicit instance layer VK_LAYER_AMD_switchable_graphics, the layer dispatch
// table provides only two instance APIs, for the other instance APIs, call next link's function pointer
// pfnGetInstanceProcAddr to have a pass through implement since we don't want to do anything special in these APIs
static PFN_vkVoidFunction GetInstanceProcAddrSG(
    VkInstance  instance,
    VkDevice    device,
    const char* pName)
{
    const LayerDispatchTableEntry* pTable = vk::entry::g_LayerDispatchTable_SG;
    void* pFunc = nullptr;
    bool found = false;

    // Find the API in the layer dispatch table at first
    for (const LayerDispatchTableEntry* pEntry = pTable; (found == false) && (pEntry->pName != 0); pEntry++)
    {
        if (strstr(pEntry->pName, pName) != nullptr)
        {
            found = true;
            pFunc = pEntry->pFunc;
            break;
        }
    }

    // If the API isn't found in layer dispatch table, then call next link's function pointer to have a pass through
    // implementation for the layer interface
    if (pFunc == nullptr)
    {
        pFunc = reinterpret_cast<void*>(g_nextLinkFuncs.pfnGetInstanceProcAddr(instance, pName));
    }

    return reinterpret_cast<PFN_vkVoidFunction>(pFunc);
}

namespace entry
{

// =====================================================================================================================
// Layer's implementation for vkCreateInstance, call next link's vkCreateInstance and store the next link's dispatch
// table function pointers that we need in this layer
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance_SG(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VK_ASSERT(pCreateInfo != nullptr);

    union
    {
        const VkStructHeader*       pHeader;
        const VkInstanceCreateInfo* pInstanceCreateInfo;
    };

    for (pInstanceCreateInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<int>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO:
        {
            VkLayerInstanceCreateInfo* pLayerInstanceCreateInfo =
                const_cast<VkLayerInstanceCreateInfo*>(reinterpret_cast<const VkLayerInstanceCreateInfo*>(pHeader));

            if (pLayerInstanceCreateInfo->function == VK_LAYER_LINK_INFO)
            {
                PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr =
                    pLayerInstanceCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;

                g_nextLinkFuncs.pfnCreateInstance =
                    reinterpret_cast<PFN_vkCreateInstance>(pfnGetInstanceProcAddr(*pInstance, "vkCreateInstance"));

                // Advance the link info for the next element on the chain
                pLayerInstanceCreateInfo->u.pLayerInfo = pLayerInstanceCreateInfo->u.pLayerInfo->pNext;

                result = g_nextLinkFuncs.pfnCreateInstance(pCreateInfo, pAllocator, pInstance);
                if (result == VK_SUCCESS)
                {
                    // Store the next link's dispatch table function pointers that we need in the layer
                    g_nextLinkFuncs.pfnGetInstanceProcAddr = pfnGetInstanceProcAddr;
                    g_nextLinkFuncs.pfnEnumeratePhysicalDevices =
                        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
                            pfnGetInstanceProcAddr(*pInstance, "vkEnumeratePhysicalDevices"));
                    g_nextLinkFuncs.pfnGetPhysicalDeviceProperties =
                        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
                            pfnGetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties"));
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Layer's implementation for vkEnumeratePhysicalDevices, call next link's vkEnumeratePhysicalDevices implementation,
// then adjust the returned physical devices result by checking hybrid graphics platform and querying Dlist interface
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
    VkResult result = VK_SUCCESS;
    const VkAllocationCallbacks* pAllocCb = &allocator::g_DefaultAllocCallback;
    VK_ASSERT(pAllocCb != nullptr);

    VK_ASSERT(pPhysicalDeviceCount != nullptr);
    uint32_t physicalDeviceCount = *pPhysicalDeviceCount;
    VkPhysicalDevice* pLayerPhysicalDevices = nullptr;

    if (pPhysicalDevices != nullptr)
    {
        void* pMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                physicalDeviceCount * sizeof(VkPhysicalDevice),
                                                sizeof(void*),
                                                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
        if (pMemory == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            pLayerPhysicalDevices = static_cast<VkPhysicalDevice*>(pMemory);
        }
    }

    // Call loader's terminator function into ICDs to get all the physical devices
    if (result == VK_SUCCESS)
    {
        result = g_nextLinkFuncs.pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, pLayerPhysicalDevices);
    }

    if (result == VK_SUCCESS)
    {
        bool isHybridGraphics = false;
        bool loaderCallNvLayerFirst = false;

        if (physicalDeviceCount > 1)
        {
            isHybridGraphics = IsHybridGraphicsSupported();
        }

        // For the case of HG I+A+eN and HG I+N+eA, loader will load both NV layer and AMD layer, the order of the two
        // layers is important to return correct physical device to loader and apps. If loader call NV layer first, the
        // call sequence should be like: loader -> NV layer -> AMD layer -> loader terminator function -> ICD function;
        // if loader call AMD layer first, the call sequence should be like: loader -> AMD layer -> NV layer -> loader
        // terminator function -> ICD function. The first layer called by loader needs to return the specified physical
        // device for HG platforms, AMD layer should add the following logic to support the case of loader call NV
        // first, in this case, AMD layer needs to report all physical devices to NV layer, then NV layer can have them
        // and then filter out the correct physical device and report it to loader and apps.
        if (isHybridGraphics && (physicalDeviceCount >= 3))
        {
            void* pTmpLayerPhysicalDeviceMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                    physicalDeviceCount * sizeof(VkPhysicalDevice),
                                                    sizeof(void*),
                                                    VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
            void* pPropertiesMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                    physicalDeviceCount * sizeof(VkPhysicalDeviceProperties),
                                                    sizeof(void*),
                                                    VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
            if ((pTmpLayerPhysicalDeviceMemory == nullptr) || (pPropertiesMemory == nullptr))
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                VkPhysicalDevice* pTmpLayerPhysicalDevice =
                    static_cast<VkPhysicalDevice*>(pTmpLayerPhysicalDeviceMemory);
                VkPhysicalDeviceProperties* pProperties = static_cast<VkPhysicalDeviceProperties*>(pPropertiesMemory);

                result = g_nextLinkFuncs.pfnEnumeratePhysicalDevices(instance,
                                                                     &physicalDeviceCount,
                                                                     pTmpLayerPhysicalDevice);
                if (result == VK_SUCCESS)
                {
                    for (uint32_t i = 0; i < physicalDeviceCount; i++)
                    {
                       g_nextLinkFuncs.pfnGetPhysicalDeviceProperties(pTmpLayerPhysicalDevice[i], &pProperties[i]);
                    }

                    // For HG I+N+A and HG I+A+N, the last physical device is always Intel device, if NVIDIA device
                    // is next to Intel device, then loader will call NV layer first
                    if ((pProperties[physicalDeviceCount - 1].vendorID == VENDOR_ID_INTEL) &&
                        (pProperties[physicalDeviceCount - 2].vendorID == VENDOR_ID_NVIDIA))
                    {
                        loaderCallNvLayerFirst = true;
                    }
                }
            }

            if (pTmpLayerPhysicalDeviceMemory != nullptr)
            {
                pAllocCb->pfnFree(pAllocCb->pUserData, pTmpLayerPhysicalDeviceMemory);
            }
            if (pPropertiesMemory != nullptr)
            {
                pAllocCb->pfnFree(pAllocCb->pUserData, pPropertiesMemory);
            }
        }

        if ((physicalDeviceCount >= 2) && (!loaderCallNvLayerFirst))
        {
            if (isHybridGraphics)
            {
                if (pPhysicalDevices != nullptr)
                {
                    pPhysicalDevices[0] = pLayerPhysicalDevices[0];

                    VkPhysicalDeviceProperties properties = {};
                    uint32_t vendorId = 0;
                    uint32_t deviceId = 0;
                    QueryOsSettingsForApplication(&vendorId, &deviceId);

                    for (uint32_t i = 0; i < physicalDeviceCount; i++)
                    {
                        g_nextLinkFuncs.pfnGetPhysicalDeviceProperties(pLayerPhysicalDevices[i], &properties);
                        if ((properties.vendorID == vendorId) && (properties.deviceID == deviceId))
                        {
                            pPhysicalDevices[0] = pLayerPhysicalDevices[i];
                            break;
                        }
                    }
                }

                // Always report physical device count as 1 for hybrid graphics when loader call AMD layer first
                *pPhysicalDeviceCount = 1;
            }
        }

        if (!isHybridGraphics || loaderCallNvLayerFirst)
        {
            // If it is not HG platform or loader call NV layer at first, report all the physical devices to
            // upper layer or loader
            *pPhysicalDeviceCount = physicalDeviceCount;
            if (pPhysicalDevices != nullptr)
            {
                for (uint32_t i = 0; i < physicalDeviceCount; i++)
                {
                    pPhysicalDevices[i] = pLayerPhysicalDevices[i];
                }
            }
        }
    }

    if (pPhysicalDevices != nullptr)
    {
        pAllocCb->pfnFree(pAllocCb->pUserData, pLayerPhysicalDevices);
    }

    return result;
}

// Helper macro used to create an entry for the "primary" entry point implementation (i.e. the one that goes straight
// to the driver, unmodified.
#define LAYER_DISPATCH_ENTRY(entry_name) VK_LAYER_DISPATCH_ENTRY(entry_name, vk::entry::entry_name)

// Implicit layer VK_LAYER_AMD_switchable_graphics dispatch table
const LayerDispatchTableEntry g_LayerDispatchTable_SG[] =
{
    LAYER_DISPATCH_ENTRY(vkCreateInstance_SG),
    LAYER_DISPATCH_ENTRY(vkEnumeratePhysicalDevices_SG),
    VK_LAYER_DISPATCH_TABLE_END()
};

// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddrSG(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::GetInstanceProcAddrSG(instance, VK_NULL_HANDLE, pName);
}

} // namespace entry
} // namespace vk

extern "C"
{
// =====================================================================================================================
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddrSG(
    VkInstance                                  instance,
    const char*                                 pName)
{
    return vk::entry::vkGetInstanceProcAddrSG(instance, pName);
}
}
