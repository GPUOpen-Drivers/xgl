/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  gpu_decode_layer.cpp
* @brief Implementation of gpu decode layer.
***********************************************************************************************************************
*/
#if VKI_GPU_DECOMPRESS
#include "gpu_decode_layer.h"
#include "include/vk_image.h"
#include "palImage.h"
#include "palHashMapImpl.h"
#include "palListImpl.h"
#include "palUtil.h"
#include "palInlineFuncs.h"
#include "include/vk_buffer.h"
#include "include/vk_utils.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"

namespace GpuTexDecoder
{
    Pal::Result ClientCreateInternalComputePipeline(
        const DeviceInitInfo&       initInfo,
        const CompileTimeConstants& constInfo,
        const PipelineBuildInfo&    buildInfo,
        Pal::IPipeline**            ppResultPipeline,
        void**                      ppResultMemory)
    {
        VkResult result = VK_SUCCESS;
        vk::Device* pDevice = reinterpret_cast<vk::Device*>(initInfo.pClientUserData);
        VkSpecializationMapEntry mapEntries[4] =
        {
            // local_thread_x
            {
                0,
                0,
                sizeof(uint32_t)
            },

            // local_thread_y
            {
                1,
                1 * sizeof(uint32_t),
                sizeof(uint32_t)
            },

            // isSrgb Format
            {
                2,
                2 * sizeof(uint32_t),
                sizeof(uint32_t)
            },

            // isBufferTexture
            {
                3,
                3 * sizeof(uint32_t),
                sizeof(uint32_t)
            }

        };

        VkSpecializationInfo specializationInfo =
        {
            constInfo.numConstants,
            &mapEntries[0],
            constInfo.numConstants * sizeof(uint32_t),
            constInfo.pConstants
        };

        Vkgc::ResourceMappingRootNode rootNode = {};
        Vkgc::ResourceMappingNode nodes[GpuTexDecoder::AstcInternalPipelineNodes] = {};
        if (buildInfo.shaderType == GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8)
        {
            GpuTexDecoder::GpuDecodeMappingNode* pDecodeNode = buildInfo.pUserDataNodes;
            for (size_t index = 0; index < GpuTexDecoder::AstcInternalPipelineNodes; index++)
            {
                if (pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::Image)
                {
                    nodes[index].type = Vkgc::ResourceMappingNodeType::DescriptorResource;
                    nodes[index].sizeInDwords = pDecodeNode[index].sizeInDwords;
                    nodes[index].offsetInDwords = pDecodeNode[index].offsetInDwords;
                    nodes[index].srdRange.binding = pDecodeNode[index].binding;
                    nodes[index].srdRange.set = pDecodeNode[index].set;
                }
                else
                {
                    Vkgc::ResourceMappingNodeType vkgcType =
                        (pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::Buffer) ?
                        Vkgc::ResourceMappingNodeType::DescriptorBuffer :
                        Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
                    nodes[index].type = vkgcType;
                    nodes[index].sizeInDwords = pDecodeNode[index].sizeInDwords;
                    nodes[index].offsetInDwords = pDecodeNode[index].offsetInDwords;
                    nodes[index].srdRange.binding = pDecodeNode[index].binding;
                    nodes[index].srdRange.set = pDecodeNode[index].set;
                }
            }

            rootNode.node.type = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
            rootNode.node.offsetInDwords = 0;
            rootNode.node.sizeInDwords = 1;
            rootNode.node.tablePtr.nodeCount = GpuTexDecoder::AstcInternalPipelineNodes;
            rootNode.node.tablePtr.pNext = &nodes[0];
            rootNode.visibility = Vkgc::ShaderStageComputeBit;
        }

        Vkgc::BinaryData spvBin = { buildInfo.code.spvSize, buildInfo.code.pSpvCode };

        result = pDevice->CreateInternalComputePipeline(buildInfo.code.spvSize,
            static_cast<const uint8_t*>(buildInfo.code.pSpvCode),
            buildInfo.nodeCount,
            &rootNode,
            0,
            false,//forceWave64,
            &specializationInfo,
            &pDevice->GetInternalTexDecodePipeline());

        *ppResultPipeline = pDevice->GetInternalTexDecodePipeline().pPipeline[0];

        return result == VK_SUCCESS ? Pal::Result::Success : Pal::Result::ErrorUnknown;
    }
}

