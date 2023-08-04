/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_utils.h"
#include "include/vk_layer_switchable_graphics.h"
#include "include/query_dlist.h"
#include "palHashMapImpl.h"
#include "palMutex.h"

namespace vk
{

DispatchTableHashMap* g_pDispatchTables;
Util::Mutex g_traceMutex;

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
        g_traceMutex.Lock();
        NextLinkFuncPointers nextLinkFuncs = *(g_pDispatchTables->FindKey(instance));
        g_traceMutex.Unlock();
        pFunc = reinterpret_cast<void*>(nextLinkFuncs.pfnGetInstanceProcAddr(instance, pName));
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

                PFN_vkCreateInstance pfnCreateInstance =
                    reinterpret_cast<PFN_vkCreateInstance>(pfnGetInstanceProcAddr(*pInstance, "vkCreateInstance"));

                // Advance the link info for the next element on the chain
                pLayerInstanceCreateInfo->u.pLayerInfo = pLayerInstanceCreateInfo->u.pLayerInfo->pNext;

                result = pfnCreateInstance(pCreateInfo, pAllocator, pInstance);
                if (result == VK_SUCCESS)
                {
                    Util::MutexAuto lock(&g_traceMutex);
                    const VkAllocationCallbacks* pAllocCb = &allocator::g_DefaultAllocCallback;
                    static PalAllocator palAllocater(const_cast<VkAllocationCallbacks*>(pAllocCb));
                    static DispatchTableHashMap dispatchTables(32, &palAllocater);
                    g_pDispatchTables = &dispatchTables;

                    // Initialize HashMap once
                    static bool isInitialized = false;
                    if (!isInitialized)
                    {
                        g_pDispatchTables->Init();
                        isInitialized = true;
                    }

                    // Temporary store the next link's dispatch table
                    NextLinkFuncPointers nextLinkFuncs;

                    // Store the next link's dispatch table function pointers that we need in the layer
                    nextLinkFuncs.pfnGetInstanceProcAddr = pfnGetInstanceProcAddr;
                    nextLinkFuncs.pfnCreateInstance = pfnCreateInstance;
                    nextLinkFuncs.pfnDestroyInstance =
                        reinterpret_cast<PFN_vkDestroyInstance>(
                            pfnGetInstanceProcAddr(*pInstance, "vkDestroyInstance"));
                    nextLinkFuncs.pfnEnumeratePhysicalDevices =
                        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
                            pfnGetInstanceProcAddr(*pInstance, "vkEnumeratePhysicalDevices"));
                    nextLinkFuncs.pfnGetPhysicalDeviceProperties =
                        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
                            pfnGetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties"));
                    nextLinkFuncs.pfnEnumeratePhysicalDeviceGroups =
                        reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(
                            pfnGetInstanceProcAddr(*pInstance, "vkEnumeratePhysicalDeviceGroups"));
                    nextLinkFuncs.pfnEnumeratePhysicalDeviceGroupsKHR =
                        reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroupsKHR>(
                            pfnGetInstanceProcAddr(*pInstance, "vkEnumeratePhysicalDeviceGroupsKHR"));

                    // Store the next link's dispatch table to the hashmap
                    g_pDispatchTables->Insert(*pInstance, nextLinkFuncs);
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

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance_SG(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator)
{
    Util::MutexAuto lock(&g_traceMutex);
    NextLinkFuncPointers nextLinkFuncs = *g_pDispatchTables->FindKey(instance);
    nextLinkFuncs.pfnDestroyInstance(instance, pAllocator);
    g_pDispatchTables->Erase(instance);
}

// =====================================================================================================================
// Layer's implementation for vkEnumeratePhysicalDevices, call next link's vkEnumeratePhysicalDevices implementation,
// then adjust the returned physical devices result by checking hybrid graphics platform and querying Dlist interface
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
    g_traceMutex.Lock();
    NextLinkFuncPointers nextLinkFuncs = *g_pDispatchTables->FindKey(instance);
    g_traceMutex.Unlock();
    VkResult result = VK_SUCCESS;
    const VkAllocationCallbacks* pAllocCb = &allocator::g_DefaultAllocCallback;
    VK_ASSERT(pAllocCb != nullptr);

    VK_ASSERT(pPhysicalDeviceCount != nullptr);
    uint32_t physicalDeviceCount = 0;
    VkPhysicalDevice* pLayerPhysicalDevices = nullptr;

    {
        result = nextLinkFuncs.pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL);

