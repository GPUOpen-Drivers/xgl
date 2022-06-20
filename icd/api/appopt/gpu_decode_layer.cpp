/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_buffer.h"
#include "include/vk_utils.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"

namespace GpuTexDecoder
{

// ====================================================================================================================
Pal::Result ClientCreateInternalComputePipeline(
    const DeviceInitInfo&        initInfo,
    const CompileTimeConstants&  constInfo,
    const PipelineBuildInfo&     buildInfo,
    Pal::IPipeline**             ppResultPipeline,
    void**                       ppResultMemory)
{
    VkResult result = VK_SUCCESS;
    vk::Device* pDevice = reinterpret_cast<vk::Device *>(initInfo.pClientUserData);

    VK_ASSERT(constInfo.numConstants <= 6);
    VkSpecializationMapEntry mapEntries[6] =
        {
            // local_thread_x - ASTC
            // ALPHA_BITS - ETC2
            // g_num_block_x - DXT5
            {
                0,
                0,
                sizeof(uint32_t)
            },

            // local_thread_y - ASTC
            // WIDTH = ETC2
            // g_start_block_id - DXT5
            {
                1,
                1 * sizeof(uint32_t),
                sizeof(uint32_t)
            },

            // isSrgb Format - ASTC
            // HEIGHT - ETC2
            // g_quality - DXT5
            {
                2,
                2 * sizeof(uint32_t),
                sizeof(uint32_t)
            },

            // isBufferTexture - ASTC
            // eacComponent -ETC2
            {
                3,
                3 * sizeof(uint32_t),
                sizeof(uint32_t)
            },
            // isSinged -ETC2/EAC
            {
                4,
                4 * sizeof(uint32_t),
                sizeof(uint32_t)
            },
            // isBuffer -ETC2
            {
                5,
                5 * sizeof(uint32_t),
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
    // use the max node type here, ASTC has 7 nodes which is the maximum right now
    Vkgc::ResourceMappingNode nodes[GpuTexDecoder::AstcInternalPipelineNodes] = {};

    uint32_t nodeCount = 0;
    if (buildInfo.shaderType == GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        nodeCount = GpuTexDecoder::AstcInternalPipelineNodes;
    }
    else if (buildInfo.shaderType == GpuTexDecoder::InternalTexConvertCsType::ConvertETC2ToRGBA8)
    {
        nodeCount = GpuTexDecoder::Etc2InternalPipelineNodes;
    }
    else
    {
        VK_ASSERT(buildInfo.shaderType == GpuTexDecoder::InternalTexConvertCsType::ConvertRGBA8ToDXT5);
        nodeCount = GpuTexDecoder::DXT5InternalPipelineNodes;
    }

    GpuTexDecoder::GpuDecodeMappingNode *pDecodeNode = buildInfo.pUserDataNodes;
    for (size_t index = 0; index < nodeCount; index++)
    {
        if (pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::Image)
        {
            nodes[index].type = Vkgc::ResourceMappingNodeType::DescriptorResource;
        }
        else if (pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::TexBuffer)
        {
            nodes[index].type = Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
        }
        else if (pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::Buffer)
        {
             nodes[index].type = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
        }
        else
        {
            VK_ASSERT(pDecodeNode[index].nodeType == GpuTexDecoder::NodeType::PushConstant);
            nodes[index].type = Vkgc::ResourceMappingNodeType::PushConst;
            VK_ASSERT(Vkgc::InternalDescriptorSetId == pDecodeNode[index].set);
        }

        nodes[index].sizeInDwords = pDecodeNode[index].sizeInDwords;
        nodes[index].offsetInDwords = pDecodeNode[index].offsetInDwords;
        nodes[index].srdRange.binding = pDecodeNode[index].binding;
        nodes[index].srdRange.set = pDecodeNode[index].set;
    }

    rootNode.node.type = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
    rootNode.node.offsetInDwords = 0;
    rootNode.node.sizeInDwords = 1;
    rootNode.node.tablePtr.nodeCount = nodeCount;
    rootNode.node.tablePtr.pNext = &nodes[0];
    rootNode.visibility = Vkgc::ShaderStageComputeBit;

    Vkgc::BinaryData spvBin = {buildInfo.code.spvSize, buildInfo.code.pSpvCode};

    result = pDevice->CreateInternalComputePipeline(
        buildInfo.code.spvSize,
        static_cast<const uint8_t *>(buildInfo.code.pSpvCode),
        buildInfo.nodeCount,
        &rootNode,
        0,
        false, // forceWave64,
        &specializationInfo,
        &pDevice->GetInternalTexDecodePipeline());

    *ppResultPipeline = pDevice->GetInternalTexDecodePipeline().pPipeline[0];

    return result == VK_SUCCESS ? Pal::Result::Success : Pal::Result::ErrorUnknown;
}

// ====================================================================================================================
void ClientDestroyInternalComputePipeline(
    const DeviceInitInfo& initInfo,
    Pal::IPipeline*       pPipeline,
    void*                 pMemory)
{
    vk::Device *pDevice = reinterpret_cast<vk::Device *>(initInfo.pClientUserData);

    if (pMemory == nullptr)
    {
        pMemory = pPipeline;
    }

    pPipeline->Destroy();
    pPipeline = nullptr;
    pDevice->VkInstance()->FreeMem(pMemory);
}

} // namespace GpuTexDecoder

namespace vk
{
// =====================================================================================================================
GpuDecoderLayer::GpuDecoderLayer(
    Device *pDevice)
    :
    m_pDevice(pDevice),
    m_cachedStagingRes(8, pDevice->VkInstance()->GetPrivateAllocator()),
    m_decodedImages(8, pDevice->VkInstance()->GetPrivateAllocator())
{
}

// =====================================================================================================================
GpuDecoderLayer::~GpuDecoderLayer()
{
    if (m_pGpuTexDecoder != nullptr)
    {
        Util::Destructor(m_pGpuTexDecoder);
        m_pGpuTexDecoder = nullptr;
        m_cachedStagingRes.Reset();
        m_decodedImages.Reset();
    }
}

// ====================================================================================================================
VkResult GpuDecoderLayer::Init(
    Device *pDevice)
{
    VkResult result = VK_SUCCESS;

    void *pMemory = m_pDevice->VkInstance()->AllocMem(
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
        m_cachedStagingRes.Init();
        m_decodedImages.Init();
    }

    return result;
}

// ====================================================================================================================
uint32_t GpuDecoderLayer::FindMemoryType(
    Device*               device,
    uint32_t              typeFilter,
    VkMemoryPropertyFlags properties)
{
    GpuDecoderLayer* pDecodeWrapper = this;
    VkDevice apiDev = VkDevice(ApiDevice::FromObject(device));
    const VkPhysicalDeviceMemoryProperties& memProperties =
        device->VkPhysicalDevice(DefaultDeviceIndex)->GetMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    VK_ASSERT(0); // wrong request
    return 0;
}

// =====================================================================================================================
VkImage GpuDecoderLayer::CreateStagingImage(
    Device*         device,
    VkCommandBuffer userCmdBuf,
    VkImage         dstImage)
{
    GpuDecoderLayer* pDecodeWrapper = this;
    const Image* const pDstImage = Image::ObjectFromHandle(dstImage);
    Pal::IImage* pPalImage = pDstImage->PalImage(DefaultDeviceIndex);

    StagingResourcePair* pResPair = m_cachedStagingRes.FindKey(dstImage);

    if (pResPair != nullptr)
    {
        return pResPair->image;
    }

    VkImageCreateInfo imageInfo;

    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.imageType = (VkImageType)pPalImage->GetImageCreateInfo().imageType;
    imageInfo.extent.width = pPalImage->GetImageCreateInfo().extent.width;
    imageInfo.extent.height = pPalImage->GetImageCreateInfo().extent.height;
    imageInfo.extent.depth = pPalImage->GetImageCreateInfo().extent.depth;
    imageInfo.mipLevels = pPalImage->GetImageCreateInfo().mipLevels;
    imageInfo.arrayLayers = pPalImage->GetImageCreateInfo().arraySize;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices = nullptr;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VkDevice apiDev = VkDevice(ApiDevice::FromObject(device));

    VkImage stagingImage;

    VkResult vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(
        apiDev,
        &imageInfo,
        0,
        &stagingImage);

    VK_ASSERT(vkResult == VK_SUCCESS);

    VkMemoryRequirements memRequirements;
    DECODER_WAPPER_CALL_NEXT_LAYER(vkGetImageMemoryRequirements)
        (apiDev, stagingImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(device, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory stagingImageMemory;
    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkAllocateMemory)(apiDev, &allocInfo, nullptr, &stagingImageMemory);

    VK_ASSERT(vkResult == VK_SUCCESS);

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkBindImageMemory)(apiDev, stagingImage, stagingImageMemory, 0);

    VK_ASSERT(vkResult == VK_SUCCESS);

    VkCommandPoolCreateInfo cmdPoolInfo;
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.pNext = nullptr;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cmdPoolInfo.queueFamilyIndex = 0;

    VkCommandPool cmdPool;
    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateCommandPool)(apiDev, &cmdPoolInfo, nullptr, &cmdPool);
    VK_ASSERT(vkResult == VK_SUCCESS);

    VkCommandBufferAllocateInfo cmdAllocInfo;

    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;
    cmdAllocInfo.commandPool = cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf;

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkAllocateCommandBuffers)(apiDev, &cmdAllocInfo, &cmdBuf);
    VK_ASSERT(vkResult == VK_SUCCESS);

    VkCommandBufferInheritanceInfo inheriInfo;
    inheriInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheriInfo.pNext = nullptr;
    inheriInfo.renderPass = VK_NULL_HANDLE;
    inheriInfo.subpass = 0;
    inheriInfo.framebuffer = VK_NULL_HANDLE;
    inheriInfo.occlusionQueryEnable = 0;
    inheriInfo.queryFlags = 0;
    inheriInfo.pipelineStatistics = 0;

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = &inheriInfo;

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkBeginCommandBuffer)(cmdBuf, &beginInfo);
    VK_ASSERT(vkResult == VK_SUCCESS);

