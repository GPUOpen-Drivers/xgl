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
* @file  query_dlist.h
* @brief Helper functions for querying Dlist to get AMD Radeon Settings configurations for switchable graphics
************************************************************************************************************************
*/

#ifndef __QUERY_DLIST_H__
#define __QUERY_DLIST_H__

namespace vk
{

#define VENDOR_ID_ATI     0x1002
#define VENDOR_ID_AMD     0x1022
#define VENDOR_ID_NVIDIA  0x10DE
#define VENDOR_ID_INTEL   0x8086

extern bool IsHybridGraphicsSupported();

extern void QueryOsSettingsForApplication(unsigned int* pVendorId, unsigned int* pDeviceId);

}

#endif /* __QUERY_DLIST_H__ */
