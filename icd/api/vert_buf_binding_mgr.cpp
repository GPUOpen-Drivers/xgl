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
    m_vbSrdDwSize(pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t)),
    m_pVbTblSysMem(nullptr),
    m_pDevice(pDevice),
    m_bindingTableSize(0)
{

}

// =====================================================================================================================
VertBufBindingMgr::~VertBufBindingMgr()
{

}

// =====================================================================================================================
size_t VertBufBindingMgr::GetMaxVertBufTableDwSize(
    const PhysicalDevice* pPhysDevice)
{
    const size_t vbSrdSize = pPhysDevice->PalProperties().gfxipProperties.srdSizes.bufferView;

    VK_ASSERT((vbSrdSize % sizeof(uint32_t)) == 0);

    return (MaxVertexBuffers * vbSrdSize) / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the amount of bytes of command buffer state memory this manager needs.
size_t VertBufBindingMgr::GetSize(
    const Device* pDevice)
{
    const size_t sysMemTblSize = GetMaxVertBufTableDwSize(pDevice->VkPhysicalDevice()) * sizeof(uint32_t);

    return sysMemTblSize;
}

// =====================================================================================================================
// Initializes VB binding manager state.  Should be called when the command buffer is being initialized.
Pal::Result VertBufBindingMgr::Initialize(
    const CmdBuffer* pCmdBuf,
    void*            pVbMem)
{
    Pal::Result result = Pal::Result::Success;

    m_pVbTblSysMem = reinterpret_cast<uint32_t*>(pVbMem);

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        for (uint32_t i = 0; i < MaxVertexBuffers; ++i)
        {
            // Format needs to be set to invalid for struct srv SRDs
            m_bindings[deviceIdx][i].view.swizzledFormat = Pal::UndefinedSwizzledFormat;

            // These are programmed during BindVertexBuffers()
            m_bindings[deviceIdx][i].size = 0;
            m_bindings[deviceIdx][i].view.gpuAddr = 0;
            m_bindings[deviceIdx][i].view.range = 0;

            // Stride is programmed during GraphicsPipelineChanged()
            m_bindings[deviceIdx][i].view.stride = 0;
        }
    }

    memset(m_pVbTblSysMem,
           0,
           m_vbSrdDwSize * MaxVertexBuffers * sizeof(uint32_t) * pCmdBuf->VkDevice()->NumPalDevices());

    return result;
}

// =====================================================================================================================
// Should be called when vkBindVertexBuffer is called.  Updates the vertex buffer binding table with the new binding,
// and dirties the internal state so that it is validated before the next draw.
void VertBufBindingMgr::BindVertexBuffers(
    CmdBuffer*          pCmdBuf,
    uint32_t            firstBinding,
    uint32_t            bindingCount,
    const VkBuffer*     pInBuffers,
    const VkDeviceSize* pInOffsets)
{
    const uint32_t strideDw      = m_vbSrdDwSize * MaxVertexBuffers;
    const uint32_t startDwOffset = firstBinding  * m_vbSrdDwSize;

    utils::IterateMask deviceGroup(pCmdBuf->GetDeviceMask());
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        uint32_t* pFirstSrd = m_pVbTblSysMem + (strideDw * deviceIdx) + startDwOffset;
        uint32_t* pDestSrd  = pFirstSrd;

        const VkBuffer*     pBuffers = pInBuffers;
        const VkDeviceSize* pOffsets = pInOffsets;

        Binding* pBinding    = &m_bindings[deviceIdx][firstBinding];
        Binding* pEndBinding = pBinding + bindingCount;

        while (pBinding != pEndBinding)
        {
            const VkBuffer     buffer = *pBuffers;
            const VkDeviceSize offset = *pOffsets;

            if (buffer != VK_NULL_HANDLE)
            {
                const Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

                pBinding->view.gpuAddr = pBuffer->GpuVirtAddr(deviceIdx) + offset;
                pBinding->size         = pBuffer->GetSize() - offset;

                // PAL requires that the range be a multiple of the stride. We must round the range if it has space for
                // a final partial element. Rounding down matches our current behavior for buffer views.
                if (pBinding->view.stride > 1)
                {
                    pBinding->view.range = Util::RoundUpToMultiple(pBinding->size, pBinding->view.stride);
                }
                else
                {
                    pBinding->view.range = pBinding->size;
                }

                m_pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &pBinding->view, pDestSrd);
            }
            else
            {
                for (uint32_t dw = 0; dw < m_vbSrdDwSize; ++dw)
                {
                    pDestSrd[dw] = 0;
                }
            }

            pBuffers++;
            pOffsets++;
            pBinding++;

            pDestSrd += m_vbSrdDwSize;
        }
        pCmdBuf->PalCmdBuffer(deviceIdx)->CmdSetIndirectUserData(
                                          VertexBufferTableId, startDwOffset, m_vbSrdDwSize * bindingCount, pFirstSrd);
    }
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

    const size_t strideDw = m_vbSrdDwSize * MaxVertexBuffers;

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        uint32_t firstChanged = UINT_MAX;
        uint32_t lastChanged = 0;

        for (uint32_t bindex = 0; bindex < bindingInfo.bindingCount; ++bindex)
        {
            const uint32_t slot       = bindingInfo.bindings[bindex].slot;
            const uint32_t byteStride = bindingInfo.bindings[bindex].byteStride;
            Binding*const  pBinding   = &m_bindings[deviceIdx][slot];

            if (pBinding->view.stride != byteStride)
            {
                pBinding->view.stride = byteStride;

                uint32_t* pDestSrd = &m_pVbTblSysMem[(strideDw * deviceIdx) + (slot * m_vbSrdDwSize)];

                if (pBinding->view.gpuAddr != 0)
                {
                    // PAL requires that the range be a multiple of the stride. We must round the range if it has space
                    // for a final partial element. Rounding down matches our current behavior for buffer views.
                    if (pBinding->view.stride > 1)
                    {
                        pBinding->view.range = Util::RoundUpToMultiple(pBinding->size, pBinding->view.stride);
                    }
                    else
                    {
                        pBinding->view.range = pBinding->size;
                    }

                    m_pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &pBinding->view, pDestSrd);
                }
                else
                {
                    for (uint32_t dw = 0; dw < m_vbSrdDwSize; ++dw)
                    {
                        pDestSrd[dw] = 0;
                    }
                }

                firstChanged = Util::Min(firstChanged, slot);
                lastChanged  = slot;
            }
        }
        // Upload new SRD values to CE-RAM for those that changed above
        if (firstChanged <= lastChanged)
        {
            const uint32_t dwOffset = firstChanged * m_vbSrdDwSize;
            const uint32_t dwSize = (lastChanged - firstChanged + 1) * m_vbSrdDwSize;

            pCmdBuf->PalCmdBuffer(deviceIdx)->CmdSetIndirectUserData(
                VertexBufferTableId, dwOffset, dwSize, &m_pVbTblSysMem[dwOffset]);
        }
    }

    // Update the active size of the current vertex buffer table.  PAL only dumps CE-RAM up to this limit.
    if (bindingInfo.bindingTableSize != m_bindingTableSize)
    {
        m_bindingTableSize = bindingInfo.bindingTableSize;

        pCmdBuf->PalCmdSetIndirectUserDataWatermark(VertexBufferTableId, m_bindingTableSize * m_vbSrdDwSize);
    }
}

}//namespace vk

