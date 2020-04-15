/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 39
#define Vkgc Llpc
#endif

#include "include/vk_descriptor_set_layout.h"
#include "include/vk_device.h"
#include "include/vk_sampler.h"

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

    for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
    {
        GenerateHashFromBinding(&hasher, pCreateInfo->pBindings[i]);
    }

    if (pCreateInfo->pNext != nullptr)
    {
        union
        {
            const VkStructHeader*                              pInfo;
            const VkDescriptorSetLayoutBindingFlagsCreateInfo* pBindingFlagsCreateInfo;
        };

        pInfo = static_cast<const VkStructHeader*>(pCreateInfo->pNext);

        while (pInfo != nullptr)
        {
            switch (static_cast<uint32_t>(pInfo->sType))
            {
            case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
                hasher.Update(pBindingFlagsCreateInfo->sType);
                hasher.Update(pBindingFlagsCreateInfo->bindingCount);

                for (uint32_t i = 0; i < pBindingFlagsCreateInfo->bindingCount; i++)
                {
                    hasher.Update(pBindingFlagsCreateInfo->pBindingFlags[i]);
                }

                break;
            default:
                break;
            }

            pInfo = pInfo->pNext;
        }
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t* const>(&hash));

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
uint32_t DescriptorSetLayout::GetSingleDescStaticSize(const Device* pDevice, VkDescriptorType type)
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
uint32_t DescriptorSetLayout::GetDescStaticSectionDwSize(const Device* pDevice, const  VkDescriptorSetLayoutBinding *descriptorInfo)
{
    uint32_t size = GetSingleDescStaticSize(pDevice, descriptorInfo->descriptorType);

    if (descriptorInfo->descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
        // A single binding corresponds to a whole uniform block, so handle it as one descriptor not array.
        size *= descriptorInfo->descriptorCount;
    }

    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the dword size of the dynamic descriptor
uint32_t DescriptorSetLayout::GetDynamicBufferDescDwSize(const Device* pDevice)
{
    // Currently we store the whole buffer SRD in the dynamic section (i.e. user data registers).
    uint32_t size = 0;
    if (pDevice->GetEnabledFeatures().robustBufferAccess == false)
    {
        size = sizeof(Pal::gpusize);
    }
    else
    {
        const Device::Properties& props = pDevice->GetProperties();
        size = props.descriptorSizes.bufferView;
    }
    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the dword size required in the dynamic section for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetDescDynamicSectionDwSize(const Device* pDevice, VkDescriptorType type)
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
    pBindingSectionInfo->dwOffset = Util::Pow2Align(pSectionInfo->dwSize, descAlignmentInDw);

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
        pSectionInfo->dwSize += pBindingSectionInfo->dwSize;

        // Update total number of ResourceMappingNodes required by this binding.
        pSectionInfo->numRsrcMapNodes++;

        // Combined image sampler descriptors in static section need an additional ResourceMappingNode.
        if (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
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
    BindingSectionInfo*                 pBindingSectionInfo)
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

        // By default, we assume the YCbCr meta data is not included.
        bool isYCbCrMetaDataIncluded = false;

        // For this binding section, it will be marked if any smapler contains ycbcr meta data,
        // this is used to control the dw array stride.
        for (uint32_t i = 0; i < descCount; ++i)
        {
            if (Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->IsYCbCrSampler())
            {
                isYCbCrMetaDataIncluded = true;
            }
        }

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

            for (uint32_t i = 0; i < descCount; ++i, pDestAddr += pBindingSectionInfo->dwArrayStride)
            {
                const void* pSamplerDesc = Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->Descriptor();

                memcpy(pDestAddr, pSamplerDesc, descSizeInDw * sizeof(uint32_t));

                if (Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->IsYCbCrSampler())
                {
                    // Copy the YCbCrMetaData
                    const void* pYCbCrMetaData = Util::VoidPtrInc(pSamplerDesc, descSizeInDw * sizeof(uint32_t));
                    void* pImmutableYCbCrMetaDataDestAddr = Util::VoidPtrInc(pDestAddr, descSizeInDw * sizeof(uint32_t));
                    memcpy(pImmutableYCbCrMetaDataDestAddr, pYCbCrMetaData, yCbCrMetaDataSizeInDW * sizeof(uint32_t));
                }
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
    if (pIn == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    union
    {
        const VkStructHeader*                              pHeader;
        const VkDescriptorSetLayoutCreateInfo*             pInfo;
        const VkDescriptorSetLayoutBindingFlagsCreateInfo* pBindingFlags;
    };

    pOut->activeStageMask = VK_SHADER_STAGE_ALL; // TODO set this up properly enumerating the active stages.
                                                 // currently this flag is only tested for non zero, so
                                                 // setting all flags active makes no difference...

    pOut->varDescStride                 = 0;

    pOut->sta.dwSize                    = 0;
    pOut->sta.numRsrcMapNodes           = 0;

    pOut->dyn.dwSize                    = 0;
    pOut->dyn.numRsrcMapNodes           = 0;

    pOut->imm.numImmutableYCbCrMetaData = 0;
    pOut->imm.numImmutableSamplers      = 0;

    for (pInfo = pIn; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
            {

               EXTRACT_VK_STRUCTURES_0(
                   BindingFlag,
                   DescriptorSetLayoutBindingFlagsCreateInfo,
                   pBindingFlags,
                   DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO)

                if (pDescriptorSetLayoutBindingFlagsCreateInfo != nullptr)
                {
                    VK_ASSERT(pDescriptorSetLayoutBindingFlagsCreateInfo->bindingCount == pInfo->bindingCount);

                    for (uint32_t inIndex = 0; inIndex < pInfo->bindingCount; ++inIndex)
                    {
                        const VkDescriptorSetLayoutBinding& currentBinding = pInfo->pBindings[inIndex];
                        pOutBindings[currentBinding.binding].bindingFlags  =
                            pDescriptorSetLayoutBindingFlagsCreateInfo->pBindingFlags[inIndex];
                    }
                }

                // Bindings numbers are allowed to come in out-of-order, as well as with gaps.
                // We compute offsets using the size we've seen so far as we iterate, so we need to handle
                // the bindings in binding-number order, rather than array order.

                VK_IGNORE(pInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

                // First, copy the binding info into our output array in order.
                for (uint32_t inIndex = 0; inIndex < pInfo->bindingCount; ++inIndex)
                {
                    const VkDescriptorSetLayoutBinding & currentBinding = pInfo->pBindings[inIndex];
                    pOutBindings[currentBinding.binding].info = currentBinding;
                }

                // Now iterate over our output array to convert the binding info.  Any gaps in
                // the input binding numbers will be dummy entries in this array, but it
                // should be safe to call ConvertBindingInfo on those as well.
                for (uint32_t bindingNumber = 0; bindingNumber < pOut->count; ++bindingNumber)
                {
                    BindingInfo* pBinding = &pOutBindings[bindingNumber];

                    // Determine the alignment requirement of descriptors in dwords.
                    uint32_t descAlignmentInDw = pDevice->GetProperties().descriptorSizes.alignment / sizeof(uint32_t);

                    // If the last binding has the VARIABLE_DESCRIPTOR_COUNT_BIT set, write the varDescDwStride
                    if ((bindingNumber == (pOut->count - 1)) &&
                        (pBinding->bindingFlags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
                    {
                        pOut->varDescStride = GetSingleDescStaticSize(pDevice, pBinding->info.descriptorType);
                    }

                    // Construct the information specific to the static section of the descriptor set layout.
                    ConvertBindingInfo(
                        &pBinding->info,
                        GetDescStaticSectionDwSize(pDevice, &pBinding->info),
                        descAlignmentInDw,
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
                        &pBinding->imm);

                    if ((pBinding->info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                        (pBinding->info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
                    {
                        pOut->numDynamicDescriptors += pBinding->info.descriptorCount;
                    }
                }
            }
            break;

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    VK_ASSERT(pOut->numDynamicDescriptors <= MaxDynamicDescriptors);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Creates a descriptor set layout object.
VkResult DescriptorSetLayout::Create(
    Device*                                      pDevice,
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

    void* pSysMem = pDevice->AllocApiObject(objSize, pAllocator);

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

    // Fill descriptor set layout information
    VkResult result = ConvertCreateInfo(
        pDevice,
        pCreateInfo,
        &info,
        pBindings);

    if (result != VK_SUCCESS)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pSysMem);

        return result;
    }

    VK_PLACEMENT_NEW (pSysMem) DescriptorSetLayout (pDevice, info, apiHash);

    *pLayout = DescriptorSetLayout::HandleFromVoidPointer(pSysMem);

    return result;
}

// =====================================================================================================================
// Copy descriptor set layout object
void DescriptorSetLayout::Copy(
    const Device*                   pDevice,
    DescriptorSetLayout*            pOutLayout) const
{
    const size_t apiSize = sizeof(DescriptorSetLayout);

    CreateInfo info = Info();

    // Copy the bindings array
    void* pBindings = Util::VoidPtrInc(pOutLayout, apiSize);

    memcpy(pBindings, Util::VoidPtrInc(this, apiSize), GetBindingInfoArrayByteSize());

    // Copy the immutable sampler data
    void* pImmutableSamplerData = Util::VoidPtrInc(pOutLayout, apiSize + GetBindingInfoArrayByteSize());

    memcpy(pImmutableSamplerData,
           Util::VoidPtrInc(this, apiSize + GetBindingInfoArrayByteSize()),
           GetImmSamplerArrayByteSize() + GetImmYCbCrMetaDataArrayByteSize());

    // Set the base pointer of the immutable sampler data to the appropriate location within the allocated memory
    info.imm.pImmutableSamplerData = reinterpret_cast<uint32_t*>(pImmutableSamplerData);

    VK_PLACEMENT_NEW(pOutLayout) DescriptorSetLayout(pDevice, info, GetApiHash());
}

// =====================================================================================================================
// Get the size in bytes of immutable samplers array
uint32_t DescriptorSetLayout::GetImmSamplerArrayByteSize() const
{
    return m_info.imm.numImmutableSamplers * m_pDevice->GetProperties().descriptorSizes.sampler;
}

// =====================================================================================================================
// Get the size in bytes of immutable ycbcr meta data array
uint32_t DescriptorSetLayout::GetImmYCbCrMetaDataArrayByteSize() const
{
    return m_info.imm.numImmutableYCbCrMetaData * sizeof(Vkgc::SamplerYCbCrConversionMetaData);
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
        pAllocator->pfnFree(pAllocator->pUserData, this);
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
