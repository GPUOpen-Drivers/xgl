/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_layer_all_null_devices.h
 * @brief When the AMDVLK_NULL_GPU=ALL environment variable is set, this layer will provide a list of all possible
 *        NULL devices to the application. No physical device objects will be exposed to the application because there
 *        is a limit on the number of physical device objects that can be created. VkPhysicalDeviceProperties pointers
 *        are exposed through the VkPhysicalDevice handles in vkEnumeratePhysicalDevices_ND so that
 *        vkGetPhysicalDeviceProperties_ND can expose the properties for the appropriate NULL device.
 ***********************************************************************************************************************
 */

#ifndef __VK_LAYER_ALL_NULL_DEVICES_H__
#define __VK_LAYER_ALL_NULL_DEVICES_H__

#pragma once

namespace vk
{

class DispatchTable;

void OverrideDispatchTable_ND(DispatchTable* pDispatchTable);

} // namespace vk

#endif /* __VK_LAYER_ALL_NULL_DEVICES_H__ */
