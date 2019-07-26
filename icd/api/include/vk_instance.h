/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_instance.h
 * @brief Instance class for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_INSTANCE_H__
#define __VK_INSTANCE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/app_profile.h"
#include "include/vk_alloccb.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "include/vk_extensions.h"
#include "include/vk_debug_report.h"
#include "include/vk_debug_utils.h"

#include "palDeveloperHooks.h"
#include "palLib.h"
#include "palList.h"
#include "palMutex.h"
#include "palScreen.h"
#include "palSysMemory.h"

namespace Pal
{
class IPlatform;
}

namespace vk
{

// Forward declare classes used in this file.
class DevModeMgr;
class DispatchableInstance;
class DisplayManager;
class PhysicalDeviceManager;
class VirtualStackMgr;
class VulkanSettingsLoader;

// =====================================================================================================================
// Represents the per-Vulkan instance data as seen by the applicaton.
class Instance
{
public:
    typedef VkInstance ApiType;

    // Instances are a special type of objects, they are dispatchable but don't have the loader header as other
    // dispatchable object types.
    static Instance* ObjectFromHandle(VkInstance instance)
    {
        return reinterpret_cast<Instance*>(instance);
    }

    struct Properties
    {
        uint32_t  supportNonSwapChainPresents :  1;     // Tells whether the platform support present without swap chain
        uint32_t  supportExplicitPresentMode  :  1;     // Tells whether the platform support client to specific the
                                                        // present mode.
        uint32_t  reserved                    : 30;
    };

    static VkResult EnumerateVersion(
        uint32_t*                    pApiVersion);

    static VkResult Create(
        const VkInstanceCreateInfo*  pCreateInfo,
        const VkAllocationCallbacks* pAllocCb,
        VkInstance*                  pInstance);

    // Tell if the hidden extensions enabled by AMDVLK_ENABLE_DEVELOPING_EXT?
    static bool IsExtensionEnabledByEnv(
        const char* pExtensionName
    );

    VkResult Init(
        const VkApplicationInfo* pAppInfo);

    void InitDispatchTable();

    VkResult Destroy(void);

    VkResult EnumeratePhysicalDevices(
        uint32_t*         pPhysicalDeviceCount,
        VkPhysicalDevice* pPhysicalDevices);

    VkResult EnumeratePhysicalDeviceGroups(
        uint32_t*                           pPhysicalDeviceGroupCount,
        VkPhysicalDeviceGroupProperties*    pPhysicalDeviceGroupProperties);

    void PhysicalDevicesChanged();

    VK_INLINE void* AllocMem(
        size_t                  size,
        size_t                  alignment,
        VkSystemAllocationScope allocType);

    VK_INLINE void* AllocMem(
        size_t                  size,
        VkSystemAllocationScope allocType);

    VK_INLINE void FreeMem(void* pMem);

    VK_INLINE VirtualStackMgr* StackMgr()
        { return m_pVirtualStackMgr; }

    VK_INLINE PalAllocator* Allocator()
        { return &m_palAllocator; }

    VK_INLINE PalAllocator* GetPrivateAllocator()
        { return &m_privateAllocator; }

    VK_INLINE VkAllocationCallbacks* GetAllocCallbacks()
        { return &m_allocCallbacks; }

    VK_FORCEINLINE Pal::IPlatform* PalPlatform() const
        { return m_pPalPlatform; }

    VK_FORCEINLINE const Properties& GetProperties() const
        { return m_properties; }

    static VkResult EnumerateExtensionProperties(
        const char*                 pLayerName,
        uint32_t*                   pPropertyCount,
        VkExtensionProperties*      pProperties);

    VK_INLINE uint32_t GetAPIVersion() const
        { return m_apiVersion; }

    VK_INLINE uint32_t GetAppVersion() const
        { return m_appVersion; }

    VK_FORCEINLINE const PhysicalDeviceManager* GetPhysicalDeviceManager() const
    {
        return m_pPhysicalDeviceManager;
    }

    static const InstanceExtensions::Supported& GetSupportedExtensions();

