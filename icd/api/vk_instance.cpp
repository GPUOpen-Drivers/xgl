/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/log.h"
#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_conv.h"
#include "include/vk_instance.h"
#include "include/vk_physical_device.h"
#include "include/vk_physical_device_manager.h"

#include "include/virtual_stack_mgr.h"

#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

#include "include/internal_layer_hooks.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

#include "res/ver.h"

#include "palLib.h"
#include "palDevice.h"
#include "palPlatform.h"
#include "palOglPresent.h"
#include "palListImpl.h"
#include "palInlineFuncs.h"

#include <new>

namespace vk
{

static VkAllocationCallbacks g_privateCallbacks = allocator::g_DefaultAllocCallback;

const char* Instance::m_extensionsEnv = getenv("AMDVLK_ENABLE_DEVELOPING_EXT");
const int   MaxExtensionStringLen     = 512;

// =====================================================================================================================
Instance::Instance(
    const VkAllocationCallbacks*        pAllocCb,
    uint32_t                            apiVersion,
    uint32_t                            appVersion,
    const InstanceExtensions::Enabled&  enabledExtensions,
    AppProfile                          preInitProfile
    )
    :
    m_pPalPlatform(nullptr),
    m_allocCallbacks(*pAllocCb),
    m_palAllocator(&m_allocCallbacks),
    m_privateAllocator(&g_privateCallbacks),
    m_pVirtualStackMgr(nullptr),
    m_pPhysicalDeviceManager(nullptr),
    m_apiVersion(apiVersion),
    m_appVersion(appVersion),
    m_enabledExtensions(enabledExtensions),
    m_dispatchTable(DispatchTable::Type::INSTANCE, this),
    m_nullGpuId(Pal::NullGpuId::Max),
    m_preInitAppProfile(preInitProfile),
    m_screenCount(0),
    m_pScreenStorage(nullptr),
    m_pDevModeMgr(nullptr),
    m_debugReportCallbacks(&m_palAllocator),
    m_debugUtilsMessengers(&m_palAllocator),
    m_logTagIdMask(0),
    m_pGpuMemoryEventHandler(nullptr)
{
    m_flags.u32All = 0;

    memset(m_screens, 0, sizeof(m_screens));

    memset(m_applicationName, 0, sizeof(m_applicationName));
}

// =====================================================================================================================
bool Instance::IsExtensionEnabledByEnv(
    const char* pExtensionName
    )
{
    bool       isEnabled  = false;
    char*      pBuffer    = nullptr;
    char*      pExtension = nullptr;
    const char delim[]    = " ";

    char  envExtensionString[MaxExtensionStringLen] = {};

    if (Instance::m_extensionsEnv != nullptr)
    {
        Util::Strncpy(envExtensionString, Instance::m_extensionsEnv, MaxExtensionStringLen);
        pExtension = Util::Strtok(envExtensionString, delim, &pBuffer);
    }

    while ((pExtension != nullptr) && (strlen(pExtension) > 0))
    {
        if ((Util::Strcasecmp(pExtensionName, pExtension) == 0) || (Util::Strcasecmp("ALL", pExtension) == 0))
        {
            isEnabled = true;
            break;
        }
        pExtension = Util::Strtok(nullptr, delim, &pBuffer);
    }

    return isEnabled;
}

// =====================================================================================================================
// Returns supported instance API version.
VkResult Instance::EnumerateVersion(
    uint32_t*                       pApiVersion)
{
    // Report 1.3 support
    *pApiVersion = (VK_API_VERSION_1_3 | VK_HEADER_VERSION);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Creates a new instance of Vulkan.
VkResult Instance::Create(
    const VkInstanceCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkInstance*                     pInstance)
{
    // Detect an initial app profile (if any).  This may later be overridden by private panel settings.
    AppProfile preInitAppProfile = ScanApplicationProfile(*pCreateInfo);

    const VkAllocationCallbacks* pAllocCb = pAllocator;
    const VkApplicationInfo* pAppInfo = pCreateInfo->pApplicationInfo;

    if (pAllocCb == nullptr)
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
                                                  Instance::GetIgnoredExtensions(),
                                                  &enabledInstanceExtensions))
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    // According to the Vulkan 1.1 spec, if pApplicationInfo is not provided or if the apiVersion requested is 0
    // it is equivalent of providing an apiVersion of 1.0.0
    uint32_t apiVersion = VK_MAKE_API_VERSION( 0,1,0,0);

