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

#ifndef __VK_ACCELERATION_STRUCTURE_H__
#define __VK_ACCELERATION_STRUCTURE_H__

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"

#include "gpurt/gpurt.h"

#pragma once
namespace vk
{

class Buffer;
class DeferredHostOperation;
class Device;
struct GeometryConvertHelper;
class VirtualStackFrame;

// =====================================================================================================================
// VkAccelerationStructureKHR (VK_KHR_acceleration_structure)
class AccelerationStructure final : public NonDispatchable<VkAccelerationStructureKHR, AccelerationStructure>
{
public:
    static VkResult CreateKHR(
        Device*                                     pDevice,
        const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkAccelerationStructureKHR*                 pAccelerationStructure);

    void Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    const GpuRt::AccelStructPrebuildInfo& GetPrebuildInfo() const { return m_prebuild; }

    VkDeviceAddress GetDeviceAddress(uint32_t deviceIndex) const;

    Pal::Result Map(uint32_t deviceIndex, void** pCpuAddr);

    Pal::Result Unmap(uint32_t deviceIndex);

    Pal::gpusize GetGpuMemorySize(uint32_t deviceIndex) const;

    typedef VkImage ApiType;

    // TODO #raytracing Merge Convert Builds Inputs into one
    static VkResult ConvertBuildInputsKHR(
        bool                                               host,
        Device*                                            pDevice,
        uint32_t                                           deviceIndex,
        const VkAccelerationStructureBuildGeometryInfoKHR& info,
        const VkAccelerationStructureBuildRangeInfoKHR*    ppBuildRangeInfos,
        GeometryConvertHelper*                             pHelper,
        GpuRt::AccelStructBuildInputs*                     pInputs);

    static VkResult ConvertBuildSizeInputs(
        uint32_t                                           deviceIndex,
        const VkAccelerationStructureBuildGeometryInfoKHR& info,
        const uint32_t*                                    pMaxPrimitiveCounts,
        GeometryConvertHelper*                             pHelper,
        GpuRt::AccelStructBuildInputs*                     pInputs);

    static GpuRt::AccelStructCopyMode ConvertCopyAccelerationStructureModeKHR(
        VkCopyAccelerationStructureModeKHR copyAccelerationStructureInfo);

    // GPURT Callback Functions
    static GpuRt::Geometry ClientConvertAccelStructBuildGeometry(
        const GpuRt::AccelStructBuildInputs& inputs,
        uint32_t                             geometryIndex);

    static GpuRt::InstanceBottomLevelInfo ClientConvertAccelStructBuildInstanceBottomLevel(
        const GpuRt::AccelStructBuildInputs&    inputs,
        uint32_t                                instanceIndex);

    static GpuRt::AccelStructPostBuildInfo ClientConvertAccelStructPostBuildInfo(
        const GpuRt::AccelStructBuildInfo& buildInfo,
        uint32_t                           postBuildIndex);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(AccelerationStructure);

    AccelerationStructure(
        Device*         pDevice,
        Buffer*         pBuffer,
        VkDeviceAddress offset,
        VkDeviceSize    size);

    // GPURT Callback Functions
    static GpuRt::AccelStructType ConvertAccelerationStructureType(
        const VkAccelerationStructureTypeKHR& accelerationStructureType);

    static GpuRt::AccelStructBuildFlags ConvertAccelerationStructureFlags(
        const VkBuildAccelerationStructureModeKHR&  mode,
        const VkBuildAccelerationStructureFlagsKHR& flags);

    static GpuRt::GpuCpuAddr ConvertBufferAddress(
        bool                                 host,
        const VkDeviceOrHostAddressConstKHR& addr,
        VkDeviceSize                         offset);

    static GpuRt::Geometry ClientConvertAccelStructBuildGeometryKHR(
        bool                                            hostBuild,
        const uint32_t                                  deviceIndex,
        const VkAccelerationStructureGeometryKHR*       pBuildInfo,
        const uint32_t*                                 pMaxPrimitiveCount,
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos);

    const Device*                  m_pDevice;
    const Buffer*                  m_pBuffer;
    const VkDeviceAddress          m_bufferOffset;
    GpuRt::AccelStructPrebuildInfo m_prebuild;
};

// Helper structure to faciliate VkGeometry node conversion via GPURT callbacks.
struct GeometryConvertHelper
{
    bool     host;
    uint32_t deviceIndex;
    Device*  pDevice;

    const VkAccelerationStructureGeometryKHR*               pBuildGeometries;
    const VkAccelerationStructureGeometryKHR* const*        ppBuildGeometries;
    const VkAccelerationStructureBuildRangeInfoKHR*         pBuildRangeInfos;
    const uint32_t*                                         pMaxPrimitiveCounts;
};

namespace entry
{

extern VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureKHR(
    VkDevice                                    device,
    VkAccelerationStructureKHR                  accelerationStructure,
    const VkAllocationCallbacks*                pAllocator);

extern VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetAccelerationStructureDeviceAddressKHR(
    VkDevice                                           device,
    const VkAccelerationStructureDeviceAddressInfoKHR* pInfo);

}; // namespace entry
}; // namespace vk
#endif
