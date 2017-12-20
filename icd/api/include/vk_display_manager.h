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
 * @file  vk_display_manager.h
 * @brief Contains declaration of Vulkan display manager classes.
 ***********************************************************************************************************************
 */
#ifndef __VK_DISPLAY_MANAGER_H__
#define __VK_DISPLAY_MANAGER_H__

#pragma once

#include "include/vk_display.h"
#include "include/khronos/vulkan.h"

namespace Pal
{
class IScreen;
}

namespace vk
{
class Instance;
class PhysicalDeviceManager;
class Surface;

class DisplayManager
{
public:

    DisplayManager(Instance* pInstance);
    ~DisplayManager();

    VkResult  Initialize();

    uint32_t  EnumerateDisplays(PhysicalDeviceManager* pPhysicalDeviceManager);

    bool     SetColorSpace(
        const Surface*  pSurface,
        VkColorSpaceKHR colorSpace) const;

    VkResult GetFormats(
            Pal::IScreen*       pPalScreen,
            uint32_t*           pSurfaceFormatCount,
            VkSurfaceFormatKHR* pSurfaceFormat) const;

protected:

    bool     SetupADL();

    Instance*                                    m_pInstance;
    bool                                         m_IsValid;
    uint32_t                                     m_DisplayCount;

};

} // namespace vk

#endif /* __VK_DISPLAY_MANAGER_H__ */
