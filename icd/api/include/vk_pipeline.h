/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_PIPELINE_H__
#define __VK_PIPELINE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/internal_mem_mgr.h"

namespace Pal
{

class IDevice;
class IPipeline;

}

namespace vk
{

class Device;
class ComputePipeline;
class GraphicsPipeline;
class PipelineLayout;

// Structure containing information about a retrievable pipeline binary.  These are only retained by Pipeline objects
// when specific device extensions (VK_AMD_shader_info) that can query them are enabled.
struct PipelineBinaryInfo
{
    static PipelineBinaryInfo* Create(size_t size, const void* pBinary, const VkAllocationCallbacks* pAllocator);

    void Destroy(const VkAllocationCallbacks* pAllocator);

    size_t binaryByteSize;
    void*  pBinary;
};

// =====================================================================================================================
// Base class of all pipeline objects.
class Pipeline
{
public:
    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    const PipelineLayout* GetLayout(void) const { return m_pLayout; }

    static VK_FORCEINLINE Pipeline* ObjectFromHandle(VkPipeline pipeline)
    {
        return reinterpret_cast<Pipeline*>(pipeline);
    }

    const Pal::IPipeline* PalPipeline(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalPipeline[idx];
    }

    Pal::IPipeline* PalPipeline(int32_t idx = DefaultDeviceIndex)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalPipeline[idx];
    }

    uint64_t PalPipelineHash(int32_t idx = DefaultDeviceIndex) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_palPipelineHash[idx];
    }

    VK_INLINE const PipelineBinaryInfo* GetBinary() const
        { return m_pBinary; }

    static void CreateLegacyPathElfBinary(
        Device*         pDevice,
        bool            graphicsPipeline,
        Pal::IPipeline* pPalPipeline,
        size_t*         pPipelineBinarySize,
        void**          ppPipelineBinary);

protected:
    Pipeline(
        Device* const         pDevice,
        Pal::IPipeline**      pPalPipeline,
        const PipelineLayout* pLayout,
        PipelineBinaryInfo*   pBinary);

    virtual ~Pipeline();

    Device* const                   m_pDevice;
    const PipelineLayout* const     m_pLayout;
    Pal::IPipeline*                 m_pPalPipeline[MaxPalDevices];
    uint64_t                        m_palPipelineHash[MaxPalDevices];

private:
    PipelineBinaryInfo* const       m_pBinary;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetShaderInfoAMD(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    VkShaderStageFlagBits                       shaderStage,
    VkShaderInfoTypeAMD                         infoType,
    size_t*                                     pBufferSize,
    void*                                       pBuffer);

};

} // namespace vk

#endif /* __VK_PIPELINE_H__ */
