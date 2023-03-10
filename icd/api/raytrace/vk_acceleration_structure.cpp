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

#include <climits>

#include "include/vk_buffer.h"
#include "include/vk_device.h"
#include "include/vk_memory.h"
#include "include/vk_conv.h"
#include "include/virtual_stack_mgr.h"

#include "raytrace/vk_acceleration_structure.h"
#include "raytrace/ray_tracing_device.h"
#include "raytrace/ray_tracing_util.h"

#include "palAssert.h"

namespace vk
{

// =====================================================================================================================
// For host TLAS builds, the instance geometry buffer passed to GPURT actually points to a GeometryHelper containing
// API inputs.  This function extracts (optionally) the actual instance description and the referenced BLAS object
// pointer for some given instance index.
static GpuRt::InstanceDesc UnpackInstanceDesc(
    const GpuRt::AccelStructBuildInputs& inputs,
    uint32_t                             instanceIndex,
    GpuRt::InstanceDesc*                 pDesc,
    AccelerationStructure**              ppBlas)
{
    AccelerationStructure* pBlas = nullptr;

    // Fetch the instance build geometry info (there is only one for top-level structs)
    const GeometryConvertHelper* pHelper = static_cast<const GeometryConvertHelper*>(inputs.pClientData);

    const VkAccelerationStructureGeometryKHR& geom = *pHelper->pBuildGeometries;
    const void* pHostAddr = geom.geometry.instances.data.hostAddress;

    // Get the i-th instance description based on layout
    const GpuRt::InstanceDesc& desc =
        (inputs.inputElemLayout == GpuRt::InputElementLayout::ArrayOfPointers) ?
        *(static_cast<const GpuRt::InstanceDesc* const*>(pHostAddr)[instanceIndex]) :
        static_cast<const GpuRt::InstanceDesc*>(pHostAddr)[instanceIndex];

    if (pDesc != nullptr)
    {
        *pDesc = desc;
    }

    if (ppBlas != nullptr)
    {
        const VkAccelerationStructureKHR handle = VkAccelerationStructureKHR(desc.accelerationStructure);

        *ppBlas = AccelerationStructure::ObjectFromHandle(handle);
    }

    return desc;
}

// =====================================================================================================================
AccelerationStructure::AccelerationStructure(
    Device*         pDevice,
    Buffer*         pBuffer,
    VkDeviceAddress bufferOffset,
    VkDeviceSize    size
    )
    :
    m_pDevice(pDevice),
    m_pBuffer(pBuffer),
    m_bufferOffset(bufferOffset)
{
    m_prebuild = {};
    m_prebuild.resultDataMaxSizeInBytes = size;
}

// =====================================================================================================================
VkDeviceAddress AccelerationStructure::GetDeviceAddress(uint32_t deviceIndex) const
{
    VkDeviceAddress addr = m_pBuffer->GpuVirtAddr(deviceIndex) + m_bufferOffset;

    VK_ASSERT((addr % GpuRt::RayTraceAccelMemoryBaseAlignment) == 0);

    return addr;
}

// =====================================================================================================================
GpuRt::AccelStructBuildFlags AccelerationStructure::ConvertAccelerationStructureFlags(
    const VkBuildAccelerationStructureModeKHR&  mode,
    const VkBuildAccelerationStructureFlagsKHR& flags)
{
    GpuRt::AccelStructBuildFlags gpuRtFlags = 0;

    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagAllowUpdate;
    }

    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagAllowCompaction;
    }

    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagPreferFastTrace;
    }

    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagPreferFastBuild;
    }

    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagMinimizeMemory;
    }

    if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
    {
        gpuRtFlags |= GpuRt::AccelStructBuildFlagPerformUpdate;
    }

    return gpuRtFlags;
}

// =====================================================================================================================
GpuRt::AccelStructType AccelerationStructure::ConvertAccelerationStructureType(
    const VkAccelerationStructureTypeKHR& accelerationStructureType)
{
    GpuRt::AccelStructType accelStructType = GpuRt::AccelStructType::TopLevel;

    switch (accelerationStructureType)
    {
    case VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR:
        accelStructType = GpuRt::AccelStructType::TopLevel;
        break;
    case VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR:
        accelStructType = GpuRt::AccelStructType::BottomLevel;
        break;
    default:
        VK_NEVER_CALLED();
        accelStructType = GpuRt::AccelStructType::TopLevel;
    }

    return accelStructType;
}