    VkImageSubresourceRange subRange;
    subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subRange.baseMipLevel = 0;
    subRange.levelCount = imageInfo.mipLevels;
    subRange.baseArrayLayer = 0;
    subRange.layerCount = imageInfo.arrayLayers;

    VkImageMemoryBarrier imageBarrier;
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = 0;
    imageBarrier.dstQueueFamilyIndex = 0;
    imageBarrier.image = stagingImage;
    imageBarrier.subresourceRange = subRange;

    DECODER_WAPPER_CALL_NEXT_LAYER(vkCmdPipelineBarrier)
        (cmdBuf,
         VK_PIPELINE_STAGE_HOST_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         0,
         0,
         nullptr,
         0,
         nullptr,
         1,
         &imageBarrier);

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkEndCommandBuffer)(cmdBuf);
    VK_ASSERT(vkResult == VK_SUCCESS);

    VkQueue queue;
    DECODER_WAPPER_CALL_NEXT_LAYER(vkGetDeviceQueue)(apiDev, 0, 0, &queue);

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkQueueSubmit)(queue, 1, &submitInfo, VK_NULL_HANDLE);
    VK_ASSERT(vkResult == VK_SUCCESS);

    vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkQueueWaitIdle)(queue);
    VK_ASSERT(vkResult == VK_SUCCESS);

    DECODER_WAPPER_CALL_NEXT_LAYER(vkFreeCommandBuffers)(apiDev, 0, 1, &cmdBuf);

    DECODER_WAPPER_CALL_NEXT_LAYER(vkDestroyCommandPool)(apiDev, cmdPool, nullptr);

    StagingResourcePair resPair;
    resPair.image = stagingImage;
    resPair.memory = stagingImageMemory;
    m_cachedStagingRes.Insert(dstImage, resPair);

    return stagingImage;
}

