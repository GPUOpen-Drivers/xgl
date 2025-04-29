/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "split_raytracing_layer.h"

#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_conv.h"
#include "raytrace/ray_tracing_device.h"
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "sqtt/sqtt_rgp_annotations.h"

namespace vk
{

// =====================================================================================================================
// The method TraceRaysDispatchPerDevice is used to split a dispatch into multiple smaller ones, it helps prevent TDR
// for some specified scenarios and allows the Windows GUI to operate without stuttering.
// The limiations of this method:
//  1) It cannot prevent TDR when the IB needs more than 5 ~ 6 to be exectued on a Windows platform.
//  2) It cannot prevent TDR when there is no preemption request arrives in 2 seconds.
void SplitRaytracingLayer::TraceRaysDispatchPerDevice(
    CmdBuffer*  pCmdBuffer,
    uint32_t    deviceIdx,
    uint32_t    width,
    uint32_t    height,
    uint32_t    depth)
{
    const RuntimeSettings& settings     = pCmdBuffer->VkDevice()->GetRuntimeSettings();
    const RayTracingPipeline* pPipeline = pCmdBuffer->RenderState()->pRayTracingPipeline;

    const Pal::DispatchDims traceSize =
    {
        .x = width,
        .y = height,
        .z = depth
    };

    const uint32_t splitX = settings.rtDispatchSplitX;
    const uint32_t splitY = settings.rtDispatchSplitY;
    const uint32_t splitZ = settings.rtDispatchSplitZ;

    const Pal::DispatchDims blockSize =
    {
        .x = (traceSize.x + splitX - 1) / splitX,
        .y = (traceSize.y + splitY - 1) / splitY,
        .z = (traceSize.z + splitZ - 1) / splitZ
    };

    const Pal::DispatchDims blockDispatchSize = pPipeline->GetDispatchSize(blockSize);
    const Pal::DispatchDims traceDispatchSize = pPipeline->GetDispatchSize(traceSize);

    // Lambda function used to help dispatch.
    auto dispatch = [pCmdBuffer, deviceIdx](Pal::DispatchDims offset, Pal::DispatchDims size)
        {
            pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdDispatchOffset(
                offset,
                size,
                size);

            // To avoid TDR, the large dispatch is split into multiple smaller sub-dispatches. However,
            // when a MCBP event arrives, PFP may have already processed all dispatch commands, so mulitple
            // smaller sub-dispatches cannot be interrupted by MCBP in this case.
            // The Barrier below is used to stall the PFP and allow MCBP to happen between dispatches.
            Pal::AcquireReleaseInfo barrierInfo = {};

            barrierInfo.srcGlobalStageMask  = Pal::PipelineStageCs;
            barrierInfo.dstGlobalStageMask  = Pal::PipelineStageTopOfPipe;
            barrierInfo.srcGlobalAccessMask = Pal::CoherShaderRead;
            barrierInfo.dstGlobalAccessMask = Pal::CoherShaderRead;
            barrierInfo.reason              = RgpBarrierUnknownReason;

            pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdReleaseThenAcquire(barrierInfo);
        };

    // Lambda function used to help splitting.
    auto split = [](uint32_t size, uint32_t incSize, auto&& fun)
        {
            uint32_t i = 0;
            for (; i <= size - incSize; i += incSize)
            {
                fun(i, incSize);
            }
            if (i < size)
            {
                fun(i, size - i);
            }
        };

    // Split Z axis.
    split(traceDispatchSize.z, blockDispatchSize.z,
        [split, traceDispatchSize, blockDispatchSize, &dispatch]
        (uint32_t offsetZ, uint32_t sizeZ)
        {
            // Split Y axis.
            split(traceDispatchSize.y, blockDispatchSize.y,
                [split, traceDispatchSize, blockDispatchSize, &dispatch, offsetZ, sizeZ]
                (uint32_t offsetY, uint32_t sizeY)
                {
                    //Split X axis.
                    split(traceDispatchSize.x, blockDispatchSize.x,
                        [&dispatch, offsetZ, sizeZ, offsetY, sizeY]
                        (uint32_t offsetX, uint32_t sizeX)
                        {
                            Pal::DispatchDims offset =
                            {
                                .x = offsetX,
                                .y = offsetY,
                                .z = offsetZ
                            };
                            Pal::DispatchDims size =
                            {
                                .x = sizeX,
                                .y = sizeY,
                                .z = sizeZ
                            };
                            dispatch(offset, size);
                        });
                });
        }
        );
}

// =====================================================================================================================
VkResult SplitRaytracingLayer::CreateLayer(
    Device*                 pDevice,
    SplitRaytracingLayer**  ppLayer)
{
    VkResult               result   = VK_SUCCESS;
    SplitRaytracingLayer*  pLayer   = nullptr;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if (settings.splitRayTracingDispatch)
    {
        void* pMem = pDevice->VkInstance()->AllocMem(sizeof(SplitRaytracingLayer), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMem != nullptr)
        {
            pLayer = VK_PLACEMENT_NEW(pMem) SplitRaytracingLayer(pDevice);
            *ppLayer = pLayer;
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return result;
}

// =====================================================================================================================
SplitRaytracingLayer::SplitRaytracingLayer(Device* pDevice)
    :
    m_pInstance(pDevice->VkInstance())
{
}

// =====================================================================================================================
void SplitRaytracingLayer::DestroyLayer()
{
    Util::Destructor(this);
    m_pInstance->FreeMem(this);
}

namespace entry
{

namespace splitRaytracingLayer
{
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    pCmdBuffer->SetTraceRaysDispatchPerDevice(SplitRaytracingLayer::TraceRaysDispatchPerDevice);

    SplitRaytracingLayer* pLayer = pCmdBuffer->VkDevice()->RayTrace()->GetSplitRaytracingLayer();
    pLayer->GetNextLayer()->GetEntryPoints().vkCmdTraceRaysKHR(
         commandBuffer,
         pRaygenShaderBindingTable,
         pMissShaderBindingTable,
         pHitShaderBindingTable,
         pCallableShaderBindingTable,
         width,
         height,
         depth);
}
} // splitRaytracingLayer entry
} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SPLIT_RAYTRACING_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::splitRaytracingLayer::func_name

#define SPLIT_RAYTRACING_OVERRIDE_ENTRY(entry_name) SPLIT_RAYTRACING_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void SplitRaytracingLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    SPLIT_RAYTRACING_OVERRIDE_ENTRY(vkCmdTraceRaysKHR);
}

} // namespace vk
#endif
