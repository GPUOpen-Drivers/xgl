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
 * @file  vk_physical_device_manager.h
 * @brief Contains declaration of Vulkan physical device manager classes.
 ***********************************************************************************************************************
 */

#ifndef __VK_PHYSICAL_DEVICE_MANAGER_H__
#define __VK_PHYSICAL_DEVICE_MANAGER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"
#include "include/vk_alloccb.h"

#include "palDevice.h"
#include "palMutex.h"
#include "palPlatform.h"
#include "palVector.h"

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

    uint32_t GetDeviceCount() const
        { return m_devices.NumElements(); }

    VK_FORCEINLINE Instance* VkInstance() const
        { return m_pInstance; }

    DisplayManager* GetDisplayManager() const
    {
        return m_pDisplayManager;
    }

    VkResult EnumerateAllNullPhysicalDeviceProperties(
        uint32_t*                       pPhysicalDeviceCount,
        VkPhysicalDeviceProperties**    ppPhysicalDeviceProperties);

protected:
    PhysicalDeviceManager(
        Instance*       pInstance,
        DisplayManager* pDisplayManager);

    VkResult Initialize();
    VkResult UpdateLockedPhysicalDeviceList(void);
    void     DestroyLockedPhysicalDeviceList(void);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PhysicalDeviceManager);

    Instance*                   m_pInstance;
    DisplayManager*             m_pDisplayManager;

    typedef Util::Vector<VkPhysicalDevice, MaxPhysicalDevices, PalAllocator> DeviceVector;

    DeviceVector                m_devices;     // Physical device handles in the order of EnumeratePhysicalDevices
    Util::Mutex                 m_devicesLock; // Mutex used to lock access to the vector of physical devices

    VkPhysicalDeviceProperties* m_pAllNullProperties; // Physical device properties exposed when NULL_GPU=ALL
};

}

#endif /* __VK_PHYSICAL_DEVICE_MANAGER_H__ */
