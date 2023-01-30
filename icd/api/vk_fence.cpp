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
 * @file  vk_fence.cpp
 * @brief Contains implementation of Vulkan fence objects.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_fence.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"

#include "palFence.h"

namespace vk
{

// =====================================================================================================================
// Create a new fence object - implementation of vkCreateFence
VkResult Fence::Create(
    Device*                         pDevice,
    const VkFenceCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkFence*                        pFence)
{
    Instance* pInstance = pDevice->VkInstance();
    VK_ASSERT(pCreateInfo != nullptr);
    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);

    const VkExportFenceCreateInfo*         pVkExportCreateInfo = nullptr;

    Pal::FenceCreateInfo palFenceCreateInfo = {};
    palFenceCreateInfo.flags.signaled = (pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT) != 0;

    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
        {
            break;
        }
        default:
            VK_NOT_IMPLEMENTED;
            break;
        }

        pNext = pHeader->pNext;
    }
    const uint32_t numGroupedFences = pDevice->NumPalDevices();
    const uint32_t apiSize          = sizeof(Fence);
    const size_t   palSize          = pDevice->PalDevice(DefaultDeviceIndex)->GetFenceSize(nullptr);
    const size_t   totalSize        = apiSize + (palSize * numGroupedFences);

    // Allocate system memory
    void* pMemory = pDevice->AllocApiObject(pAllocator, totalSize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    Pal::IFence* pPalFences[MaxPalDevices] = {};

    // Allocate the PAL fence object

    void* pPalMemory = Util::VoidPtrInc(pMemory, apiSize);

    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
         (deviceIdx < numGroupedFences) && (palResult == Pal::Result::Success);
         deviceIdx++)
    {
        VK_ASSERT(palSize == pDevice->PalDevice(deviceIdx)->GetFenceSize(nullptr));

        palResult  = pDevice->PalDevice(deviceIdx)->CreateFence(palFenceCreateInfo, pPalMemory, &pPalFences[deviceIdx]);
        pPalMemory = Util::VoidPtrInc(pPalMemory, palSize);
    }

    if (palResult == Pal::Result::Success)
    {
        // On success, wrap it in an API object and return to application
        VK_PLACEMENT_NEW (pMemory) Fence(numGroupedFences, pPalFences, palFenceCreateInfo.flags.eventCanBeInherited);

        *pFence = Fence::HandleFromVoidPointer(pMemory);

        return VK_SUCCESS;
    }

    pDevice->FreeApiObject(pAllocator, pMemory);

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Destroy fence object
VkResult Fence::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    VK_ASSERT(m_groupedFenceCount == pDevice->NumPalDevices());

    RestoreFence(pDevice);

    for (uint32_t groupIdx = 0; groupIdx < m_groupedFenceCount; groupIdx++)
    {
        PalFence(groupIdx)->Destroy();
    }

    // Call my own destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
// Retrieve the status of a fence object
VkResult Fence::GetStatus(void)
{
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t deviceIdx = 0; (deviceIdx < m_groupedFenceCount) && (palResult == Pal::Result::Success); deviceIdx++)
    {
        // Some conformance tests will wait on fences that were never submitted, so use only the first device
        // for these cases.
        const bool forceWait = (m_activeDeviceMask == 0) && (deviceIdx == DefaultDeviceIndex);

        if (forceWait || ((m_activeDeviceMask & (1 << deviceIdx)) != 0))
        {
            palResult = PalFence(deviceIdx)->GetStatus();
        }
    }

    VkResult result = VK_SUCCESS;

    if (palResult == Pal::Result::Success)
    {
        result = VK_SUCCESS;
    }
    else if ((palResult == Pal::Result::ErrorUnavailable) ||
             (palResult == Pal::Result::NotReady)         ||
             (palResult == Pal::Result::ErrorFenceNeverSubmitted))
    {
        result = VK_NOT_READY;
    }
    else
    {
        result = PalToVkResult(palResult);
    }

    return result;
}

