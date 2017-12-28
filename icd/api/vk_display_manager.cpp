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
 * @file  vk_display_manager.cpp
 * @brief Contains implementation of Vulkan display manager classes.
 ***********************************************************************************************************************
 */

#include "include/vk_conv.h"
#include "include/vk_display.h"
#include "include/vk_display_manager.h"
#include "include/vk_physical_device.h"
#include "include/vk_surface.h"

namespace vk
{

// =====================================================================================================================
DisplayManager::DisplayManager(Instance* pInstance) :
m_pInstance(pInstance),
m_IsValid(false),
m_DisplayCount(0)
{
}

// =====================================================================================================================
DisplayManager::~DisplayManager()
{
    if (m_IsValid == true)
    {
    }
}

// =====================================================================================================================
VkResult DisplayManager::Initialize()
{
    return (SetupADL() == true) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// TODO When SWDEV-121790 is implemented we can remove this code.
bool DisplayManager::SetupADL()
{

    return m_IsValid;
}

// =====================================================================================================================
// TODO When SWDEV-121790 is implemented we can remove the code which calls into ADL
uint32_t DisplayManager::EnumerateDisplays(
    PhysicalDeviceManager* pPhysicalDeviceManager)
{
    VK_ASSERT(m_DisplayCount == 0);

    return m_DisplayCount;
}

// =====================================================================================================================
VkResult DisplayManager::GetFormats(
    Pal::IScreen*           pPalScreen,
    uint32_t*               pSurfaceFormatCount,
    VkSurfaceFormatKHR*     pSurfaceFormats) const
{
    VkResult result = VK_SUCCESS;

    return result;
}

// =====================================================================================================================
// Set the display mode for the attached high dynamic range display
bool DisplayManager::SetColorSpace(
    const Surface*  pSurface,
    VkColorSpaceKHR colorSpace) const
{
    return false;
}

}//namespace vk

