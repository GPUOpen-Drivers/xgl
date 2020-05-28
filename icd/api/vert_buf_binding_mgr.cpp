/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"

#include "include/vert_buf_binding_mgr.h"

#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palGpuMemory.h"

namespace vk
{

constexpr VbBindingInfo NullVbBindingInfo = {};

// =====================================================================================================================
VertBufBindingMgr::VertBufBindingMgr(
    Device* pDevice)
    :
    m_pDevice(pDevice)
{

}

// =====================================================================================================================
VertBufBindingMgr::~VertBufBindingMgr()
{

}

// =====================================================================================================================
// Initializes VB binding manager state.  Should be called when the command buffer is being initialized.
Pal::Result VertBufBindingMgr::Initialize()
{
    Reset();
    return Pal::Result::Success;
}

// =====================================================================================================================
// Called to reset the state of the VB manager because the parent command buffer is being reset.
void VertBufBindingMgr::Reset()
{

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        for (uint32_t i = 0; i < Pal::MaxVertexBuffers; ++i)
        {
            // Format needs to be set to invalid for struct srv SRDs
            m_bindings[deviceIdx][i].swizzledFormat = Pal::UndefinedSwizzledFormat;

            // These are programmed during BindVertexBuffers()
            m_bindings[deviceIdx][i].gpuAddr = 0;
            m_bindings[deviceIdx][i].range = 0;

            // Stride is programmed during GraphicsPipelineChanged()
            m_bindings[deviceIdx][i].stride = 0;
        }
    }
}

// =====================================================================================================================
// Should be called when vkBindVertexBuffer is called.  Updates the vertex buffer binding table with the new binding,
// and dirties the internal state so that it is validated before the next draw.
void VertBufBindingMgr::BindVertexBuffers(
    CmdBuffer*          pCmdBuf,
    uint32_t            firstBinding,
    uint32_t            bindingCount,
    const VkBuffer*     pInBuffers,
    const VkDeviceSize* pInOffsets,
    const VkDeviceSize* pInSizes,
    const VkDeviceSize* pInStrides)
{
    utils::IterateMask deviceGroup(pCmdBuf->GetDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        Pal::BufferViewInfo* pBinding    = &m_bindings[deviceIdx][firstBinding];
        Pal::BufferViewInfo* pEndBinding = pBinding + bindingCount;
        uint32_t             inputIdx    = 0;

        while (pBinding != pEndBinding)
        {
            const VkBuffer     buffer = pInBuffers[inputIdx];
            const VkDeviceSize offset = pInOffsets[inputIdx];

            if (buffer != VK_NULL_HANDLE)
            {
                const Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

                pBinding->gpuAddr = pBuffer->GpuVirtAddr(deviceIdx) + offset;
                pBinding->range   = (pInSizes != nullptr) ? pInSizes[inputIdx] : pBuffer->GetSize() - offset;
            }
            else
            {
                pBinding->gpuAddr = 0;
                pBinding->range   = 0;
            }

            if (pInStrides != nullptr)
            {
                pBinding->stride = pInStrides[inputIdx];
            }

            inputIdx++;
            pBinding++;
        }

        pCmdBuf->PalCmdBuffer(deviceIdx)->CmdSetVertexBuffers(
            firstBinding, bindingCount, &m_bindings[deviceIdx][firstBinding]);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Should be called whenever the graphics pipeline binding changes and the new pipeline uses vertex buffer bindings.
// Handles updating any state that depends on the pipeline's state, such as the user data location of the vertex
// buffer table.
void VertBufBindingMgr::GraphicsPipelineChanged(
    CmdBuffer*              pCmdBuf,
    const GraphicsPipeline* pPipeline)
{
    const VbBindingInfo& bindingInfo = (pPipeline != nullptr) ? *pPipeline->GetVbBindingInfo() : NullVbBindingInfo;

    // Update strides for each binding used by the graphics pipeline.  Rebuild SRD data for those bindings
    // whose strides changed.
    utils::IterateMask deviceGroup(pCmdBuf->GetDeviceMask());
    do
    {
        uint32_t deviceIdx = deviceGroup.Index();

        uint32_t firstChanged = UINT_MAX;
        uint32_t lastChanged = 0;

        for (uint32_t bindex = 0; bindex < bindingInfo.bindingCount; ++bindex)
        {
            const uint32_t slot                  = bindingInfo.bindings[bindex].slot;
            const uint32_t byteStride            = bindingInfo.bindings[bindex].byteStride;
            Pal::BufferViewInfo*const  pBinding  = &m_bindings[deviceIdx][slot];

            if (pBinding->stride != byteStride)
            {
                pBinding->stride = byteStride;

                if (pBinding->gpuAddr != 0)
                {
                    firstChanged = Util::Min(firstChanged, slot);
                    lastChanged  = Util::Max(lastChanged, slot);
                }
            }
        }
        // Upload new SRD values to CE-RAM for those that changed above
        if (firstChanged <= lastChanged)
        {
            pCmdBuf->PalCmdBuffer(deviceIdx)->CmdSetVertexBuffers(
                firstChanged, (lastChanged - firstChanged) + 1, &m_bindings[deviceIdx][firstChanged]);
        }
    }
    while (deviceGroup.IterateNext());
}
}//namespace vk

