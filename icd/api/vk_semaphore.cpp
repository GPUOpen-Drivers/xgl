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

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_semaphore.h"

#include "palQueueSemaphore.h"

namespace vk
{

// =====================================================================================================================
VkResult Semaphore::PopulateInDeviceGroup(
    Device*                         pDevice,
    Pal::IQueueSemaphore*           pPalSemaphores[MaxPalDevices],
    uint32_t*                       pSemaphoreCount)
{
    Pal::Result palResult = Pal::Result::Success;
    uint32_t count = 1;
#if defined(__unix__)
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
#endif
    {
        *pSemaphoreCount = count;
    }
    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Creates a new queue semaphore object.
VkResult Semaphore::Create(
    Device*                       pDevice,
    const VkSemaphoreCreateInfo*  pCreateInfo,
    const VkAllocationCallbacks*  pAllocator,
    VkSemaphore*                  pSemaphore)
{
    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);

    Pal::QueueSemaphoreCreateInfo palCreateInfo = {};
    palCreateInfo.maxCount = 1;

    Pal::QueueSemaphoreExportInfo exportInfo = {};
    // Allocate sufficient memory
    VkResult vkResult = VK_SUCCESS;
    Pal::Result palResult;
    const size_t palSemaphoreSize = pDevice->PalDevice(DefaultDeviceIndex)->GetQueueSemaphoreSize(palCreateInfo, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
        {
            const auto* pExtInfo = static_cast<const VkSemaphoreTypeCreateInfo*>(pNext);

            // Mark this semaphore is timeline or not
            palCreateInfo.flags.timeline = (pExtInfo->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE);
            palCreateInfo.initialCount   = pExtInfo->initialValue;

            break;
        }
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
        {
            const auto* pExtInfo = static_cast<const VkExportSemaphoreCreateInfo*>(pNext);

            // Mark this semaphore as shareable.
            palCreateInfo.flags.shareable = 1;
            break;
        }
        default:
            break;
        }

        pNext = pHeader->pNext;
    }
#if defined(__unix__)
    if (pDevice->NumPalDevices() > 1)
    {
        // mark this semaphore as shareable.
        palCreateInfo.flags.shareable = 1;
    }
#endif
    // Allocate memory for VK_Semaphore and palSemaphore separately
    void* pVKSemaphoreMemory = pDevice->AllocApiObject(
        pAllocator,
        sizeof(Semaphore));

