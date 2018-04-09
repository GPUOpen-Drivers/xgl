/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_instance.cpp
 * @brief Contains implementation of Vulkan top-level instance object.
 ***********************************************************************************************************************
 */

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_conv.h"
#include "include/vk_display_manager.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_physical_device_manager.h"

#include "include/virtual_stack_mgr.h"

#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

#include "res/ver.h"

#include "palLib.h"
#include "palDevice.h"
#include "palPlatform.h"
#include "palOglPresent.h"
#include "palListImpl.h"

#include <new>

namespace vk
{

static VkAllocationCallbacks g_privateCallbacks = allocator::g_DefaultAllocCallback;

// =====================================================================================================================
Instance::Instance(
    const VkAllocationCallbacks*        pAllocCb,
    uint32_t                            apiVersion,
    const InstanceExtensions::Enabled&  enabledExtensions
#ifdef ICD_BUILD_APPPROFILE
    ,
    AppProfile                          preInitProfile
#endif
    )
    :
    m_pPalPlatform(nullptr),
    m_allocCallbacks(*pAllocCb),
    m_palAllocator(&m_allocCallbacks),
    m_privateAllocator(&g_privateCallbacks),
    m_pVirtualStackMgr(nullptr),
    m_pPhysicalDeviceManager(nullptr),
    m_apiVersion(apiVersion),
    m_enabledExtensions(enabledExtensions),
    m_dispatchTable(DispatchTable::Type::INSTANCE, this),
#ifdef ICD_BUILD_APPPROFILE
    m_preInitAppProfile(preInitProfile),
#endif
    m_screenCount(0),
    m_pScreenStorage(nullptr),
    m_pDevModeMgr(nullptr),
    m_debugReportCallbacks(&m_palAllocator)
{
    m_flags.u32All = 0;

    memset(m_pScreens, 0, sizeof(m_pScreens));
}

// =====================================================================================================================
// Returns supported instance API version.
VkResult Instance::EnumerateVersion(
    uint32_t*                       pApiVersion)
{
    // Report 1.1 support
    *pApiVersion = (VK_API_VERSION_1_1 | VK_HEADER_VERSION);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Creates a new instance of Vulkan.
VkResult Instance::Create(
    const VkInstanceCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkInstance*                     pInstance)
{
#ifdef ICD_BUILD_APPPROFILE
    // Detect an initial app profile (if any).  This may later be overridden by private panel settings.
    AppProfile preInitAppProfile = ScanApplicationProfile(*pCreateInfo);
#endif

    const VkAllocationCallbacks* pAllocCb = pAllocator;
    const VkApplicationInfo* pAppInfo = pCreateInfo->pApplicationInfo;

    // It's temporary for vulkancts-imgtec.
    if ((pAllocCb == nullptr) ||
        ((pAllocCb->pfnAllocation == nullptr) && (pAllocCb->pfnFree == nullptr)))
    {
        pAllocCb = &allocator::g_DefaultAllocCallback;
    }
    else
    {
        if (pAllocCb->pfnAllocation == nullptr ||
            pAllocCb->pfnFree == nullptr)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    InstanceExtensions::Enabled enabledInstanceExtensions;

    // Make sure the caller only requests extensions we actually support.
    if (pCreateInfo->enabledExtensionCount > 0)
    {
        if (!InstanceExtensions::EnableExtensions(pCreateInfo->ppEnabledExtensionNames,
                                                  pCreateInfo->enabledExtensionCount,
                                                  Instance::GetSupportedExtensions(),
                                                  enabledInstanceExtensions))
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

#if VKI_SDK_1_0 == 0
    // According to the Vulkan 1.1 spec, if pApplicationInfo is not provided or if the apiVersion requested is 0
    // it is equivalent of providing an apiVersion of 1.0.0
    uint32_t apiVersion = VK_MAKE_VERSION(1,0,0);
#else
    // Default to the highest supported API version
    uint32_t apiVersion = (VK_API_VERSION_1_0 | VK_HEADER_VERSION);
#endif

    if ((pAppInfo != nullptr) && (pAppInfo->apiVersion != 0))
    {
#if VKI_SDK_1_0
        // Check if the requested API version is valid. Zero indicates we should ignore the field, non-zero values
        // must be validated.
        if (!(VK_VERSION_MAJOR(pAppInfo->apiVersion) == 1 &&
              VK_VERSION_MINOR(pAppInfo->apiVersion) == 0))
        {
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
#endif

        apiVersion = pAppInfo->apiVersion;
    }

    // pAllocCb is never NULL here because the entry point will fill it in if the
    // application doesn't.
    VK_ASSERT(pAllocCb != nullptr);

    // Allocate memory using applicaton-supplied allocator callbacks
    void* pInstanceData = pAllocCb->pfnAllocation(pAllocCb->pUserData,
                                                  sizeof(Instance),
                                                  sizeof(void*),
                                                  VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    Instance* pNewInstance;
    VkResult result;

    // Failure due to out-of memory
    if (pInstanceData == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Placement new on instance object.
    pNewInstance = reinterpret_cast<Instance*>(pInstanceData);
    new (pInstanceData) Instance(pAllocCb,
                                 apiVersion,
                                 enabledInstanceExtensions
#ifdef ICD_BUILD_APPPROFILE
                                 , preInitAppProfile
#endif
                                 );

    // Two-step initialization
    result = pNewInstance->Init(pAppInfo);

    if (result == VK_SUCCESS)
    {
        *pInstance = reinterpret_cast<VkInstance>(pNewInstance);
    }
    else
    {
        // On failure, free the memory we just allocated.
        pAllocCb->pfnFree(pAllocCb->pUserData, pInstanceData);
    }

    return result;
}

// =====================================================================================================================
// This function determines whether PAL should be initialied in "null" GPU support mode, which causes PAL to enumerate
// one or more fake GPU devices that can be used mainly as targets for offline shader compilation tools, but not much
// else.  This feature is tied directly to shader analyzer tool support and indirectly to the VK_AMD_shader_info
// extension.
bool Instance::DetermineNullGpuSupport(
    Pal::NullGpuId* pNullGpuId
    ) const
{
    Pal::NullGpuId nullGpuId = Pal::NullGpuId::Max;
    bool nullGpuSupport      = false;

    const char* pNullGpuEnv = getenv("AMDVLK_NULL_GPU");

    if (pNullGpuEnv != nullptr)
    {
        nullGpuSupport = true;

        if (utils::StrCmpCaseInsensitive(pNullGpuEnv, "ALL") == 0)
        {
            nullGpuId = Pal::NullGpuId::All;
        }
        else
        {
            Pal::NullGpuInfo nullGpus[Pal::MaxDevices] = {};
            uint32_t nullGpuCount = VK_ARRAY_SIZE(nullGpus);

            if (Pal::EnumerateNullDevices(&nullGpuCount, nullGpus) == Pal::Result::Success)
            {
                for (uint32_t nullGpuIdx = 0; nullGpuIdx < nullGpuCount; ++nullGpuIdx)
                {
                    if (utils::StrCmpCaseInsensitive(pNullGpuEnv, nullGpus[nullGpuIdx].pGpuName) == 0)
                    {
                        nullGpuId = nullGpus[nullGpuIdx].nullGpuId;
                    }
                }
            }
        }

        *pNullGpuId = nullGpuId;
    }

    return nullGpuSupport;
}

// =====================================================================================================================
// Second stage initialization of Vulkan instance.
VkResult Instance::Init(
    const VkApplicationInfo* pAppInfo)
{
    VkResult status;

    m_palAllocator.Init();
    m_privateAllocator.Init();

    size_t palSize = Pal::GetPlatformSize();

    void* pPalMemory = AllocMem(palSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pPalMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Initialize mutexes used for debug report extension before registering the callback with the Platform.
    if ((m_logCallbackInternalOnlyMutex.Init() != Pal::Result::Success) ||
        (m_logCallbackInternalExternalMutex.Init() != Pal::Result::Success))
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Thunk PAL's memory allocator callbacks to our own
    const Util::AllocCallbacks allocCb =
    {
        &m_allocCallbacks,
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    Pal::PlatformCreateInfo createInfo = { 0 };
    createInfo.pAllocCb = &allocCb;

    const Util::LogCallbackInfo callbackInfo =
    {
        this,
        &LogCallback
    };

    createInfo.pLogInfo = &callbackInfo;

    createInfo.pSettingsPath = "/etc/amd";

    // Switch to "null" GPU mode if requested
    if (DetermineNullGpuSupport(&createInfo.nullGpuId))
    {
        createInfo.flags.createNullDevice = 1;
        m_flags.nullGpuMode               = 1;
    }

    Pal::Result palResult = Pal::CreatePlatform(createInfo, pPalMemory, &m_pPalPlatform);

    if (palResult != Pal::Result::ErrorUnknown)
    {
        status = PalToVkResult(palResult);
    }
    else
    {
        // We _might_ hit this case when addrLib fails to initialize when an upper limit to the number
        // of allocations is set by the application. So we report VK_ERROR_OUT_OF_HOST_MEMORY here.
        // While receiving ErrorUnknown doesn't necessarily guarantee that the error came from AddrLib
        // due to an OOM condition, the time needed to have a propper fix for all the possible cases is
        // not worth spending.
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (status == VK_SUCCESS)
    {
        // Get the platform property. Vulkan doesn't use it so far.
        Pal::PlatformProperties platformProps;

        status = PalToVkResult(m_pPalPlatform->GetProperties(&platformProps));

        m_properties.supportNonSwapChainPresents = platformProps.supportNonSwapChainPresents;
        m_properties.supportExplicitPresentMode  = platformProps.explicitPresentModes;
    }

    if (status == VK_SUCCESS)
    {
        // Initialize virtual stack manager
        status = PalToVkResult(VirtualStackMgr::Create(this, &m_pVirtualStackMgr));
    }

    // Early-initialize the GPUOpen developer mode manager.  Needs to be called prior to enumerating PAL devices.
    if (status == VK_SUCCESS)
    {
        DevModeEarlyInitialize();
    }

    if (status == VK_SUCCESS)
    {
        // Create physical device manager
        status = PhysicalDeviceManager::Create(this, &m_pPhysicalDeviceManager);
    }

    // Get all enumerated devices
    uint32_t deviceCount = PhysicalDeviceManager::MaxPhysicalDevices;
    VkPhysicalDevice devices[PhysicalDeviceManager::MaxPhysicalDevices] = {};

    if ((status != VK_SUCCESS) ||
        (m_pPhysicalDeviceManager->EnumeratePhysicalDevices(&deviceCount, devices) != VK_SUCCESS))
    {
        deviceCount = 0;
    }

    // Late-initialize the developer mode manager.  Needs to be called after settings are committed but BEFORE
    // physical devices are late-initialized (below).
    if ((status == VK_SUCCESS) && (m_pDevModeMgr != nullptr))
    {
        DevModeLateInitialize();
    }

    // Do late initialization of physical devices
    if (status == VK_SUCCESS)
    {
        for (uint32_t deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx)
        {
            ApiPhysicalDevice::ObjectFromHandle(devices[deviceIdx])->LateInitialize();
        }
    }

    // Install PAL developer callback if the SQTT layer is enabled.  This is required to trap internal barriers
    // and dispatches performed by PAL so that they can be correctly annotated to RGP.
    if ((status == VK_SUCCESS) && IsTracingSupportEnabled())
    {
        Pal::IPlatform::InstallDeveloperCb(m_pPalPlatform, &Instance::PalDeveloperCallback, this);
    }

    if (status == VK_SUCCESS)
    {
        size_t screenSize = m_pPalPlatform->GetScreenObjectSize();

        if (screenSize != 0)
        {
            void* pScreenStorage[Pal::MaxScreens] = {};
            Pal::IScreen* pScreens[Pal::MaxScreens] = {};
            uint32_t screenCount = 0;

            pScreenStorage[0] = AllocMem(screenSize * Pal::MaxScreens, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

            Pal::Result result = Pal::Result::Success;

            if (pScreenStorage[0] != nullptr)
            {
                for (uint32_t i = 1; i < Pal::MaxScreens; ++i)
                {
                    pScreenStorage[i] = Util::VoidPtrInc(pScreenStorage[0], i * screenSize);
                }

                result = m_pPalPlatform->GetScreens(&screenCount, pScreenStorage, pScreens);
            }
            else
            {
                result = Pal::Result::ErrorOutOfMemory;
            }

            if (result == Pal::Result::Success)
            {
                m_screenCount = screenCount;
                memcpy(m_pScreens, pScreens, sizeof(m_pScreens));
                m_pScreenStorage = pScreenStorage[0];
            }
            else
            {
                FreeMem(pScreenStorage[0]);
            }
        }
    }

    if (status != VK_SUCCESS)
    {
        // If something went wrong delete any created objects
        if (m_pPhysicalDeviceManager != nullptr)
        {
            m_pPhysicalDeviceManager->Destroy();
        }

        if (m_pVirtualStackMgr != nullptr)
        {
            m_pVirtualStackMgr->Destroy();
        }

        if (m_pPalPlatform != nullptr)
        {
            m_pPalPlatform->Destroy();
        }

        if (pPalMemory != nullptr)
        {
            FreeMem(pPalMemory);
        }
    }

    if (status == VK_SUCCESS)
    {
        InitDispatchTable();
    }

    return status;
}

// =====================================================================================================================
// This function initializes the instance dispatch table and allows the chance to override entries in it if necessary.
// NOTE: Any entry points overridden in the instance dispatch table may need to be also overriden in the device dispatch
// table as the overrides are not inherited.
void Instance::InitDispatchTable()
{
    // =================================================================================================================
    // Initialize dispatch table.
    m_dispatchTable.Init();

    // =================================================================================================================
    // Override dispatch table entries.
    EntryPoints* ep = m_dispatchTable.OverrideEntryPoints();
    // There are no entry point overrides currently.

    // =================================================================================================================
    // After generic overrides, apply any internal layer specific dispatch table override.

    // Install SQTT marker annotation layer if needed
    if (IsTracingSupportEnabled())
    {
        SqttOverrideDispatchTable(&m_dispatchTable, nullptr);
    }
}

// =====================================================================================================================
// Loads panel settings for all devices and commits them to PAL.  This happens immediately after device enumeration
// from PAL and this function is called by the physical device manager.
VkResult Instance::LoadAndCommitSettings(
    uint32_t         deviceCount,
    Pal::IDevice**   ppDevices,
    RuntimeSettings* pSettings
#ifdef ICD_BUILD_APPPROFILE
    ,
    AppProfile*      pAppProfiles
#endif
    )
{
    VkResult result = VK_SUCCESS;

    for (uint32_t deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx)
    {
#ifdef ICD_BUILD_APPPROFILE
        pAppProfiles[deviceIdx] = m_preInitAppProfile;
#endif

        // Load per-device settings
        ProcessSettings(ppDevices[deviceIdx],
#ifdef ICD_BUILD_APPPROFILE
                        &pAppProfiles[deviceIdx],
#endif
                        &pSettings[deviceIdx]);

#ifdef ICD_BUILD_APPPROFILE
        // Overlay the application profile from Radeon Settings
        QueryApplicationProfile(&pSettings[deviceIdx]);
#endif

        // Make sure the final settings have legal values and update dependant parameters
        ValidateSettings(ppDevices[deviceIdx], &pSettings[deviceIdx]);

        // Update PAL settings based on runtime settings and desired driver defaults if needed
        UpdatePalSettings(ppDevices[deviceIdx], &pSettings[deviceIdx]);
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    // Inform developer mode manager of settings.  This also finalizes the developer mode manager.
    if (m_pDevModeMgr != nullptr)
    {
        m_pDevModeMgr->Finalize(deviceCount, ppDevices, pSettings);
    }
#endif

    // After all of the settings have been finalized, initialize each device
    for (uint32_t deviceIdx = 0; ((deviceIdx < deviceCount) && (result == VK_SUCCESS)); ++deviceIdx)
    {
        result = PalToVkResult(ppDevices[deviceIdx]->CommitSettingsAndInit());
    }

    return result;
}

// =====================================================================================================================
// Destroys the Instance.
VkResult Instance::Destroy(void)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    if (m_pDevModeMgr != nullptr)
    {
        m_pDevModeMgr->Destroy();
    }
#endif

    // Destroy physical device manager
    if (m_pPhysicalDeviceManager != nullptr)
    {
        m_pPhysicalDeviceManager->Destroy();
    }

    // Destroy screens
    for (uint32_t i = 0; i < m_screenCount; ++i)
    {
        m_pScreens[i]->Destroy();
    }

    FreeMem(m_pScreenStorage);

    // Destroy virtual stack manager
    if (m_pVirtualStackMgr != nullptr)
    {
        m_pVirtualStackMgr->Destroy();
    }

    // Destroy PAL platform
    if (m_pPalPlatform != nullptr)
    {
        m_pPalPlatform->Destroy();

        FreeMem(m_pPalPlatform);
    }

    // This was created with placement new. Need to explicitly call destructor.
    this->~Instance();

    // Free memory
    FreeMem(this);

    // Cannot fail.
    return VK_SUCCESS;
}

// =====================================================================================================================
// Called when the physical devices in the system have been re-enumerated.
void Instance::PhysicalDevicesChanged()
{
}

// =====================================================================================================================
// Enumerates the GPUs in the system.
VkResult Instance::EnumeratePhysicalDevices(
        uint32_t*                   pPhysicalDeviceCount,
        VkPhysicalDevice*           pPhysicalDevices)
{
    // Query physical devices from the manager
    return m_pPhysicalDeviceManager->EnumeratePhysicalDevices(pPhysicalDeviceCount, pPhysicalDevices);
}

// =====================================================================================================================
// Returns whether a device extension is available.
bool Instance::IsDeviceExtensionAvailable(DeviceExtensions::ExtensionId id) const
{
    return PhysicalDevice::GetAvailableExtensions(this, nullptr).IsExtensionSupported(id);
}

// =====================================================================================================================
// Populates and returns the instance extensions.
const InstanceExtensions::Supported& Instance::GetSupportedExtensions()
{
    static InstanceExtensions::Supported supportedExtensions;
    static bool supportedExtensionsPopulated = false;

    if (!supportedExtensionsPopulated)
    {
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_SURFACE));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_XCB_SURFACE));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_XLIB_SURFACE));
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_WAYLAND_SURFACE));
#endif
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_GET_PHYSICAL_DEVICE_PROPERTIES2));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_MEMORY_CAPABILITIES));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHX_DEVICE_GROUP_CREATION));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_GET_SURFACE_CAPABILITIES2));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_DEVICE_GROUP_CREATION));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_SEMAPHORE_CAPABILITIES));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_FENCE_CAPABILITIES));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_DEBUG_REPORT));

        supportedExtensionsPopulated = true;
    }

    return supportedExtensions;
}