// =====================================================================================================================
VkResult AccelerationStructure::ConvertBuildInputsKHR(
    bool                                               host,
    Device*                                            pDevice,
    uint32_t                                           deviceIndex,
    const VkAccelerationStructureBuildGeometryInfoKHR& info,
    const VkAccelerationStructureBuildRangeInfoKHR*    pBuildRangeInfos,
    GeometryConvertHelper*                             pHelper,
    GpuRt::AccelStructBuildInputs*                     pInputs)
{
    VkResult result = VK_SUCCESS;

    pHelper->pMaxPrimitiveCounts = nullptr;
    pHelper->pBuildRangeInfos    = pBuildRangeInfos;
    pInputs->type                = ConvertAccelerationStructureType(info.type);
    pInputs->flags               = ConvertAccelerationStructureFlags(info.mode, info.flags);
    pHelper->host                = host;
    pHelper->deviceIndex         = deviceIndex;
    pHelper->pDevice             = pDevice;

    if (info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
    {
        VK_ASSERT(info.geometryCount <= 1);

        pInputs->inputElemCount  = 0;
        pInputs->inputElemLayout = GpuRt::InputElementLayout::Array;
        pInputs->instances.gpu   = 0;

        if (info.geometryCount > 0)
        {
            const VkAccelerationStructureGeometryKHR* pInstanceGeom =
                info.ppGeometries != nullptr ? *info.ppGeometries : info.pGeometries;

            if (pInstanceGeom->geometryType == VK_GEOMETRY_TYPE_INSTANCES_KHR)
            {
                pInputs->inputElemCount  = (pBuildRangeInfos != nullptr) ? pBuildRangeInfos->primitiveCount : 1;
                pInputs->inputElemLayout = pInstanceGeom->geometry.instances.arrayOfPointers ?
                                           GpuRt::InputElementLayout::ArrayOfPointers :
                                           GpuRt::InputElementLayout::Array;

                pInputs->instances.gpu   = pInstanceGeom->geometry.instances.data.deviceAddress;

                if (info.ppGeometries != nullptr)
                {
                    pHelper->ppBuildGeometries = info.ppGeometries;
                }
                else
                {
                    pHelper->pBuildGeometries = info.pGeometries;
                }
            }
        }
    }
    else
    {
        if (info.ppGeometries != nullptr)
        {
            pHelper->ppBuildGeometries = info.ppGeometries;
            pInputs->inputElemLayout   = GpuRt::InputElementLayout::ArrayOfPointers;
        }
        else
        {
            pHelper->pBuildGeometries = info.pGeometries;
            pInputs->inputElemLayout  = GpuRt::InputElementLayout::Array;
        }

         pInputs->inputElemCount = info.geometryCount;
         pInputs->pGeometries    = nullptr; // Converted by gpurt callback
    }

    pInputs->pClientData = pHelper;

    return result;
}

// =====================================================================================================================
VkResult AccelerationStructure::ConvertBuildSizeInputs(
    uint32_t                                           deviceIndex,
    const VkAccelerationStructureBuildGeometryInfoKHR& info,
    const uint32_t*                                    pMaxPrimitiveCounts,
    GeometryConvertHelper*                             pHelper,
    GpuRt::AccelStructBuildInputs*                     pInputs)
{
    VkResult result = VK_SUCCESS;

    pInputs->type  = ConvertAccelerationStructureType(info.type);
    pInputs->flags = ConvertAccelerationStructureFlags(info.mode, info.flags);

    pHelper->deviceIndex         = deviceIndex;
    pHelper->pBuildGeometries    = info.pGeometries;
    pHelper->ppBuildGeometries   = info.ppGeometries;
    pHelper->pMaxPrimitiveCounts = pMaxPrimitiveCounts;

    if (info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
    {
        pInputs->inputElemCount = 0;
        pInputs->instances.gpu  = 0;
        pInputs->pGeometries    = nullptr;

        VK_ASSERT(info.geometryCount == 1);
        pInputs->inputElemCount = pMaxPrimitiveCounts[0];
    }
    else
    {
        pInputs->inputElemCount = info.geometryCount;
        pInputs->pGeometries    = nullptr; // Converted by gpurt callback
    }

    pInputs->inputElemLayout = (info.pGeometries != nullptr) ?
        GpuRt::InputElementLayout::Array : GpuRt::InputElementLayout::ArrayOfPointers;

    pInputs->pClientData     = pHelper;

    return result;
}

// =====================================================================================================================
GpuRt::AccelStructCopyMode AccelerationStructure::ConvertCopyAccelerationStructureModeKHR(
    VkCopyAccelerationStructureModeKHR copyAccelerationStructureInfo)
{
    GpuRt::AccelStructCopyMode structCopyMode = GpuRt::AccelStructCopyMode::Clone;

    switch (copyAccelerationStructureInfo)
    {
    case VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR:
        structCopyMode = GpuRt::AccelStructCopyMode::Clone;
        break;
    case VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR:
        structCopyMode = GpuRt::AccelStructCopyMode::Compact;
        break;
    case VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR:
        structCopyMode = GpuRt::AccelStructCopyMode::Serialize;
        break;
    case VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR:
        structCopyMode = GpuRt::AccelStructCopyMode::Deserialize;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return structCopyMode;
}

// =====================================================================================================================
VkResult AccelerationStructure::CreateKHR(
    Device*                                     pDevice,
    const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkAccelerationStructureKHR*                 pAccelerationStructure)
{
    VkResult result = VK_SUCCESS;

    void* pMemory = pDevice->AllocApiObject(
        pAllocator,
        sizeof(AccelerationStructure));

    if (pMemory != nullptr)
    {
        if (result == VK_SUCCESS)
        {
            Buffer* pBuffer = Buffer::ObjectFromHandle(pCreateInfo->buffer);
            VK_ASSERT(pBuffer != nullptr);

            AccelerationStructure* pAccel = VK_PLACEMENT_NEW(pMemory) AccelerationStructure(
                pDevice,
                pBuffer,
                pCreateInfo->offset,
                pCreateInfo->size);

            *pAccelerationStructure = AccelerationStructure::HandleFromObject(pAccel);
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if ((result != VK_SUCCESS) && (pMemory != nullptr))
    {
        pDevice->FreeApiObject(pAllocator, pMemory);
    }

    return result;
}

// =====================================================================================================================
void AccelerationStructure::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);
}

// =====================================================================================================================
Pal::Result AccelerationStructure::Map(
    uint32_t    deviceIndex,
    void**      ppCpuAddr)
{
    Pal::Result result = m_pBuffer->PalMemory(deviceIndex)->Map(ppCpuAddr);
    if (result == Pal::Result::Success)
    {
        // add in offset of buffer with in the memory object
        // add in offset of acceleration with in the buffer object
        *ppCpuAddr = Util::VoidPtrInc(*ppCpuAddr, m_pBuffer->MemOffset() + m_bufferOffset);
    }

    return result;
}

// =====================================================================================================================
Pal::Result AccelerationStructure::Unmap(uint32_t deviceIndex)
{
    return m_pBuffer->PalMemory(deviceIndex)->Unmap();
}

// =====================================================================================================================
Pal::gpusize AccelerationStructure::GetGpuMemorySize(uint32_t deviceIndex) const
{
    return m_pBuffer->PalMemory(deviceIndex)->Desc().size;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureKHR(
    VkDevice                                    device,
    VkAccelerationStructureKHR                  accelerationStructure,
    const VkAllocationCallbacks*                pAllocator)
{
    if (accelerationStructure != VK_NULL_HANDLE)
    {
        Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        AccelerationStructure::ObjectFromHandle(accelerationStructure)->Destroy(pDevice, pAllocCB);
    };
}

// =====================================================================================================================
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetAccelerationStructureDeviceAddressKHR(
    VkDevice                                           device,
    const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    const AccelerationStructure* pAccel = AccelerationStructure::ObjectFromHandle(pInfo->accelerationStructure);

    return pAccel->GetDeviceAddress(0);
}

}; // namespace entry

#define SET_GEOM_FLAG(var, vkflag, gpurtFlag) \
    if (var & vkflag) \
    { \
        geometry.flags |= GpuRt::GeometryFlags(gpurtFlag); \
    }

// =====================================================================================================================
GpuRt::GpuCpuAddr AccelerationStructure::ConvertBufferAddress(
    bool                                 host,
    const VkDeviceOrHostAddressConstKHR& addr,
    VkDeviceSize                         offset)
{
    GpuRt::GpuCpuAddr address;

    if (host)
    {
        address.pCpu = Util::VoidPtrInc(addr.hostAddress, static_cast<ptrdiff_t>(offset));
    }
    else
    {
        address.gpu = addr.deviceAddress + offset;
    }

    return address;
}

// =====================================================================================================================
GpuRt::Geometry AccelerationStructure::ClientConvertAccelStructBuildGeometryKHR(
    bool                                                    hostBuild,
    const uint32_t                                          deviceIndex,
    const VkAccelerationStructureGeometryKHR*               pBuildInfo,
    const uint32_t*                                         pMaxPrimitiveCount,
    const VkAccelerationStructureBuildRangeInfoKHR*         pBuildRangeInfos)
{
    GpuRt::Geometry geometry = {};

    VkGeometryTypeKHR geometryType;
    VkGeometryFlagsKHR geometryFlags;
    VkIndexType indexType;
    VkFormat vertexFormat;
    VkDeviceOrHostAddressConstKHR transformAddr;
    VkDeviceOrHostAddressConstKHR indexAddr;
    VkDeviceOrHostAddressConstKHR vertexAddr;
    VkDeviceSize vertexStride;
    VkDeviceOrHostAddressConstKHR aabbAddr;
    VkDeviceSize aabbStride;

    VkDeviceSize transformOffset;
    VkDeviceSize primitiveOffset;
    uint32_t firstVertex;
    uint32_t primitiveCount;

    // Building
    VK_ASSERT(pBuildInfo != nullptr);

    geometryType  = pBuildInfo->geometryType;
    geometryFlags = pBuildInfo->flags;

    indexType     = pBuildInfo->geometry.triangles.indexType;
    vertexFormat  = pBuildInfo->geometry.triangles.vertexFormat;
    transformAddr = pBuildInfo->geometry.triangles.transformData;
    indexAddr     = pBuildInfo->geometry.triangles.indexData;
    vertexAddr    = pBuildInfo->geometry.triangles.vertexData;
    vertexStride  = pBuildInfo->geometry.triangles.vertexStride;

    aabbAddr      = pBuildInfo->geometry.aabbs.data;
    aabbStride    = pBuildInfo->geometry.aabbs.stride;

    primitiveCount = (pMaxPrimitiveCount != nullptr) ? *pMaxPrimitiveCount : pBuildRangeInfos->primitiveCount;

    if (pBuildRangeInfos != nullptr) // Non-indirect build.  For indirect builds, this structure is in GPU memory
    {
        firstVertex      = pBuildRangeInfos->firstVertex;
        transformOffset  = pBuildRangeInfos->transformOffset;
        primitiveOffset  = pBuildRangeInfos->primitiveOffset;
    }
    else
    {
        firstVertex      = 0;
        transformOffset  = 0;
        primitiveOffset  = 0;
    }

    if (geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
    {
        GpuRt::GeometryTriangles* pTriangles = &geometry.triangles;

        geometry.type = GpuRt::GeometryType::Triangles;

        GpuRt::GpuCpuAddr zeroAddress;
        zeroAddress.gpu = 0;

        pTriangles->columnMajorTransform3x4 = (transformAddr.deviceAddress != 0) ?
            ConvertBufferAddress(hostBuild, transformAddr, transformOffset) : zeroAddress;

        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            pTriangles->indexFormat = GpuRt::IndexFormat::R16_Uint;
            break;
        case VK_INDEX_TYPE_UINT32:
            pTriangles->indexFormat = GpuRt::IndexFormat::R32_Uint;
            break;
        default:
            pTriangles->indexFormat = GpuRt::IndexFormat::Unknown;
            break;
        }

        switch (vertexFormat)
        {
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R16G16B16A16_Float;
            break;
        case VK_FORMAT_R16G16B16A16_SNORM:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R16G16B16A16_Snorm;
            break;
        case VK_FORMAT_R16G16B16A16_UNORM:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R16G16B16A16_Unorm;
            break;
        case VK_FORMAT_R32G32B32_SFLOAT:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R32G32B32_Float;
            break;
        case VK_FORMAT_R32G32_SFLOAT:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R32G32_Float;
            break;
        case VK_FORMAT_R16G16_SFLOAT:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R16G16_Float;
            break;
        case VK_FORMAT_R16G16_SNORM:
            pTriangles->vertexFormat = GpuRt::VertexFormat::R16G16_Snorm;
            break;
        default:
            VK_NEVER_CALLED();
            pTriangles->vertexFormat = GpuRt::VertexFormat::R32G32B32_Float;
            break;
        }

        if ((indexType != VK_INDEX_TYPE_NONE_KHR) && (indexAddr.deviceAddress != 0))
        {
            pTriangles->indexCount       = primitiveCount * 3;
            pTriangles->indexBufferAddr  = ConvertBufferAddress(hostBuild, indexAddr, primitiveOffset);
            pTriangles->vertexCount      = pBuildInfo->geometry.triangles.maxVertex + 1;
            pTriangles->vertexBufferAddr = ConvertBufferAddress(hostBuild,
                                                                vertexAddr,
                                                                firstVertex * vertexStride);
        }
        else
        {
            pTriangles->indexFormat      = GpuRt::IndexFormat::Unknown;
            pTriangles->indexCount       = 0;
            pTriangles->indexBufferAddr  = {};
            pTriangles->vertexCount      = primitiveCount * 3;
            pTriangles->vertexBufferAddr = ConvertBufferAddress(hostBuild,
                                                                vertexAddr,
                                                                primitiveOffset);
        }

        pTriangles->vertexBufferByteStride = vertexStride;
    }
    else if (geometryType == VK_GEOMETRY_TYPE_AABBS_KHR)
    {
        geometry.type                 = GpuRt::GeometryType::Aabbs;

        geometry.aabbs.aabbCount      = primitiveCount;
        geometry.aabbs.aabbAddr       = ConvertBufferAddress(hostBuild, aabbAddr, primitiveOffset);
        geometry.aabbs.aabbByteStride = aabbStride;
    }
    else
    {
        // ClientConvertAccelStructBuildGeometryKHR should only be called for bottom level structures,
        // which should be VK_GEOMETRY_TYPE_TRIANGLES_KHR or VK_GEOMETRY_TYPE_AABBS_KHR
        VK_ASSERT(false);
    }

    geometry.flags = 0;

    SET_GEOM_FLAG(geometryFlags,
                  VK_GEOMETRY_OPAQUE_BIT_KHR,
                  GpuRt::GeometryFlag::Opaque);

    SET_GEOM_FLAG(geometryFlags,
                  VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
                  GpuRt::GeometryFlag::NoDuplicateAnyHitInvocation);

    return geometry;
}

// =====================================================================================================================
GpuRt::Geometry AccelerationStructure::ClientConvertAccelStructBuildGeometry(
    const GpuRt::AccelStructBuildInputs& inputs,
    uint32_t                             geometryIndex)
{
    // ClientConvertAccelStructBuildGeometry should be just be called for BottomLevel Structures
    VK_ASSERT(inputs.type == GpuRt::AccelStructType::BottomLevel);

    const vk::GeometryConvertHelper* pHelper = static_cast<const vk::GeometryConvertHelper*>(inputs.pClientData);

    const VkAccelerationStructureGeometryKHR* pBuildInfo = nullptr;

    if (inputs.inputElemLayout == GpuRt::InputElementLayout::Array)
    {
        pBuildInfo = (pHelper->pBuildGeometries != nullptr) ?
                      &pHelper->pBuildGeometries[geometryIndex] : nullptr;
    }
    else
    {
        // ArrayOfPointers
        pBuildInfo = (pHelper->ppBuildGeometries != nullptr) ?
                        pHelper->ppBuildGeometries[geometryIndex] : nullptr;
    }

    return ClientConvertAccelStructBuildGeometryKHR(
        pHelper->host,
        pHelper->deviceIndex,
        pBuildInfo,
        (pHelper->pMaxPrimitiveCounts != nullptr) ? &pHelper->pMaxPrimitiveCounts[geometryIndex] : nullptr,
        (pHelper->pBuildRangeInfos    != nullptr) ? &pHelper->pBuildRangeInfos[geometryIndex]    : nullptr);
}

// =====================================================================================================================
GpuRt::AccelStructPostBuildInfo AccelerationStructure::ClientConvertAccelStructPostBuildInfo(
    const GpuRt::AccelStructBuildInfo& buildInfo,
    uint32_t                           postBuildIndex)
{
    GpuRt::AccelStructPostBuildInfo info = {};

    // there are no post build calls in VK,
    // thus this should never get called from GpuRT
    VK_NEVER_CALLED();

    return info;
}

// =====================================================================================================================
GpuRt::InstanceBottomLevelInfo AccelerationStructure::ClientConvertAccelStructBuildInstanceBottomLevel(
    const GpuRt::AccelStructBuildInputs& inputs,
    uint32_t                      instanceIndex)
{
    const vk::GeometryConvertHelper* pHelper = static_cast<const vk::GeometryConvertHelper*>(inputs.pClientData);

    GpuRt::InstanceBottomLevelInfo blasInfo = {};

    // Extract the instance description from the host-provided instance geometry buffer
    vk::AccelerationStructure* pBlas = nullptr;

    vk::UnpackInstanceDesc(inputs, instanceIndex, &blasInfo.desc, &pBlas);

    // Get the required GPU and CPU addresses (the unmap will happen separately post-build
    void* pCpuAddr = nullptr;

    if ((pBlas != nullptr) &&
        (pBlas->Map(pHelper->deviceIndex, &pCpuAddr) == Pal::Result::Success))
    {
        blasInfo.pCpuAddr                   = pCpuAddr;
        blasInfo.desc.accelerationStructure = pBlas->GetDeviceAddress(pHelper->deviceIndex);
    }

    return blasInfo;
}

}; // namespace vk
