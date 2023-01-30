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
 * @file  sqtt_mgr.h
 * @brief Contains the SQTT tracing manager which handles managing SQTT traces as well as any device state required
 *        to inject SQTT annotation markers into command buffer streams.
 ***********************************************************************************************************************
 */

#ifndef __SQTT_SQTT_MGR_H__
#define __SQTT_SQTT_MGR_H__

#pragma once

#include "include/khronos/vulkan.h"

#include "include/vk_dispatch.h"
#include "include/vk_queue.h"

#include "sqtt/sqtt_object_mgr.h"
#include "sqtt/sqtt_rgp_annotations.h"

namespace vk
{

class CmdBuffer;
class Device;

// =====================================================================================================================
// This class manages any SQTT thread tracing state at the device level.
class SqttMgr
{
public:
    SqttMgr(Device* pDevice);
    ~SqttMgr();

    static bool IsTracingSupported(const PhysicalDevice* pDevice, uint32_t queueFamilyIndex);
    void PostPresent();

    RgpSqttMarkerCbID GetNextCmdBufID(
        uint32_t                        queueFamilyIndex,
        const VkCommandBufferBeginInfo* pBeginInfo);

    const DispatchTable* GetNextLayer() const
        { return &m_nextLayer; }

    static void PalDeveloperCallback(
        Instance*                    pInstance,
        const Pal::uint32            deviceIndex,
        Pal::Developer::CallbackType type,
        void*                        pCbData);

    SqttObjectMgr* GetObjectMgr()
        { return &m_objectMgr; }

    void SaveNextLayer();

private:
    void InitLayer();

    Device*           m_pDevice;

    // Current "frame number".  Incremented whenever present is called.
    volatile uint32_t m_frameIndex;

    // Current per-frame command buffer index within the frame.
    volatile uint32_t m_frameCmdBufIndex;

    // Global ID counters per queue family.
    volatile uint32_t m_globalIDsPerQueue[Queue::MaxQueueFamilies];

    // Dispatch table to the next layer's functions
    DispatchTable     m_nextLayer;

    // Metadata tracking for Vulkan objects
    SqttObjectMgr     m_objectMgr;
};

}; // namespace vk

#endif /* __SQTT_SQTT_MGR_H__ */