// =====================================================================================================================
// Retrieve an array of supported physical device-level extensions.
VkResult Instance::EnumerateExtensionProperties(
    const char*                 pLayerName,
    uint32_t*                   pPropertyCount,
    VkExtensionProperties*      pProperties)
{
    VkResult result = VK_SUCCESS;

    const InstanceExtensions::Supported& supportedExtensions = GetSupportedExtensions();

    const uint32_t extensionCount = supportedExtensions.GetExtensionCount();

    if (pProperties == nullptr)
    {
        *pPropertyCount = extensionCount;
        return VK_SUCCESS;
    }

    // Expect to return all extensions
    uint32_t copyCount = extensionCount;

    // If not all extensions can be reported then we have to adjust the copy count and return VK_INCOMPLETE at the end
    if (*pPropertyCount < extensionCount)
    {
        copyCount = *pPropertyCount;
        result = VK_INCOMPLETE;
    }

    // Report the actual number of extensions that will be returned
    *pPropertyCount = copyCount;

    // Loop through all extensions known to the driver
    for (int32_t i = 0; (i < InstanceExtensions::Count) && (copyCount > 0); ++i)
    {
        const InstanceExtensions::ExtensionId id = static_cast<InstanceExtensions::ExtensionId>(i);

        // If this extension is supported then report it
        if (supportedExtensions.IsExtensionSupported(id))
        {
            *pProperties = supportedExtensions.GetExtensionInfo(id);
            pProperties++;
            copyCount--;
        }
    }

    return result;
}