// ====================================================================================================================
void GpuDecoderLayer::ClearStagingResources(
    VkImage image)
{
    GpuDecoderLayer* pDecodeWrapper = this;
    StagingResourcePair* pResPair   = m_cachedStagingRes.FindKey(image);

    if (pResPair != nullptr)
    {
        DECODER_WAPPER_CALL_NEXT_LAYER(vkFreeMemory)(VkDevice(ApiDevice::FromObject(m_pDevice)), pResPair->memory, nullptr);
        DECODER_WAPPER_CALL_NEXT_LAYER(vkDestroyImage)(VkDevice(ApiDevice::FromObject(m_pDevice)), pResPair->image, nullptr);
    }
}

namespace entry
{
namespace gpuDecoderWapper
{

// ====================================================================================================================
static uint32_t getAlphaBits(
    VkFormat format)
{
    uint32_t alphaBits = 0;

    switch (format)
    {
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        alphaBits = 0;
        break;
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        alphaBits = 1;
        break;
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        alphaBits = 8;
        break;
    default:
        VK_ASSERT(0); // should not happen
        break;
    }

    return alphaBits;
}

// ====================================================================================================================
static uint32_t GetEacComponents(
    VkFormat format)
{
    uint32_t comp = 0;
    switch (format)
    {
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        comp = 2;
        break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        comp = 1;
        break;
    default:
        break;
    }
    return comp;
}

// ====================================================================================================================
static uint32_t GetEacSigned(
    VkFormat format)
{
    uint32_t isSigned = 0;
    switch (format)
    {
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        isSigned = 1;
        break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        isSigned = 0;
        break;
    default:
        break;
    }
    return isSigned;
}

// =====================================================================================================================
static VkFormat getETC2SourceViewFormat(
    VkFormat format)
{
    VkFormat sourceFormat = VK_FORMAT_R32G32B32A32_UINT;
    switch (format)
    {
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        sourceFormat = VK_FORMAT_R32G32B32A32_UINT;
        break;
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        sourceFormat = VK_FORMAT_R32G32_UINT;
        break;
    default:
        VK_ASSERT(0); // should not happen
        break;
    }
    return sourceFormat;
}

// =====================================================================================================================
static VkResult gpuBlitImage(
    CmdBuffer*                              pCmdBuffer,
    const Image *const                      pSrcImage,
    const Image *const                      pDstImage,
    GpuTexDecoder::InternalTexConvertCsType type,
    uint32_t                                regionCount,
    const VkImageCopy*                      pRegions)
{
    Device* pDevice = pCmdBuffer->VkDevice();
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat(), pDevice->GetRuntimeSettings());
    const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat(), pDevice->GetRuntimeSettings());
    uint32_t maxObj = pCmdBuffer->EstimateMaxObjectsOnVirtualStack(sizeof(Pal::ImageCopyRegion));

