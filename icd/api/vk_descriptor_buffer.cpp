/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_descriptor_buffer.cpp
 * @brief Contains implementation of Vulkan descriptor buffer.
 ***********************************************************************************************************************
 */

#include "include/vk_descriptor_buffer.h"
#include "include/vk_descriptor_set.h"
#include "include/vk_device.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_sampler.h"
#include "include/vk_buffer.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_buffer_view.h"

namespace vk
{

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSizeEXT(
    VkDevice                            device,
    VkDescriptorSetLayout               layout,
    VkDeviceSize*                       pLayoutSizeInBytes)
{
    DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(layout);

    const uint32_t lastBindingIdx = pLayout->Info().count - 1;
    const uint32_t varBindingStaDWSize = (pLayout->Info().varDescStride != 0) ?
        pLayout->Binding(lastBindingIdx).sta.dwSize : 0;

    // Total size = STA section size - last binding STA size (if it's variable)
    *pLayoutSizeInBytes = (pLayout->Info().sta.dwSize - varBindingStaDWSize) * sizeof(uint32_t);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutBindingOffsetEXT(
    VkDevice                            device,
    VkDescriptorSetLayout               layout,
    uint32_t                            binding,
    VkDeviceSize*                       pOffset)
{
    DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(layout);
    *pOffset = pLayout->GetDstStaOffset(pLayout->Binding(binding), 0) * sizeof(uint32_t);
}

// =====================================================================================================================
// Input dataSize can be ignored in our implementation because the size is known. It is for tooling and other stuff.
VKAPI_ATTR void VKAPI_CALL vkGetDescriptorEXT(
    VkDevice                            device,
    const VkDescriptorGetInfoEXT*       pDescriptorInfo,
    size_t                              dataSize,
    void*                               pDescriptor)
{
    static_assert((DefaultDeviceIndex == 0),
        "Used BuildSRD in this function assuming that DefaultDeviceIndex is 0");

    const Device*             pDevice = ApiDevice::ObjectFromHandle(device);
    const Device::Properties& props   = pDevice->GetProperties();

    VK_ASSERT((props.descriptorSizes.imageView  == 32) &&
              (props.descriptorSizes.sampler    == 16) &&
              (props.descriptorSizes.bufferView == 16));

    switch (static_cast<uint32_t>(pDescriptorInfo->type))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
        const void* pSamplerDesc = Sampler::ObjectFromHandle(*pDescriptorInfo->data.pSampler)->Descriptor();
        memcpy(pDescriptor, pSamplerDesc, props.descriptorSizes.sampler);
        break;
    }
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    {
        if (pDescriptorInfo->data.pCombinedImageSampler != nullptr)
        {
            uint32_t* pDes = static_cast<uint32_t*>(pDescriptor);

            const ImageView* pImageView = ImageView::ObjectFromHandle(pDescriptorInfo->data.pCombinedImageSampler->imageView);

            if ((pImageView != nullptr) && Formats::IsYuvFormat(pImageView->GetViewFormat()))
            {
                DescriptorUpdate::WriteImageDescriptorsYcbcr<32+16>(
                    pDescriptorInfo->data.pCombinedImageSampler,
                    DefaultDeviceIndex,
                    pDes,
                    1u,
                    0u,
                    0u);
            }
            else
            {
                DescriptorUpdate::WriteImageSamplerDescriptors<32, 16>(
                    pDescriptorInfo->data.pCombinedImageSampler,
                    DefaultDeviceIndex,
                    pDes,
                    1u,
                    0u,
                    0);
            }
        }
        else
        {
            memset(pDescriptor, 0, (props.descriptorSizes.imageView + props.descriptorSizes.sampler));
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {
        if (pDescriptorInfo->data.pInputAttachmentImage != nullptr)
        {
            uint32_t* pDes = static_cast<uint32_t*>(pDescriptor);

            DescriptorUpdate::WriteImageDescriptors<32, false>(
                pDescriptorInfo->data.pInputAttachmentImage,
                DefaultDeviceIndex,
                pDes,
                1u,
                0u,
                0);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.imageView);
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    {
        if (pDescriptorInfo->data.pSampledImage != nullptr)
        {
            uint32_t* pDes = static_cast<uint32_t*>(pDescriptor);

            DescriptorUpdate::WriteImageDescriptors<32, false>(
                pDescriptorInfo->data.pSampledImage,
                DefaultDeviceIndex,
                pDes,
                1u,
                0u,
                0);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.imageView);
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    {
        if (pDescriptorInfo->data.pStorageImage != nullptr)
        {
            uint32_t* pDes = static_cast<uint32_t*>(pDescriptor);

            DescriptorUpdate::WriteImageDescriptors<32, true>(
                pDescriptorInfo->data.pStorageImage,
                DefaultDeviceIndex,
                pDes,
                1u,
                0u,
                0);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.imageView);
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    {
        if (pDescriptorInfo->data.pUniformTexelBuffer != nullptr)
        {
            BufferView::BuildSrd(
                pDevice,
                0,
                pDescriptorInfo->data.pUniformTexelBuffer->range,
                static_cast<const Pal::gpusize*> (&pDescriptorInfo->data.pUniformTexelBuffer->address),
                pDescriptorInfo->data.pUniformTexelBuffer->format,
                1,
                props.descriptorSizes.bufferView,
                pDescriptor);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.bufferView);
        }
        break;
    }
#if VKI_RAY_TRACING
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
        if (pDescriptorInfo->data.accelerationStructure != 0)
        {
            uint32_t*           pDestAddr      = static_cast<uint32_t*>(pDescriptor);
            Pal::BufferViewInfo bufferViewInfo = {};

            bufferViewInfo.gpuAddr = pDescriptorInfo->data.accelerationStructure;
            bufferViewInfo.range   = 0xFFFFFFFF;

            DescriptorUpdate::SetAccelerationDescriptorsBufferViewFlags(pDevice, &bufferViewInfo);

            pDevice->PalDevice(DefaultDeviceIndex)->CreateUntypedBufferViewSrds(1, &bufferViewInfo, pDestAddr);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.bufferView);
        }

        break;
    }
#endif
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    {
        if (pDescriptorInfo->data.pUniformBuffer != nullptr)
        {
            BufferView::BuildSrd(
                pDevice,
                0,
                pDescriptorInfo->data.pUniformBuffer->range,
                static_cast<const Pal::gpusize*> (&pDescriptorInfo->data.pUniformBuffer->address),
                VK_FORMAT_UNDEFINED,
                1,
                props.descriptorSizes.bufferView,
                pDescriptor);
        }
        else
        {
            memset(pDescriptor, 0, props.descriptorSizes.bufferView);
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    default:
    {
        VK_NEVER_CALLED();

        break;
    }
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetBufferOpaqueCaptureDescriptorDataEXT(
    VkDevice                                    device,
    const VkBufferCaptureDescriptorDataInfoEXT* pInfo,
    void*                                       pData)
{
    // We currently don't use any opaque data.
    *(static_cast<uint32_t*>(pData)) = 0u;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetImageOpaqueCaptureDescriptorDataEXT(
    VkDevice                                    device,
    const VkImageCaptureDescriptorDataInfoEXT*  pInfo,
    void*                                       pData)
{
    // We currently don't use any opaque data.
    *(static_cast<uint32_t*>(pData)) = 0u;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetImageViewOpaqueCaptureDescriptorDataEXT(
    VkDevice                                       device,
    const VkImageViewCaptureDescriptorDataInfoEXT* pInfo,
    void*                                          pData)
{
    // We currently don't use any opaque data.
    *(static_cast<uint32_t*>(pData)) = 0u;

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetSamplerOpaqueCaptureDescriptorDataEXT(
    VkDevice                                     device,
    const VkSamplerCaptureDescriptorDataInfoEXT* pInfo,
    void*                                        pData)
{
    uint32_t*      borderColorIndex = static_cast<uint32_t*>(pData);
    const Sampler* pSampler         = Sampler::ObjectFromHandle(pInfo->sampler);

    *borderColorIndex = pSampler->GetBorderColorPaletteIndex();

    return VK_SUCCESS;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(
    VkDevice                                                   device,
    const VkAccelerationStructureCaptureDescriptorDataInfoEXT* pInfo,
    void*                                                      pData)
{
    // We currently don't use any opaque data.
    *(static_cast<uint32_t*>(pData)) = 0u;

    return VK_SUCCESS;
}

} // namespace entry

} // namespace vk
