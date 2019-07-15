/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  wolfenstein2_layer.cpp
* @brief Implementation of the Wolfenstein 2 Layer.
***********************************************************************************************************************
*/

#include "wolfenstein2_layer.h"

#include "include/vk_conv.h"
#include "include/vk_device.h"

namespace vk
{

// =====================================================================================================================
Wolfenstein2Layer::Wolfenstein2Layer()
{
}

// =====================================================================================================================
Wolfenstein2Layer::~Wolfenstein2Layer()
{
}

namespace entry
{

namespace wolfenstein2_layer
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImage*                                    pImage)
{
    Device*   pDevice = ApiDevice::ObjectFromHandle(device);
    OptLayer* pLayer  = pDevice->GetAppOptLayer();

    VkImageCreateInfo createInfo = *pCreateInfo;

    constexpr VkImageUsageFlags UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (((createInfo.usage & UsageFlags) == UsageFlags) && (createInfo.format != VK_FORMAT_R8G8B8A8_UNORM))
    {
        // The mutable flag forces off DCC (as long as no image view format list is specified).
        createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    return pLayer->GetNextLayer()->GetEntryPoints().vkCreateImage(device,
                                                                  &createInfo,
                                                                  pAllocator,
                                                                  pImage);
}

} // namespace wolfenstein2_layer

} // namespace entry

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define WOLFENSTEIN2_LAYER_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::wolfenstein2_layer::func_name

#define WOLFENSTEIN2_LAYER_OVERRIDE_ENTRY(entry_name) \
    WOLFENSTEIN2_LAYER_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void Wolfenstein2Layer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    WOLFENSTEIN2_LAYER_OVERRIDE_ENTRY(vkCreateImage);
}

} // namespace vk
