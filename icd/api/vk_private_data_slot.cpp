/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_private_data_slot.cpp
 * @brief Contains implementation of Vulkan private data slot object.
 ***********************************************************************************************************************
 */

#include "include/vk_private_data_slot.h"
#include "include/vk_device.h"

namespace vk
{
// =====================================================================================================================
VkResult PrivateDataSlotEXT::Create(
        Device*                                 pDevice,
        const VkPrivateDataSlotCreateInfoEXT*   pCreateInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkPrivateDataSlotEXT*                   pPrivateDataSlotEXT)
{
    PrivateDataSlotEXT*          pPrivate = nullptr;
    VkResult                     vkResult = VK_SUCCESS;

    void* pMemory = pDevice->AllocApiObject(
                        pAllocator,
                        sizeof(PrivateDataSlotEXT));

    if (pMemory != nullptr)
    {
        uint64  slotIndex  = 0;
        bool    isReserved = pDevice->ReserveFastPrivateDataSlot(&slotIndex);
        // set flag for Device create reserved private data slot request
        // m_index indicate the offset in object reserved memory
        // m_index > PrivateDataSlotRequestCount means this slot not reserved
        pPrivate = VK_PLACEMENT_NEW(pMemory) PrivateDataSlotEXT(
                pDevice,
                isReserved,
                slotIndex);

        *pPrivateDataSlotEXT = PrivateDataSlotEXT::HandleFromObject(pPrivate);
    }
    else
    {
        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return vkResult;
}

// =====================================================================================================================
void PrivateDataSlotEXT::Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator)
{
    Util::Destructor(this);
    pDevice->FreeApiObject(pAllocator, this);
}

// =====================================================================================================================
PrivateDataSlotEXT::PrivateDataSlotEXT(
        Device*                         pDevice,
        const bool                      isReserved,
        const uint64                    index)
    :
    m_hashedPrivateData(32u, pDevice->VkInstance()->Allocator())
{
    m_hashedPrivateData.Init();

    m_index                 = index;
    m_isReserved            = isReserved;
}

// =====================================================================================================================
template <bool isSet>
uint64* PrivateDataSlotEXT::GetPrivateDataItemAddr(
        Device*                         pDevice,
        const VkObjectType              objectType,
        const uint64                    objectHandle)
{
    uint64* pItem = nullptr;

    switch (objectType)
    {
    case VK_OBJECT_TYPE_BUFFER:
    case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
    case VK_OBJECT_TYPE_BUFFER_VIEW:
    case VK_OBJECT_TYPE_IMAGE:
    case VK_OBJECT_TYPE_IMAGE_VIEW:
    case VK_OBJECT_TYPE_SEMAPHORE:
    case VK_OBJECT_TYPE_EVENT:
    case VK_OBJECT_TYPE_FENCE:
    case VK_OBJECT_TYPE_QUERY_POOL:
    case VK_OBJECT_TYPE_SAMPLER:
    case VK_OBJECT_TYPE_SHADER_MODULE:
    case VK_OBJECT_TYPE_PIPELINE_CACHE:
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
    case VK_OBJECT_TYPE_RENDER_PASS:
    case VK_OBJECT_TYPE_PIPELINE:
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
    case VK_OBJECT_TYPE_FRAMEBUFFER:
    case VK_OBJECT_TYPE_COMMAND_POOL:
    case VK_OBJECT_TYPE_COMMAND_BUFFER:
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
    case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
    {
        PrivateDataStorage* pPrivateDataStorage = reinterpret_cast<PrivateDataStorage*>(objectHandle - pDevice->GetPrivateDataTotalSize());

        if (m_isReserved)
        {
            pItem = &(pPrivateDataStorage->reserved[m_index]);
        }
        else
        {
            if (isSet)
            {
                Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> lock(pDevice->GetPrivateDataRWLock());

                HashedPrivateDataMap* pHashed = GetUnreservedPrivateDataAddr<isSet>(pDevice, pPrivateDataStorage);

                if (pHashed != nullptr)
                {
                    bool existed = false;
                    pHashed->FindAllocate(m_index, &existed, &pItem);
                }
            }
            else
            {
                Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> lock(pDevice->GetPrivateDataRWLock());

                HashedPrivateDataMap* pHashed = GetUnreservedPrivateDataAddr<isSet>(pDevice, pPrivateDataStorage);

                if (pHashed != nullptr)
                {
                    pItem = pHashed->FindKey(m_index);
                }
            }
        }
        break;
    }

    default:
        //This is a temporary path while incrementally add fast support for all objects.
        if (isSet)
        {
            Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> lock(pDevice->GetPrivateDataRWLock());

            bool existed = false;
            m_hashedPrivateData.FindAllocate(objectHandle, &existed, &pItem);
        }
        else
        {
            Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> lock(pDevice->GetPrivateDataRWLock());

            pItem = m_hashedPrivateData.FindKey(objectHandle);
        }

        break;
    }

    return pItem;
}

// =====================================================================================================================
VkResult PrivateDataSlotEXT::SetPrivateDataEXT(
        Device*                         pDevice,
        const VkObjectType              objectType,
        const uint64                    objectHandle,
        const uint64                    data)
{
    VkResult vkResult = VK_SUCCESS;
    uint64*  pItem     = GetPrivateDataItemAddr<true>(pDevice, objectType, objectHandle);

    if (pItem != nullptr)
    {
        *pItem = data;
    }
    else
    {
        vkResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return vkResult;
}

// =====================================================================================================================
void PrivateDataSlotEXT::GetPrivateDataEXT(
        Device*                         pDevice,
        const VkObjectType              objectType,
        const uint64                    objectHandle,
        uint64* const                   pData)
{
    uint64* pItem = GetPrivateDataItemAddr<false>(pDevice, objectType, objectHandle);

    if (pItem != nullptr)
    {
        *pData = *pItem;
    }
    else
    {
        *pData = 0;
    }
}

// =====================================================================================================================
// Caller should take a RWLock
template <bool isSet>
HashedPrivateDataMap* PrivateDataSlotEXT::GetUnreservedPrivateDataAddr(
        Device*                         pDevice,
        PrivateDataStorage* const       pPrivateDataStorage)
{
    HashedPrivateDataMap* pHashed = pPrivateDataStorage->pUnreserved;

    if ((pHashed == nullptr) && isSet)
    {
        void* pMemory = pDevice->VkInstance()->AllocMem(
                                            sizeof(HashedPrivateDataMap),
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory != nullptr)
        {
            pHashed = VK_PLACEMENT_NEW(pMemory) HashedPrivateDataMap(32u, pDevice->VkInstance()->Allocator());
            pHashed->Init();
            pPrivateDataStorage->pUnreserved = pHashed;
        }
    }

    return pHashed;
}

namespace entry
{
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePrivateDataSlotEXT(
    VkDevice                                device,
    const VkPrivateDataSlotCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkPrivateDataSlotEXT*                   pPrivateDataSlot)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return PrivateDataSlotEXT::Create(pDevice, pCreateInfo, pAllocCB, pPrivateDataSlot);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyPrivateDataSlotEXT(
    VkDevice                                device,
    VkPrivateDataSlotEXT                    privateDataSlot,
    const VkAllocationCallbacks*            pAllocator)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();
    PrivateDataSlotEXT*          pPrivate = PrivateDataSlotEXT::ObjectFromHandle(privateDataSlot);

    pPrivate->Destroy(pDevice, pAllocCB);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetPrivateDataEXT(
    VkDevice                                device,
    VkObjectType                            objectType,
    uint64_t                                objectHandle,
    VkPrivateDataSlotEXT                    privateDataSlot,
    uint64_t                                data)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    PrivateDataSlotEXT*          pPrivate = PrivateDataSlotEXT::ObjectFromHandle(privateDataSlot);

    return pPrivate->SetPrivateDataEXT(pDevice, objectType, objectHandle, data);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetPrivateDataEXT(
    VkDevice                                device,
    VkObjectType                            objectType,
    uint64_t                                objectHandle,
    VkPrivateDataSlotEXT                    privateDataSlot,
    uint64_t*                               pData)
{
    Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
    PrivateDataSlotEXT*          pPrivate = PrivateDataSlotEXT::ObjectFromHandle(privateDataSlot);

    return pPrivate->GetPrivateDataEXT(pDevice, objectType, objectHandle, pData);
}

} // namespace entry

} // namespace vk
