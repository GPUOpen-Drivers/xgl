/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace vk
{

// =====================================================================================================================
DescriptorSetLayout::DescriptorSetLayout(
    const Device*     pDevice,
    const CreateInfo& info) :
    m_info(info),
    m_pDevice(pDevice)
{

}

// =====================================================================================================================
// Returns the dword size required in the static section for a particular type of descriptor.
uint32_t DescriptorSetLayout::GetDescStaticSectionDwSize(const Device* pDevice, VkDescriptorType type)
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
    default:
        VK_NEVER_CALLED();
        size = 0;
        break;
    }

    VK_ASSERT(Util::IsPow2Aligned(size, sizeof(uint32_t)));

    return size / sizeof(uint32_t);
}

// =====================================================================================================================
// Returns the dword size required in the fmask section for a particular type of descriptor.
uint32_t  DescriptorSetLayout::GetDescFmaskSectionDwSize(const Device* pDevice, VkDescriptorType type)
{
    const Device::Properties& props = pDevice->GetProperties();

    uint32_t size;

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        size = props.descriptorSizes.fmaskView;
        break;
    default:
        size = 0;
        break;
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
    BindingSectionInfo*                 pBindingSectionInfo,
    bool                                isFmaskSection)
{
    uint32_t descCount = pBindingInfo->descriptorCount;

    // Dword offset to this binding
    pBindingSectionInfo->dwOffset = Util::Pow2Align(pSectionInfo->dwSize, descAlignmentInDw);

    // Array stride in dwords.
    pBindingSectionInfo->dwArrayStride = descSizeInDw;

    // Size of the whole array in dwords.
    pBindingSectionInfo->dwSize = descCount * pBindingSectionInfo->dwArrayStride;

    // If this descriptor actually requires storage in the section then also update the global section information.
    if (pBindingSectionInfo->dwSize > 0)
    {
        // Update total section size by how much space this binding takes.
        pSectionInfo->dwSize += pBindingSectionInfo->dwSize;

        // Update total number of PAL ResourceMappingNodes required by this binding.
        pSectionInfo->numPalRsrcMapNodes++;

        // Combined image sampler descriptors in static section need an additional PAL ResourceMappingNode.
        if (!isFmaskSection && (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
        {
            pSectionInfo->numPalRsrcMapNodes++;
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
    if (pBindingInfo->pImmutableSamplers != nullptr)
    {
        // Only samplers and the sampler part of combined image samplers is allowed to be immutable.
        VK_ASSERT((pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
                  (pBindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
        uint32_t descCount = pBindingInfo->descriptorCount;

        // Dword offset to this binding's immutable data.
        pBindingSectionInfo->dwOffset = pSectionInfo->numImmutableSamplers * descSizeInDw;

        // Array stride in dwords.
        pBindingSectionInfo->dwArrayStride = descSizeInDw;

        // Size of the whole array in dwords.
        pBindingSectionInfo->dwSize = descCount * pBindingSectionInfo->dwArrayStride;

        // If this descriptor actually requires storage in the section then also update the global section information,
        // and populate the immutable descriptor data.
        if (pBindingSectionInfo->dwSize > 0)
        {
            // Update the number of immutable samplers.
            pSectionInfo->numImmutableSamplers += descCount;
            ++pSectionInfo->numDescriptorValueNodes;

            // Copy the immutable descriptor data.
            uint32_t* pDestAddr = &pSectionInfo->pImmutableSamplerData[pBindingSectionInfo->dwOffset];

            for (uint32_t i = 0; i < descCount; ++i, pDestAddr += descSizeInDw)
            {
                const void* pSamplerDesc = Sampler::ObjectFromHandle(pBindingInfo->pImmutableSamplers[i])->Descriptor();

                memcpy(pDestAddr, pSamplerDesc, descSizeInDw * sizeof(uint32_t));
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
    CreateInfo*                            pOut)
{
    if (pIn == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    union
    {
        const VkStructHeader*                   pHeader;
        const VkDescriptorSetLayoutCreateInfo*  pInfo;
    };

    pOut->activeStageMask = VK_SHADER_STAGE_ALL; // TODO set this up properly enumerating the active stages.
                                                 // currently this flag is only tested for non zero, so
                                                 // setting all flags active makes no difference...

    pOut->sta.dwSize                = 0;
    pOut->sta.numPalRsrcMapNodes    = 0;

    pOut->dyn.dwSize                = 0;
    pOut->dyn.numPalRsrcMapNodes    = 0;

    pOut->imm.numImmutableSamplers  = 0;

    pOut->fmask.dwSize              = 0;
    pOut->fmask.numPalRsrcMapNodes  = 0;

    for (pInfo = pIn; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
            {
                // Bindings numbers are allowed to come in out-of-order, as well as with gaps.
                // We compute offsets using the size we've seen so far as we iterate, so we need to handle
                // the bindings in binding-number order, rather than array order.

                // First, copy the binding info into our output array in order.
                for (uint32_t inIndex = 0; inIndex < pInfo->bindingCount; ++inIndex)
                {
                    const VkDescriptorSetLayoutBinding & currentBinding = pInfo->pBindings[inIndex];
                    pOut->bindings[currentBinding.binding].info = currentBinding;
                }

                // Now iterate over our output array to convert the binding info.  Any gaps in
                // the input binding numbers will be dummy entries in this array, but it
                // should be safe to call ConvertBindingInfo on those as well.
                for (uint32_t bindingNumber = 0; bindingNumber < pOut->count; ++bindingNumber)
                {
                    BindingInfo* pBinding = &pOut->bindings[bindingNumber];

                    // Determine the alignment requirement of descriptors in dwords.
                    uint32_t descAlignmentInDw = pDevice->GetProperties().descriptorSizes.alignment / sizeof(uint32_t);

                    // Construct the information specific to the static section of the descriptor set layout.
                    ConvertBindingInfo(
                        &pBinding->info,
                        GetDescStaticSectionDwSize(pDevice, pBinding->info.descriptorType),
                        descAlignmentInDw,
                        &pOut->sta,
                        &pBinding->sta,
                        false);

                    // Construct the information specific to the dynamic section of the descriptor set layout.
                    ConvertBindingInfo(
                        &pBinding->info,
                        GetDescDynamicSectionDwSize(pDevice, pBinding->info.descriptorType),
                        descAlignmentInDw,
                        &pOut->dyn,
                        &pBinding->dyn,
                        false);

                    // Construct the information specific to the immutable section of the descriptor set layout.
                    ConvertImmutableInfo(
                        &pBinding->info,
                        GetDescImmutableSectionDwSize(pDevice, pBinding->info.descriptorType),
                        &pOut->imm,
                        &pBinding->imm);

                    if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
                    {
                        // Construct the information specific to the fmask section of the descriptor set layout.
                        ConvertBindingInfo(
                            &pBinding->info,
                            GetDescFmaskSectionDwSize(pDevice, pBinding->info.descriptorType),
                            descAlignmentInDw,
                            &pOut->fmask,
                            &pBinding->fmask,
                            true);
                    }

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
    // We add pBinding size to the apiSize so that they would reside consecutively in memory
    // The reasoning is that we don't know the size of pBinding until now in creation time,
    // and we don't want to use dynamic allocation for small sets, where we don't know where
    // that memory would end up.
    // Additionally, we also have to add auxiliary memory for the immutable sampler arrays of
    // each binding.

    size_t immSamplerCount  = 0;
    uint32_t bindingCount   = 0;
    for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i)
    {
        if (pCreateInfo->pBindings[i].pImmutableSamplers != nullptr)
        {
            immSamplerCount += pCreateInfo->pBindings[i].descriptorCount;
        }

        bindingCount = Util::Max(bindingCount, pCreateInfo->pBindings[i].binding + 1);
    }

    const size_t bindingInfoAuxSize = bindingCount * sizeof(BindingInfo);
    const size_t immSamplerAuxSize  = immSamplerCount * pDevice->GetProperties().descriptorSizes.sampler;

    const size_t apiSize = sizeof(DescriptorSetLayout);
    const size_t auxSize = bindingInfoAuxSize + immSamplerAuxSize;
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
    info.bindings = reinterpret_cast<BindingInfo*>(reinterpret_cast<uint8_t*>(pSysMem) + apiSize);

    // Also memset it as not all bindings may be actually used
    memset(info.bindings, 0, bindingInfoAuxSize);

    // Set the base pointer of the immutable sampler data to the appropriate location within the allocated memory
    info.imm.pImmutableSamplerData = reinterpret_cast<uint32_t*>(Util::VoidPtrInc(pSysMem, apiSize + bindingInfoAuxSize));

    // Fill descriptor set layout information
    VkResult result = ConvertCreateInfo(
        pDevice,
        pCreateInfo,
        &info);

    if (result != VK_SUCCESS)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pSysMem);

        return result;
    }

    VK_PLACEMENT_NEW (pSysMem) DescriptorSetLayout (pDevice, info);

    *pLayout = DescriptorSetLayout::HandleFromVoidPointer(pSysMem);

    return result;
}

// =====================================================================================================================
// Destroy descriptor set layout object
VkResult DescriptorSetLayout::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    this->~DescriptorSetLayout();

    pAllocator->pfnFree(pAllocator->pUserData, this);

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

        DescriptorSetLayout::ObjectFromHandle(descriptorSetLayout)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
