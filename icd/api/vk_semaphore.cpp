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

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_semaphore.h"
#include "include/vk_object.h"

#include "palQueueSemaphore.h"

namespace vk
{

// =====================================================================================================================
VkResult Semaphore::PopulateInDeviceGroup(
    Device*                         pDevice,
    Pal::IQueueSemaphore*           pPalSemaphores[MaxPalDevices],
    int32_t*                        pSemaphoreCount)
{
    Pal::Result palResult = Pal::Result::Success;
    int32_t count = 1;
    // Linux don't support LDA chain. The semaphore allocated from one device cannot be used directly
    // on Peer devices.
    // In order to support that, we have to create the semaphore in the first device and import the payload
    // as reference to all peer devices in the same device group.
    // the samething need to be applied to import operation as well.
    // Create Peer Semaphore Object if it is created in a device group.
    if (pDevice->NumPalDevices() > 1)
    {
        Pal::QueueSemaphoreExportInfo palExportInfo = {};
        // always import to peer device as reference.
        palExportInfo.flags.isReference = true;
        Pal::OsExternalHandle handle = pPalSemaphores[0]->ExportExternalHandle(palExportInfo);

        Pal::ExternalQueueSemaphoreOpenInfo palOpenInfo = {};
        palOpenInfo.externalSemaphore  = handle;
        palOpenInfo.flags.crossProcess = false;
        palOpenInfo.flags.isReference  = true;

        for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx ++)
        {
            size_t semaphoreSize = pDevice->PalDevice(1)->GetExternalSharedQueueSemaphoreSize(
                    palOpenInfo,
                    &palResult);

            void* pMemory = pDevice->VkInstance()->AllocMem(
                    semaphoreSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            if (pMemory)
            {
                palResult = pDevice->PalDevice(1)->OpenExternalSharedQueueSemaphore(
                        palOpenInfo,
                        pMemory,
                        &pPalSemaphores[deviceIdx]);

                if (palResult == Pal::Result::Success)
                {
                    count ++;
                }
                else
                {
                    pDevice->VkInstance()->FreeMem(pMemory);
                    pPalSemaphores[deviceIdx] = nullptr;
                    break;
                }
            }
        }

        // close the handle to avoid the resource leak.
        close(handle);

    }

    if (palResult != Pal::Result::Success)
    {
        for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx ++)
        {
            // clean up the allocated resources and return false;
            if (pPalSemaphores[deviceIdx] != nullptr)
            {
                pPalSemaphores[deviceIdx]->Destroy();
                pDevice->VkInstance()->FreeMem(pPalSemaphores[deviceIdx]);
                pPalSemaphores[deviceIdx] = nullptr;
            }
        }
    }
    else
    {
        *pSemaphoreCount = count;
    }
    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Creates a new queue semaphore object.
VkResult Semaphore::Create(
    Device*                         pDevice,
    const VkSemaphoreCreateInfo*    pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkSemaphore*                    pSemaphore)
{
    Pal::QueueSemaphoreCreateInfo palCreateInfo = {};
    palCreateInfo.maxCount = 1;

    Pal::QueueSemaphoreExportInfo exportInfo = {};

    // Allocate sufficient memory
    Pal::Result palResult;
    const size_t palSemaphoreSize = pDevice->PalDevice(DefaultDeviceIndex)->GetQueueSemaphoreSize(palCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);
    union
    {
        const VkStructHeader*                pHeader;
        const VkSemaphoreCreateInfo*         pInfo;
        const VkExportSemaphoreCreateInfo*   pExportSemaphoreCreateInfo;
    };
    for (pInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO:
            {
                break;
            }
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
            {
                // mark this semaphore as shareable.
                palCreateInfo.flags.shareable         = 1;
                break;
            }
        default:
            break;
        }
    }

    void* pMemory = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(Semaphore) + palSemaphoreSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    size_t palOffset = sizeof(Semaphore);

    // Create the PAL object
    Pal::IQueueSemaphore* pPalSemaphores[MaxPalDevices] = {nullptr};

