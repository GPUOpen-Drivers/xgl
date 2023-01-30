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
************************************************************************************************************************
* @file  query_dlist.cpp
* @brief Helper functions for querying Dlist get AMD Radeon Settings configurations for switchable graphics.
************************************************************************************************************************
*/

#include "include/query_dlist.h"

namespace vk
{

// =====================================================================================================================
// Use GDI APIs to enumerate all the adapters, query AMD adapter whether it is a Hybrid Graphics platform
bool IsHybridGraphicsSupported()
{
    bool isHybridGraphics = false;

    return isHybridGraphics;
}

// =====================================================================================================================
// Call DXGI interface to query the specified GPU adapter configured by MS Graphics Settings or AMD Radeon Settings for
// Hybrid Graphics, which will make Vulkan GPU adapter selection behavior consistent with DX APIs.
void QueryOsSettingsForApplication(unsigned int* pVendorId, unsigned int* pDeviceId)
{
}

} // namespace vk