// =====================================================================================================================
// Finds the PAL screens attached to a given physical device
VkResult Instance::FindScreens(
    const Pal::IDevice* pDevice,
    uint32_t*           pDisplayCount,
    Pal::IScreen**      ppScreens) const
{
    VkResult       result = VK_SUCCESS;
    uint32_t       numFound = 0;
    const uint32_t maxEntries = (ppScreens == nullptr) ? 0 : *pDisplayCount;

    for (uint32_t screenIdx = 0; screenIdx < m_screenCount; ++screenIdx)
    {
        Pal::ScreenProperties props = {};

        if (m_pScreens[screenIdx]->GetProperties(&props) == Pal::Result::Success)
        {
            if (props.pMainDevice == pDevice)
            {
                if (numFound < maxEntries)
                {
                    ppScreens[numFound] = m_pScreens[screenIdx];
                }

                numFound++;
            }
        }
    }

    if (ppScreens != nullptr && (numFound > maxEntries))
    {
        result = VK_INCOMPLETE;
    }

    *pDisplayCount = numFound;

    return result;
}

// =====================================================================================================================
// Finds the PAL screen (if any) associated with the given window handle
Pal::IScreen* Instance::FindScreen(
    Pal::IDevice*        pDevice,
    Pal::OsWindowHandle  windowHandle,
    Pal::OsDisplayHandle monitorHandle
) const
{
    Pal::IScreen* pScreen = nullptr;

    VK_NOT_IMPLEMENTED;

    return pScreen;
}

