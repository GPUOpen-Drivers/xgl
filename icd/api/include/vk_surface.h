/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

class Surface : public NonDispatchable<VkSurfaceKHR, Surface>
{
public:
    static VkResult Create(
        Instance*                       pInstance,
        const VkStructHeader*           pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSurfaceKHR*                   pSurface);

        VkIcdSurfaceXcb*   GetXcbSurface() { return &m_xcbSurface; }
        VkIcdSurfaceXlib*  GetXlibSurface() { return &m_xlibSurface; }

    Pal::OsDisplayHandle GetOSDisplayHandle() { return m_osDisplayHandle; }

    bool IsExplicitFullscreenSurface() const { return m_osDisplayHandle != 0; }

    void Destroy(
        Instance*                            pInstance,
        const VkAllocationCallbacks*         pAllocator);

protected:
    virtual ~Surface() {}

    Surface(Instance*           pInstance,
        Pal::OsDisplayHandle    osDisplayHandle,
        const VkIcdSurfaceXcb&  xcbSurface)
        :
        m_xcbSurface(xcbSurface),
        m_pInstance(pInstance),
        m_osDisplayHandle(osDisplayHandle)
    {
    }

    Surface(Instance*            pInstance,
        Pal::OsDisplayHandle     osDisplayHandle,
        const VkIcdSurfaceXlib&  xlibSurface)
        :
        m_xlibSurface(xlibSurface),
        m_pInstance(pInstance),
        m_osDisplayHandle(osDisplayHandle)
    {
    }

    union
    {
        VkIcdSurfaceXcb     m_xcbSurface;
        VkIcdSurfaceXlib    m_xlibSurface;
    };

    Instance*            m_pInstance;
    Pal::OsDisplayHandle m_osDisplayHandle;
};

namespace entry
{

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateXcbSurfaceKHR(
        VkInstance                                  instance,
        const VkXcbSurfaceCreateInfoKHR*            pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateXlibSurfaceKHR(
        VkInstance                                  instance,
        const VkXlibSurfaceCreateInfoKHR*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkSurfaceKHR*                               pSurface);

VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(
    VkInstance                                  instance,
    VkSurfaceKHR                                surface,
    const VkAllocationCallbacks*                pAllocator);

} // namespace entry

} // namespace vk