        if (result == VK_SUCCESS)
        {
            void* pMemory = nullptr;

            if (physicalDeviceCount > 0)
            {
                pMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                physicalDeviceCount * sizeof(VkPhysicalDevice),
                                                sizeof(void*),
                                                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
            }

            if ((physicalDeviceCount > 0) && (pMemory == nullptr))
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
            result = nextLinkFuncs.pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, pLayerPhysicalDevices);
        }
#if defined(__unix__)

        if (result == VK_SUCCESS)
        {
            void* pPropertiesMemory = nullptr;

            if (physicalDeviceCount > 0)
            {
                // Allocate memory space to place the PhysicalDeviceProperties
                pPropertiesMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                            physicalDeviceCount * sizeof(VkPhysicalDeviceProperties),
                                                            sizeof(void*),
                                                            VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
            }

            VkPhysicalDeviceProperties* pProperties = static_cast<VkPhysicalDeviceProperties*>(pPropertiesMemory);

            if ((physicalDeviceCount > 0) && (pProperties == nullptr))
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                for (uint32_t i = 0; i < physicalDeviceCount; i++)
                {
                    nextLinkFuncs.pfnGetPhysicalDeviceProperties(pLayerPhysicalDevices[i], &pProperties[i]);
                }
            }

            if (result == VK_SUCCESS)
            {
                uint32_t returnedPhysicalDeviceCount = 0;
                uint32_t availablePhysicalDeviceCount = 0;

                // Return specified physical devices according to environment variable AMD_VULKAN_ICD
                const char* pEnv = getenv("AMD_VULKAN_ICD");
                bool preferRadv = pEnv && (strcmp(pEnv, "RADV") == 0);
                bool amdvlkExists = false;
                uint32_t nonAmdvlkCount = 0;

                for (uint32_t i = 0; i < physicalDeviceCount; i++)
                {
                    bool isAmd = (pProperties[i].vendorID == VENDOR_ID_AMD) || (pProperties[i].vendorID == VENDOR_ID_ATI);
                    bool isRadv = isAmd && strstr(pProperties[i].deviceName, "RADV") != nullptr;
                    bool isLlvmpipe = strstr(pProperties[i].deviceName, "llvmpipe") != nullptr;

                    if ((!isAmd || (isRadv == preferRadv)) && (!isLlvmpipe || preferRadv))
                    {
                        if (pPhysicalDevices != nullptr)
                        {
                            if (returnedPhysicalDeviceCount < *pPhysicalDeviceCount)
                            {
                                pPhysicalDevices[returnedPhysicalDeviceCount++] = pLayerPhysicalDevices[i];
                            }
                        }
                        else
                        {
                            returnedPhysicalDeviceCount++;
                        }

                        availablePhysicalDeviceCount++;
                    }

                    if (isAmd && !isRadv)
                    {
                        amdvlkExists = true;
                    }

                    if (isRadv || isLlvmpipe)
                    {
                        pLayerPhysicalDevices[nonAmdvlkCount++] = pLayerPhysicalDevices[i];
                    }
                }

                if (!amdvlkExists && !preferRadv)
                {
                    for (uint32_t i = 0; i < nonAmdvlkCount; i++)
                    {
                        if (pPhysicalDevices != nullptr)
                        {
                            if (returnedPhysicalDeviceCount < *pPhysicalDeviceCount)
                            {
                                pPhysicalDevices[returnedPhysicalDeviceCount++] = pLayerPhysicalDevices[i];
                            }
                        }
                        else
                        {
                            returnedPhysicalDeviceCount++;
                        }

                        availablePhysicalDeviceCount++;
                    }
                }
                *pPhysicalDeviceCount = returnedPhysicalDeviceCount;

                if ((pPhysicalDevices != nullptr) && (returnedPhysicalDeviceCount < availablePhysicalDeviceCount))
                {
                    result = VK_INCOMPLETE;
                }
            }

            if (pProperties != nullptr)
            {
                // Free previous allocated PhysicalDeviceProperties memory
                pAllocCb->pfnFree(pAllocCb->pUserData, pProperties);
            }
        }
#endif
        if (pLayerPhysicalDevices != nullptr)
        {
            pAllocCb->pfnFree(pAllocCb->pUserData, pLayerPhysicalDevices);
        }
    }

    return result;
}