// =====================================================================================================================
// This function notifies the instance that it should return versions of Vulkan entry points that support SQTT thread-
// trace annotations for RGP.
//
// IMPORTANT: This function should only be called by physical devices during Instance initialization when those
// devices are first initialized and they read the PAL settings.
void Instance::EnableTracingSupport()
{
    // This function should not be called after the loader/application has queried this ICD's per-instance dispatch
    // table.
    m_flags.sqttSupport = 1;
}

// =====================================================================================================================
// Early-initializes the GPU Open Developer Mode manager if that mode is enabled.  This is called prior to enumerating
// PAL devices (before physical device manager is created).
void Instance::DevModeEarlyInitialize()
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    VK_ASSERT(m_pPhysicalDeviceManager == nullptr);
    VK_ASSERT(m_pDevModeMgr == nullptr);

    // Initialize the devmode manager which abstracts interaction with the gpuopen dev driver component
    if (m_pPalPlatform->GetDevDriverServer() != nullptr)
    {
        const VkResult result = DevModeMgr::Create(this, &m_pDevModeMgr);

        VK_ASSERT(result == VK_SUCCESS);
    }
#endif
}

// =====================================================================================================================
// Late-initializes the GPU Open Developer Mode manager if that mode is enabled.  This is called after enumerating
// PAL devices (after physical device manager is created).
void Instance::DevModeLateInitialize()
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    VK_ASSERT(m_pPhysicalDeviceManager != nullptr);
    VK_ASSERT(m_pDevModeMgr != nullptr);

    // Query if we need support for SQTT tracing, and notify the instance so that the correct dispatch table
    // layer can be installed.
    if (m_pDevModeMgr->IsTracingEnabled())
    {
        EnableTracingSupport();
    }
