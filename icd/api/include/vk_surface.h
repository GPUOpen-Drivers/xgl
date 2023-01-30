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
* @file  vk_surface.h
* @brief Surface object related functionality for Vulkan
***********************************************************************************************************************
*/

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"

namespace vk
{

class Surface final : public NonDispatchable<VkSurfaceKHR, Surface>
{
public:
    static VkResult Create(
        Instance*                       pInstance,
        const VkStructHeader*           pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSurfaceKHR*                   pSurface);

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
        VkIcdSurfaceXcb*     GetXcbSurface() { return &m_xcbSurface; }
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VkIcdSurfaceXlib*    GetXlibSurface() { return &m_xlibSurface; }
#endif
        VkIcdSurfaceDisplay* GetDisplaySurface() { return &m_displaySurface; }
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        VkIcdSurfaceWayland*  GetWaylandSurface() { return &m_waylandSurface; }
#endif
#endif

    void Destroy(
        Instance*                            pInstance,
        const VkAllocationCallbacks*         pAllocator);

protected:
    virtual ~Surface() {}

#if defined(__unix__)
    Surface(Instance*               pInstance,
        const VkIcdSurfaceDisplay&  displaySurface)
        :
        m_displaySurface(displaySurface),
        m_pInstance(pInstance)
    {
    }

#ifdef VK_USE_PLATFORM_XCB_KHR
    Surface(Instance*           pInstance,
        const VkIcdSurfaceXcb&  xcbSurface)
        :
        m_xcbSurface(xcbSurface),
        m_pInstance(pInstance)
    {
    }
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
    Surface(Instance*            pInstance,
        const VkIcdSurfaceXlib&  xlibSurface)
        :
        m_xlibSurface(xlibSurface),
        m_pInstance(pInstance)
    {
    }
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    Surface(Instance*               pInstance,
        const VkIcdSurfaceWayland&  waylandSurface)
        :
        m_waylandSurface(waylandSurface),
        m_pInstance(pInstance)
    {
    }
#endif

    union
    {
#ifdef VK_USE_PLATFORM_XCB_KHR
        VkIcdSurfaceXcb     m_xcbSurface;
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VkIcdSurfaceXlib    m_xlibSurface;
#endif
        VkIcdSurfaceDisplay m_displaySurface;
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        VkIcdSurfaceWayland m_waylandSurface;
#endif
    };
#endif

    Instance*            m_pInstance;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Surface);
};

namespace entry
{

#if defined(__unix__)
#ifdef VK_USE_PLATFORM_XCB_KHR
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateXcbSurfaceKHR(
        VkInstance                                  instance,
        const VkXcbSurfaceCreateInfoKHR*            pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateXlibSurfaceKHR(
        VkInstance                                  instance,
        const VkXlibSurfaceCreateInfoKHR*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateWaylandSurfaceKHR(
        VkInstance                                  instance,
        const VkWaylandSurfaceCreateInfoKHR*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);
#endif
#endif

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDisplayPlaneSurfaceKHR(
        VkInstance                                  instance,
        const VkDisplaySurfaceCreateInfoKHR*        pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);

VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(
    VkInstance                                  instance,
    VkSurfaceKHR                                surface,
    const VkAllocationCallbacks*                pAllocator);

} // namespace entry

} // namespace vk