#if defined(__unix__)
// =====================================================================================================================
VkResult Fence::ImportFenceFd(
    Device*                         pDevice,
    const VkImportFenceFdInfoKHR*   pImportFenceFdInfo)
{
    VkResult result = VkResult::VK_SUCCESS;
    Pal::FenceOpenInfo openInfo = {};

    openInfo.externalFence = pImportFenceFdInfo->fd;
    // VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR: Reference
    // VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR: Copy
    openInfo.flags.isReference = (pImportFenceFdInfo->handleType & VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);

    bool isPermanence = (pImportFenceFdInfo->flags & VK_FENCE_IMPORT_TEMPORARY_BIT_KHR) == 0;

    m_flags.isOpened       = 1;
    m_flags.isPermanence   = isPermanence;
    m_flags.isReference    = openInfo.flags.isReference;
    Pal::IFence* pPalFence = PalFence(DefaultDeviceIndex);

    if (isPermanence)
    {
        pPalFence->Destroy();
        result = PalToVkResult(pDevice->PalDevice(DefaultDeviceIndex)->OpenFence(openInfo, pPalFence, &pPalFence));
    }
    else
    {
        void* pMemory = nullptr;

        if (m_pPalTemporaryFences != nullptr)
        {
            m_pPalTemporaryFences->Destroy();

            // Reuse the existing memory
            pMemory = m_pPalTemporaryFences;
        }
        else
        {
            const size_t palSize = pDevice->PalDevice(DefaultDeviceIndex)->GetFenceSize(nullptr);
            VkAllocationCallbacks* pAllocator = pDevice->VkInstance()->GetAllocCallbacks();

            // Allocate system memory
            pMemory = pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    palSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        }

        if (pMemory != nullptr)
        {
            // According to the spec,If handleType is VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
            // the special value -1 for fd is treated like a valid sync file descriptor referring
            // to an object that has already signaled.

            // Since -1 is an invalid fd, it can't be opened.
            // Therefore, create a signaled fence here to return to the application.
            if ((pImportFenceFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR) &&
                (static_cast<int32_t>(pImportFenceFdInfo->fd) == InvalidFd))
            {
                Pal::FenceCreateInfo palFenceCreateInfo = {};
                palFenceCreateInfo.flags.signaled = 1;

                result = PalToVkResult(pDevice->PalDevice(DefaultDeviceIndex)->CreateFence(
                    palFenceCreateInfo,
                    pMemory,
                    &m_pPalTemporaryFences));
            }
            else
            {
                result = PalToVkResult(pDevice->PalDevice(DefaultDeviceIndex)->OpenFence(
                    openInfo,
                    pMemory,
                    &m_pPalTemporaryFences));
            }
        }
        else
        {
            result = VkResult::VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return result;
}

// =====================================================================================================================
// Export the payload of a fence as fd. Currently only support OPAQUE_FD, will implement SYNC_FD export later.
VkResult Fence::GetFenceFd(
    Device*                         pDevice,
    const VkFenceGetFdInfoKHR*      pGetFdInfo,
    int*                            pFd)
{
    VK_ASSERT((pGetFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR) ||
              (pGetFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR));

    Pal::FenceExportInfo exportInfo = {};
    exportInfo.flags.isReference   = (pGetFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
    exportInfo.flags.implicitReset = (pGetFdInfo->handleType == VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR);
    *pFd  = PalFence(DefaultDeviceIndex)->ExportExternalHandle(exportInfo);

    return VkResult::VK_SUCCESS;
}
#endif

// =====================================================================================================================

// =====================================================================================================================
VkResult Fence::RestoreFence(
    const Device* pDevice)
{
    VkResult ret = VK_SUCCESS;

    if ((m_flags.isPermanence == 0) && m_flags.isOpened)
    {
        m_pPalTemporaryFences->Destroy();

        m_flags.isPermanence  = 1;
        m_flags.isOpened      = 0;

        VkAllocationCallbacks* pAllocator = pDevice->VkInstance()->GetAllocCallbacks();
        pAllocator->pfnFree(pAllocator->pUserData, m_pPalTemporaryFences);
        m_pPalTemporaryFences = nullptr;
    }

    return ret;
}

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(
    VkDevice                                    device,
    VkFence                                     fence)
{
    return Fence::ObjectFromHandle(fence)->GetStatus();
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(
    VkDevice                                    device,
    VkFence                                     fence,
    const VkAllocationCallbacks*                pAllocator)
{
    if (fence != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Fence::ObjectFromHandle(fence)->Destroy(pDevice, pAllocCB);
    }
}

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkImportFenceFdKHR(
    VkDevice                                    device,
    const VkImportFenceFdInfoKHR*               pImportFenceFdInfo)
{
    Device*    pDevice  = ApiDevice::ObjectFromHandle(device);

    return Fence::ObjectFromHandle(pImportFenceFdInfo->fence)->ImportFenceFd(pDevice, pImportFenceFdInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceFdKHR(
    VkDevice                                    device,
    const VkFenceGetFdInfoKHR*                  pGetFdInfo,
    int*                                        pFd)
{
    Device*    pDevice  = ApiDevice::ObjectFromHandle(device);

    return Fence::ObjectFromHandle(pGetFdInfo->fence)->GetFenceFd(pDevice, pGetFdInfo, pFd);
}
#endif

} // namespace entry

} // namespace vk