#endif
}

// =====================================================================================================================
// Enumerate DeviceGroups.
// Called in response to vkEnumeratePhysicalDeviceGroupsXXX (both KHR and KHX versions)
template <typename T>
VkResult Instance::EnumeratePhysicalDeviceGroups(
    uint32_t*   pPhysicalDeviceGroupCount,
    T*          pPhysicalDeviceGroupProperties)
{
    if (pPhysicalDeviceGroupProperties == nullptr)
    {
        *pPhysicalDeviceGroupCount = m_pPhysicalDeviceManager->GetDeviceGroupIndices(0, nullptr);
        return VK_SUCCESS;
    }

    int32_t  deviceGroupIndices[Pal::MaxDevices];
    uint32_t numDeviceGroups = m_pPhysicalDeviceManager->GetDeviceGroupIndices(Pal::MaxDevices, deviceGroupIndices);

    VkResult result = VK_SUCCESS;
    if (numDeviceGroups > *pPhysicalDeviceGroupCount)
    {
        numDeviceGroups = *pPhysicalDeviceGroupCount;
        result          = VK_INCOMPLETE;
    }
    else
    {
        *pPhysicalDeviceGroupCount = numDeviceGroups;
    }

    // Enumerate Pal devices in the order as defined in m_pPhysicalDeviceManager->EnumeratePhysicalDevices(...)
    uint32_t physicalDeviceCount = m_pPhysicalDeviceManager->GetDeviceCount();
    VkPhysicalDevice devices[Pal::MaxDevices];
    m_pPhysicalDeviceManager->EnumeratePhysicalDevices(&physicalDeviceCount, devices);

    // Initialize group data
    for (uint32_t i = 0; i < numDeviceGroups; i++)
    {
        pPhysicalDeviceGroupProperties[i].physicalDeviceCount = 0;
        pPhysicalDeviceGroupProperties[i].subsetAllocation    = VK_FALSE;
    }

    // Fill out VkPhysicalDeviceGroupPropertiesKHX structures
    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        const uint32_t deviceIndex      = m_pPhysicalDeviceManager->FindDeviceIndex(devices[i]);
        const int32_t  deviceGroupIndex = deviceGroupIndices[deviceIndex];

        if ((deviceGroupIndex >= 0) && (deviceGroupIndex < static_cast<int32_t>(numDeviceGroups)))
        {
            auto& deviceGroup = pPhysicalDeviceGroupProperties[deviceGroupIndex];

            deviceGroup.physicalDevices[deviceGroup.physicalDeviceCount++] = devices[i];
        }
    }

    return result;
}

