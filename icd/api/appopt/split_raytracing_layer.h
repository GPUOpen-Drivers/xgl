/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if VKI_RAY_TRACING
#ifndef __SPLIT_RAYTRACING_LAYER_H__
#define __SPLIT_RAYTRACING_LAYER_H__

#pragma once

#include "opt_layer.h"
#include "vk_cmdbuffer.h"

namespace vk
{
// =====================================================================================================================
// Class for the Split Raytracing Layer to simplify calls to the overriden dispatch table from the layer's entrypoints
class SplitRaytracingLayer final : public OptLayer
{
public:
    SplitRaytracingLayer(Device*);
    virtual ~SplitRaytracingLayer() {}

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

    static VkResult CreateLayer(Device* pDevice, SplitRaytracingLayer** ppLayer);
    void DestroyLayer();

    Instance* VkInstance() { return m_pInstance; }
    static void TraceRaysDispatchPerDevice(
        CmdBuffer*  pCmdBuffer,
        uint32_t    deviceIdx,
        uint32_t    width,
        uint32_t    height,
        uint32_t    depth);

private:
    Instance*      m_pInstance;
    PAL_DISALLOW_COPY_AND_ASSIGN(SplitRaytracingLayer);
};

}; // namespace vk

#endif /* __SPLIT_RAYTRACING_LAYER_H__ */
#endif /* VKI_RAY_TRACING */