namespace vk
{
// =====================================================================================================================
GpuDecoderLayer::GpuDecoderLayer(
    Device* pDevice)
    :
    m_pDevice(pDevice)
{
}

// =====================================================================================================================
GpuDecoderLayer::~GpuDecoderLayer()
{
    if(m_pGpuTexDecoder != nullptr)
    {
        m_pDevice->VkInstance()->FreeMem(m_pGpuTexDecoder);
        m_pGpuTexDecoder = nullptr;
    }
}

VkResult GpuDecoderLayer::Init(Device* pDevice)
{
    VkResult result = VK_SUCCESS;

    void* pMemory = m_pDevice->VkInstance()->AllocMem(
        sizeof(GpuTexDecoder::Device),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        GpuTexDecoder::DeviceInitInfo initInfo = {};
        initInfo.pDeviceProperties = &pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
        initInfo.gpuIdx = DefaultDeviceIndex;
        initInfo.pClientUserData = pDevice;
        initInfo.pPalDevice = pDevice->PalDevice(DefaultDeviceIndex);
        initInfo.pPlatform = pDevice->VkInstance()->PalPlatform();

        m_pGpuTexDecoder = VK_PLACEMENT_NEW(pMemory) GpuTexDecoder::Device();
        m_pGpuTexDecoder->Init(initInfo);
    }

    return result;
}

namespace entry
{
namespace gpuDecoderWapper
{
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageCopy*                          pRegions)
{
    CmdBuffer* pCmdBuffer          = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);
    Device* pDevice                = pCmdBuffer->VkDevice();
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();

    const Image* const pSrcImage        = Image::ObjectFromHandle(srcImage);
    const Image* const pDstImage        = Image::ObjectFromHandle(dstImage);

    const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat(), pDevice->GetRuntimeSettings());
    const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat(), pDevice->GetRuntimeSettings());

    if (Formats::IsASTCFormat(pDstImage->GetFormat()))
    {
        VK_ASSERT(Formats::IsASTCFormat(pDstImage->GetFormat()));
        uint32_t maxObj = pCmdBuffer->EstimateMaxObjectsOnVirtualStack(sizeof(Pal::ImageCopyRegion));

        const auto maxRegions = Util::Max(maxObj, MaxPalAspectsPerMask);
        auto       regionBatch = Util::Min(regionCount * MaxPalAspectsPerMask, maxRegions);

        VirtualStackFrame virtStackFrame(pCmdBuffer->GetStackAllocator());
        Pal::ImageCopyRegion* pPalRegions =
            virtStackFrame.AllocArray<Pal::ImageCopyRegion>(regionBatch);

        VkFormat format = pDstImage->GetFormat();
        AstcMappedInfo mapInfo = {};
        Formats::GetAstcMappedInfo(format, &mapInfo);
        uint32_t const_data[4] =
        {
            // local_thread_x
            mapInfo.wScale,
            // local_thread_y
            mapInfo.hScale,
            // is srgb
            pDecodeWrapper->isAstcSrgbaFormat(format),
            // is buffer copy
            false
        };

        GpuTexDecoder::CompileTimeConstants constInfo = {};
        constInfo.numConstants = 4;
        constInfo.pConstants = const_data;

        for (uint32_t regionIdx = 0; regionIdx < regionCount;)
        {
            uint32_t palRegionCount = 0;

            while ((regionIdx < regionCount) &&
                (palRegionCount <= (regionBatch - MaxPalAspectsPerMask)))
            {
                VkToPalImageCopyRegion(pRegions[regionIdx], srcFormat.format, dstFormat.format,
                    pPalRegions, &palRegionCount);

                ++regionIdx;
            }

            pDevice->GetGpuDecoderLayer()->GetTexDecoder()->GpuDecodeImage(
                GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8,
                pCmdBuffer->PalCmdBuffer(DefaultDeviceIndex),
                pSrcImage->PalImage(DefaultDeviceIndex),
                pDstImage->PalImage(DefaultDeviceIndex),
                regionCount, pPalRegions, constInfo);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        DECODER_WAPPER_CALL_NEXT_LAYER(vkCmdCopyImage(cmdBuffer,
                srcImage,
                srcImageLayout,
                dstImage,
                dstImageLayout,
                regionCount,
                pRegions));
    }
}

// ====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo*                    pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkImage*                                    pImage)
{
    VkResult vkResult = VK_SUCCESS;
    VkFormat format = pCreateInfo->format;
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();

    if (Formats::IsASTCFormat(format) &&
        (pCreateInfo->usage == VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        AstcMappedInfo mapInfo = {};
        Formats::GetAstcMappedInfo(format, &mapInfo);

        VkImageCreateInfo astcSrcInfo = *pCreateInfo;
        VkExtent3D extent =
        {
            (astcSrcInfo.extent.width  + mapInfo.wScale - 1) / mapInfo.wScale,
            (astcSrcInfo.extent.height + mapInfo.hScale - 1) / mapInfo.hScale,
             astcSrcInfo.extent.depth
        };

        astcSrcInfo.format = VK_FORMAT_R32G32B32A32_UINT;
        astcSrcInfo.extent = extent;
        vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(device,
                                                                 &astcSrcInfo,
                                                                 pAllocator,
                                                                 pImage);

    }
    else
    {
        vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(device,
                                                                 pCreateInfo,
                                                                 pAllocator,
                                                                 pImage);
    }

    return vkResult;
}

}// namespace gpuDecoderWapper
}// namespace entry

// =====================================================================================================================
void GpuDecoderLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    DECODER_WAPPER_OVERRIDE_ENTRY(vkCreateImage);
    DECODER_WAPPER_OVERRIDE_ENTRY(vkCmdCopyImage);
}
} // namespace vk
#endif  /* VKI_GPU_DECOMPRESS */
