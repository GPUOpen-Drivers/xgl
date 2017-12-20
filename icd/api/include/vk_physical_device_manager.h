/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  vk_physical_device_manager.h
 * @brief Contains declaration of Vulkan physical device manager classes.
 ***********************************************************************************************************************
 */

#ifndef __VK_PHYSICAL_DEVICE_MANAGER_H__
#define __VK_PHYSICAL_DEVICE_MANAGER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_utils.h"
#include "include/vk_alloccb.h"

#include "palHashMap.h"
#include "palMutex.h"
#include "palDevice.h"
#include "palPlatform.h"

namespace vk
{

// Forward declare Vulkan classes used in this file.
class Instance;
class PhysicalDevice;

class PhysicalDeviceManager
{
public:
    static VkResult Create(
        Instance*               pInstance,
        PhysicalDeviceManager** ppPhysicalDeviceManager);

    ~PhysicalDeviceManager();

    VkResult Destroy(void);

    enum
    {
        MaxPhysicalDevices = Pal::MaxDevices,          // Maximum number of physical devices
    };

    VkResult EnumeratePhysicalDevices(
        uint32_t*                   pPhysicalDeviceCount,
        VkPhysicalDevice*           pPhysicalDevices);

    uint32_t GetDeviceGroupIndices(
        uint32_t  maxDeviceGroupIndices,
        int32_t*  pDeviceGroupIndices) const;

    uint32_t FindDeviceIndex(VkPhysicalDevice physicalDevice) const;

    PhysicalDevice* GetDevice(uint32_t index) const;

    VK_INLINE uint32_t GetDeviceCount() const
        { return m_devices.GetNumEntries(); }

    VK_FORCEINLINE Instance* VkInstance() const
        { return m_pInstance; }

    DisplayManager* GetDisplayManager() const
    {
        return m_pDisplayManager;
    }

protected:
    PhysicalDeviceManager(
        Instance*       pInstance,
        DisplayManager* pDisplayManager);

    VkResult Initialize();
    VkResult UpdateLockedPhysicalDeviceList(void);
    void     DestroyLockedPhysicalDeviceList(void);

private:
    Instance*                   m_pInstance;
    DisplayManager*             m_pDisplayManager;

    typedef Util::HashMap<Pal::IDevice*, VkPhysicalDevice, PalAllocator> Gpu2DeviceMap;

    Gpu2DeviceMap               m_devices;              // Map of physical devices hashed by PAL physical GPU handle
    Util::Mutex                 m_devicesLock;          // Mutex used to lock access to the map of physical devices
};

}

#endif /* __VK_PHYSICAL_DEVICE_MANAGER_H__ */