// =====================================================================================================================
// Master function that handles developer callbacks from PAL.
void PAL_STDCALL Instance::PalDeveloperCallback(
    void*                        pPrivateData,
    const Pal::uint32            deviceIndex,
    Pal::Developer::CallbackType type,
    void*                        pCbData)
{
    Instance* pInstance = static_cast<Instance*>(pPrivateData);

    if (pInstance->IsTracingSupportEnabled())
    {
        SqttMgr::PalDeveloperCallback(pInstance, deviceIndex, type, pCbData);
    }
}

#ifdef ICD_BUILD_APPPROFILE
// =====================================================================================================================
// Query dynamic applicaiton profile settings
VkResult Instance::QueryApplicationProfile(RuntimeSettings* pRuntimeSettings)
{
    VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;
    if (ReloadAppProfileSettings(this, pRuntimeSettings, &m_chillSettings))
    {
        result = VK_SUCCESS;
    }
    return result;
}
#endif

// =====================================================================================================================
// Callback function used to route debug prints to the VK_EXT_debug_report extension
void PAL_STDCALL Instance::LogCallback(
    void*       pClientData,
    Pal::uint32 level,
    Pal::uint64 categoryMask,
    const char* pFormat,
    va_list     args)
{
    Instance* pInstance = reinterpret_cast<Instance*>(pClientData);
    pInstance->LogMessage(level, categoryMask, pFormat, args);
}

