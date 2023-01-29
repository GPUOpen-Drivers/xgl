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
 * @file  sqtt_mgr.cpp
 * @brief Implementation of SQTT layer state management objects
 ***********************************************************************************************************************
 */

#include "devmode/devmode_mgr.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"
#include "include/vk_instance.h"
#include "sqtt/sqtt_mgr.h"
#include "sqtt/sqtt_layer.h"

namespace vk
{

// =====================================================================================================================
// This function atomically increments the given 32-bit unsigned int until a given max value, at which point it
// wraps to 0.
static uint32_t AtomicWrappedIncrement(
    uint32_t           maxValue,
    volatile uint32_t* pValue)
{
    uint32_t oldValue;
    uint32_t newValue;

    do
    {
        oldValue = *pValue;
        newValue = oldValue + 1;

        if (newValue > maxValue)
        {
            newValue = 0;
        }
    }
    while (Util::AtomicCompareAndSwap(pValue, oldValue, newValue) != oldValue);

    return oldValue;
}

// =====================================================================================================================
// Initialize per-device SQTT layer info (function pointers, etc.)
SqttMgr::SqttMgr(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_frameIndex(0),
    m_frameCmdBufIndex(0)
{
    InitLayer();

    m_objectMgr.Init(pDevice);
}

// =====================================================================================================================
// Initializes state for the annotation marker layer.
void SqttMgr::InitLayer()
{
    for (uint32_t i = 0; i < Queue::MaxQueueFamilies; ++i)
    {
        m_globalIDsPerQueue[i] = 0;
    }
}

// =====================================================================================================================
void SqttMgr::SaveNextLayer()
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = m_pDevice->GetDispatchTable();
}

// =====================================================================================================================
bool SqttMgr::IsTracingSupported(
    const PhysicalDevice* pDevice,
    uint32_t              queueFamilyIndex)
{
    bool supported = true;

    Pal::QueueType palType = pDevice->GetQueueFamilyPalQueueType(queueFamilyIndex);

    if ((palType != Pal::QueueTypeUniversal) && (palType != Pal::QueueTypeCompute))
    {
        supported = false;
    }

    return supported;
}

// =====================================================================================================================
// Called after a present to increment the current frame index.
void SqttMgr::PostPresent()
{
    AtomicWrappedIncrement(RgpSqttMaxFrameIndex, &m_frameIndex);

    m_frameCmdBufIndex = 0;
}

// =====================================================================================================================
// Returns the next command buffer ID for a command buffer whose building is about to begin.
RgpSqttMarkerCbID SqttMgr::GetNextCmdBufID(
    uint32_t                        queueFamilyIndex,
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    RgpSqttMarkerCbID newID = {};

    newID.perFrameCbID.perFrame = ((pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != 0);

    if (newID.perFrameCbID.perFrame)
    {
        newID.perFrameCbID.frameIndex = m_frameIndex;
        newID.perFrameCbID.cbIndex    = AtomicWrappedIncrement(RgpSqttMaxPerFrameCbIndex, &m_frameCmdBufIndex);
    }
    else
    {
        newID.globalCbID.cbIndex = AtomicWrappedIncrement(
            RgpSqttMaxGlobalCbIndex, &m_globalIDsPerQueue[queueFamilyIndex]);
    }

    return newID;
}

// =====================================================================================================================
SqttMgr::~SqttMgr()
{

}

// =====================================================================================================================
// Handles any SQTT work related to PAL developer callbacks.  This usually means inserting RGP instrumentation markers
// for various low-level PAL operations like barriers, draws, dispatches.
void SqttMgr::PalDeveloperCallback(
    Instance*                    pInstance,
    const Pal::uint32            deviceIndex,
    Pal::Developer::CallbackType type,
    void*                        pCbData)
{
    switch (type)
    {
    case Pal::Developer::CallbackType::BarrierBegin:
    case Pal::Developer::CallbackType::BarrierEnd:
    case Pal::Developer::CallbackType::ImageBarrier:
        {
            const auto& barrier = *static_cast<const Pal::Developer::BarrierData*>(pCbData);

            CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(barrier.pCmdBuffer->GetClientData());

            if (pCmdBuffer != nullptr)
            {
                SqttCmdBufferState* pSqtt = pCmdBuffer->GetSqttState();

                if (pSqtt != nullptr)
                {
                    pSqtt->PalBarrierCallback(type, barrier);
                }
            }
        }
        break;

    case Pal::Developer::CallbackType::DrawDispatch:
        {
            const auto& drawDispatch = *static_cast<const Pal::Developer::DrawDispatchData*>(pCbData);

            CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(drawDispatch.pCmdBuffer->GetClientData());

            if (pCmdBuffer != nullptr)
            {
                SqttCmdBufferState* pSqtt = pCmdBuffer->GetSqttState();

                if (pSqtt != nullptr)
                {
                    pSqtt->PalDrawDispatchCallback(drawDispatch);
                }
            }
        }
        break;

    case Pal::Developer::CallbackType::BindPipeline:
        {
            const auto& bindPipeline = *static_cast<const Pal::Developer::BindPipelineData*>(pCbData);

            CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(bindPipeline.pCmdBuffer->GetClientData());

            if (pCmdBuffer != nullptr)
            {
                SqttCmdBufferState* pSqtt = pCmdBuffer->GetSqttState();

                if (pSqtt != nullptr)
                {
                    pSqtt->PalBindPipelineCallback(bindPipeline);
                }
            }
        }
        break;

    default:
        break;
    }
}

} // namespace vk
