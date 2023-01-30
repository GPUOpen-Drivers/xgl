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
 * @file  vk_physical_device_manager.cpp
 * @brief Contains implementation of Vulkan physical device manager classes.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_physical_device.h"
#include "include/vk_physical_device_manager.h"
#include "../layers/include/query_dlist.h"
#include "palDevice.h"
#include "palPlatform.h"
#include "palScreen.h"
#include "palVectorImpl.h"
#include <algorithm>
#include <vector>

namespace vk
{

// =====================================================================================================================
PhysicalDeviceManager::PhysicalDeviceManager(
    Instance*       pInstance,
    DisplayManager* pDisplayManager)
    :
    m_pInstance(pInstance),
    m_pDisplayManager(pDisplayManager),
    m_devices(pInstance->Allocator()),
    m_pAllNullProperties(nullptr)
{

}

// =====================================================================================================================
// Creates the physical device manager object
VkResult PhysicalDeviceManager::Create(
    Instance*               pInstance,
    PhysicalDeviceManager** ppPhysicalDeviceManager)
{
    VkResult result = VK_SUCCESS;

    VK_ASSERT(pInstance != nullptr);

    const uint32_t objSize = sizeof(PhysicalDeviceManager);

    void* pMemory = pInstance->AllocMem(objSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PhysicalDeviceManager* pManager        = nullptr;
    DisplayManager*        pDisplayManager = nullptr;

    if (result == VK_SUCCESS)
    {
        pManager = VK_PLACEMENT_NEW(pMemory) PhysicalDeviceManager(pInstance, nullptr);

        result = pManager->Initialize();
    }

    if (result == VK_SUCCESS)
    {
        *ppPhysicalDeviceManager = pManager;
    }
    else
    {
        if (pManager != nullptr)
        {
            pManager->Destroy();
        }
        else if (pMemory != nullptr)
        {
            pInstance->FreeMem(pMemory);
        }
    }

    return result;
}

// =====================================================================================================================
VkResult PhysicalDeviceManager::Initialize()
{
    return UpdateLockedPhysicalDeviceList();
}

// =====================================================================================================================
PhysicalDeviceManager::~PhysicalDeviceManager()
{
    if (m_pAllNullProperties != nullptr)
    {
        m_pInstance->FreeMem(m_pAllNullProperties);
    }

    DestroyLockedPhysicalDeviceList();
}

// =====================================================================================================================
VkResult PhysicalDeviceManager::Destroy(void)
{
    Util::Destructor(this);

    m_pInstance->FreeMem(this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Enumerate tracked physical devices.
VkResult PhysicalDeviceManager::EnumeratePhysicalDevices(
    uint32_t*                   pPhysicalDeviceCount,
    VkPhysicalDevice*           pPhysicalDevices)
{
    Util::MutexAuto lock(&m_devicesLock);

    VkResult status = VK_SUCCESS;

    // Only get the devices if we don't already have them, since doing so causes Pal device cleanup/creation to occur.
    //       Without this we can't update the device list if a device has been added/removed while the application is
    //       running.
    if (m_devices.NumElements() == 0)
    {
        status = UpdateLockedPhysicalDeviceList();
    }

    if (status == VK_SUCCESS)
    {
        const uint32_t numWritablePhysicalDevices = *pPhysicalDeviceCount;

        *pPhysicalDeviceCount = m_devices.NumElements();

        // If only the count was requested then we're done
        if (pPhysicalDevices == nullptr)
        {
            return VK_SUCCESS;
        }

        const uint32_t numItemsToWrite = Util::Min(m_devices.NumElements(), numWritablePhysicalDevices);
        uint32_t       numItemsWritten = 0;

        for (auto it = m_devices.Begin(); numItemsWritten < numItemsToWrite; it.Next(), ++numItemsWritten)
        {
            *pPhysicalDevices++ = it.Get();
        }

        if (numItemsToWrite != m_devices.NumElements())
        {
            // Update the count to only what was written.
            *pPhysicalDeviceCount = numItemsToWrite;

            status = VK_INCOMPLETE;
        }
    }

    return status;
}

// =====================================================================================================================
// Returns the number of grouped devices in the system and the grouped index that each device belongs to
uint32_t PhysicalDeviceManager::GetDeviceGroupIndices(
    uint32_t   maxDeviceGroupIndices,
    int32_t*   pDeviceGroupIndices
) const
{
    uint32_t      deviceGroupCount = 0;
    Pal::IDevice* deviceGroupPalDevice[Pal::MaxDevices];

    if (pDeviceGroupIndices != nullptr)
    {
        memset(pDeviceGroupIndices, -1, maxDeviceGroupIndices * sizeof(pDeviceGroupIndices[0]));
    }

    uint32_t deviceIndex = 0;
    for (auto it = m_devices.Begin(); it.IsValid(); it.Next(), deviceIndex++)
    {
        PhysicalDevice* pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(it.Get());
        Pal::IDevice*   pPalDevice      = pPhysicalDevice->PalDevice();

        uint32_t groupIdx;
        for (groupIdx = 0; groupIdx < deviceGroupCount; groupIdx++)
        {
            Pal::GpuCompatibilityInfo compatInfo = {};
            Pal::Result result = pPalDevice->GetMultiGpuCompatibility(*deviceGroupPalDevice[groupIdx], &compatInfo);
            PAL_ALERT(result != Pal::Result::Success);

            if ((compatInfo.flags.gpuFeatures == 1) && (compatInfo.flags.peerTransferWrite == 1))
            {
                if (pDeviceGroupIndices != nullptr)
                {
                    pDeviceGroupIndices[deviceIndex] = groupIdx;
                }
                break;
            }
        }

        // If no match, add new device group
        if (groupIdx == deviceGroupCount)
        {
            if (pDeviceGroupIndices != nullptr)
            {
                VK_ASSERT(groupIdx < maxDeviceGroupIndices);
                pDeviceGroupIndices[deviceIndex] = groupIdx;
            }
            deviceGroupPalDevice[deviceGroupCount] = pPalDevice;
            deviceGroupCount++;
        }
    }

    return deviceGroupCount;
}

// =====================================================================================================================
// Update the list of physical devices tracked by the physical device manager (assumes mutex is locked).
VkResult PhysicalDeviceManager::UpdateLockedPhysicalDeviceList(void)
{
    VkResult result = VK_SUCCESS;

    Pal::IDevice* pPalDeviceList[Pal::MaxDevices] = {};
    uint32_t palDeviceCount = 0;

    VkPhysicalDevice deviceList[Pal::MaxDevices] = {};
    uint32_t deviceCount = 0;

    // Query the physical GPUs from the PAL platform
    Pal::Result palResult = m_pInstance->PalPlatform()->EnumerateDevices(&palDeviceCount, &pPalDeviceList[0]);

    // Workaround addrlib returning an invalid error code
    if (palResult == Pal::Result::ErrorUnknown)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        result = PalToVkResult(palResult);
    }

    DestroyLockedPhysicalDeviceList();

    // Create a temporary array of pointers to VulkanSettingsLoader objects
    VulkanSettingsLoader* settingsArray[Pal::MaxDevices] = { nullptr };

    if ((result == VK_SUCCESS) && (palDeviceCount > 0))
    {
        for (uint32_t i = 0; i < palDeviceCount; ++i)
        {
            void* pLoader = m_pInstance->AllocMem(sizeof(VulkanSettingsLoader), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

            if (pLoader != nullptr)
            {
                settingsArray[i] = VK_PLACEMENT_NEW(pLoader) VulkanSettingsLoader(pPalDeviceList[i], m_pInstance->PalPlatform(), i);
            }
            else
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
                break;
            }
        }
    }

    AppProfile appProfiles[Pal::MaxDevices] = {};

    // Process panel settings for all PAL devices.  This needs to happen globally up front because some instance-level
    // work must occur in between after loading settings but prior to finalizing all devices (mainly developer driver
    // related).
    if (result == VK_SUCCESS)
    {
        result = m_pInstance->LoadAndCommitSettings(palDeviceCount, pPalDeviceList, settingsArray, appProfiles);
    }

    if (result == VK_SUCCESS)
    {
        for (uint32_t i = 0; i < palDeviceCount; ++i)
        {
            // This physical device is currently not known by the physical device manager
            // so we have to create a new API physical device object for it
            VkPhysicalDevice newPhysicalDevice = VK_NULL_HANDLE;

            result = PhysicalDevice::Create(
                this,
                pPalDeviceList[i],
                settingsArray[i],
                appProfiles[i],
                &newPhysicalDevice);

            if (result == VK_SUCCESS)
            {
                // Add the new physical device object to the newly constructed list
                deviceList[deviceCount++] = newPhysicalDevice;
            }
            else
            {
                break;
            }
        }
    }

    if (result != VK_SUCCESS)
    {
        // Destroy already created devices
        while (deviceCount-- > 0)
        {
            PhysicalDevice* pDevice = ApiPhysicalDevice::ObjectFromHandle(deviceList[deviceCount]);
            pDevice->Destroy();
        }

        // Destroy any settings loaders left and free memory
        for (uint32_t i = 0; i < palDeviceCount; i++)
        {
            if (settingsArray[i] != nullptr)
            {
                settingsArray[i]->~VulkanSettingsLoader();
                m_pInstance->FreeMem(settingsArray[i]);
                settingsArray[i] = nullptr;
            }
        }
    }
    else
    {
        // Sort the PAL enumerated devices in a consistent order and save it for vkEnumeratePhysicalDevices

        struct PerfIndex
        {
            bool operator<(const PerfIndex& rhs) const
            {
                // Obey the panel setting to return the preferred device always first
                if (isPreferredDevice != rhs.isPreferredDevice)
                {
                    return isPreferredDevice > rhs.isPreferredDevice;
                }

                if (rhs.perfRating == perfRating)
                {
                    // Ensure the master gpu (index==0) is ordered first.
                    if (gpuIndex == rhs.gpuIndex)
                    {
                        // If the GPU indices match, then we are probably in Crossfire mode,
                        // ensure we prioritize the Gpu which has present capability and attached to the screen.
                        if (hasAttachedScreens != rhs.hasAttachedScreens)
                        {
                            return hasAttachedScreens == true;
                        }
                        if (presentMode != rhs.presentMode)
                        {
                            return presentMode != 0;
                        }
                    }

                    return gpuIndex < rhs.gpuIndex;
                }

                return rhs.perfRating < perfRating;
            }

            uint32_t         gpuIndex;
            uint32_t         perfRating;
            uint32_t         presentMode;
            bool             isPreferredDevice;
            bool             hasAttachedScreens;
            VkPhysicalDevice device;
        };
        std::vector<PerfIndex> sortedList;
        sortedList.reserve(deviceCount);

        constexpr float memPerfFactor = 0.1f;

        // Populate the list with the physical device handles, sorted by gfxipPerfRating and other criteria.
        for (uint32_t currentDeviceIndex = 0; currentDeviceIndex < deviceCount; ++currentDeviceIndex)
        {
            PhysicalDevice* pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(deviceList[currentDeviceIndex]);

            Pal::DeviceProperties info;

            pPhysicalDevice->PalDevice()->GetProperties(&info);

            PerfIndex perf;

            perf.gpuIndex            = info.gpuIndex;
            perf.perfRating          = info.gfxipProperties.performance.gfxipPerfRating +
                                       static_cast<uint32_t>(info.gpuMemoryProperties.performance.memPerfRating * memPerfFactor);

            perf.presentMode         = 0;
            perf.hasAttachedScreens  = info.attachedScreenCount > 0;
            perf.device              = deviceList[currentDeviceIndex];
            perf.isPreferredDevice   = (settingsArray[0]->GetSettings().enumPreferredDeviceIndex == currentDeviceIndex);

            sortedList.push_back(perf);
        }

        // Sort the devices by gfxipPerfRating, high to low
        std::sort(sortedList.begin(), sortedList.end());

        // Now we can add back the active physical devices to the vector
        for (auto it = sortedList.begin(); it != sortedList.end(); ++it)
        {
            m_devices.PushBack(it->device);
        }
    }

    if (result == VK_SUCCESS)
    {
        m_pInstance->PhysicalDevicesChanged();
    }

    return result;
}

// =====================================================================================================================
// Destroy currently tracked physical devices (assumes mutex is locked).
void PhysicalDeviceManager::DestroyLockedPhysicalDeviceList(void)
{
    VkPhysicalDevice physicalDevice;

    while (m_devices.NumElements() > 0)
    {
        m_devices.PopBack(&physicalDevice);

        PhysicalDevice* pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(physicalDevice);

        // Get settingsLoader pointer so its memory can be freed after physical device is destroyed
        VulkanSettingsLoader* pSettingsLoader = pPhysicalDevice->GetSettingsLoader();

        // Destroy physical device object
        pPhysicalDevice->Destroy();

        // Free settingsLoader memory
        pSettingsLoader->~VulkanSettingsLoader();
        m_pInstance->FreeMem(pSettingsLoader);
    }
}

// =====================================================================================================================
// Enumerates all NULL physical device properties
VkResult PhysicalDeviceManager::EnumerateAllNullPhysicalDeviceProperties(
    uint32_t*                    pPhysicalDeviceCount,
    VkPhysicalDeviceProperties** ppPhysicalDeviceProperties)
{
    Util::MutexAuto lock(&m_devicesLock);

    VkResult status = VK_SUCCESS;

    if (ppPhysicalDeviceProperties == nullptr)
    {
        // Only the count was requested
        status = PalToVkResult(Pal::EnumerateNullDevices(pPhysicalDeviceCount, nullptr));
    }
    else
    {
        Pal::NullGpuInfo nullGpus[static_cast<uint32_t>(Pal::NullGpuId::Max)] = {};
        uint32_t nullGpuCount = VK_ARRAY_SIZE(nullGpus);

        status = PalToVkResult(Pal::EnumerateNullDevices(&nullGpuCount, nullGpus));

        if (status == VK_SUCCESS)
        {
            size_t memSize = sizeof(VkPhysicalDeviceProperties) * static_cast<size_t>(Pal::NullGpuId::Max);

            if (m_pAllNullProperties == nullptr)
            {
                m_pAllNullProperties = reinterpret_cast<VkPhysicalDeviceProperties*>(
                    m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));

                if (m_pAllNullProperties == nullptr)
                {
                    status = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }

            if (status == VK_SUCCESS)
            {
                // Clear any previous contents
                memset(m_pAllNullProperties, 0, memSize);

                const uint32_t numItemsToWrite = Util::Min(nullGpuCount, *pPhysicalDeviceCount);

                for (uint32_t numItemsWritten = 0; numItemsWritten < numItemsToWrite; ++numItemsWritten)
                {
                    VkPhysicalDeviceProperties &props = m_pAllNullProperties[numItemsWritten];

                    // Copy null gpu id and name
                    props.deviceID = static_cast<uint32_t>(nullGpus[numItemsWritten].nullGpuId);
                    strcpy(props.deviceName, nullGpus[numItemsWritten].pGpuName);

                    *ppPhysicalDeviceProperties++ = &props;
                }

                if (numItemsToWrite != nullGpuCount)
                {
                    // Update the count to only what was written.
                    *pPhysicalDeviceCount = numItemsToWrite;

                    status = VK_INCOMPLETE;
                }
            }
        }

    }

    return status;
}

} // namespace vk
