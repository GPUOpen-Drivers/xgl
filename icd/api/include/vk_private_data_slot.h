/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_private_data_slot.h
 * @brief Private data slot object related functionality for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_PRIVATE_DATA_SLOT_H__
#define __VK_PRIVATE_DATA_SLOT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"

#include "palHashMap.h"
#include "palHashMapImpl.h"
#include "palUtil.h"

using namespace Util;

namespace vk
{

typedef Util::HashMap<uint64, uint64, PalAllocator> HashedPrivateDataMap;

struct PrivateDataStorage
{
    HashedPrivateDataMap*   pUnreserved;
    // The memory for the array is calculated dynamically based on the device createInfo
    // default count = 1
    uint64                  reserved[1];
};

class PrivateDataSlotEXT final : public NonDispatchable<VkPrivateDataSlotEXT, PrivateDataSlotEXT>
{
public:
    static VkResult Create(
                        Device*                                 pDevice,
                        const VkPrivateDataSlotCreateInfoEXT*   pCreateInfo,
                        const VkAllocationCallbacks*            pAllocator,
                        VkPrivateDataSlotEXT*                   pPrivateDataSlotEXT);

    void Destroy(
                        Device*                                 pDevice,
                        const VkAllocationCallbacks*            pAllocator);

    ~PrivateDataSlotEXT(){}

    VkResult SetPrivateDataEXT(
                        Device*                                 pDevice,
                        const VkObjectType                      objectType,
                        const uint64                            objectHandle,
                        const uint64                            data);

    void GetPrivateDataEXT(
                        Device*                                 pDevice,
                        const VkObjectType                      objectType,
                        const uint64                            objectHandle,
                        uint64* const                           pData);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PrivateDataSlotEXT);

    PrivateDataSlotEXT(
                        Device*                                 pDevice,
                        const bool                              isReserved,
                        const uint64                            index);

    template <bool isSet>
    HashedPrivateDataMap* GetUnreservedPrivateDataAddr(
                        Device*                                 pDevice,
                        PrivateDataStorage* const               pPrivateDataStorage);

    template <bool isSet>
    uint64* GetPrivateDataItemAddr(
                        Device*                                 pDevice,
                        const VkObjectType                      objectType,
                        const uint64                            objectHandle);

    uint64                  m_index;
    bool                    m_isReserved;
};

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePrivateDataSlot(
    VkDevice                                    device,
    const VkPrivateDataSlotCreateInfoEXT*       pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPrivateDataSlotEXT*                       pPrivateDataSlot);

VKAPI_ATTR void VKAPI_CALL vkDestroyPrivateDataSlot(
    VkDevice                                    device,
    VkPrivateDataSlotEXT                        privateDataSlot,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkSetPrivateData(
    VkDevice                                    device,
    VkObjectType                                objectType,
    uint64_t                                    objectHandle,
    VkPrivateDataSlotEXT                        privateDataSlot,
    uint64_t                                    data);

VKAPI_ATTR void VKAPI_CALL vkGetPrivateData(
    VkDevice                                    device,
    VkObjectType                                objectType,
    uint64_t                                    objectHandle,
    VkPrivateDataSlotEXT                        privateDataSlot,
    uint64_t*                                   pData);
} // namespace entry

} // namespace vk

#endif /*__VK_PRIVATE_DATA_SLOT_H__*/