// =====================================================================================================================
// General part for both vkEnumeratePhysicalDeviceGroups_SG and vkEnumeratePhysicalDeviceGroupsKHR_SG
static VkResult vkEnumeratePhysicalDeviceGroupsComm(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties,
    PFN_EnumPhysDeviceGroupsFunc                pEnumPhysDeviceGroupsFunc)
{
    g_traceMutex.Lock();
    NextLinkFuncPointers nextLinkFuncs = *g_pDispatchTables->FindKey(instance);
    g_traceMutex.Unlock();
    VkResult result = VK_SUCCESS;
    const VkAllocationCallbacks* pAllocCb = &allocator::g_DefaultAllocCallback;
    VK_ASSERT(pAllocCb != nullptr);

    VK_ASSERT(pPhysicalDeviceGroupCount != nullptr);
    uint32_t physicalDeviceGroupCount = *pPhysicalDeviceGroupCount;
    VkPhysicalDeviceGroupProperties* pLayerPhysicalDeviceGroups = nullptr;

    if ((pPhysicalDeviceGroupCount == 0) && (pPhysicalDeviceGroupProperties != nullptr))
    {
        result = pEnumPhysDeviceGroupsFunc(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    }
    else
    {
        // Get real device groups count at first
        result = pEnumPhysDeviceGroupsFunc(instance, &physicalDeviceGroupCount, nullptr);

        if (result == VK_SUCCESS)
        {
            void* pMemory = nullptr;

            if (physicalDeviceGroupCount > 0)
            {
                pMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                    physicalDeviceGroupCount * sizeof(VkPhysicalDeviceGroupProperties),
                    sizeof(void*),
                    VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
            }

            if ((physicalDeviceGroupCount > 0) && (pMemory == nullptr))
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                pLayerPhysicalDeviceGroups = static_cast<VkPhysicalDeviceGroupProperties*>(pMemory);
            }
        }

        // Call loader's terminator function into ICDs to get all the physical device groups
        if (result == VK_SUCCESS)
        {
            for (uint32_t i = 0; i < physicalDeviceGroupCount; i++)
            {
                pLayerPhysicalDeviceGroups[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
                pLayerPhysicalDeviceGroups[i].pNext = nullptr;
            }

            result = pEnumPhysDeviceGroupsFunc(instance, &physicalDeviceGroupCount, pLayerPhysicalDeviceGroups);
        }

        if (result == VK_SUCCESS)
        {
            bool processDevices = false;

            if (physicalDeviceGroupCount > 1)
            {

#if defined(__unix__)
                processDevices = true;
#endif
            }

            if (processDevices)
            {
                void* pTmpLayerPhysicalDeviceMemory = nullptr;
                void* pPropertiesMemory = nullptr;

                uint32_t physicalDeviceCount = 0;
                nextLinkFuncs.pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

                if (physicalDeviceCount > 0)
                {
                    pTmpLayerPhysicalDeviceMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                        physicalDeviceCount * sizeof(VkPhysicalDevice),
                        sizeof(void*),
                        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
                    pPropertiesMemory = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                        physicalDeviceCount * sizeof(VkPhysicalDeviceProperties),
                        sizeof(void*),
                        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
                }

                VkPhysicalDevice* pTmpLayerPhysicalDevice = static_cast<VkPhysicalDevice*>(pTmpLayerPhysicalDeviceMemory);
                VkPhysicalDeviceProperties* pProperties = static_cast<VkPhysicalDeviceProperties*>(pPropertiesMemory);

                if ((physicalDeviceCount > 0) && ((pTmpLayerPhysicalDevice == nullptr) || (pProperties == nullptr)))
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
                else
                {
                    // Call vkEnumeratePhysicalDevices_SG to get all valid physical devices, store all valid
                    // physical device properies in pProperties
                    result = vkEnumeratePhysicalDevices_SG(instance, &physicalDeviceCount, pTmpLayerPhysicalDevice);
                    if (result == VK_SUCCESS)
                    {
                        for (uint32_t i = 0; i < physicalDeviceCount; i++)
                        {
                            nextLinkFuncs.pfnGetPhysicalDeviceProperties(pTmpLayerPhysicalDevice[i], &pProperties[i]);
                        }
                    }
                }

                if (result == VK_SUCCESS)
                {
                    uint32_t deviceGroupCount = 0;

                    // Check the physical devices in device group, only report the device groups which contains valid
                    // physical devices
                    for (uint32_t i = 0; i < physicalDeviceGroupCount; i++)
                    {
                        VkPhysicalDeviceProperties properties = {};
                        nextLinkFuncs.pfnGetPhysicalDeviceProperties(
                            pLayerPhysicalDeviceGroups[i].physicalDevices[0], &properties);
                        for (uint32_t j = 0; j < physicalDeviceCount; j++)
                        {
                            if ((properties.vendorID == pProperties[j].vendorID) &&
                                (properties.deviceID == pProperties[j].deviceID) &&
                                (strcmp(properties.deviceName, pProperties[j].deviceName) == 0))
                            {
                                if ((pPhysicalDeviceGroupProperties != nullptr) &&
                                    (deviceGroupCount < *pPhysicalDeviceGroupCount))
                                {
                                    pPhysicalDeviceGroupProperties[deviceGroupCount] = pLayerPhysicalDeviceGroups[i];
                                }

                                deviceGroupCount++;
                                break;
                            }
                        }
                    }

                    if (pPhysicalDeviceGroupProperties != nullptr)
                    {
                        if (*pPhysicalDeviceGroupCount < deviceGroupCount)
                        {
                            result = VK_INCOMPLETE;
                        }
                        else
                        {
                            *pPhysicalDeviceGroupCount = deviceGroupCount;
                        }
                    }
                    else
                    {
                        *pPhysicalDeviceGroupCount = deviceGroupCount;
                    }
                }

                if (pTmpLayerPhysicalDevice != nullptr)
                {
                    pAllocCb->pfnFree(pAllocCb->pUserData, pTmpLayerPhysicalDevice);
                }

                if (pProperties != nullptr)
                {
                    pAllocCb->pfnFree(pAllocCb->pUserData, pProperties);
                }
            }
            else
            {
                if (pPhysicalDeviceGroupProperties != nullptr)
                {
                    if (*pPhysicalDeviceGroupCount < physicalDeviceGroupCount)
                    {
                        result = VK_INCOMPLETE;
                    }
                    else
                    {
                        *pPhysicalDeviceGroupCount = physicalDeviceGroupCount;
                    }

                    for (uint32_t i = 0; i < *pPhysicalDeviceGroupCount; i++)
                    {
                        pPhysicalDeviceGroupProperties[i] = pLayerPhysicalDeviceGroups[i];
                    }
                }
                else
                {
                    *pPhysicalDeviceGroupCount = physicalDeviceGroupCount;
                }
            }
        }

        if (pLayerPhysicalDeviceGroups != nullptr)
        {
            pAllocCb->pfnFree(pAllocCb->pUserData, pLayerPhysicalDeviceGroups);
        }
    }

    return result;
}

// =====================================================================================================================
// Layer's implementation for vkEnumeratePhysicalDeviceGroupsKHR, call next link's vkEnumeratePhysicalDeviceGroupsKHR,
// implementation, then adjust the returned physical device groups result by checking hybrid graphics platform
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroupsKHR_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties)
{
    g_traceMutex.Lock();
    PFN_EnumPhysDeviceGroupsFunc pEnumPhysDeviceGroupsFunc = (*(g_pDispatchTables->FindKey(instance))).pfnEnumeratePhysicalDeviceGroupsKHR;
    g_traceMutex.Unlock();
    return vkEnumeratePhysicalDeviceGroupsComm(instance,
                                               pPhysicalDeviceGroupCount,
                                               pPhysicalDeviceGroupProperties,
                                               pEnumPhysDeviceGroupsFunc);
}