// =====================================================================================================================
// Add the given Debug Report Callback to the instance.
VkResult Instance::RegisterDebugCallback(
    DebugReportCallback* pCallback)
{
    VkResult result = VK_SUCCESS;

    Pal::Result palResult = m_debugReportCallbacks.PushBack(pCallback);

    if (palResult == Pal::Result::Success)
    {
        result = VK_SUCCESS;
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
// Remove the given Debug Report Callback from the instance.
void Instance::UnregisterDebugCallback(
    DebugReportCallback* pCallback)
{
    auto it = m_debugReportCallbacks.Begin();

    DebugReportCallback* element = *it.Get();

    while (element != nullptr)
    {
        if (pCallback == element)
        {
            m_debugReportCallbacks.Erase(&it);

            // Each element should only be in the list once; break out of loop once found
            element = nullptr;
        }
        else
        {
            it.Next();
            element = *it.Get();
        }
    }
}

// =====================================================================================================================
// Convert log message data to match the format of the external callback, then call required external callbacks
void Instance::LogMessage(uint32_t    level,
                          uint64_t    categoryMask,
                          const char* pFormat,
                          va_list     args)
{
    // Guarantee serialization of this function to keep internal log messages from getting intermixed
    m_logCallbackInternalOnlyMutex.Lock();

    uint32_t flags = 0;

    if (categoryMask == Pal::LogCategoryMaskInternal)
    {
        if ((level == static_cast<uint32_t>(Pal::LogLevel::Info)) ||
            (level == static_cast<uint32_t>(Pal::LogLevel::Verbose)))
        {
            flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Alert))
        {
            flags = VK_DEBUG_REPORT_WARNING_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Error))
        {
            flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Debug))
        {
            flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Always))
        {
            flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT |
                    VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                    VK_DEBUG_REPORT_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_ERROR_BIT_EXT;
        }
    }
    else if (categoryMask == Pal::LogCategoryMaskPerformance)
    {
        flags = VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    }

    constexpr uint64_t  object = 0;
    constexpr size_t    location = 0;
    constexpr int32_t   messageCode = 0;
    constexpr char      layerPrefix[] = "AMDVLK\0";

    constexpr uint32_t messageSize = 256;
    char message[messageSize];

    Util::Vsnprintf(message,
                    messageSize,
                    pFormat,
                    args);

    CallExternalCallbacks(flags,
                          VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT,
                          object,
                          location,
                          messageCode,
                          layerPrefix,
                          message);

    m_logCallbackInternalOnlyMutex.Unlock();
}

