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

#include "include/vk_debug_utils.h"
#include "include/vk_instance.h"
#include "include/vk_device.h"

namespace vk
{
// =====================================================================================================================
// Create a DebugUtilsMessenger object.
VkResult DebugUtilsMessenger::Create(
    Instance*                                   pInstance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    VkResult result = VK_SUCCESS;

    void* pSystemMem = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(DebugUtilsMessenger),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pSystemMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) DebugUtilsMessenger();

        *pMessenger = DebugUtilsMessenger::HandleFromVoidPointer(pSystemMem);

        result = pInstance->RegisterDebugUtilsMessenger(DebugUtilsMessenger::ObjectFromHandle(*pMessenger));

        if (result == VK_SUCCESS)
        {
            DebugUtilsMessenger::ObjectFromHandle(*pMessenger)->m_createInfo = *pCreateInfo;
        }
        else
        {
            Util::Destructor(DebugUtilsMessenger::ObjectFromHandle(*pMessenger));
            pAllocator->pfnFree(pAllocator->pUserData, DebugUtilsMessenger::ObjectFromHandle(*pMessenger));
        }
    }

    return result;
}

// =====================================================================================================================
// Destroy a DebugReportCallback object.
void DebugUtilsMessenger::Destroy(
    Instance*                                 pInstance,
    const VkAllocationCallbacks*              pAllocator)
{
    pInstance->UnregisterDebugUtilsMessenger(this);

    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
// Get the message severity flags for this callback
VkDebugUtilsMessageSeverityFlagsEXT DebugUtilsMessenger::GetMessageSeverityFlags()
{
    return m_createInfo.messageSeverity;
}

// =====================================================================================================================
// Get the message type flags for this callback
VkDebugUtilsMessageTypeFlagsEXT DebugUtilsMessenger::GetMessageTypeFlags()
{
    return m_createInfo.messageType;
}

// =====================================================================================================================
// Get the external callback function pointer for this callback
PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessenger::GetCallbackFunc()
{
    return m_createInfo.pfnUserCallback;
}

// =====================================================================================================================
// Get the client-provided user data pointer for this callback
void* DebugUtilsMessenger::GetUserData()
{
    return m_createInfo.pUserData;
}

namespace entry
{
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pInstance->GetAllocCallbacks();

    return DebugUtilsMessenger::Create(pInstance, pCreateInfo, pAllocCB, pMessenger);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    VkDebugUtilsMessengerEXT                    messenger,
    const VkAllocationCallbacks*                pAllocator)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pInstance->GetAllocCallbacks();

    DebugUtilsMessenger::ObjectFromHandle(messenger)->Destroy(pInstance, pAllocCB);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
    VkInstance                                  instance,
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)
{
    Instance* pInstance = Instance::ObjectFromHandle(instance);

    pInstance->CallExternalMessengers(messageSeverity, messageTypes, pCallbackData);
}
} // namespace entry

} // namespace vk
