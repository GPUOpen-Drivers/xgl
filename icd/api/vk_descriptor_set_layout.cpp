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
/**
 ***********************************************************************************************************************
 * @file  vk_descriptor_set_layout.cpp
 * @brief Contains implementation of Vulkan descriptor set layout objects.
 ***********************************************************************************************************************
 */
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_device.h"
#include "include/vk_sampler.h"
#include "palVectorImpl.h"

#include "palMetroHash.h"

namespace vk
{

// =====================================================================================================================
// Generates a hash using the contents of a VkDescriptorSetLayoutBinding struct
void DescriptorSetLayout::GenerateHashFromBinding(
    Util::MetroHash64*                  pHasher,
    const VkDescriptorSetLayoutBinding& desc)
{
    pHasher->Update(desc.binding);
    pHasher->Update(desc.descriptorType);
    pHasher->Update(desc.descriptorCount);
    pHasher->Update(desc.stageFlags);

    if ((desc.pImmutableSamplers != nullptr) &&
        ((desc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
         (desc.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)))
    {
        Sampler* sampler = nullptr;

        for (uint32_t i = 0; i < desc.descriptorCount; i++)
        {
            sampler = Sampler::ObjectFromHandle(desc.pImmutableSamplers[i]);

            pHasher->Update(sampler->GetApiHash());
        }
    }
}

// =====================================================================================================================
// Generates the API hash using the contents of the VkDescriptorSetLayoutCreateInfo struct
uint64_t DescriptorSetLayout::BuildApiHash(
    const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
{
    Util::MetroHash64    hasher;

    hasher.Update(pCreateInfo->flags);
    hasher.Update(pCreateInfo->bindingCount);

    for (uint32 i = 0; i < pCreateInfo->bindingCount; i++)
    {
        GenerateHashFromBinding(&hasher, pCreateInfo->pBindings[i]);
    }

    if (pCreateInfo->pNext != nullptr)
    {
        const void* pNext = pCreateInfo->pNext;

        while (pNext != nullptr)
        {
            const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
            {
                const auto* pExtInfo = reinterpret_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(pNext);
                hasher.Update(pExtInfo->sType);
                hasher.Update(pExtInfo->bindingCount);

                for (uint32 i = 0; i < pExtInfo->bindingCount; i++)
                {
                    hasher.Update(pExtInfo->pBindingFlags[i]);
                }
                break;
            }
            case VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = reinterpret_cast<const VkMutableDescriptorTypeCreateInfoEXT*>(pNext);
                hasher.Update(pExtInfo->sType);
                hasher.Update(pExtInfo->mutableDescriptorTypeListCount);

                for (uint32 i = 0; i < pExtInfo->mutableDescriptorTypeListCount; i++)
                {
                    hasher.Update(pExtInfo->pMutableDescriptorTypeLists[i].descriptorTypeCount);
                    for (uint32 j = 0; j < pExtInfo->pMutableDescriptorTypeLists[i].descriptorTypeCount; j++)
                    {
                        hasher.Update(pExtInfo->pMutableDescriptorTypeLists[i].pDescriptorTypes[j]);
                    }
                }
                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t*>(&hash));

    return hash;
}

// =====================================================================================================================
DescriptorSetLayout::DescriptorSetLayout(
    const Device*     pDevice,
    const CreateInfo& info,
    uint64_t          apiHash) :
    m_info(info),
    m_pDevice(pDevice),
    m_apiHash(apiHash)
{

}

// =====================================================================================================================
// Returns the byte size for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetSingleDescStaticSize(
    const Device*    pDevice,
    VkDescriptorType type)
{
    const Device::Properties& props = pDevice->GetProperties();

    uint32_t size;

    switch (static_cast<uint32_t>(type))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        size = props.descriptorSizes.sampler;
        break;

    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        size = props.descriptorSizes.combinedImageSampler;
        break;

    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        size = props.descriptorSizes.imageView;
        break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
#if VKI_RAY_TRACING
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
#endif
        size = props.descriptorSizes.bufferView;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        // Currently we don't use any storage in the static section of descriptor sets for dynamic buffer descriptors
        // as we pack the whole buffer SRD in the dynamic section (i.e. user data registers).
        size = 0;
        break;
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        size = 1;
        break;
    default:
        VK_NEVER_CALLED();
        size = 0;
        break;
    }

    VK_ASSERT((type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) || (Util::IsPow2Aligned(size, sizeof(uint32_t))));

    return size;
}

// =====================================================================================================================
// Returns the dword size required in the static section for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetDescStaticSectionDwSize(
    const Device*                       pDevice,
    const VkDescriptorSetLayoutBinding* descriptorInfo,
    const DescriptorBindingFlags        bindingFlags,
    const bool                          useFullYcbrImageSampler)
{
    VK_ASSERT(descriptorInfo->descriptorType != VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
    uint32_t size = GetSingleDescStaticSize(pDevice, descriptorInfo->descriptorType);

    if ((descriptorInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
        (descriptorInfo->pImmutableSamplers != nullptr) &&
        (bindingFlags.ycbcrConversionUsage != 0))
    {
        const VkSampler* multiPlaneSampler = descriptorInfo->pImmutableSamplers;
        uint32_t maxMultiPlaneCount = Sampler::ObjectFromHandle(*multiPlaneSampler)->GetMultiPlaneCount();

        // Go through the descriptor array to find the maximum multiplane count.
        for (uint32_t idx = 1; (idx < descriptorInfo->descriptorCount) && (maxMultiPlaneCount < 3); ++idx)
        {
            uint32_t multiPlaneCount = Sampler::ObjectFromHandle(multiPlaneSampler[idx])->GetMultiPlaneCount();
            maxMultiPlaneCount = multiPlaneCount > maxMultiPlaneCount ? multiPlaneCount : maxMultiPlaneCount;
        }

        if (useFullYcbrImageSampler == false)
        {
            size = pDevice->GetProperties().descriptorSizes.imageView;
        }

        size *= maxMultiPlaneCount;
    }

    if (descriptorInfo->descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
        // A single binding corresponds to a whole uniform block, so handle it as one descriptor not array.
        size *= descriptorInfo->descriptorCount;
    }

    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the dword size required in the static section of the given binding point of the given DescriptorSetLayout.
uint32_t DescriptorSetLayout::GetDescStaticSectionDwSize(
    const DescriptorSetLayout* pSrcDescSetLayout,
    const uint32_t             binding)
{
    const BindingInfo& bindingInfo = pSrcDescSetLayout->Binding(binding);

    return (bindingInfo.info.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ?
           bindingInfo.sta.dwSize : bindingInfo.sta.dwArrayStride;
}

// =====================================================================================================================
// Returns the dword size of the dynamic descriptor
uint32_t DescriptorSetLayout::GetDynamicBufferDescDwSize(
    const Device* pDevice)
{
    // Currently we store the whole buffer SRD in the dynamic section (i.e. user data registers).
    uint32_t size = 0;

    if (pDevice->UseCompactDynamicDescriptors())
    {
        size = sizeof(Pal::gpusize);
    }
    else
    {
        size = pDevice->GetProperties().descriptorSizes.bufferView;
    }

    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the dword size required in the dynamic section for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetDescDynamicSectionDwSize(
    const Device*    pDevice,
    VkDescriptorType type)
{
    uint32_t size = 0;

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        size = GetDynamicBufferDescDwSize(pDevice);
        break;

    default:
        // No other descriptor type needs storage in the dynamic section.
        size = 0;
        break;
    }

    return size;
}

// =====================================================================================================================
// Returns the dword size required in the immutable section for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetDescImmutableSectionDwSize(const Device* pDevice, VkDescriptorType type)
{
    const Device::Properties& props = pDevice->GetProperties();

    uint32_t size;

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        // We store the sampler SRD in the immutable section for sampler and combined image sampler descriptors.
        size = props.descriptorSizes.sampler;
        break;

    default:
        // No other descriptor type needs storage in the immutable section.
        size = 0;
        break;
    }

    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Converts information about a binding for the specified section.
void DescriptorSetLayout::ConvertBindingInfo(
    const VkDescriptorSetLayoutBinding* pBindingInfo,
    uint32_t                            descSizeInDw,
    uint32_t                            descAlignmentInDw,
    SectionInfo*                        pSectionInfo,
    BindingSectionInfo*                 pBindingSectionInfo)
{
    // Dword offset to this binding
    pBindingSectionInfo->dwOffset = Util::RoundUpToMultiple(pSectionInfo->dwSize, descAlignmentInDw);

    if (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
        // This allows access to inline uniform blocks using dwords offsets.
        // Vk(Write/Copy/Update)DescriptorSet use byte values, convert them to dword.
        pBindingSectionInfo->dwArrayStride = 1;

        // Size of the whole block in dwords.
        pBindingSectionInfo->dwSize        = descSizeInDw;
    }
    else
    {
        // Array stride in dwords.
        pBindingSectionInfo->dwArrayStride = descSizeInDw;

        // Size of the whole array in dwords.
        pBindingSectionInfo->dwSize        = pBindingInfo->descriptorCount * descSizeInDw;
    }

    // If this descriptor actually requires storage in the section then also update the global section information.
    if (pBindingSectionInfo->dwSize > 0)
    {
        // Update total section size by how much space this binding takes.
        pSectionInfo->dwSize = pBindingSectionInfo->dwOffset + pBindingSectionInfo->dwSize;

        // Update total number of ResourceMappingNodes required by this binding.
        pSectionInfo->numRsrcMapNodes++;

        // Combined image sampler, storage image, and input attachment descriptors in static section need an
        // additional ResourceMappingNode.
        if ((pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
            (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
            (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT))
        {
            pSectionInfo->numRsrcMapNodes++;
        }
    }
}

// =====================================================================================================================
void DescriptorSetLayout::ConvertImmutableInfo(
    const VkDescriptorSetLayoutBinding* pBindingInfo,
    uint32_t                            descSizeInDw,
    ImmSectionInfo*                     pSectionInfo,
    BindingSectionInfo*                 pBindingSectionInfo,
    const DescriptorBindingFlags        bindingFlags,
    const DescriptorSetLayout*          pSrcDescSetLayout)
{
    if ((pBindingInfo->pImmutableSamplers != nullptr) &&
        ((pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
         (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)))
    {
        uint32_t descCount = pBindingInfo->descriptorCount;
        const uint32_t yCbCrMetaDataSizeInDW = sizeof(Vkgc::SamplerYCbCrConversionMetaData) / sizeof(uint32_t);

        // Dword offset to this binding's immutable data.
        pBindingSectionInfo->dwOffset = pSectionInfo->numImmutableSamplers * descSizeInDw +
                                        pSectionInfo->numImmutableYCbCrMetaData * yCbCrMetaDataSizeInDW;

        bool isYCbCrMetaDataIncluded = bindingFlags.ycbcrConversionUsage;

        // Array stride in dwords.
        // If YCbCr meta data is included, then the dwArrayStride should contains both desc and ycbcr meta data size
        if (isYCbCrMetaDataIncluded)
        {
            pBindingSectionInfo->dwArrayStride = descSizeInDw + yCbCrMetaDataSizeInDW;
        }
        else
        {
            pBindingSectionInfo->dwArrayStride = descSizeInDw;
        }

        // Size of the whole array in dwords.
        pBindingSectionInfo->dwSize = descCount * pBindingSectionInfo->dwArrayStride;

        // If this descriptor actually requires storage in the section then also update the global section information,
        // and populate the immutable descriptor data.
        if (pBindingSectionInfo->dwSize > 0)
        {
            // Update the number of immutable samplers.
            pSectionInfo->numImmutableSamplers += descCount;

            if (isYCbCrMetaDataIncluded)
            {
                pSectionInfo->numImmutableYCbCrMetaData += descCount;
            }

            ++pSectionInfo->numDescriptorValueNodes;

            // Copy the immutable descriptor data.
            uint32_t* pDestAddr = &pSectionInfo->pImmutableSamplerData[pBindingSectionInfo->dwOffset];

            if (pSrcDescSetLayout == nullptr)
            {
                for (uint32_t i = 0; i < descCount; ++i, pDestAddr += pBindingSectionInfo->dwArrayStride)
                {
                    const void* pSamplerDesc = Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->Descriptor();

                    memcpy(pDestAddr, pSamplerDesc, descSizeInDw * sizeof(uint32_t));

                    if (Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->IsYCbCrSampler())
                    {
                        // Copy the YCbCrMetaData
                        const void* pYCbCrMetaData = Util::VoidPtrInc(pSamplerDesc, descSizeInDw * sizeof(uint32_t));
                        void* pImmutableYCbCrMetaDataDestAddr = Util::VoidPtrInc(pDestAddr, descSizeInDw * sizeof(uint32_t));
                        Sampler* pSampler = Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i]);
                        Vkgc::SamplerYCbCrConversionMetaData* pCurrentYCbCrMetaData = pSampler->GetYCbCrConversionMetaData();

                        if (pSampler->IsYCbCrConversionMetaDataUpdated(
                                              static_cast<const Vkgc::SamplerYCbCrConversionMetaData*>(pYCbCrMetaData)))
                        {
                            memcpy(pImmutableYCbCrMetaDataDestAddr, pCurrentYCbCrMetaData,
                                                                    yCbCrMetaDataSizeInDW * sizeof(uint32_t));
                        }
                        else
                        {
                            memcpy(pImmutableYCbCrMetaDataDestAddr, pYCbCrMetaData,
                                                                    yCbCrMetaDataSizeInDW * sizeof(uint32_t));
                        }
                    }
                }
            }
            else
            {
                const DescriptorSetLayout::BindingInfo& refBindingInfo =
                    pSrcDescSetLayout->Binding(pBindingInfo->binding);

                const void* pSamplerDesc = Util::VoidPtrInc(pSrcDescSetLayout->Info().imm.pImmutableSamplerData,
                                                            refBindingInfo.imm.dwOffset * sizeof(uint32_t));

                memcpy(pDestAddr, pSamplerDesc, refBindingInfo.imm.dwSize * sizeof(uint32_t));
            }
        }
    }
    else
    {
        // If this binding does not have immutable section data then initialize the parameters to defaults.
        pBindingSectionInfo->dwOffset       = 0;
        pBindingSectionInfo->dwArrayStride  = 0;
        pBindingSectionInfo->dwSize         = 0;
    }
}

// =====================================================================================================================
VkResult DescriptorSetLayout::ConvertCreateInfo(
    const Device*                          pDevice,
    const VkDescriptorSetLayoutCreateInfo* pIn,
    CreateInfo*                            pOut,
    BindingInfo*                           pOutBindings)
{
    VK_ASSERT((pIn != nullptr) && (pIn->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO));

    pOut->activeStageMask               = 0;

    pOut->varDescStride                 = 0;

    pOut->sta.dwSize                    = 0;
    pOut->sta.numRsrcMapNodes           = 0;

    pOut->dyn.dwSize                    = 0;
    pOut->dyn.numRsrcMapNodes           = 0;

    pOut->imm.numImmutableYCbCrMetaData = 0;
    pOut->imm.numImmutableSamplers      = 0;

    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);

    const VkDescriptorSetLayoutBindingFlagsCreateInfo* pDescriptorSetLayoutBindingFlagsCreateInfo = nullptr;
    const VkMutableDescriptorTypeCreateInfoEXT* pMutableDescriptorTypeCreateInfoEXT = nullptr;
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pIn->pNext);

        while (pHeader != nullptr)
        {
            switch (static_cast<uint32_t>(pHeader->sType))
            {
                case VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT:
                {
                    pMutableDescriptorTypeCreateInfoEXT =
                        reinterpret_cast<const VkMutableDescriptorTypeCreateInfoEXT*>(pHeader);

                    break;
                }

                case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
                {
                    pDescriptorSetLayoutBindingFlagsCreateInfo =
                        reinterpret_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(pHeader);

                    break;
                }

                default:
                    break;
            }

            pHeader = static_cast<const VkStructHeader*>(pHeader->pNext);
        }
    }

    if (pDescriptorSetLayoutBindingFlagsCreateInfo != nullptr)
    {
        VK_ASSERT(pDescriptorSetLayoutBindingFlagsCreateInfo->bindingCount == pIn->bindingCount);

        for (uint32 inIndex = 0; inIndex < pIn->bindingCount; ++inIndex)
        {
            const VkDescriptorSetLayoutBinding& currentBinding = pIn->pBindings[inIndex];
            pOutBindings[currentBinding.binding].bindingFlags = VkToInternalDescriptorBindingFlag(
                pDescriptorSetLayoutBindingFlagsCreateInfo->pBindingFlags[inIndex]);
        }
    }

    for (uint32 inIndex = 0; inIndex < pIn->bindingCount; ++inIndex)
    {
        pOut->activeStageMask |= pIn->pBindings[inIndex].stageFlags;
    }

    const bool useFullYcbrImageSampler = ((pIn->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT) != 0);

    // Bindings numbers are allowed to come in out-of-order, as well as with gaps.
    // We compute offsets using the size we've seen so far as we iterate, so we need to handle
    // the bindings in binding-number order, rather than array order. Hence we can ignore
    //     - VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT

    // First, copy the binding info into our output array in order.
    for (uint32 inIndex = 0; inIndex < pIn->bindingCount; ++inIndex)
    {
        const VkDescriptorSetLayoutBinding & currentBinding = pIn->pBindings[inIndex];
        pOutBindings[currentBinding.binding].info = currentBinding;

        // Calculate the maximum size required for mutable descriptors
        if (currentBinding.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
            VkMutableDescriptorTypeListEXT list =
                pMutableDescriptorTypeCreateInfoEXT->pMutableDescriptorTypeLists[inIndex];
            uint32_t maxSize = 0;
            for (uint32_t j = 0; j < list.descriptorTypeCount; ++j)
            {
                VK_ASSERT(list.pDescriptorTypes[j] != VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
                uint32_t size = GetSingleDescStaticSize(pDevice, list.pDescriptorTypes[j]);
                maxSize = Util::Max(maxSize, size);
            }

            VK_ASSERT(maxSize > 0);
            pOutBindings[currentBinding.binding].sta.dwArrayStride = maxSize / sizeof(uint32_t);

            // See below loop where we write non mutable variable descriptor sizes
            if ((currentBinding.binding == (pOut->count - 1)) &&
                pOutBindings[currentBinding.binding].bindingFlags.variableDescriptorCount)
            {
                pOut->varDescStride = maxSize;
            }
        }

        if (currentBinding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            // For this binding section, it will be marked if any sampler contains ycbcr meta data,
            // this is used to control the dw array stride.
            for (uint32 i = 0; i < currentBinding.descriptorCount; ++i)
            {
                if ((currentBinding.pImmutableSamplers != nullptr) &&
                    Sampler::ObjectFromHandle(currentBinding.pImmutableSamplers[i])->IsYCbCrSampler())
                {
                    pOutBindings[currentBinding.binding].bindingFlags.ycbcrConversionUsage = 1;
                    break;
                }
            }
        }
    }
    // Now iterate over our output array to convert the binding info.  Any gaps in
    // the input binding numbers will be dummy entries in this array, but it
    // should be safe to call ConvertBindingInfo on those as well.
    for (uint32 bindingNumber = 0; bindingNumber < pOut->count; ++bindingNumber)
    {
        BindingInfo* pBinding = &pOutBindings[bindingNumber];

        VkDescriptorType type = pBinding->info.descriptorType;

        // Determine the alignment requirement of descriptors in dwords.
        uint32 descAlignmentInDw    = pDevice->GetProperties().descriptorSizes.alignmentInDwords;
        uint32 staDescAlignmentInDw = descAlignmentInDw;
        if (pDevice->GetRuntimeSettings().pipelineLayoutMode == PipelineLayoutAngle)
        {
            VK_ASSERT(AngleDescPattern::DescriptorSetBindingStride % descAlignmentInDw == 0);
            staDescAlignmentInDw = AngleDescPattern::DescriptorSetBindingStride;
        }
        // If the last binding has the VARIABLE_DESCRIPTOR_COUNT_BIT set, write the varDescDwStride
        if ((bindingNumber == (pOut->count - 1)) && pBinding->bindingFlags.variableDescriptorCount)
        {
            // Mutable descriptor sizes calculated in loop above
            if (type != VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
            {
                pOut->varDescStride =
                    GetSingleDescStaticSize(pDevice, pBinding->info.descriptorType);
            }
        }

        // Construct the information specific to the static section of the descriptor set layout.
        uint32_t staticSectionDwSize = 0;

        if (pBinding->info.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
            staticSectionDwSize = pBinding->sta.dwArrayStride;
        }
        else
        {
            staticSectionDwSize = GetDescStaticSectionDwSize(pDevice, &pBinding->info, pBinding->bindingFlags,
                useFullYcbrImageSampler);
        }

        ConvertBindingInfo(
            &pBinding->info,
            staticSectionDwSize,
            staDescAlignmentInDw,
            &pOut->sta,
            &pBinding->sta);

        // Construct the information specific to the dynamic section of the descriptor set layout.
        ConvertBindingInfo(
            &pBinding->info,
            GetDescDynamicSectionDwSize(pDevice, pBinding->info.descriptorType),
            descAlignmentInDw,
            &pOut->dyn,
            &pBinding->dyn);

        // Construct the information specific to the immutable section of the descriptor set layout.
        ConvertImmutableInfo(
            &pBinding->info,
            GetDescImmutableSectionDwSize(pDevice, pBinding->info.descriptorType),
            &pOut->imm,
            &pBinding->imm,
            pBinding->bindingFlags);

        if ((pBinding->info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
            (pBinding->info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
        {
            pOut->numDynamicDescriptors += pBinding->info.descriptorCount;
        }
    }

    VK_ASSERT(pOut->numDynamicDescriptors <= MaxDynamicDescriptors);

    VK_ASSERT(((pIn->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0) ||
              ((pIn->bindingCount <= MaxPushDescriptors) && (pOut->numDynamicDescriptors == 0)));

    return VK_SUCCESS;
}

// =====================================================================================================================
// Creates a descriptor set layout object.
VkResult DescriptorSetLayout::Create(
    const Device*                                pDevice,
    const VkDescriptorSetLayoutCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*                 pAllocator,
    VkDescriptorSetLayout*                       pLayout)
{
    uint64_t apiHash = BuildApiHash(pCreateInfo);

    // We add pBinding size to the apiSize so that they would reside consecutively in memory
    // The reasoning is that we don't know the size of pBinding until now in creation time,
    // and we don't want to use dynamic allocation for small sets, where we don't know where
    // that memory would end up.
    // Additionally, we also have to add auxiliary memory for the immutable sampler arrays of
    // each binding.

    size_t immSamplerCount  = 0;
    size_t immYCbCrMetaDataCount = 0;
    uint32_t bindingCount   = 0;
    for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
    {
        VkDescriptorSetLayoutBinding desc = pCreateInfo->pBindings[i];

        if ((desc.pImmutableSamplers != nullptr) &&
            ((desc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
             (desc.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)))
        {
            immSamplerCount += desc.descriptorCount;
            if (Sampler::ObjectFromHandle(*desc.pImmutableSamplers)->IsYCbCrSampler())
            {
                immYCbCrMetaDataCount += desc.descriptorCount;
            }
        }

        bindingCount = Util::Max(bindingCount, desc.binding + 1);
    }

    const size_t bindingInfoAuxSize     = bindingCount          * sizeof(BindingInfo);
    const size_t immSamplerAuxSize      = immSamplerCount       * pDevice->GetProperties().descriptorSizes.sampler;
    const size_t immYCbCrMetaDataSize   = immYCbCrMetaDataCount * sizeof(Vkgc::SamplerYCbCrConversionMetaData);

    const size_t apiSize = sizeof(DescriptorSetLayout);
    const size_t auxSize = bindingInfoAuxSize + immSamplerAuxSize + immYCbCrMetaDataSize;
    const size_t objSize = apiSize + auxSize;

    void* pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

    if (pSysMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    CreateInfo info = {};

    // Set the binding count
    info.count = bindingCount;

    // Set the bindings array to the appropriate location within the allocated memory
    BindingInfo* pBindings = reinterpret_cast<BindingInfo*>(reinterpret_cast<uint8_t*>(pSysMem) + apiSize);

    // Also memset it as not all bindings may be actually used
    memset(pBindings, 0, bindingInfoAuxSize);

    // Set the base pointer of the immutable sampler data to the appropriate location within the allocated memory
    info.imm.pImmutableSamplerData = reinterpret_cast<uint32_t*>(Util::VoidPtrInc(pSysMem, apiSize + bindingInfoAuxSize));

    info.flags = pCreateInfo->flags;

    // Fill descriptor set layout information
    VkResult result = ConvertCreateInfo(
        pDevice,
        pCreateInfo,
        &info,
        pBindings);

    if (result != VK_SUCCESS)
    {
        pDevice->FreeApiObject(pAllocator, pSysMem);

        return result;
    }

    VK_PLACEMENT_NEW (pSysMem) DescriptorSetLayout (pDevice, info, apiHash);

    *pLayout = DescriptorSetLayout::HandleFromVoidPointer(pSysMem);

    return result;
}

// =====================================================================================================================
// Get the size in byte of a merged DescriptorSetLayout
size_t DescriptorSetLayout::GetObjectSize(
    const VkDescriptorSetLayout* pLayouts,
    const VkShaderStageFlags*    pShaderMasks,
    const uint32_t               count)
{
    size_t size = sizeof(DescriptorSetLayout);

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorSetLayout* pSetLayout = DescriptorSetLayout::ObjectFromHandle(pLayouts[i]);
        const VkShaderStageFlags   shaderMask = pShaderMasks[i];

        size += pSetLayout->GetBindingInfoArrayByteSize(shaderMask);
        size += pSetLayout->GetImmSamplerArrayByteSize(shaderMask);
        size += pSetLayout->GetImmYCbCrMetaDataArrayByteSize(shaderMask);
    }

    return size;
}

// =====================================================================================================================
// Copy descriptor set layout object
void DescriptorSetLayout::Copy(
    const Device*        pDevice,
    DescriptorSetLayout* pOutLayout) const
{
    constexpr uint32_t shaderMask = VK_SHADER_STAGE_ALL;
    constexpr size_t apiSize = sizeof(DescriptorSetLayout);

    CreateInfo info = Info();

    // Copy the bindings array
    void* pBindings = Util::VoidPtrInc(pOutLayout, apiSize);

    memcpy(pBindings, Util::VoidPtrInc(this, apiSize), GetBindingInfoArrayByteSize(shaderMask));

    // Copy the immutable sampler data
    void* pImmutableSamplerData = Util::VoidPtrInc(pOutLayout, apiSize + GetBindingInfoArrayByteSize(shaderMask));

    memcpy(pImmutableSamplerData,
            Util::VoidPtrInc(this, apiSize + GetBindingInfoArrayByteSize(shaderMask)),
            GetImmSamplerArrayByteSize(shaderMask) + GetImmYCbCrMetaDataArrayByteSize(shaderMask));

    // Set the base pointer of the immutable sampler data to the appropriate location within the allocated memory
    info.imm.pImmutableSamplerData = reinterpret_cast<uint32_t*>(pImmutableSamplerData);

    VK_PLACEMENT_NEW(pOutLayout) DescriptorSetLayout(pDevice, info, GetApiHash());
}

// =====================================================================================================================
// Get the size in byte of the refBinding info of the specific shader stages
size_t DescriptorSetLayout::GetBindingInfoArrayByteSize(VkShaderStageFlags shaderMask) const
{
    uint32_t numActiveBindings = 0;

    if (CoverAllActiveShaderStages(shaderMask))
    {
        numActiveBindings = m_info.count;
    }
    else
    {
        for (uint32_t i = 0; i < m_info.count; ++i)
        {
            const VkDescriptorSetLayoutBinding& binding = Binding(i).info;

            if ((binding.stageFlags & shaderMask) != 0)
            {
                numActiveBindings = Util::Max(numActiveBindings, binding.binding + 1);
            }
        }
    }

    return numActiveBindings * sizeof(DescriptorSetLayout::BindingInfo);
}

// =====================================================================================================================
// Get the size in bytes of immutable samplers array
size_t DescriptorSetLayout::GetImmSamplerArrayByteSize(VkShaderStageFlags shaderMask) const
{
    uint32_t numActiveImmSamplers = 0;

    if (CoverAllActiveShaderStages(shaderMask))
    {
        numActiveImmSamplers = m_info.imm.numImmutableSamplers;
    }
    else
    {
        for (uint32_t i = 0; i < m_info.count; ++i)
        {
            const VkDescriptorSetLayoutBinding& binding = Binding(i).info;

            if (((binding.stageFlags & shaderMask) != 0) &&
                (binding.pImmutableSamplers != nullptr) &&
                ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
                 (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)))
            {
                numActiveImmSamplers += binding.descriptorCount;
            }
        }
    }

    return numActiveImmSamplers * m_pDevice->GetProperties().descriptorSizes.sampler;
}

// =====================================================================================================================
// Get the size in bytes of immutable ycbcr meta data array
size_t DescriptorSetLayout::GetImmYCbCrMetaDataArrayByteSize(VkShaderStageFlags shaderMask) const
{
    uint32_t numActiveImmYcbcrMetaData = 0;

    if (CoverAllActiveShaderStages(shaderMask))
    {
        numActiveImmYcbcrMetaData = m_info.imm.numImmutableYCbCrMetaData;
    }
    else
    {
        for (uint32_t i = 0; i < m_info.count; ++i)
        {
            const VkDescriptorSetLayoutBinding& binding = Binding(i).info;
            const DescriptorBindingFlags& flags = Binding(i).bindingFlags;

            if (((binding.stageFlags & shaderMask) != 0) &&
                (binding.pImmutableSamplers != nullptr) &&
                (flags.ycbcrConversionUsage != 0) &&
                ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
                 (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)))
            {
                numActiveImmYcbcrMetaData += binding.descriptorCount;
            }
        }
    }

    return numActiveImmYcbcrMetaData * sizeof(Vkgc::SamplerYCbCrConversionMetaData);
}

// =====================================================================================================================
// Get the size in byte of a DescriptorSetLayout
size_t DescriptorSetLayout::GetObjectSize(VkShaderStageFlags shaderMask) const
{
    const uint32_t apiSize = sizeof(DescriptorSetLayout);

    return apiSize + GetBindingInfoArrayByteSize(shaderMask)
        + GetImmSamplerArrayByteSize(shaderMask)
        + GetImmYCbCrMetaDataArrayByteSize(shaderMask);
}

// =====================================================================================================================
// Check whether there is refBinding of the specified shader(s) in this DescriptorSetLayout
bool DescriptorSetLayout::IsEmpty(VkShaderStageFlags shaderMask) const
{
    bool isEmpty = true;

    if (CoverAllActiveShaderStages(shaderMask))
    {
        isEmpty = (m_info.count == 0);
    }
    else
    {
        for (uint32_t i = 0; i < m_info.count; ++i)
        {
            if ((Binding(i).info.stageFlags & shaderMask) != 0)
            {
                isEmpty = false;
                break;
            }
        }
    }

    return isEmpty;
}

// =====================================================================================================================
// Destroy descriptor set layout object
VkResult DescriptorSetLayout::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator,
    bool                            freeMemory)
{
    this->~DescriptorSetLayout();

    if (freeMemory)
    {
        pDevice->FreeApiObject(pAllocator, this);
    }

    return VK_SUCCESS;
}

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice                                    device,
    VkDescriptorSetLayout                       descriptorSetLayout,
    const VkAllocationCallbacks*                pAllocator)
{
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        DescriptorSetLayout::ObjectFromHandle(descriptorSetLayout)->Destroy(pDevice, pAllocCB, true);
    }
}

} // namespace entry

} // namespace vk