    if ((pAppInfo != nullptr) && (pAppInfo->apiVersion != 0))
    {
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

    // Get the app version
    uint32_t appVersion = 0;
    if (pAppInfo != nullptr)
    {
        appVersion = pAppInfo->applicationVersion;
    }

    // Placement new on instance object.
    pNewInstance = reinterpret_cast<Instance*>(pInstanceData);
    new (pInstanceData) Instance(pAllocCb,
                                 apiVersion,
                                 appVersion,
                                 enabledInstanceExtensions,
                                 preInitAppProfile);

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
    Pal::NullGpuId* pNullGpuId)
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
            uint32_t nullGpuCount = 0u;

            // Populate nullGpuCount
            if (Pal::EnumerateNullDevices(&nullGpuCount, nullptr) == Pal::Result::Success)
            {
                Pal::NullGpuInfo* nullGpus = reinterpret_cast<Pal::NullGpuInfo*>(
                    AllocMem(nullGpuCount * sizeof(Pal::NullGpuInfo), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));

                if (nullGpus != nullptr)
                {
                    // Populate nullGpus
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

                    FreeMem(nullGpus);
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
    VkResult status = VK_SUCCESS;

    if (pAppInfo != nullptr)
    {
        if (pAppInfo->pApplicationName != nullptr)
        {
            strncpy(m_applicationName, pAppInfo->pApplicationName, APP_INFO_MAX_CHARS - 1);
        }

    }

    m_palAllocator.Init();
    m_privateAllocator.Init();

    size_t palSize = Pal::GetPlatformSize();

    void* pPalMemory = AllocMem(palSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pPalMemory == nullptr)
    {
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (status == VK_SUCCESS)
    {
        status = GpuMemoryEventHandler::Create(this, &m_pGpuMemoryEventHandler);
    }

    if (status == VK_SUCCESS)
    {
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

#if   defined(__unix__)
        createInfo.pSettingsPath = "/etc/amd";
#else
        createInfo.pSettingsPath = "Vulkan";
#endif

        // We use shadow descriptors to support FMASK based MSAA reads so we need to request support from PAL.
        createInfo.flags.requestShadowDescriptorVaRange = 1;

        // Switch to "null" GPU mode if requested
        if (DetermineNullGpuSupport(&createInfo.nullGpuId))
        {
            createInfo.flags.createNullDevice = 1;
            m_flags.nullGpuMode               = 1;
            m_nullGpuId                       = createInfo.nullGpuId;
        }

#if ICD_GPUOPEN_DEVMODE_BUILD
        createInfo.flags.supportRgpTraces = 1;
#endif

        //Check the KHR_DISPALY extension, and then determine whether to open the primaryNode.
        if (IsExtensionEnabled(InstanceExtensions::KHR_DISPLAY) == false)
        {
            createInfo.flags.dontOpenPrimaryNode = 1;
        }

        createInfo.clientApiId = Pal::ClientApi::Vulkan;

        Pal::Result palResult = Pal::CreatePlatform(createInfo, pPalMemory, &m_pPalPlatform);

        status = PalToVkResult(palResult);
    }

    if (status == VK_SUCCESS)
    {
        Pal::IPlatform::InstallDeveloperCb(m_pPalPlatform, &Instance::PalDeveloperCallback, this);

        // Get the platform property. Vulkan doesn't use it so far.
        Pal::PlatformProperties platformProps;

        status = PalToVkResult(m_pPalPlatform->GetProperties(&platformProps));

        m_properties.supportNonSwapChainPresents = platformProps.supportNonSwapChainPresents;
        m_properties.supportExplicitPresentMode  = platformProps.explicitPresentModes;
        m_properties.supportBlockIfFlipping      = platformProps.supportBlockIfFlipping;
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

    if ((status == VK_SUCCESS) && (deviceCount == 0))
    {
        // Prevent an instance from ever being created without any devices present.
        status = VK_ERROR_INITIALIZATION_FAILED;
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

    if (status == VK_SUCCESS)
    {
        PhysicalDevice* pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(devices[DefaultDeviceIndex]);
        Pal::DeviceProperties info;
        pPhysicalDevice->PalDevice()->GetProperties(&info);
        if (pPhysicalDevice->GetRuntimeSettings().enableSPP && info.gfxipProperties.flags.supportSpp)
        {
            wchar_t executableName[PATH_MAX];
            wchar_t executablePath[PATH_MAX];
            utils::GetExecutableNameAndPath(executableName, executablePath);
            m_pPalPlatform->EnableSppProfile(executableName, executablePath);
        }
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
                for (uint32_t i = 0; i < m_screenCount; i++)
                {
                    m_screens[i].pPalScreen = pScreens[i];
                }
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

        if (m_pGpuMemoryEventHandler != nullptr)
        {
            m_pGpuMemoryEventHandler->Destroy();
        }
    }

    if (status == VK_SUCCESS)
    {
        PhysicalDevice* pPhysicalDevice = ApiPhysicalDevice::ObjectFromHandle(devices[DefaultDeviceIndex]);
        m_logTagIdMask = pPhysicalDevice->GetRuntimeSettings().logTagIdMask;
        AmdvlkLog(m_logTagIdMask, GeneralPrint, "%s Begin ********\n",
            GetApplicationName());

        InitDispatchTable();

#if DEBUG
        // Optionally wait for a debugger to be attached
        utils::WaitIdleForDebugger(pPhysicalDevice->GetRuntimeSettings().waitForDebugger,
            &pPhysicalDevice->GetRuntimeSettings().waitForDebuggerExecutableName[0],
            pPhysicalDevice->GetRuntimeSettings().debugTimeout);
#endif
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

    // Install ALL NULL Devices layer if AMDVLK_NULL_GPU=ALL environment variable is set
    if ((IsNullGpuModeEnabled()) &&
        (GetNullGpuId() == Pal::NullGpuId::All))
    {
        OverrideDispatchTable_ND(&m_dispatchTable);
    }

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
    uint32_t              deviceCount,
    Pal::IDevice**        ppDevices,
    VulkanSettingsLoader* settingsLoaders[],
    AppProfile*           pAppProfiles)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t deviceIdx = 0; ((deviceIdx < deviceCount) && (result == VK_SUCCESS)); ++deviceIdx)
    {
        pAppProfiles[deviceIdx] = m_preInitAppProfile;

        // Load per-device settings
        result = PalToVkResult(settingsLoaders[deviceIdx]->Init());

        if (result == VK_SUCCESS)
        {
            result = settingsLoaders[deviceIdx]->ProcessSettings(
                                &m_allocCallbacks, m_appVersion, &pAppProfiles[deviceIdx]);
        }

        if (result == VK_SUCCESS)
        {
            UpdateSettingsWithAppProfile(settingsLoaders[deviceIdx]->GetSettingsPtr());

            // Make sure the final settings have legal values and update dependant parameters
            settingsLoaders[deviceIdx]->ValidateSettings();

            // Update PAL settings based on runtime settings and desired driver defaults if needed
            settingsLoaders[deviceIdx]->UpdatePalSettings();
        }
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    // Inform developer mode manager of settings.  This also finalizes the developer mode manager.
    if (m_pDevModeMgr != nullptr)
    {
        m_pDevModeMgr->Finalize(deviceCount, settingsLoaders);
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
// Overlay the application profile settings on top of the default settings.
void Instance::UpdateSettingsWithAppProfile(
    RuntimeSettings*    pSettings)
{
    ProfileSettings profileSettings = {};

    // Set the default values
    profileSettings.texFilterQuality = pSettings->vulkanTexFilterQuality;

    ReloadAppProfileSettings(nullptr,
                            this,
                            &profileSettings,
                            pSettings->appGpuID);

    pSettings->vulkanTexFilterQuality =
        static_cast<TextureFilterOptimizationSettings>(profileSettings.texFilterQuality);
}

// =====================================================================================================================
// Destroys the Instance.
VkResult Instance::Destroy(void)
{
    AmdvlkLog(m_logTagIdMask, GeneralPrint, "%s End ********\n", GetApplicationName());

#if ICD_GPUOPEN_DEVMODE_BUILD
    // Pipeline binary cache is required to be freed before destroying DevModeMgr
    // because DevModeMgr manages the state of pipeline binary cache.
    uint32_t deviceCount = PhysicalDeviceManager::MaxPhysicalDevices;
    VkPhysicalDevice devices[PhysicalDeviceManager::MaxPhysicalDevices] = {};
    m_pPhysicalDeviceManager->EnumeratePhysicalDevices(&deviceCount, devices);
    for (uint32_t deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx)
    {
        ApiPhysicalDevice::ObjectFromHandle(devices[deviceIdx])->GetCompiler()->DestroyPipelineBinaryCache();
    }

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
        m_screens[i].pPalScreen->Destroy();
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

    if (m_pGpuMemoryEventHandler != nullptr)
    {
        m_pGpuMemoryEventHandler->Destroy();
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
// Enumerates the GPUs in the system.

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
#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_XCB_SURFACE));
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_XLIB_SURFACE));
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_WAYLAND_SURFACE));
#endif
#endif

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_GET_PHYSICAL_DEVICE_PROPERTIES2));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_MEMORY_CAPABILITIES));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_GET_SURFACE_CAPABILITIES2));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_DEVICE_GROUP_CREATION));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_SEMAPHORE_CAPABILITIES));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_EXTERNAL_FENCE_CAPABILITIES));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_DEBUG_REPORT));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_DEBUG_UTILS));