// =====================================================================================================================
// Call all registered callbacks with the given VkDebugReportFlagsEXT.
void Instance::CallExternalCallbacks(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage)
{
    // Guarantee serialization of this function to keep internal and external log messages from getting intermixed
    m_logCallbackInternalExternalMutex.Lock();

    for (auto it = m_debugReportCallbacks.Begin(); it.Get() != nullptr; it.Next())
    {
        DebugReportCallback* element = *it.Get();

        if (flags & element->GetFlags())
        {
            PFN_vkDebugReportCallbackEXT pfnCallback = element->GetCallbackFunc();
            void* pUserData = element->GetUserData();

            (*pfnCallback)(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage, pUserData);
        }
    }

    m_logCallbackInternalExternalMutex.Unlock();
}

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceVersion(
    uint32_t*                                   pApiVersion)
{
    return Instance::EnumerateVersion(pApiVersion);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance)
{
    VkResult result = Instance::Create(pCreateInfo, pAllocator, pInstance);

    return result;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator)
{
    if (instance != VK_NULL_HANDLE)
    {
        Instance::ObjectFromHandle(instance)->Destroy();
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
    return Instance::ObjectFromHandle(instance)->EnumeratePhysicalDevices(
        pPhysicalDeviceCount, pPhysicalDevices);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroupsKHX(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupPropertiesKHX*         pPhysicalDeviceGroupProperties)
{
    return Instance::ObjectFromHandle(instance)->EnumeratePhysicalDeviceGroups(
        pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroups(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*            pPhysicalDeviceGroupProperties)
{
    return Instance::ObjectFromHandle(instance)->EnumeratePhysicalDeviceGroups(
        pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties)
{
    return Instance::EnumerateExtensionProperties(
        pLayerName,
        pPropertyCount,
        pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties)
{
    // We do not export any internal layers
    if (pProperties == nullptr)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

} // namespace entry

} // namespace vk

// =====================================================================================================================
// These functions are declared like this because they're exported directly from the DLL.

extern "C"
{
// =====================================================================================================================

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceVersion(
    uint32_t*                                   pApiVersion)
{
    return vk::entry::vkEnumerateInstanceVersion(pApiVersion);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance)
{
    return vk::entry::vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance                                  instance,
    const VkAllocationCallbacks*                pAllocator)
{
    vk::entry::vkDestroyInstance(instance, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
    return vk::entry::vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties)
{
    return vk::entry::vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties)
{
    return vk::entry::vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VKAPI_ATTR void VKAPI_CALL IcdSetCallbackProcs(
    void*                                       pPrivateData,
    uint32_t                                    numProcs,
    void*                                       pProcsTable)
{
#ifdef PAL_KMT_BUILD
    Pal::OglSetCallbackProcs(pPrivateData, numProcs, pProcsTable);
#endif
}

VKAPI_ATTR bool VKAPI_CALL IcdPresentBuffers(
#ifdef PAL_KMT_BUILD
    Pal::PresentBufferInfo*                     pPresentBufferInfo
#endif
)
{
#ifdef PAL_KMT_BUILD
    return Pal::OglPresentBuffers(pPresentBufferInfo);
#else
    return true;
#endif
}

} // extern "C"
