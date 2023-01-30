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
* @file  vk_surface.cpp
* @brief Contains implementation of Vulkan Surface object.
***********************************************************************************************************************
*/
#include "vk_instance.h"
#include "vk_physical_device_manager.h"
#include "vk_surface.h"

namespace vk
{

// =====================================================================================================================
VkResult Surface::Create(
    Instance*                           pInstance,
    const VkStructHeader*               pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkSurfaceKHR*                       pSurfaceHandle)
{

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
    VkIcdSurfaceXcb  xcbSurface  = {};
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    VkIcdSurfaceXlib xlibSurface = {};
#endif
    VkIcdSurfaceDisplay displaySurface = {};
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VkIcdSurfaceWayland waylandSurface = {};
#endif
#endif

    VkResult result = VK_SUCCESS;

    const void* pNext = pCreateInfo;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
        case VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR:
        {
            const auto* pExtInfo     = static_cast<const VkXcbSurfaceCreateInfoKHR*>(pNext);
            xcbSurface.base.platform = VK_ICD_WSI_PLATFORM_XCB;
            xcbSurface.connection    = pExtInfo->connection;
            xcbSurface.window        = pExtInfo->window;

            break;
        }
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
        case VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR:
        {
            const auto* pExtInfo      = static_cast<const VkXlibSurfaceCreateInfoKHR*>(pNext);
            xlibSurface.base.platform = VK_ICD_WSI_PLATFORM_XLIB;
            xlibSurface.dpy           = pExtInfo->dpy;
            xlibSurface.window        = pExtInfo->window;

            break;
        }
#endif

        case VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR:
        {
            const auto* pExtInfo              = static_cast<const VkDisplaySurfaceCreateInfoKHR*>(pNext);
            displaySurface.base.platform      = VK_ICD_WSI_PLATFORM_DISPLAY;
            displaySurface.displayMode        = pExtInfo->displayMode;
            displaySurface.planeIndex         = pExtInfo->planeIndex;
            displaySurface.planeStackIndex    = pExtInfo->planeStackIndex;
            displaySurface.transform          = pExtInfo->transform;
            displaySurface.globalAlpha        = pExtInfo->globalAlpha;
            displaySurface.alphaMode          = pExtInfo->alphaMode;
            displaySurface.imageExtent.width  = pExtInfo->imageExtent.width;
            displaySurface.imageExtent.height = pExtInfo->imageExtent.height;

            break;
        }

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        case VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR:
        {
            const auto* pExtInfo         = static_cast<const VkWaylandSurfaceCreateInfoKHR*>(pNext);
            waylandSurface.base.platform = VK_ICD_WSI_PLATFORM_WAYLAND;
            waylandSurface.display       = pExtInfo->display;
            waylandSurface.surface       = pExtInfo->surface;

            break;
        }
#endif
#endif
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }

    if (pCreateInfo == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // allocate the memory
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pInstance->GetAllocCallbacks();

    void* pMemory = pAllocCB->pfnAllocation(pAllocCB->pUserData,
                                            sizeof(Surface),
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        Surface* pSurface = nullptr;

#if defined(__unix__)
        if (displaySurface.base.platform == VK_ICD_WSI_PLATFORM_DISPLAY)
        {
            pSurface = VK_PLACEMENT_NEW(pMemory) Surface(pInstance, displaySurface);
        }
#ifdef VK_USE_PLATFORM_XCB_KHR
        else if (xcbSurface.base.platform == VK_ICD_WSI_PLATFORM_XCB)
        {
            pSurface = VK_PLACEMENT_NEW(pMemory) Surface(pInstance, xcbSurface);
        }
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        else if (waylandSurface.base.platform == VK_ICD_WSI_PLATFORM_WAYLAND)
        {
            pSurface = VK_PLACEMENT_NEW(pMemory) Surface(pInstance, waylandSurface);
        }
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        else
        {
            pSurface = VK_PLACEMENT_NEW(pMemory) Surface(pInstance, xlibSurface);
        }
#endif
#endif
        *pSurfaceHandle = Surface::HandleFromObject(pSurface);
    }

    return result;
}

// =====================================================================================================================
void Surface::Destroy(
    Instance*                        pInstance,
    const VkAllocationCallbacks*     pAllocator)
{
    pInstance->FreeMem(reinterpret_cast<void*>(this));
}

namespace entry
{

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateXcbSurfaceKHR(
    VkInstance                          instance,
    const VkXcbSurfaceCreateInfoKHR*    pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkSurfaceKHR*                       pSurface)
{
    return Surface::Create(Instance::ObjectFromHandle(instance),
        reinterpret_cast<const VkStructHeader*>(pCreateInfo), pAllocator, pSurface);
}
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateXlibSurfaceKHR(
    VkInstance                          instance,
    const VkXlibSurfaceCreateInfoKHR*   pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkSurfaceKHR*                       pSurface)
{
    return Surface::Create(Instance::ObjectFromHandle(instance),
        reinterpret_cast<const VkStructHeader*>(pCreateInfo), pAllocator, pSurface);
}
#endif

// =====================================================================================================================
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
VKAPI_ATTR VkResult VKAPI_CALL vkCreateWaylandSurfaceKHR(
    VkInstance                          instance,
    const VkWaylandSurfaceCreateInfoKHR*   pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkSurfaceKHR*                       pSurface)
{
    return Surface::Create(Instance::ObjectFromHandle(instance),
        reinterpret_cast<const VkStructHeader*>(pCreateInfo), pAllocator, pSurface);
}
#endif
#endif

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDisplayPlaneSurfaceKHR(
    VkInstance                                  instance,
    const VkDisplaySurfaceCreateInfoKHR*        pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface)
{
    return Surface::Create(Instance::ObjectFromHandle(instance),
        reinterpret_cast<const VkStructHeader*>(pCreateInfo), pAllocator, pSurface);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(
    VkInstance                                   instance,
    VkSurfaceKHR                                 surface,
    const VkAllocationCallbacks*                 pAllocator)
{
    Surface::ObjectFromHandle(surface)->Destroy(Instance::ObjectFromHandle(instance), pAllocator);
}

} // namespace entry
} // namespace vk
