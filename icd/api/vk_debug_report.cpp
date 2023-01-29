/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_debug_report.h"
#include "include/vk_instance.h"
#include "palDbgPrint.h"

namespace vk
{
// =====================================================================================================================
// Create a DebugReportCallback object.
VkResult DebugReportCallback::Create(
    Instance*                                 pInstance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks*              pAllocator,
    VkDebugReportCallbackEXT*                 pCallback)
{
    VkResult result = VK_SUCCESS;

    void* pSystemMem = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(DebugReportCallback),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pSystemMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) DebugReportCallback();

        *pCallback = DebugReportCallback::HandleFromVoidPointer(pSystemMem);

        result = pInstance->RegisterDebugCallback(DebugReportCallback::ObjectFromHandle(*pCallback));

        if (result == VK_SUCCESS)
        {
            DebugReportCallback::ObjectFromHandle(*pCallback)->m_createInfo = *pCreateInfo;
        }
        else
        {
            Util::Destructor(DebugReportCallback::ObjectFromHandle(*pCallback));
            pAllocator->pfnFree(pAllocator->pUserData, DebugReportCallback::ObjectFromHandle(*pCallback));
        }
    }

    return result;
}

// =====================================================================================================================
// Destroy a DebugReportCallback object.
void DebugReportCallback::Destroy(
    Instance*                                 pInstance,
    const VkAllocationCallbacks*              pAllocator)
{
    pInstance->UnregisterDebugCallback(this);

    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Get the flags for this callback
VkDebugReportFlagsEXT DebugReportCallback::GetFlags()
{
    return m_createInfo.flags;
}

// =====================================================================================================================
// Get the external callback function pointer for this callback
PFN_vkDebugReportCallbackEXT DebugReportCallback::GetCallbackFunc()
{
    return m_createInfo.pfnCallback;
}

// =====================================================================================================================
// Get the client-provided user data pointer for this callback
void* DebugReportCallback::GetUserData()
{
    return m_createInfo.pUserData;
}

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(
    VkInstance                                instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks*              pAllocator,
    VkDebugReportCallbackEXT*                 pCallback)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pInstance->GetAllocCallbacks();

    return DebugReportCallback::Create(pInstance, pCreateInfo, pAllocCB, pCallback);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(
    VkInstance                                instance,
    VkDebugReportCallbackEXT                  callback,
    const VkAllocationCallbacks*              pAllocator)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pInstance->GetAllocCallbacks();

    DebugReportCallback::ObjectFromHandle(callback)->Destroy(pInstance, pAllocCB);
}

VKAPI_ATTR void VKAPI_CALL vkDebugReportMessageEXT(
    VkInstance                                instance,
    VkDebugReportFlagsEXT                     flags,
    VkDebugReportObjectTypeEXT                objectType,
    uint64_t                                  object,
    size_t                                    location,
    int32_t                                   messageCode,
    const char*                               pLayerPrefix,
    const char*                               pMessage)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    pInstance->CallExternalCallbacks(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
}

} // namespace entry

} // namespace vk