#if defined(__unix__)
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_DISPLAY));
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_DISPLAY_SURFACE_COUNTER));

#if VK_USE_PLATFORM_XLIB_XRANDR_EXT
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_ACQUIRE_XLIB_DISPLAY));
#endif
        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(KHR_GET_DISPLAY_PROPERTIES2));

        supportedExtensions.AddExtension(VK_INSTANCE_EXTENSION(EXT_DIRECT_MODE_DISPLAY));
#endif
        supportedExtensionsPopulated = true;
    }

    return supportedExtensions;
}

const InstanceExtensions::Supported& Instance::GetIgnoredExtensions()
{
    static InstanceExtensions::Supported ignoredExtensions;

    return ignoredExtensions;
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
            supportedExtensions.GetExtensionInfo(id, pProperties);
            pProperties++;
            copyCount--;
        }
    }

    return result;
}

// =====================================================================================================================
// Get mode list for specific screen.
VkResult Instance::GetScreenModeList(
    const Pal::IScreen*     pScreen,
    uint32_t*               pModeCount,
    Pal::ScreenMode**       ppModeList)
{
    VkResult result = VK_SUCCESS;
    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t screenIdx = 0; screenIdx < m_screenCount; ++screenIdx)
    {
        if (m_screens[screenIdx].pPalScreen == pScreen)
        {
            if (ppModeList == nullptr)
            {
                palResult = pScreen->GetScreenModeList(pModeCount, nullptr);
                VK_ASSERT(palResult == Pal::Result::Success);
            }
            else
            {
                if (m_screens[screenIdx].pModeList[0] == nullptr)
                {
                    uint32_t modeCount = 0;
                    palResult = pScreen->GetScreenModeList(&modeCount, nullptr);
                    VK_ASSERT(palResult == Pal::Result::Success);

                    m_screens[screenIdx].pModeList[0] = reinterpret_cast<Pal::ScreenMode*>(
                            AllocMem(modeCount * sizeof(Pal::ScreenMode), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));

                    for (uint32_t i = 1; i < modeCount; ++i)
                    {
                        m_screens[screenIdx].pModeList[i] = reinterpret_cast<Pal::ScreenMode*>(Util::VoidPtrInc(
                                                                            m_screens[screenIdx].pModeList[0],
                                                                            i * sizeof(Pal::ScreenMode)));
                    }

                    palResult = pScreen->GetScreenModeList(&modeCount, m_screens[screenIdx].pModeList[0]);
                    VK_ASSERT(palResult == Pal::Result::Success);

                    m_screens[screenIdx].modeCount = modeCount;
                }

                uint32_t loopCount = m_screens[screenIdx].modeCount;

                if (*pModeCount < m_screens[screenIdx].modeCount)
                {
                    result = VK_INCOMPLETE;
                    loopCount = *pModeCount;
                }

                for (uint32_t i = 0; i < loopCount; i++)
                {
                    ppModeList[i] = m_screens[screenIdx].pModeList[i];
                }

                *pModeCount = loopCount;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Finds the PAL screens attached to a given physical device
VkResult Instance::FindScreens(
    const Pal::IDevice* pDevice,
    uint32_t*           pScreenCount,
    Pal::IScreen**      ppScreens) const
{
    VkResult       result = VK_SUCCESS;
    uint32_t       numFound = 0;
    const uint32_t maxEntries = (ppScreens == nullptr) ? 0 : *pScreenCount;

    for (uint32_t screenIdx = 0; screenIdx < m_screenCount; ++screenIdx)
    {
        Pal::ScreenProperties props = {};

        if (m_screens[screenIdx].pPalScreen->GetProperties(&props) == Pal::Result::Success)
        {
            if (props.pMainDevice == pDevice)
            {
                if (numFound < maxEntries)
                {
                    ppScreens[numFound] = m_screens[screenIdx].pPalScreen;
                }

                numFound++;
            }
        }
    }

    if (ppScreens != nullptr && (numFound > maxEntries))
    {
        result = VK_INCOMPLETE;
    }

    *pScreenCount = numFound;

    return result;
}

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
// =====================================================================================================================
Pal::IScreen* Instance::FindScreenFromRandrOutput(
    const Pal::IDevice* pDevice,
    Display*            pDpy,
    uint32_t            randrOutput
) const
{
    Pal::IScreen* pScreen = nullptr;

    for (uint32_t screenIdx = 0; screenIdx < m_screenCount; ++screenIdx)
    {
        Pal::ScreenProperties props = {};

        if (m_screens[screenIdx].pPalScreen->GetProperties(&props) == Pal::Result::Success)
        {
            if (props.pMainDevice == pDevice)
            {
                uint32_t             screenRandrOutput = 0;
                Pal::OsDisplayHandle displayHandle     = reinterpret_cast<Pal::OsDisplayHandle>(pDpy);

                Pal::Result result = m_screens[screenIdx].pPalScreen->GetRandrOutput(displayHandle, &screenRandrOutput);

                if ((result == Pal::Result::Success) && (screenRandrOutput == randrOutput))
                {
                    pScreen = m_screens[screenIdx].pPalScreen;
                    break;
                }
            }
        }
    }
    return pScreen;
}
#endif
#endif
// =====================================================================================================================
// Finds the PAL screen (if any) associated with the given window handle
Pal::IScreen* Instance::FindScreen(
    Pal::IDevice*        pDevice,
    Pal::OsWindowHandle  windowHandle,
    Pal::OsDisplayHandle monitorHandle
) const
{
    Pal::IScreen* pScreen = nullptr;

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
VkResult Instance::EnumeratePhysicalDeviceGroups(
    uint32_t*                           pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties*    pPhysicalDeviceGroupProperties)
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

    // Fill out VkPhysicalDeviceGroupProperties structures
    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        const int32_t deviceGroupIndex = deviceGroupIndices[i];

        if ((deviceGroupIndex >= 0) && (deviceGroupIndex < static_cast<int32_t>(numDeviceGroups)))
        {
            auto& deviceGroup = pPhysicalDeviceGroupProperties[deviceGroupIndex];

            deviceGroup.physicalDevices[deviceGroup.physicalDeviceCount++] = devices[i];
        }
    }

    return result;
}

// =====================================================================================================================
// Enumerates all NULL physical device properties
VkResult Instance::EnumerateAllNullPhysicalDeviceProperties(
    uint32_t*                    pPhysicalDeviceCount,
    VkPhysicalDeviceProperties** ppPhysicalDeviceProperties)
{
    // Query physical devices from the manager
    return m_pPhysicalDeviceManager->EnumerateAllNullPhysicalDeviceProperties(
        pPhysicalDeviceCount,
        ppPhysicalDeviceProperties);
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
        // This is required to trap internal barriers and dispatches performed by PAL so that they can be correctly
        // annotated to RGP.
        SqttMgr::PalDeveloperCallback(pInstance, deviceIndex, type, pCbData);
    }

    if (pInstance->m_pGpuMemoryEventHandler->IsGpuMemoryEventHandlerEnabled())
    {
        pInstance->m_pGpuMemoryEventHandler->PalDeveloperCallback(type, pCbData);
    }
}

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
    VkDebugUtilsMessageSeverityFlagBitsEXT debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT        debugUtilsTypes = 0;

    if (categoryMask == Pal::LogCategoryMaskInternal)
    {
        if (level == static_cast<uint32_t>(Pal::LogLevel::Info))
        {
            flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Verbose))
        {
            flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Alert))
        {
            flags = VK_DEBUG_REPORT_WARNING_BIT_EXT;
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Error))
        {
            flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Debug))
        {
            flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT;
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        }
        else if (level == static_cast<uint32_t>(Pal::LogLevel::Always))
        {
            flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT |
                    VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                    VK_DEBUG_REPORT_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_ERROR_BIT_EXT;

            // Map Always to error, as it is intended to be the most severe log level
            debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        }

        debugUtilsTypes = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    }
    else if (categoryMask == Pal::LogCategoryMaskPerformance)
    {
        flags = VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debugUtilsSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugUtilsTypes    = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
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

    VkDebugUtilsMessengerCallbackDataEXT callbackData = {};

    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pNext = nullptr;
    callbackData.flags = 0; // reserved for future use
    callbackData.pMessageIdName = nullptr;
    callbackData.messageIdNumber = 0;
    callbackData.pMessage = message;
    callbackData.queueLabelCount = 0;
    callbackData.pQueueLabels = nullptr;
    callbackData.cmdBufLabelCount = 0;
    callbackData.pCmdBufLabels = nullptr;
    callbackData.objectCount = 0;
    callbackData.pObjects = nullptr;

    CallExternalMessengers(debugUtilsSeverity,
                           debugUtilsTypes,
                           &callbackData);

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

// =====================================================================================================================
// Add the given Debug Utils Messenger to the instance.
VkResult Instance::RegisterDebugUtilsMessenger(
    DebugUtilsMessenger* pMessenger)
{
    VkResult result = VK_SUCCESS;

    Pal::Result palResult = m_debugUtilsMessengers.PushBack(pMessenger);

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
// Remove the given Debug Utils Messenger from the instance.
void Instance::UnregisterDebugUtilsMessenger(
    DebugUtilsMessenger* pMessenger)
{
    auto it = m_debugUtilsMessengers.Begin();

    DebugUtilsMessenger* element = *it.Get();

    while (element != nullptr)
    {
        if (pMessenger == element)
        {
            m_debugUtilsMessengers.Erase(&it);

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
// Call all registered callbacks with the given VkDebugUtilsMessageSeverityFlagsBitsEXT.
void Instance::CallExternalMessengers(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)
{
    // Guarantee serialization of this function to keep internal and external log messages from getting intermixed
    m_logCallbackInternalExternalMutex.Lock();

    for (auto it = m_debugUtilsMessengers.Begin(); it.Get() != nullptr; it.Next())
    {
        DebugUtilsMessenger* element = *it.Get();

        if (messageSeverity & element->GetMessageSeverityFlags())
        {
            if (messageTypes & element->GetMessageTypeFlags())
            {
                PFN_vkDebugUtilsMessengerCallbackEXT pfnCallback = element->GetCallbackFunc();
                void* pUserData = element->GetUserData();

                (*pfnCallback)(messageSeverity, messageTypes, pCallbackData, pUserData);
            }
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
}

VKAPI_ATTR bool VKAPI_CALL IcdPresentBuffers(
)
{
    return true;
}

} // extern "C"