// =====================================================================================================================
// Layer's implementation for vkEnumeratePhysicalDeviceGroups, call next link's vkEnumeratePhysicalDeviceGroups,
// implementation, then adjust the returned physical device groups result by checking hybrid graphics platform
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroups_SG(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties)
{
    g_traceMutex.Lock();
    PFN_EnumPhysDeviceGroupsFunc pEnumPhysDeviceGroupsFunc = (*(g_pDispatchTables->FindKey(instance))).pfnEnumeratePhysicalDeviceGroups;
    g_traceMutex.Unlock();
    return vkEnumeratePhysicalDeviceGroupsComm(instance,
                                               pPhysicalDeviceGroupCount,
                                               pPhysicalDeviceGroupProperties,
                                               pEnumPhysDeviceGroupsFunc);
}

// Helper macro used to create an entry for the "primary" entry point implementation (i.e. the one that goes straight
// to the driver, unmodified.
#define LAYER_DISPATCH_ENTRY(entry_name) VK_LAYER_DISPATCH_ENTRY(entry_name, vk::entry::entry_name)

// Implicit layer VK_LAYER_AMD_switchable_graphics dispatch table
const LayerDispatchTableEntry g_LayerDispatchTable_SG[] =
{
    LAYER_DISPATCH_ENTRY(vkCreateInstance_SG),
    LAYER_DISPATCH_ENTRY(vkDestroyInstance_SG),
    LAYER_DISPATCH_ENTRY(vkEnumeratePhysicalDevices_SG),
    LAYER_DISPATCH_ENTRY(vkEnumeratePhysicalDeviceGroups_SG),
    LAYER_DISPATCH_ENTRY(vkEnumeratePhysicalDeviceGroupsKHR_SG),
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
