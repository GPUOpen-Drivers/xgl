/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_utils.cpp
 * @brief Utility functions for Vulkan. This file is rebuilt every time.
 ***********************************************************************************************************************
 */

#include "vk_utils.h"

namespace vk
{

namespace utils
{

// =====================================================================================================================
// Get driver build time hash
uint32_t GetBuildTimeHash()
{
    return Util::HashLiteralString(__DATE__ __TIME__);
}

#if DEBUG
// =====================================================================================================================
// If turned on and exe name is a match, this function spins idle until we have a debugger hooked.
void WaitIdleForDebugger(
    bool        waitIdleToggled,
    const char* pWaitIdleExeName,
    uint32_t    debugTimeout)
{
    if (waitIdleToggled)
    {
        bool waitForDebugger = false;

        if (strlen(pWaitIdleExeName) == 0)
        {
            // No executable name specified, apply on all Vulkan applications
            waitForDebugger = true;
        }
        else
        {
            // Apply if executable name is a match
            char appName[PATH_MAX];
            char appPath[PATH_MAX];
            utils::GetExecutableNameAndPath(appName, appPath);

            waitForDebugger = strcmp(pWaitIdleExeName, &appName[0]) == 0;
        }

        if (waitForDebugger)
        {
            // Timeout the driver to give debuggers a chance to load all of the symbols
            if (debugTimeout != 0)
            {
                Util::SleepMs(debugTimeout);
            }
        }
    }
}
#endif

} // namespace utils

} // namespace vk