    const auto maxRegions = Util::Max(maxObj, MaxPalAspectsPerMask);
    auto regionBatch = Util::Min(regionCount * MaxPalAspectsPerMask, maxRegions);

    VirtualStackFrame virtStackFrame(pCmdBuffer->GetStackAllocator());
    Pal::ImageCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::ImageCopyRegion>(regionBatch);

    uint32_t const_data[3];
    GpuTexDecoder::CompileTimeConstants constInfo = {};

    switch (type)
    {
        case GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8:
        {
            VkFormat format = pDstImage->GetFormat();
            AstcMappedInfo mapInfo = {};
            Formats::GetAstcMappedInfo(format, &mapInfo);
            const_data[0] = mapInfo.wScale;
            const_data[1] = mapInfo.hScale;
            const_data[2] = pDecodeWrapper->IsAstcSrgbaFormat(format);
            // only 2 constants specialized
            constInfo.numConstants = 2;
            constInfo.pConstants = const_data;
            break;
        }
        case GpuTexDecoder::InternalTexConvertCsType::ConvertETC2ToRGBA8:
        {
            const_data[0] = getAlphaBits(pDstImage->GetFormat());
            const_data[1] = GetEacComponents(pDstImage->GetFormat());
            const_data[2] = GetEacSigned(pDstImage->GetFormat());
            // no specialized constant
            constInfo.numConstants = 0;
            constInfo.pConstants = const_data;
            break;
        }
        case GpuTexDecoder::InternalTexConvertCsType::ConvertRGBA8ToDXT5:
        {
            const Pal::ImageCreateInfo &imageInfo = pSrcImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo();
            const_data[0] =  (imageInfo.extent.width + 3) / 4; //num_block_x
            const_data[1] = 0; // start_block
            const_data[2] =  1.0; // quality
            constInfo.numConstants = 3;
            constInfo.pConstants = const_data;
            break;
        }
        default:
            VK_ASSERT(0); // should not happen
            break;
    }