    bool IsDeviceExtensionAvailable(DeviceExtensions::ExtensionId id) const;

    static VK_INLINE bool IsExtensionSupported(InstanceExtensions::ExtensionId id)
        { return GetSupportedExtensions().IsExtensionSupported(id); }

    VK_INLINE bool IsExtensionEnabled(InstanceExtensions::ExtensionId id) const
        { return m_enabledExtensions.IsExtensionEnabled(id); }

    VkResult FindScreens(
        const Pal::IDevice* pDevice,
        uint32_t*           pDisplayCount,
        Pal::IScreen**      ppScreens) const;

    Pal::IScreen* FindScreen(
        Pal::IDevice*           pDevice,
        Pal::OsWindowHandle     windowHandle,
        Pal::OsDisplayHandle    monitorHandle) const;

#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    Pal::IScreen* FindScreenFromRandrOutput(
        const Pal::IDevice* pDevice,
        Display*            pDpy,
        uint32_t            randrOutput) const;
#endif

    VkResult GetScreenModeList(
        const Pal::IScreen*     pScreen,
        uint32_t*               pModeCount,
        Pal::ScreenMode**       ppModeList);

    VK_INLINE const DispatchTable& GetDispatchTable() const
        { return m_dispatchTable; }

    void EnableTracingSupport();

    VK_INLINE bool IsTracingSupportEnabled() const
        { return m_flags.sqttSupport; }

    VK_INLINE bool IsNullGpuModeEnabled() const
        { return m_flags.nullGpuMode; }

    VK_INLINE Pal::NullGpuId GetNullGpuId() const
        { return m_nullGpuId; }

    DevModeMgr* GetDevModeMgr()
        { return m_pDevModeMgr; }

    VkResult LoadAndCommitSettings(
        uint32_t              deviceCount,
        Pal::IDevice**        ppDevices,
        VulkanSettingsLoader* settingsLoaders[],
        AppProfile*           pAppProfiles);

    void QueryApplicationProfile(
        Pal::IDevice*         pPalDevice,
        VulkanSettingsLoader* pSettingsLoader = nullptr);

    VkResult RegisterDebugCallback(
        DebugReportCallback* pCallback);

    void UnregisterDebugCallback(
        DebugReportCallback* pCallback);

    void LogMessage(uint32_t    level,
                    uint64_t    categoryMask,
                    const char* pFormat,
                    va_list     args);

    void CallExternalCallbacks(
        VkDebugReportFlagsEXT       flags,
        VkDebugReportObjectTypeEXT  objectType,
        uint64_t                    object,
        size_t                      location,
        int32_t                     messageCode,
        const char*                 pLayerPrefix,
        const char*                 pMessage);

    VkResult RegisterDebugUtilsMessenger(
        DebugUtilsMessenger* pMessenger);

    void UnregisterDebugUtilsMessenger(
        DebugUtilsMessenger* pMessenger);

    void CallExternalMessengers(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData);

    VkResult EnumerateAllNullPhysicalDeviceProperties(
        uint32_t*                       pPhysicalDeviceCount,
        VkPhysicalDeviceProperties**    ppPhysicalDeviceProperties);

private:
    Instance(
        const VkAllocationCallbacks*        pAllocCb,
        uint32_t                            apiVersion,
        uint32_t                            appVersion,
        const InstanceExtensions::Enabled&  enabledExtensions,
        AppProfile                          preInitProfile
        );

    bool DetermineNullGpuSupport(Pal::NullGpuId* pNullGpuId);

    void DevModeEarlyInitialize();
    void DevModeLateInitialize();

    static void PAL_STDCALL PalDeveloperCallback(
        void*                        pPrivateData,
        const Pal::uint32            deviceIndex,
        Pal::Developer::CallbackType type,
        void*                        pCbData);

    static void PAL_STDCALL LogCallback(
        void*       pClientData,
        Pal::uint32 level,
        Pal::uint64 categoryMask,
        const char* pFormat,
        va_list     args);

    Pal::IPlatform*                     m_pPalPlatform;             // Pal Platform object.
    VkAllocationCallbacks               m_allocCallbacks;