        void* pPalSemaphoreMemory = pDevice->VkInstance()->AllocMem(
        palSemaphoreSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if ((pVKSemaphoreMemory != nullptr) && (pPalSemaphoreMemory != nullptr))
    {
        // Allocation Succeed. Create the PAL object
        Pal::IQueueSemaphore* pPalSemaphores[MaxPalDevices] = { nullptr };

        if (palResult == Pal::Result::Success)
        {
            palResult = pDevice->PalDevice(DefaultDeviceIndex)->CreateQueueSemaphore(
                palCreateInfo,
                pPalSemaphoreMemory,
                &pPalSemaphores[0]);
        }

        vkResult = PalToVkResult(palResult);

        if (vkResult == VK_SUCCESS)
        {
            uint32_t   semaphoreCount = 1;

            vkResult = PopulateInDeviceGroup(pDevice, pPalSemaphores, &semaphoreCount);

            if (vkResult == VK_SUCCESS)
            {
                Pal::OsExternalHandle handle = 0;
                // On success, construct the API object and return to the caller
                VK_PLACEMENT_NEW(pVKSemaphoreMemory) Semaphore(pPalSemaphores, semaphoreCount, palCreateInfo, handle);
                *pSemaphore = Semaphore::HandleFromVoidPointer(pVKSemaphoreMemory);

                vkResult = VK_SUCCESS;
            }
            else
            {
                vkResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
        }
    }
    else
    {
        //Allocation Failed.
        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (vkResult != VK_SUCCESS)
    {
        // Something broke. Free the memory.
        pDevice->FreeApiObject(pAllocator, pVKSemaphoreMemory);
        pDevice->VkInstance()->FreeMem(pPalSemaphoreMemory);
    }

    return vkResult;
}

// =====================================================================================================================
// Get external handle from the semaphore object.
VkResult Semaphore::GetShareHandle(
    Device*                                     device,
    VkExternalSemaphoreHandleTypeFlagBits       handleType,
    Pal::OsExternalHandle*                      pHandle)
{
#if defined(__unix__)
    PAL_ASSERT((handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
               (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT));

    Pal::QueueSemaphoreExportInfo palExportInfo = {};
    palExportInfo.flags.isReference = (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
    *pHandle = m_pPalSemaphores[0]->ExportExternalHandle(palExportInfo);
#endif

    return VK_SUCCESS;
}

// =====================================================================================================================
// Import semaphore
VkResult Semaphore::ImportSemaphore(
    Device*                     pDevice,
    const ImportSemaphoreInfo&  importInfo)
{
    VkResult vkResult       = VK_SUCCESS;
    Pal::Result palResult   = Pal::Result::Success;

    Pal::ExternalQueueSemaphoreOpenInfo   palOpenInfo = {};
    VkExternalSemaphoreHandleTypeFlagBits handleType  = importInfo.handleType;

    palOpenInfo.externalSemaphore  = importInfo.handle;
    palOpenInfo.flags.crossProcess = importInfo.crossProcess;
    palOpenInfo.flags.timeline     = m_palCreateInfo.flags.timeline;

#if defined(__unix__)
    PAL_ASSERT((handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) ||
               (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT));
    palOpenInfo.flags.isReference  = (handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
#endif

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
#if defined(__unix__)
            // According to the spec, If handleType is VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT, the special value -1
            // for fd is treated like a valid sync file descriptor referring to an object that has already signaled.

            // Since -1 is an invalid fd, it can't be opened.
            // Therefore, create a signaled semaphore here to return to the application.
            if ((handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT) &&
                (static_cast<int32_t>(importInfo.handle) == InvalidFd))

            {
                 Pal::QueueSemaphoreCreateInfo palCreateInfo = {};
                 palCreateInfo.flags.timeline = m_palCreateInfo.flags.timeline;

                 // Pal will check if this flag is 0 to determine if this semaphore create as signaled.
                 palCreateInfo.initialCount = 1;
                 palCreateInfo.flags.shareable = 1;

                 palResult = pDevice->PalDevice(DefaultDeviceIndex)->CreateQueueSemaphore(
                         palCreateInfo,
                         pMemory,
                         &pPalSemaphores[0]);
            }
            else
#endif
            {
                palResult = pDevice->PalDevice(DefaultDeviceIndex)->OpenExternalSharedQueueSemaphore(
                        palOpenInfo,
                        pMemory,
                        &pPalSemaphores[0]);
            }

            if (palResult == Pal::Result::Success)
            {
                uint32_t semaphoreCount = 1;

                vkResult = PopulateInDeviceGroup(pDevice, pPalSemaphores, &semaphoreCount);

                if (vkResult == VK_SUCCESS)
                {
                    DestroyTemporarySemaphore(pDevice);
                    if ((importInfo.importFlags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT))
                    {
                        m_useTempSemaphore = true;
                        SetTemporarySemaphore(pPalSemaphores, semaphoreCount, palOpenInfo.externalSemaphore);
                    }
                    else
                    {
                        DestroySemaphore(pDevice);
                        SetSemaphore(pPalSemaphores, semaphoreCount, palOpenInfo.externalSemaphore);
                    }
                }
                else
                {
                    pDevice->VkInstance()->FreeMem(pMemory);
                }
            }
            else
            {
                vkResult = PalToVkResult(palResult);
                pDevice->VkInstance()->FreeMem(pMemory);
            }
        }
        else
        {
            vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return vkResult;
}

// =====================================================================================================================
// vkDestroyObject entry point for queue semaphore objects.
void Semaphore::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    DestroyTemporarySemaphore(pDevice);
    DestroySemaphore(pDevice);
    Util::Destructor(this);
    pDevice->FreeApiObject(pAllocator, this);
}

// =====================================================================================================================
// Copy imported semaphore into m_pPalTemporarySemaphores
void Semaphore::SetTemporarySemaphore(
    Pal::IQueueSemaphore* pPalImportedSemaphore[],
    uint32_t              semaphoreCount,
    Pal::OsExternalHandle importedHandle)
{
    for (uint32_t i = 0; i < semaphoreCount; i++)
    {
        m_pPalTemporarySemaphores[i] = pPalImportedSemaphore[i];
    }

    m_sharedSemaphoreTempHandle = importedHandle;
}

// =====================================================================================================================
// Copy imported semaphore into m_pPalSemaphores
void Semaphore::SetSemaphore(
    Pal::IQueueSemaphore* pPalImportedSemaphore[],
    uint32_t              semaphoreCount,
    Pal::OsExternalHandle importedHandle)
{
    for (uint32_t i = 0; i < semaphoreCount; i++)
    {
        m_pPalSemaphores[i] = pPalImportedSemaphore[i];
    }

    m_sharedSemaphoreHandle = importedHandle;
}

// =====================================================================================================================
// Calling destructor, freeing memory and closing handle for temporary semaphore
void Semaphore::DestroyTemporarySemaphore(
    const Device*                pDevice)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalTemporarySemaphores[deviceIdx] != nullptr)
        {
            m_pPalTemporarySemaphores[deviceIdx]->Destroy();
            pDevice->VkInstance()->FreeMem(m_pPalTemporarySemaphores[deviceIdx]);
            m_pPalTemporarySemaphores[deviceIdx] = nullptr;
        }
    }

}

// =====================================================================================================================
// Calling destructor, freeing memory and closing handle for semaphore
void Semaphore::DestroySemaphore(
    const Device*                pDevice)
{
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (m_pPalSemaphores[deviceIdx] != nullptr)
        {
            m_pPalSemaphores[deviceIdx]->Destroy();
            pDevice->VkInstance()->FreeMem(m_pPalSemaphores[deviceIdx]);
            m_pPalSemaphores[deviceIdx] = nullptr;
        }
    }

}

// =====================================================================================================================
VkResult Semaphore::GetSemaphoreCounterValue(
    Device*                         pDevice,
    Semaphore*                      pSemaphore,
    uint64_t*                       pValue)
{
    Pal::Result palResult = Pal::Result::Success;
    Pal::IQueueSemaphore* pPalSemaphore = nullptr;

    if (pSemaphore != nullptr)
    {
        pPalSemaphore = pSemaphore->PalSemaphore(DefaultDeviceIndex);
        palResult = pPalSemaphore->QuerySemaphoreValue(pValue);
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
        pSemaphore->RestoreSemaphore();
        palResult = pPalSemaphore->WaitSemaphoreValue(value, timeout);
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
        palResult = pPalSemaphore->SignalSemaphoreValue(value);
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
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Semaphore::ObjectFromHandle(semaphore)->Destroy(pDevice, pAllocCB);
    }
}

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFdKHR(
    VkDevice                                    device,
    const VkSemaphoreGetFdInfoKHR*              pGetFdInfo,
    int*                                        pFd)
{
    Pal::OsExternalHandle handle = 0;

    VkResult result = Semaphore::ObjectFromHandle(pGetFdInfo->semaphore)->GetShareHandle(
        ApiDevice::ObjectFromHandle(device),
        pGetFdInfo->handleType,
        &handle);

    *pFd = static_cast<int>(handle);

    return result;
}
#endif

} // namespace entry

} // namespace vk