    VkFormat format = pDstImage->GetFormat();
    for (uint32_t regionIdx = 0; regionIdx < regionCount;)
    {
        uint32_t palRegionCount = 0;

        while ((regionIdx < regionCount) &&
               (palRegionCount <= (regionBatch - MaxPalAspectsPerMask)))
        {
            Pal::SwizzledFormat dstFormat = vk::convert::VkToPalSwizzledFormatLookupTableStorage[format];
            VkToPalImageCopyRegion(
                pRegions[regionIdx],
                dstFormat.format,
                dstFormat.format,
                pPalRegions,
                &palRegionCount);

            ++regionIdx;
        }

        pDevice->GetGpuDecoderLayer()->GetTexDecoder()->GpuDecodeImage(
            type,
            pCmdBuffer->PalCmdBuffer(DefaultDeviceIndex),
            pSrcImage->PalImage(DefaultDeviceIndex),
            pDstImage->PalImage(DefaultDeviceIndex),
            regionCount, pPalRegions, constInfo);
    }

    virtStackFrame.FreeArray(pPalRegions);

    return VK_SUCCESS;
}

// =====================================================================================================================
static VkResult gpuBlitBuffer(
    CmdBuffer*                              pCmdBuffer,
    const Buffer*const                      pSrcBuffer,
    const Image*const                       pDstImage,
    GpuTexDecoder::InternalTexConvertCsType type,
    uint32_t                                regionCount,
    const VkBufferImageCopy*                pRegions)
{
    Device* pDevice = pCmdBuffer->VkDevice();
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    const auto maxRegions  = pCmdBuffer->EstimateMaxObjectsOnVirtualStack(sizeof(Pal::MemoryImageCopyRegion));
    auto regionBatch = Util::Min(regionCount, maxRegions);

    VirtualStackFrame virtStackFrame(pCmdBuffer->GetStackAllocator());
    Pal::MemoryImageCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::MemoryImageCopyRegion>(regionBatch);

    uint32_t const_data[3];
    GpuTexDecoder::CompileTimeConstants constInfo = {};
    Pal::SwizzledFormat sourceViewFormat = {};

    switch (type)
    {
        case GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8:
        {
            VkFormat format = pDstImage->GetFormat();
            AstcMappedInfo mapInfo = {};
            Formats::GetAstcMappedInfo(format, &mapInfo);
            const_data[0] = mapInfo.wScale;
            const_data[1] = mapInfo.hScale;
            const_data[2] = pDecodeWrapper->IsAstcSrgbaFormat(format);
            constInfo.numConstants = 2;
            constInfo.pConstants = const_data;
            sourceViewFormat = VkToPalFormat(VK_FORMAT_R32G32B32A32_UINT, pDevice->GetRuntimeSettings());
            break;
        }
        case GpuTexDecoder::InternalTexConvertCsType::ConvertETC2ToRGBA8:
        {
            const_data[0] = getAlphaBits(pDstImage->GetFormat());
            const_data[1] = GetEacComponents(pDstImage->GetFormat());
            const_data[2] = GetEacSigned(pDstImage->GetFormat());
            // no specialized constant
            constInfo.numConstants = 0;
            constInfo.pConstants = const_data;
            sourceViewFormat = VkToPalFormat(getETC2SourceViewFormat(pDstImage->GetFormat()),pDevice->GetRuntimeSettings());
            break;
        }
        default:
            VK_ASSERT(0); // should not happen
            break;
    }

    for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
    {
        regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

        for (uint32_t i = 0; i < regionBatch; ++i)
        {
            // For image-buffer copies we have to override the format for depth-only and stencil-only copies
            VkFormat format = Formats::GetAspectFormat(pDstImage->GetFormat(),pRegions[regionIdx + i].imageSubresource.aspectMask);
            VK_ASSERT(VK_ENUM_IN_RANGE(format, VK_FORMAT));
            Pal::SwizzledFormat dstFormat = vk::convert::VkToPalSwizzledFormatLookupTableStorage[format];

            uint32 plane = VkToPalImagePlaneSingle(
                pDstImage->GetFormat(),
                pRegions[regionIdx + i].imageSubresource.aspectMask,
                pDevice->GetRuntimeSettings());

            pPalRegions[i] = VkToPalMemoryImageCopyRegion(
                pRegions[regionIdx + i],
                dstFormat.format,
                plane,
                pSrcBuffer->MemOffset());
        }

        pDevice->GetGpuDecoderLayer()->GetTexDecoder()->GpuDecodeBuffer(
            type,
            pCmdBuffer->PalCmdBuffer(DefaultDeviceIndex),
            pSrcBuffer->PalMemory(DefaultDeviceIndex),
            pDstImage->PalImage(DefaultDeviceIndex),
            regionCount,
            pPalRegions,
            constInfo,
            sourceViewFormat);
    }

    virtStackFrame.FreeArray(pPalRegions);

    return VK_SUCCESS;
}
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer    cmdBuffer,
    VkImage            srcImage,
    VkImageLayout      srcImageLayout,
    VkImage            dstImage,
    VkImageLayout      dstImageLayout,
    uint32_t           regionCount,
    const VkImageCopy* pRegions)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);
    Device* pDevice = pCmdBuffer->VkDevice();
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    const Image* const pSrcImage = Image::ObjectFromHandle(srcImage);
    const Image* const pDstImage = Image::ObjectFromHandle(dstImage);
    VkResult result;

    GpuTexDecoder::InternalTexConvertCsType convType = GpuTexDecoder::InternalTexConvertCsType::Count;

    if (Formats::IsASTCFormat(pDstImage->GetFormat()))
    {
        convType = GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8;
    }
    else if (Formats::IsEtc2Format(pDstImage->GetFormat()))
    {
        convType = GpuTexDecoder::InternalTexConvertCsType::ConvertETC2ToRGBA8;
    }

    if (Formats::IsASTCFormat(pSrcImage->GetFormat()) || Formats::IsEtc2Format(pSrcImage->GetFormat()))
    {
        //app can call vkCmdCopyBufferToImage before vkCmdCopyImage
        //if the srcImage has been already decoded in buffer copy, just skip decode process
        if (pDecodeWrapper->IsImageDecoded(srcImage))
        {
            convType = GpuTexDecoder::InternalTexConvertCsType::Count;
        }
    }

    if (convType != GpuTexDecoder::InternalTexConvertCsType::Count)
    {
        if (settings.enableBC5Encoder)
        {
            VkImage stagingImage = pDevice->GetGpuDecoderLayer()->CreateStagingImage(
                pDevice,
                cmdBuffer,
                dstImage);

            const Image *const pStagingImage = Image::ObjectFromHandle(stagingImage);

            result = gpuBlitImage(pCmdBuffer,
                         pSrcImage,
                         pStagingImage,
                         convType,
                         regionCount,
                         pRegions);

            result = gpuBlitImage(pCmdBuffer,
                         pStagingImage,
                         pDstImage,
                         GpuTexDecoder::InternalTexConvertCsType::ConvertRGBA8ToDXT5,
                         regionCount,
                         pRegions);
        }
        else
        {
            result = gpuBlitImage(pCmdBuffer,
                         pSrcImage,
                         pDstImage,
                         convType,
                         regionCount,
                         pRegions);
        }
    }
    else
    {
        DECODER_WAPPER_CALL_NEXT_LAYER(vkCmdCopyImage(
            cmdBuffer,
            srcImage,
            srcImageLayout,
            dstImage,
            dstImageLayout,
            regionCount,
            pRegions));
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32                                      regionCount,
    const VkBufferImageCopy*                    pRegions)
{
    VkResult result        = VkResult::VK_ERROR_UNKNOWN;
    CmdBuffer* pCmdBuffer  = ApiCmdBuffer::ObjectFromHandle(commandBuffer);
    Device* pDevice        = pCmdBuffer->VkDevice();
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    Image*  pDstImage      = Image::ObjectFromHandle(dstImage);
    Buffer* pSrcBuffer     = Buffer::ObjectFromHandle(srcBuffer);

    GpuTexDecoder::InternalTexConvertCsType convType = GpuTexDecoder::InternalTexConvertCsType::Count;

    if (Formats::IsASTCFormat(pDstImage->GetFormat()))
    {
        convType = GpuTexDecoder::InternalTexConvertCsType::ConvertASTCToRGBA8;
    }
    else if (Formats::IsEtc2Format(pDstImage->GetFormat()))
    {
        convType = GpuTexDecoder::InternalTexConvertCsType::ConvertETC2ToRGBA8;
    }

    if (convType != GpuTexDecoder::InternalTexConvertCsType::Count)
    {
        result = gpuBlitBuffer(
            pCmdBuffer,
            pSrcBuffer,
            pDstImage,
            convType,
            regionCount,
            pRegions);

        VK_ASSERT(result == VK_SUCCESS);
        pDecodeWrapper->AddDecodedImage(dstImage);
    }
    else
    {
        DECODER_WAPPER_CALL_NEXT_LAYER(vkCmdCopyBufferToImage)(
            commandBuffer,
            srcBuffer,
            dstImage,
            dstImageLayout,
            regionCount,
            pRegions);
    }
}