    Properties                          m_properties;               // Properties of the instance

    PalAllocator                        m_palAllocator;             // Standard allocator that uses app callbacks.
    PalAllocator                        m_privateAllocator;         // Private allocator (mainly for developer mode).
    VirtualStackMgr*                    m_pVirtualStackMgr;         // Virtual stack manager
    PhysicalDeviceManager*              m_pPhysicalDeviceManager;   // Physical device manager
    const uint32_t                      m_apiVersion;               // Requested Vulkan API version
    const uint32_t                      m_appVersion;               // Application Version
    const InstanceExtensions::Enabled   m_enabledExtensions;        // Enabled instance extensions
    DispatchTable                       m_dispatchTable;            // Instance dispatch table

    union
    {
        struct
        {
            uint32_t sqttSupport        : 1;  // True if SQTT thread trace annotation markers are enabled
            uint32_t nullGpuMode        : 1;  // True if the instance is running in null gpu mode (fake gpus for shader
                                              // compilation
            uint32_t reserved           : 30;
        };
        uint32_t u32All;
    } m_flags;

    // Denotes which null gpu mode is enabled
    Pal::NullGpuId                      m_nullGpuId;

    // The application profile that's been detected from the application name or other pattern
    // detection.  Nobody should use this value for anything because it may be overridden by
    // panel setting.  Instead, use the value tracked by the PhysicalDevice.
    AppProfile                          m_preInitAppProfile;

    struct ScreenObject
    {
        Pal::IScreen*       pPalScreen;
        uint32_t            modeCount;
        Pal::ScreenMode*    pModeList[Pal::MaxModePerScreen];
    };

    uint32_t        m_screenCount;
    ScreenObject    m_screens[Pal::MaxScreens];
    void*           m_pScreenStorage;

    DevModeMgr*       m_pDevModeMgr;       // GPUOpen Developer Mode manager.
    ChillSettings     m_chillSettings;     // Dynamic chill settings structure
    TurboSyncSettings m_turboSyncSettings; // Dynamic TurboSync settings structure

    Util::List<DebugReportCallback*, PalAllocator>  m_debugReportCallbacks;             // List of registered Debug
                                                                                        // Report Callbacks
    Util::List<DebugUtilsMessenger*, PalAllocator>  m_debugUtilsMessengers;             // List of registered Debug
                                                                                        // Utils Messengers
    Util::Mutex                                     m_logCallbackInternalOnlyMutex;     // Serialize internal log
                                                                                        // message translation prior
                                                                                        // to calling external callbacks
    Util::Mutex                                     m_logCallbackInternalExternalMutex; // Serialize all calls to
                                                                                        // external callbacks from
                                                                                        // internal and external sources

    // The ratified extensions (including instance and device) under developing could be enabled by environmental
    // variable, AMDVLK_ENABLE_DEVELOPING_EXT.
    static const char* m_extensionsEnv;
};

// =====================================================================================================================
// Allocate mem using allocator callbacks.
void* Instance::AllocMem(
    size_t                  size,
    size_t                  alignment,
    VkSystemAllocationScope allocType)
{
    VK_ASSERT(size > 0);

    return m_allocCallbacks.pfnAllocation(m_allocCallbacks.pUserData,
                                     size,
                                     alignment,
                                     allocType);
}

// =====================================================================================================================
// Allocate mem using allocator callbacks (default alignment)
void* Instance::AllocMem(
    size_t                  size,
    VkSystemAllocationScope allocType)
{
    return AllocMem(size, VK_DEFAULT_MEM_ALIGN, allocType);
}

// =====================================================================================================================
// Free memory using allocator callbacks.
void Instance::FreeMem(
    void* pMem)
{
    if (pMem != nullptr)
    {
        m_allocCallbacks.pfnFree(m_allocCallbacks.pUserData, pMem);
    }
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceVersion(
    uint32_t*                                   pApiVersion);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance);

// =====================================================================================================================

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties);

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroups(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties);

} // namespace entry

} // namespace vk

#endif /* __VK_INSTANCE_H__ */