    if (palResult == Pal::Result::Success)
    {
        palResult = pDevice->PalDevice(DefaultDeviceIndex)->CreateQueueSemaphore(
            palCreateInfo,
            Util::VoidPtrInc(pMemory, palOffset),
            &pPalSemaphores[0]);
    }

    if (palResult == Pal::Result::Success)
    {
        int32_t   semaphoreCount = 1;

        VkResult result = PopulateInDeviceGroup(pDevice, pPalSemaphores, &semaphoreCount);

        if (result == VK_SUCCESS)
        {
            Pal::OsExternalHandle handle = 0;
            // On success, construct the API object and return to the caller
            VK_PLACEMENT_NEW(pMemory) Semaphore(pPalSemaphores, semaphoreCount, palCreateInfo, handle);

            *pSemaphore = Semaphore::HandleFromVoidPointer(pMemory);

            return VK_SUCCESS;
        }
        else
        {
            palResult = Pal::Result::ErrorOutOfGpuMemory;
        }
    }

    // Something broke. Free the memory and return error.
    pAllocator->pfnFree(pAllocator->pUserData, pMemory);

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Get external handle from the semaphore object.
VkResult Semaphore::GetShareHandle(
    Device*                                     device,
    VkExternalSemaphoreHandleTypeFlagBits       handleType,
    Pal::OsExternalHandle*                      pHandle)
{
    PAL_ASSERT((handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
               (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT));

    Pal::QueueSemaphoreExportInfo palExportInfo = {};
    palExportInfo.flags.isReference = (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
    *pHandle = m_pPalSemaphores[0]->ExportExternalHandle(palExportInfo);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Import semaphore
VkResult Semaphore::ImportSemaphore(
    Device*                     pDevice,
    const ImportSemaphoreInfo&  importInfo)
{
    VkResult result       = VK_SUCCESS;
    Pal::Result palResult = Pal::Result::Success;

    Pal::ExternalQueueSemaphoreOpenInfo palOpenInfo = {};
    VkExternalSemaphoreHandleTypeFlags handleType   = importInfo.handleType;

    palOpenInfo.externalSemaphore  = importInfo.handle;
    palOpenInfo.flags.crossProcess = true;
    PAL_ASSERT((handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
               (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT));
    palOpenInfo.flags.isReference  = (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);

    //Todo: Check whether pDevice is the same as the one created the semaphore.

    // the placement new cause trouble here since we have no ways to fallback to original state if import failed!
    // therefore, a new memory is allocated for the palSemaphore object.
    size_t semaphoreSize = pDevice->PalDevice(DefaultDeviceIndex)->GetExternalSharedQueueSemaphoreSize(
                                                        palOpenInfo,
                                                        &palResult);
    if (palResult == Pal::Result::Success)
    {
        void* pMemory = pDevice->VkInstance()->AllocMem(
                    semaphoreSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory)
        {
            Pal::IQueueSemaphore* pPalSemaphores[MaxPalDevices] = { nullptr };

            palResult = pDevice->PalDevice(DefaultDeviceIndex)->OpenExternalSharedQueueSemaphore(
                    palOpenInfo,
                    pMemory,
                    &pPalSemaphores[0]);

            if (palResult == Pal::Result::Success)
            {
                m_palCreateInfo.flags.externalOpened    = 1;
                m_palCreateInfo.flags.sharedViaNtHandle = palOpenInfo.flags.sharedViaNtHandle;

                int32_t semaphoreCount = 1;

                result = PopulateInDeviceGroup(pDevice, pPalSemaphores, &semaphoreCount);

                if (result == VK_SUCCESS)
                {
                    if ((importInfo.importFlags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT))
                    {
                        SetPalTemporarySemaphore(pPalSemaphores, semaphoreCount, palOpenInfo.externalSemaphore);
                    }
                    else
                    {
                        m_pPalSemaphores[0]->Destroy();
                        m_pPalSemaphores[0]         = pPalSemaphores[0];
                        m_sharedSemaphoreTempHandle = palOpenInfo.externalSemaphore;

                        for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx ++)
                        {
                            if (m_pPalSemaphores[deviceIdx] != nullptr)
                            {
                                m_pPalSemaphores[deviceIdx]->Destroy();
                                pDevice->VkInstance()->FreeMem(m_pPalSemaphores[deviceIdx]);
                                m_pPalSemaphores[deviceIdx] = pPalSemaphores[deviceIdx];
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                else
                {
                    pDevice->VkInstance()->FreeMem(pMemory);
                }
            }
            else
            {
                result = PalToVkResult(palResult);
                pDevice->VkInstance()->FreeMem(pMemory);
            }
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    return result;
}

// =====================================================================================================================
// vkDestroyObject entry point for queue semaphore objects.
VkResult Semaphore::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{

    m_pPalSemaphores[0]->Destroy();

    for (uint32_t deviceIdx = 1; deviceIdx < pDevice->NumPalDevices(); deviceIdx ++)
    {
        if (m_pPalSemaphores[deviceIdx] != nullptr)
        {
            m_pPalSemaphores[deviceIdx]->Destroy();
            pDevice->VkInstance()->FreeMem(m_pPalSemaphores[deviceIdx]);
        }
        else
        {
            break;
        }
    }

    // the sempahore is imported from external
    if (Util::VoidPtrInc(this,sizeof(Semaphore)) != m_pPalSemaphores[0])
    {
        pDevice->VkInstance()->FreeMem(m_pPalSemaphores[0]);
    }

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult Semaphore::GetSemaphoreState(
    Device*                         pDevice,
    Semaphore*                      pSemaphore,
    uint64_t*                       pValue)
{
    Pal::Result palResult = Pal::Result::Success;
    Pal::IQueueSemaphore* pPalSemaphore = nullptr;

    if (pSemaphore != nullptr)
    {
        pPalSemaphore = pSemaphore->PalSemaphore(DefaultDeviceIndex);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        palResult = pPalSemaphore->QuerySemaphoreValue(pValue);
#endif
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult Semaphore::WaitSemaphoreValue(
    Device*                         pDevice,
    Semaphore*                      pSemaphore,
    uint64_t                        value,
    uint64_t                        timeout)
{
    Pal::Result palResult = Pal::Result::Success;

    Pal::IQueueSemaphore* pPalSemaphore = nullptr;

    if (pSemaphore != nullptr)
    {
        VK_ASSERT(pSemaphore->IsTimelineSemaphore());
        pPalSemaphore = pSemaphore->PalSemaphore(DefaultDeviceIndex);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        palResult = pPalSemaphore->WaitSemaphoreValue(value, timeout);
#endif
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
VkResult Semaphore::SignalSemaphoreValue(
    Device*                         pDevice,
    Semaphore*                      pSemaphore,
    uint64_t                        value)
{
    Pal::Result palResult = Pal::Result::Success;
    Pal::IQueueSemaphore* pPalSemaphore = nullptr;

    if (pSemaphore != nullptr)
    {
        pPalSemaphore = pSemaphore->PalSemaphore(DefaultDeviceIndex);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        palResult = pPalSemaphore->SignalSemaphoreValue(value);
#endif
    }

    return PalToVkResult(palResult);
}

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    const VkAllocationCallbacks*                pAllocator)
{
    if (semaphore != VK_NULL_HANDLE)
    {
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Semaphore::ObjectFromHandle(semaphore)->Destroy(pDevice, pAllocCB);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFdKHR(
    VkDevice                                    device,
    const VkSemaphoreGetFdInfoKHR*              pGetFdInfo,
    int*                                        pFd)
{
    return Semaphore::ObjectFromHandle(pGetFdInfo->semaphore)->GetShareHandle(
        ApiDevice::ObjectFromHandle(device),
        pGetFdInfo->handleType,
        reinterpret_cast<Pal::OsExternalHandle*>(pFd));
}

} // namespace entry

} // namespace vk