// ====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(
    VkDevice                     device,
    const VkImageCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkImage*                     pImage)
{
    VkResult vkResult = VK_SUCCESS;
    VkFormat format = pCreateInfo->format;
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if (Formats::IsASTCFormat(format) &&
        (pCreateInfo->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        VK_ASSERT(!(pCreateInfo->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT));
        AstcMappedInfo mapInfo = {};
        Formats::GetAstcMappedInfo(format, &mapInfo);
        VkImageCreateInfo astcSrcInfo = *pCreateInfo;
        VkExtent3D extent =
        {
            (astcSrcInfo.extent.width + mapInfo.wScale - 1) / mapInfo.wScale,
            (astcSrcInfo.extent.height + mapInfo.hScale - 1) / mapInfo.hScale,
            astcSrcInfo.extent.depth
        };

        astcSrcInfo.format = VK_FORMAT_R32G32B32A32_UINT;
        astcSrcInfo.extent = extent;
        vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(
            device,
            &astcSrcInfo,
            pAllocator,
            pImage);
    }
    else if (Formats::IsEtc2Format(format) &&
             (pCreateInfo->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        VK_ASSERT(!(pCreateInfo->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT));
        VkImageCreateInfo etc2SrcInfo = *pCreateInfo;
        etc2SrcInfo.format = getETC2SourceViewFormat(pCreateInfo->format);

        VkExtent3D extent =
        {
            (etc2SrcInfo.extent.width + 3)/ 4,
            (etc2SrcInfo.extent.height + 3)/ 4,
            etc2SrcInfo.extent.depth
        };

        etc2SrcInfo.extent = extent;

        vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(
            device,
            &etc2SrcInfo,
            pAllocator,
            pImage);
    }
    else
    {
        vkResult = DECODER_WAPPER_CALL_NEXT_LAYER(vkCreateImage)(
            device,
            pCreateInfo,
            pAllocator,
            pImage);
    }

    return vkResult;
}

// ====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(
    VkDevice                                    device,
    VkImage                                     image,
    const VkAllocationCallbacks*                pAllocator)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    GpuDecoderLayer* pDecodeWrapper = pDevice->GetGpuDecoderLayer();
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if (settings.enableBC5Encoder)
    {
        pDevice->GetGpuDecoderLayer()->ClearStagingResources(image);
    }
    pDecodeWrapper->RemoveDecodedImage(image);

    DECODER_WAPPER_CALL_NEXT_LAYER(vkDestroyImage)(device, image, pAllocator);
}

} // namespace gpuDecoderWapper
} // namespace entry

// =====================================================================================================================
void GpuDecoderLayer::OverrideDispatchTable(
    DispatchTable *pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    DECODER_WAPPER_OVERRIDE_ENTRY(vkCreateImage);
    DECODER_WAPPER_OVERRIDE_ENTRY(vkDestroyImage);
    DECODER_WAPPER_OVERRIDE_ENTRY(vkCmdCopyImage);
    DECODER_WAPPER_OVERRIDE_ENTRY(vkCmdCopyBufferToImage);
}
} // namespace vk
#endif /* VKI_GPU_DECOMPRESS */
