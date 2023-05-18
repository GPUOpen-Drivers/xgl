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
 * @file  vk_pipeline_layout.cpp
 * @brief Contains implementation of Vulkan pipeline layout objects.
 ***********************************************************************************************************************
 */

#include "include/graphics_pipeline_common.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_shader.h"
#include "include/vk_sampler.h"
#include "include/vk_utils.h"

#include "palMetroHash.h"
#include "palVectorImpl.h"

namespace vk
{

// =====================================================================================================================
// Returns TRUE if user data should be reserved for uber fetch shader constant buffer
// This function doesn't control whether uber fetch shader is used in practice.  There would be no additional user data
// or SGPR allocated if user data is reserved while uber fetch shader is not used during pipeline creation.
template <PipelineLayoutScheme scheme>
static bool IsUberFetchShaderEnabled(const Device* pDevice)
{
    bool enabled = false;

    if (pDevice->GetRuntimeSettings().enableUberFetchShader ||
        (pDevice->GetRuntimeSettings().enableEarlyCompile && (scheme == PipelineLayoutScheme::Compact)) ||
        pDevice->IsExtensionEnabled(DeviceExtensions::EXT_GRAPHICS_PIPELINE_LIBRARY) ||
        pDevice->IsExtensionEnabled(DeviceExtensions::EXT_VERTEX_INPUT_DYNAMIC_STATE)
    )
    {
        enabled = true;
    }

    return enabled;
}

// =====================================================================================================================
// Generates the API hash using the contents of the VkPipelineLayoutCreateInfo struct
uint64_t PipelineLayout::BuildApiHash(
    const VkPipelineLayoutCreateInfo* pCreateInfo)
{
    Util::MetroHash64 hasher;

    hasher.Update(pCreateInfo->flags);
    hasher.Update(pCreateInfo->setLayoutCount);

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
    {
        if (pCreateInfo->pSetLayouts[i] != VK_NULL_HANDLE)
        {
            hasher.Update(DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i])->GetApiHash());
        }
        else
        {
            hasher.Update(0ULL);
        }
    }

    hasher.Update(pCreateInfo->pushConstantRangeCount);

    for (uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
    {
        hasher.Update(pCreateInfo->pPushConstantRanges[i]);
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t*>(&hash));

    return hash;
}

// =====================================================================================================================
constexpr size_t PipelineLayout::GetMaxResMappingRootNodeSize()
{
    return
                sizeof(Vkgc::ResourceMappingRootNode)
        ;
}

constexpr size_t PipelineLayout::GetMaxResMappingNodeSize()
{
    return
                sizeof(Vkgc::ResourceMappingNode)
        ;
}

constexpr size_t PipelineLayout::GetMaxStaticDescValueSize()
{
    return
                sizeof(Vkgc::StaticDescriptorValue)
        ;
}

// =====================================================================================================================
PipelineLayout::PipelineLayout(
    const Device*       pDevice,
    const Info&         info,
    const PipelineInfo& pipelineInfo,
    uint64_t            apiHash)
    :
    m_info(info),
    m_pipelineInfo(pipelineInfo),
    m_pDevice(pDevice),
    m_apiHash(apiHash)
{

}

// =====================================================================================================================
VkResult PipelineLayout::ConvertCreateInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo,
    SetUserDataLayout*                pSetUserDataLayouts)
{
    VkResult                   result                         = VK_SUCCESS;
    uint32_t                   pushConstantsSizeInBytes       = 0;
    uint32_t                   pushConstantsUserDataNodeCount = 0;
    const PipelineLayoutScheme scheme                         = DeterminePipelineLayoutScheme(pDevice, pIn);

    pInfo->setCount = pIn->setLayoutCount;

    ProcessPushConstantsInfo(
        pIn,
        &pushConstantsSizeInBytes,
        &pushConstantsUserDataNodeCount);

    if (scheme == PipelineLayoutScheme::Indirect)
    {
        result = BuildIndirectSchemeInfo(
            pDevice,
            pIn,
            pushConstantsSizeInBytes,
            pInfo,
            pPipelineInfo,
            pSetUserDataLayouts);
    }
    else if (scheme == PipelineLayoutScheme::Compact)
    {
        result = BuildCompactSchemeInfo(
            pDevice,
            pIn,
            pushConstantsSizeInBytes,
            pushConstantsUserDataNodeCount,
            pInfo,
            pPipelineInfo,
            pSetUserDataLayouts);
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return result;
}

// =====================================================================================================================
void PipelineLayout::ProcessPushConstantsInfo(
    const VkPipelineLayoutCreateInfo* pIn,
    uint32_t*                         pPushConstantsSizeInBytes,
    uint32_t*                         pPushConstantsUserDataNodeCount)
{
    uint32_t pushConstantsSizeInBytes       = 0;
    uint32_t pushConstantsUserDataNodeCount = 1;

    for (uint32_t i = 0; i < pIn->pushConstantRangeCount; ++i)
    {
        const VkPushConstantRange* pRange = &pIn->pPushConstantRanges[i];

        // Test if this push constant range is active in at least one stage
        if (pRange->stageFlags != 0)
        {
            pushConstantsSizeInBytes = Util::Max(pushConstantsSizeInBytes, pRange->offset + pRange->size);
        }
    }

    *pPushConstantsSizeInBytes       = pushConstantsSizeInBytes;
    *pPushConstantsUserDataNodeCount = pushConstantsUserDataNodeCount;
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Checks if GpuRT resource mappings will need to be added to this pipeline layout
bool PipelineLayout::HasRayTracing(
    const VkPipelineLayoutCreateInfo* pIn)
{
    bool rtFound = false;

    for (uint32_t setIndex = 0; (setIndex < pIn->setLayoutCount) && (rtFound == false); ++setIndex)
    {
        if (pIn->pSetLayouts[setIndex] != VK_NULL_HANDLE)
        {
            const auto pSetLayout = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[setIndex]);

            // Test if the set layout supports the RayGen stage (required for RT pipelines)
            // Without this check, compilation fails for RT pipelines that don't utilize trace ray
            if (Util::TestAnyFlagSet(pSetLayout->Info().activeStageMask, VK_SHADER_STAGE_RAYGEN_BIT_KHR) == true)
            {
                rtFound = true;
                break;
            }

            // Test if an acceleration structure descriptor binding is present (necessary for pipelines with ray query)
            for (uint32_t bindingIndex = 0; bindingIndex < pSetLayout->Info().count; ++bindingIndex)
            {
                const DescriptorSetLayout::BindingInfo& binding = pSetLayout->Binding(bindingIndex);

                if (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {
                    rtFound = true;
                    break;
                }
            }
        }
    }

    return rtFound;
}
#endif

// =====================================================================================================================
VkResult PipelineLayout::BuildCompactSchemeInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    const uint32_t                    pushConstantsSizeInBytes,
    const uint32_t                    pushConstantsUserDataNodeCount,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo,
    SetUserDataLayout*                pSetUserDataLayouts)
{
    // We currently allocate user data registers for various resources in the following fashion:
    // First user data registers will hold the descriptor set bindings in increasing order by set index.
    // For each descriptor set binding we first store the dynamic descriptor data (if there's dynamic section data)
    // followed by the set pointer (if there's static section data).
    // Push constants follow the descriptor set binding data.
    // Finally, the vertex buffer table pointer is in the last user data register when applicable.
    // This allocation allows the descriptor set bindings to easily persist across pipeline switches.

    VkResult result          = VK_SUCCESS;
    auto*    pUserDataLayout = &pInfo->userDataLayout.compact;

    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    memset(pPipelineInfo,            0, sizeof(PipelineInfo));
    memset(&(pInfo->userDataLayout), 0, sizeof(UserDataLayout));
    pInfo->userDataLayout.scheme = PipelineLayoutScheme::Compact;

    if (settings.enableEarlyCompile)
    {
        // Early compile mode will enable uber-fetch shader and spec constant buffer on vertex shader and
        // fragment shader implicitly.  So we need three reserved node.
        // Each buffer consume 2 user data register now.
        pPipelineInfo->numUserDataNodes += 3;
        pInfo->userDataRegCount         += 3 * InternalConstBufferRegCount;

        pUserDataLayout->uberFetchConstBufRegBase    = FetchShaderInternalBufferOffset;
        pUserDataLayout->specConstBufVertexRegBase   = SpecConstBufferVertexOffset;
        pUserDataLayout->specConstBufFragmentRegBase = SpecConstBufferFragmentOffset;
    }
    else
    {
        pUserDataLayout->uberFetchConstBufRegBase    = InvalidReg;
        pUserDataLayout->specConstBufVertexRegBase   = InvalidReg;
        pUserDataLayout->specConstBufFragmentRegBase = InvalidReg;
    }

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);

    // Total number of dynamic descriptors across all descriptor sets
    uint32_t totalDynDescCount = 0;

    const uint32_t pushConstRegCount = pushConstantsSizeInBytes / sizeof(uint32_t);

    uint32_t gfxReservedCount = 0;
    // Reserve an user-data to store the VA of buffer for transform feedback.
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        gfxReservedCount++;
    }

    if (pDevice->GetRuntimeSettings().enableDebugPrintf)
    {
        gfxReservedCount++;
    }

#if VKI_RAY_TRACING
    if (HasRayTracing(pIn))
    {
        gfxReservedCount += (InternalConstBufferRegCount + MaxTraceRayUserDataRegCount);
    }
#endif

    // the user data entries for uber-fetch shader const buffer
    if (IsUberFetchShaderEnabled<PipelineLayoutScheme::Compact>(pDevice) &&
        (settings.enableEarlyCompile == false))
    {
        gfxReservedCount += InternalConstBufferRegCount;
    }

    // Reseve PAL internal user data node for base vertex, base instance, draw id and lds_esgs_size.
    gfxReservedCount += 4;

    const uint32_t gfxInlinePushDescriptorUserDataLimit =
        (settings.gfxInlinePushDescriptorUserDataLimit > gfxReservedCount) ?
        settings.gfxInlinePushDescriptorUserDataLimit - gfxReservedCount :
        0;

    // Populate user data layouts for each descriptor set that is active
    pUserDataLayout->setBindingRegBase = pInfo->userDataRegCount;

    for (uint32_t i = 0; i < pIn->setLayoutCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pSetUserDataLayouts[i];

        // Initialize the set layout info
        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescCount         = 0;
        pSetUserData->firstRegOffset       = pInfo->userDataRegCount - pUserDataLayout->setBindingRegBase;
        pSetUserData->totalRegCount        = 0;

        const DescriptorSetLayout* pDescSetLayout = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i]);

        // Test if this set is active in at least one stage
        if ((pDescSetLayout != nullptr) && (pDescSetLayout->Info().activeStageMask != 0))
        {
            const DescriptorSetLayout::CreateInfo& setLayoutInfo  = pDescSetLayout->Info();
            pSetUserData->dynDescCount = setLayoutInfo.numDynamicDescriptors;

            // Optimized path to inline push descriptors if it can be done without spilling out of user data registers
            bool inlinePushDescriptorSet = false;

            if ((setLayoutInfo.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) &&
                ((setLayoutInfo.activeStageMask & ~(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT)) == 0))
            {
                uint32_t regCountSpillLimit = (setLayoutInfo.activeStageMask == VK_SHADER_STAGE_COMPUTE_BIT) ?
                                                settings.csInlinePushDescriptorUserDataLimit :
                                                gfxInlinePushDescriptorUserDataLimit;

                uint32_t inlineRegCount     = pInfo->userDataRegCount +
                                              setLayoutInfo.sta.dwSize +
                                              pushConstRegCount;

                VK_ASSERT(regCountSpillLimit < MaxBindingRegCount);
                VK_ASSERT(setLayoutInfo.numDynamicDescriptors == 0);

                // Also check any remaining sets because pInfo->userDataRegCount doesn't yet include them.
                for (uint32_t j = i + 1; (j < pIn->setLayoutCount) && (inlineRegCount <= regCountSpillLimit); ++j)
                {
                    const auto& nextSetLayoutInfo = DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[j])->Info();

                    inlineRegCount += nextSetLayoutInfo.numDynamicDescriptors *
                                      DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);

                    if (nextSetLayoutInfo.sta.numRsrcMapNodes > 0)
                    {
                        inlineRegCount += SetPtrRegCount;
                    }
                }

                if (inlineRegCount <= regCountSpillLimit)
                {
                    inlinePushDescriptorSet = true;
                }
            }

            if (inlinePushDescriptorSet)
            {
                // Add space for the user data node entries instead of resource nodes
                pPipelineInfo->numUserDataNodes += setLayoutInfo.sta.numRsrcMapNodes;

                // Add space for immutable sampler descriptor storage needed by the set
                pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

                // Reserve additional user data register space similar to what is done for dynamic descriptor data
                pSetUserData->totalRegCount = setLayoutInfo.sta.dwSize;

                // A push descriptor set without a set pointer offset is indication that pipeline creation's resource
                // node mapping and push descriptor set calls will reference the user data directly.
                VK_ASSERT(pSetUserData->setPtrRegOffset == InvalidReg);
            }
            else
            {
                // Accumulate the space needed by all resource nodes for this set
                pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;

                // Add count for FMASK nodes
                if (settings.enableFmaskBasedMsaaRead)
                {
                    pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;
                }

                // Add space for the user data node entries needed for dynamic descriptors
                pPipelineInfo->numUserDataNodes += setLayoutInfo.dyn.numRsrcMapNodes;

                // Add space for immutable sampler descriptor storage needed by the set
                pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

                // Reserve user data register space for dynamic descriptor data
                pSetUserData->dynDescDataRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;

                pSetUserData->totalRegCount += pSetUserData->dynDescCount *
                                               DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);

                totalDynDescCount += setLayoutInfo.numDynamicDescriptors;

                if (setLayoutInfo.sta.numRsrcMapNodes > 0)
                {
                    // If the set has a static portion reserve an extra user data node entry for the set pointer
                    pPipelineInfo->numUserDataNodes++;

                    // In this case we also reserve the user data for the set pointer
                    pSetUserData->setPtrRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
                    pSetUserData->totalRegCount  += SetPtrRegCount;
                }
            }
        }

        // Add the number of user data regs used by this set to the total count for the whole layout
        pInfo->userDataRegCount += pSetUserData->totalRegCount;
        if (settings.pipelineLayoutMode == PipelineLayoutAngle)
        {
            // Force next set firstRegOffset align to AngleDescPattern.
            if ((i + 1) < Util::ArrayLen(AngleDescPattern::DescriptorSetOffset))
            {
                if (pInfo->userDataRegCount < AngleDescPattern::DescriptorSetOffset[i + 1])
                {
                    pInfo->userDataRegCount = AngleDescPattern::DescriptorSetOffset[i + 1];
                }
            }
        }
    }

    // Calculate total number of user data regs used for active descriptor set data
    pUserDataLayout->setBindingRegCount = pInfo->userDataRegCount - pUserDataLayout->setBindingRegBase;

    VK_ASSERT(totalDynDescCount <= MaxDynamicDescriptors);

    // Allocate user data for push constants
    pPipelineInfo->numUserDataNodes += pushConstantsUserDataNodeCount;

    pUserDataLayout->pushConstRegBase  = pInfo->userDataRegCount;
    pUserDataLayout->pushConstRegCount = pushConstRegCount;
    pInfo->userDataRegCount           += pushConstRegCount;

    // Reserve an user-data to store the VA of buffer for transform feedback.
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        pUserDataLayout->transformFeedbackRegBase  = pInfo->userDataRegCount;
        pUserDataLayout->transformFeedbackRegCount = 1;
        pInfo->userDataRegCount                   += pUserDataLayout->transformFeedbackRegCount;
        pPipelineInfo->numUserDataNodes           += 1;
    }

    if (pDevice->GetRuntimeSettings().enableDebugPrintf)
    {
        pPipelineInfo->numUserDataNodes    += 1;
        pUserDataLayout->debugPrintfRegBase = pInfo->userDataRegCount;
        pInfo->userDataRegCount            += 1;
        pPipelineInfo->numRsrcMapNodes     += 1;
    }

    // Allocate user data for the thread group reversal state
    ReserveAlternatingThreadGroupUserData(
        pDevice,
        pIn,
        &pPipelineInfo->numUserDataNodes,
        &pInfo->userDataRegCount,
        &pUserDataLayout->threadGroupReversalRegBase);

#if VKI_RAY_TRACING
    if (HasRayTracing(pIn))
    {
        // Reserve one node for indirect RT capture replay.
        pPipelineInfo->numUserDataNodes                += 1;
        pUserDataLayout->rtCaptureReplayConstBufRegBase = pInfo->userDataRegCount;
        pInfo->userDataRegCount                        += InternalConstBufferRegCount;

        // Dispatch ray args
        pInfo->userDataRegCount         += MaxTraceRayUserDataRegCount;
        pPipelineInfo->numUserDataNodes += MaxTraceRayUserDataNodeCount;
        pPipelineInfo->numRsrcMapNodes  += MaxTraceRayResourceNodeCount;
        pPipelineInfo->hasRayTracing     = true;
    }
#endif

    // Reserve user data nodes for vertex buffer table
    // Info::userDataRegCount is not increased since this node is always appended at the bottom of user data table
    // Same for constant buffer for uber-fetch shader
    pPipelineInfo->numUserDataNodes          += 1;
    // In case we need an internal vertex buffer table, add nodes required for its entries, and its set pointer.
    pPipelineInfo->numRsrcMapNodes           += Pal::MaxVertexBuffers;

    // If uber-fetch shader is not enabled for early compile, the user data entries for uber-fetch shader const
    // buffer is appended at the bottom of user data table.  Just following vertex buffer table.
    if (IsUberFetchShaderEnabled<PipelineLayoutScheme::Compact>(pDevice) &&
        (pDevice->GetRuntimeSettings().enableEarlyCompile == false))
    {
        VK_ASSERT(pUserDataLayout->uberFetchConstBufRegBase == InvalidReg);

        pUserDataLayout->uberFetchConstBufRegBase = pInfo->userDataRegCount + VbTablePtrRegCount;
        pPipelineInfo->numUserDataNodes          += 1;
    }

    // Calculate the buffer size necessary for all resource mapping
    pPipelineInfo->mappingBufferSize =
        (pPipelineInfo->numUserDataNodes * GetMaxResMappingRootNodeSize()) +
        (pPipelineInfo->numRsrcMapNodes * GetMaxResMappingNodeSize()) +
        (pPipelineInfo->numDescRangeValueNodes * GetMaxStaticDescValueSize());

    // If we go past our user data limit, we can't support this pipeline
    if (pInfo->userDataRegCount >=
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineLayout::BuildIndirectSchemeInfo(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn,
    const uint32_t                    pushConstantsSizeInBytes,
    Info*                             pInfo,
    PipelineInfo*                     pPipelineInfo,
    SetUserDataLayout*                pSetUserDataLayouts)
{
    // Indirect mode is designed for the case that the pipeline layout only contains part of layout information of the
    // final executable pipeline. So that user data is used in a conservative way to make sure that this pipeline layout
    // is always compatible with other layouts which may contain the descriptor which is not known currently.
    //
    // The user data registers for various resources is allocated in the following fashion:
    // 1. one user data entry for the vertex buffer table pointer
    // 2. two user data entries for constant buffer required by uber-fetch shader (if extension or setting require)
    // 3. one user data entry for the push constant buffer pointer
    // 4. one user data entry for transform feedback buffer (if extension is enabled)
#ifdef VKI_RAY_TRACING
    // 5. one user data entry for ray tracing dispatch arguments (if extension is enabled)
#endif
    // 6. MaxDescriptorSets sets of user data entries which store the information for each descriptor set. Each set
    //    contains 2 user data entry: the 1st is for the dynamic descriptors and the 2nd is for static descriptors.
    //
    // TODO: The following features have not been supported by indirect scheme:
    //       1. PipelineLayoutAngle mode

    VK_ASSERT(pIn->setLayoutCount <= MaxDescriptorSets);
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    VK_ASSERT(settings.pipelineLayoutMode != PipelineLayoutAngle);
    VK_ASSERT(settings.enableEarlyCompile == false);

    VkResult      result            = VK_SUCCESS;
    auto*         pUserDataLayout   = &pInfo->userDataLayout.indirect;
    uint32_t      totalDynDescCount = 0;

    memset(pPipelineInfo,            0, sizeof(PipelineInfo));
    memset(&(pInfo->userDataLayout), 0, sizeof(UserDataLayout));
    pInfo->userDataLayout.scheme = PipelineLayoutScheme::Indirect;

    VK_ASSERT(totalDynDescCount <= MaxDynamicDescriptors);

    // Allocate user data for vertex buffer table
    pPipelineInfo->numUserDataNodes += 1;
    pPipelineInfo->numRsrcMapNodes  += Pal::MaxVertexBuffers;
    pInfo->userDataRegCount         += 1;

    // Allocate user data for constant buffer used by uber-fetch shader
    if (IsUberFetchShaderEnabled<PipelineLayoutScheme::Indirect>(pDevice))
    {
        pUserDataLayout->uberFetchConstBufRegBase = pInfo->userDataRegCount;
        pPipelineInfo->numUserDataNodes          += 1;
        pInfo->userDataRegCount                  += InternalConstBufferRegCount;
    }

    // Allocate user data for push constant buffer pointer
    pUserDataLayout->pushConstPtrRegBase  = pInfo->userDataRegCount;
    pUserDataLayout->pushConstSizeInDword = pushConstantsSizeInBytes / sizeof(uint32_t);
    pPipelineInfo->numUserDataNodes      += 1;
    pPipelineInfo->numRsrcMapNodes       += 1;
    pInfo->userDataRegCount              += 1;

    // Allocate user data for transform feedback buffer
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK))
    {
        pUserDataLayout->transformFeedbackRegBase = pInfo->userDataRegCount;
        pPipelineInfo->numUserDataNodes          += 1;
        pInfo->userDataRegCount                  += 1;
    }

    if (settings.enableDebugPrintf)
    {
        pPipelineInfo->numUserDataNodes    += 1;
        pUserDataLayout->debugPrintfRegBase = pInfo->userDataRegCount;
        pInfo->userDataRegCount            += 1;
        pPipelineInfo->numRsrcMapNodes     += 1;
    }

    // Allocate user data for the thread group reversal state
    ReserveAlternatingThreadGroupUserData(
        pDevice,
        pIn,
        &pPipelineInfo->numUserDataNodes,
        &pInfo->userDataRegCount,
        &pUserDataLayout->threadGroupReversalRegBase);

#if VKI_RAY_TRACING
    if (HasRayTracing(pIn))
    {
        pUserDataLayout->dispatchRaysArgsPtrRegBase = pInfo->userDataRegCount;
        pPipelineInfo->numUserDataNodes            += MaxTraceRayUserDataNodeCount;
        pPipelineInfo->numRsrcMapNodes             += MaxTraceRayResourceNodeCount;
        pInfo->userDataRegCount                    += MaxTraceRayUserDataRegCount;
        pPipelineInfo->hasRayTracing                = true;

        // Reserve one node for indirect RT capture replay.
        pUserDataLayout->rtCaptureReplayConstBufRegBase = pInfo->userDataRegCount;
        pPipelineInfo->numUserDataNodes                += 1;
        pInfo->userDataRegCount                        += InternalConstBufferRegCount;
    }
#endif

    // Allocate user data for descriptor sets
    pUserDataLayout->setBindingPtrRegBase = pInfo->userDataRegCount;
    pInfo->userDataRegCount              += 2 * SetPtrRegCount * MaxDescriptorSets;

    // This simulate the descriptor set user data register layout of compact scheme
    // so as to fill pSetUserDataLayouts[]
    // Indirect scheme also need to fill pSetUserDataLayouts[] because we need the data
    // in this array to locate the descriptor set data managed by vk::CmdBuffer
    uint32_t setBindingCompactRegBase = pUserDataLayout->setBindingPtrRegBase;

    for (uint32_t i = 0; i < pIn->setLayoutCount; ++i)
    {
        SetUserDataLayout* pSetUserData = &pSetUserDataLayouts[i];

        pSetUserData->setPtrRegOffset      = InvalidReg;
        pSetUserData->dynDescDataRegOffset = 0;
        pSetUserData->dynDescCount         = 0;
        pSetUserData->firstRegOffset       = setBindingCompactRegBase - pUserDataLayout->setBindingPtrRegBase;
        pSetUserData->totalRegCount        = 0;

        if (pIn->pSetLayouts[i] != VK_NULL_HANDLE)
        {
            const DescriptorSetLayout::CreateInfo& setLayoutInfo =
                DescriptorSetLayout::ObjectFromHandle(pIn->pSetLayouts[i])->Info();

            if (setLayoutInfo.activeStageMask != 0)
            {
                pSetUserData->dynDescCount = setLayoutInfo.numDynamicDescriptors;

                // Add space for static descriptors
                if (setLayoutInfo.sta.numRsrcMapNodes > 0)
                {
                    pPipelineInfo->numUserDataNodes += 1;
                    pPipelineInfo->numRsrcMapNodes  += setLayoutInfo.sta.numRsrcMapNodes;

                    // Add count for FMASK nodes
                    if (pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
                    {
                        pPipelineInfo->numRsrcMapNodes += setLayoutInfo.sta.numRsrcMapNodes;
                    }
                }

                // Add space for immutable sampler descriptor storage needed by the set
                pPipelineInfo->numDescRangeValueNodes += setLayoutInfo.imm.numDescriptorValueNodes;

                // Add space for dynamic descriptors
                if (setLayoutInfo.dyn.numRsrcMapNodes > 0)
                {
                    pPipelineInfo->numUserDataNodes += 1;
                    pPipelineInfo->numRsrcMapNodes  += setLayoutInfo.dyn.numRsrcMapNodes;
                    totalDynDescCount += setLayoutInfo.numDynamicDescriptors;
                }

                // Fill set user data layout
                pSetUserData->dynDescDataRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
                pSetUserData->totalRegCount +=
                    pSetUserData->dynDescCount * DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice);
                if (setLayoutInfo.sta.numRsrcMapNodes > 0)
                {
                    pSetUserData->setPtrRegOffset = pSetUserData->firstRegOffset + pSetUserData->totalRegCount;
                    pSetUserData->totalRegCount += SetPtrRegCount;
                }
            }
        }

        setBindingCompactRegBase += pSetUserData->totalRegCount;
    }

    // Calculate the buffer size necessary for all resource mapping
    pPipelineInfo->mappingBufferSize =
        (pPipelineInfo->numUserDataNodes       * GetMaxResMappingRootNodeSize()) +
        (pPipelineInfo->numRsrcMapNodes        * GetMaxResMappingNodeSize()    ) +
        (pPipelineInfo->numDescRangeValueNodes * GetMaxStaticDescValueSize()   );

    // If we go past our user data limit, we can't support this pipeline
    if (pInfo->userDataRegCount >=
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
PipelineLayoutScheme PipelineLayout::DeterminePipelineLayoutScheme(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pIn)
{
    PipelineLayoutScheme scheme = PipelineLayoutScheme::Compact;

    const RuntimeSettings&     settings     = pDevice->GetRuntimeSettings();

    switch (settings.pipelineLayoutSchemeSelectionStrategy)
    {
    case AppControlled:
        if (pIn->flags & VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT)
        {
            scheme = PipelineLayoutScheme::Indirect;
        }
        else
        {
            scheme = PipelineLayoutScheme::Compact;
        }
        break;
    case ForceCompact:
        scheme = PipelineLayoutScheme::Compact;
        break;
    case ForceIndirect:
        scheme = PipelineLayoutScheme::Indirect;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return scheme;
}

// =====================================================================================================================
// Creates a pipeline layout object.
VkResult PipelineLayout::Create(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*      pAllocator,
    VkPipelineLayout*                 pPipelineLayout)
{
    VK_ASSERT((pCreateInfo->setLayoutCount == 0) || (pCreateInfo->pSetLayouts != nullptr));

    VkResult     result       = VK_SUCCESS;
    Info         info         = {};
    PipelineInfo pipelineInfo = {};
    uint64_t     apiHash      = BuildApiHash(pCreateInfo);

    size_t setLayoutsArraySize = 0;

    for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
    {
        DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);
        if (pLayout != nullptr)
        {
            setLayoutsArraySize += pLayout->GetObjectSize(VK_SHADER_STAGE_ALL);
        }
    }

    // Need to add extra storage for DescriptorSetLayout*, SetUserDataLayout, the descriptor set layouts themselves,
    // the resource mapping nodes, and the descriptor range values
    const size_t apiSize                 = sizeof(PipelineLayout);
    const size_t setUserDataLayoutSize   =
        Util::Pow2Align((pCreateInfo->setLayoutCount * sizeof(SetUserDataLayout)), ExtraDataAlignment());
    const size_t descriptorSetLayoutSize =
        Util::Pow2Align((pCreateInfo->setLayoutCount * sizeof(DescriptorSetLayout*)), ExtraDataAlignment());

    size_t objSize = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize + setLayoutsArraySize;
    void* pSysMem = pDevice->AllocApiObject(pAllocator, objSize);

    if (pSysMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    SetUserDataLayout*    pSetUserData = nullptr;
    DescriptorSetLayout** ppSetLayouts = nullptr;

    if (result == VK_SUCCESS)
    {
        pSetUserData = static_cast<SetUserDataLayout*>(Util::VoidPtrInc(pSysMem, apiSize));
        ppSetLayouts = static_cast<DescriptorSetLayout**>(
            Util::VoidPtrInc(pSysMem, apiSize + setUserDataLayoutSize));

        result = ConvertCreateInfo(
            pDevice,
            pCreateInfo,
            &info,
            &pipelineInfo,
            pSetUserData);
    }

    if (result == VK_SUCCESS)
    {
        size_t currentSetLayoutOffset = apiSize + setUserDataLayoutSize + descriptorSetLayoutSize;

        for (uint32_t i = 0; i < pCreateInfo->setLayoutCount; ++i)
        {
            const DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pCreateInfo->pSetLayouts[i]);

            if (pLayout != nullptr)
            {
                ppSetLayouts[i] = reinterpret_cast<DescriptorSetLayout*>(Util::VoidPtrInc(pSysMem, currentSetLayoutOffset));

                // Copy the original descriptor set layout object
                pLayout->Copy(pDevice, ppSetLayouts[i]);

                currentSetLayoutOffset += pLayout->GetObjectSize(VK_SHADER_STAGE_ALL);
            }
            else
            {
                ppSetLayouts[i] = nullptr;
            }
        }

        VK_PLACEMENT_NEW(pSysMem) PipelineLayout(pDevice, info, pipelineInfo, apiHash);

        *pPipelineLayout = PipelineLayout::HandleFromVoidPointer(pSysMem);
    }

    if (result != VK_SUCCESS)
    {
        if (pSysMem != nullptr)
        {
            pDevice->FreeApiObject(pAllocator, pSysMem);
        }
    }

    return result;
}

// =====================================================================================================================
// Translates VkDescriptorType to VKGC ResourceMappingNodeType
Vkgc::ResourceMappingNodeType PipelineLayout::MapLlpcResourceNodeType(
    VkDescriptorType descriptorType)
{
    auto nodeType = Vkgc::ResourceMappingNodeType::Unknown;
    switch (static_cast<uint32_t>(descriptorType))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorSampler;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorCombinedTexture;
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorResource;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorImage;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
#if VKI_RAY_TRACING
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
#endif
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorTexelBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
        break;
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        nodeType = Vkgc::ResourceMappingNodeType::InlineBuffer;
        break;
    case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
        nodeType = Vkgc::ResourceMappingNodeType::DescriptorMutable;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }
    return nodeType;
}

// =====================================================================================================================
// Builds the VKGC resource mapping node for a static descriptor in a descriptor set
void PipelineLayout::BuildLlpcStaticMapping(
    const DescriptorSetLayout*              pLayout,
    const uint32_t                          visibility,
    const uint32_t                          setIndex,
    const DescriptorSetLayout::BindingInfo& binding,
    Vkgc::ResourceMappingNode*              pNode,
    Vkgc::StaticDescriptorValue*            pDescriptorRangeValue,
    uint32_t*                               pDescriptorRangeCount
    ) const
{
    pNode->type                  = MapLlpcResourceNodeType(binding.info.descriptorType);
    pNode->offsetInDwords        = binding.sta.dwOffset;
    pNode->sizeInDwords          = binding.sta.dwSize;
    if (pNode->type == Vkgc::ResourceMappingNodeType::DescriptorMutable)
    {
        pNode->srdRange.strideInDwords = binding.sta.dwArrayStride;
    }
    else
    {
        pNode->srdRange.strideInDwords = 0;
    }
    pNode->srdRange.binding      = binding.info.binding;
    pNode->srdRange.set          = setIndex;

    if (binding.imm.dwSize > 0)
    {
        const uint32_t  arraySize             = binding.imm.dwSize / binding.imm.dwArrayStride;
        const uint32_t* pImmutableSamplerData = pLayout->Info().imm.pImmutableSamplerData +
                                                binding.imm.dwOffset;

        if (binding.bindingFlags.ycbcrConversionUsage == 0)
        {
            pDescriptorRangeValue->type = Vkgc::ResourceMappingNodeType::DescriptorSampler;
        }
        else
        {
            pNode->type = Vkgc::ResourceMappingNodeType::DescriptorYCbCrSampler;
            pDescriptorRangeValue->type = Vkgc::ResourceMappingNodeType::DescriptorYCbCrSampler;
        }

        pDescriptorRangeValue->set          = setIndex;
        pDescriptorRangeValue->binding      = binding.info.binding;
        pDescriptorRangeValue->pValue       = pImmutableSamplerData;
        pDescriptorRangeValue->arraySize    = arraySize;
        pDescriptorRangeValue->visibility   = visibility;
        ++(*pDescriptorRangeCount);
    }
}

// =====================================================================================================================
// Builds the VKGC resource mapping node for a dynamic descriptor in a descriptor set
void PipelineLayout::BuildLlpcDynamicMapping(
    const uint32_t                          setIndex,
    const uint32_t                          userDataRegBase,
    const DescriptorSetLayout::BindingInfo& binding,
    Vkgc::ResourceMappingNode*              pNode
    ) const
{
    VK_ASSERT((binding.info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
              (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC));

    if (binding.info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
    {
        pNode->type = (binding.dyn.dwArrayStride == 2) ?
            Vkgc::ResourceMappingNodeType::DescriptorBufferCompact :
            Vkgc::ResourceMappingNodeType::DescriptorBuffer;
    }
    else
    {
        pNode->type = (binding.dyn.dwArrayStride == 2) ?
            Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact :
            Vkgc::ResourceMappingNodeType::DescriptorConstBuffer;
    }

    pNode->offsetInDwords          = userDataRegBase + binding.dyn.dwOffset;
    pNode->sizeInDwords            = binding.dyn.dwSize;
    pNode->srdRange.strideInDwords = 0;
    pNode->srdRange.binding        = binding.info.binding;
    pNode->srdRange.set            = setIndex;
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for vertex buffer table
void PipelineLayout::BuildLlpcVertexBufferTableMapping(
    const VbBindingInfo*           pVbInfo,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pNode,
    uint32_t*                      pNodeCount
    ) const
{
    if (pVbInfo != nullptr)
    {
        // Build the table description itself
        const uint32_t srdDwSize = m_pDevice->GetProperties().descriptorSizes.bufferView / sizeof(uint32_t);
        const uint32_t vbTableSize = pVbInfo->bindingTableSize * srdDwSize;

        // Add the set pointer node pointing to this table
        pNode->node.type                     = Vkgc::ResourceMappingNodeType::IndirectUserDataVaPtr;
        pNode->node.offsetInDwords           = offsetInDwords;
        pNode->node.sizeInDwords             = sizeInDwords;
        pNode->node.userDataPtr.sizeInDwords = vbTableSize;
        pNode->visibility                    = Vkgc::ShaderStageVertexBit;

        ++(*pNodeCount);
    }
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for transform feedback buffer
void PipelineLayout::BuildLlpcTransformFeedbackMapping(
    const uint32_t                 stageMask,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pNode,
    uint32_t*                      pNodeCount
    ) const
{
    uint32_t xfbStages         = (stageMask & (Vkgc::ShaderStageFragmentBit - 1)) >> 1;
    uint32_t lastXfbStageBit   = Vkgc::ShaderStageVertexBit;

    while (xfbStages > 0)
    {
        lastXfbStageBit <<= 1;
        xfbStages >>= 1;
    }

    if (lastXfbStageBit != 0)
    {
        pNode->node.type           = Vkgc::ResourceMappingNodeType::StreamOutTableVaPtr;
        pNode->node.offsetInDwords = offsetInDwords;
        pNode->node.sizeInDwords   = sizeInDwords;
        pNode->visibility          = lastXfbStageBit;

        ++(*pNodeCount);
    }
}

// =====================================================================================================================
void PipelineLayout::BuildLlpcInternalConstantBufferMapping(
    const uint32_t                 stageMask,
    const uint32_t                 offsetInDwords,
    const uint32_t                 binding,
    Vkgc::ResourceMappingRootNode* pNode,
    uint32_t*                      pNodeCount
    ) const
{
    if (stageMask != 0)
    {
        pNode->node.type                      = Vkgc::ResourceMappingNodeType::DescriptorBufferCompact;
        pNode->node.offsetInDwords            = offsetInDwords;
        pNode->node.sizeInDwords              = InternalConstBufferRegCount;
        pNode->node.srdRange.strideInDwords   = 0;
        pNode->node.srdRange.set              = Vkgc::InternalDescriptorSetId;
        pNode->node.srdRange.binding          = binding;
        pNode->visibility                     = stageMask;

        ++(*pNodeCount);
    }
}

#if VKI_RAY_TRACING
// =====================================================================================================================
// Calculates the offset for the GpuRT user data constants
uint32_t PipelineLayout::GetDispatchRaysUserData() const
{
    uint32_t              dispatchRaysUserData = 0;
    const UserDataLayout& userDataLayout       = GetInfo().userDataLayout;

    if (m_pipelineInfo.hasRayTracing)
    {
        if (userDataLayout.scheme == PipelineLayoutScheme::Compact)
        {
            // The dispatch rays args is always the last entry
            // TODO #raytracing: This means it spills first.  Probably bad for perf.
            dispatchRaysUserData = m_info.userDataRegCount;
        }
        else if (userDataLayout.scheme == PipelineLayoutScheme::Indirect)
        {
            dispatchRaysUserData = userDataLayout.indirect.dispatchRaysArgsPtrRegBase;
        }
        else
        {
            VK_NEVER_CALLED();
            dispatchRaysUserData = 0;
        }
    }

    return dispatchRaysUserData;
}

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for ray tracing dispatch arguments
void PipelineLayout::BuildLlpcRayTracingDispatchArgumentsMapping(
    const uint32_t                 stageMask,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pRootNode,
    uint32_t*                      pRootNodeCount,
    Vkgc::ResourceMappingNode*     pStaNode,
    uint32_t*                      pStaNodeCount
    ) const
{
    const Vkgc::ResourceMappingNode TraceRayLayout[] =
    {
        // TODO: Replace binding and set with enum once it is defined in vkgcDefs.h
        { Vkgc::ResourceMappingNodeType::DescriptorConstBufferCompact, 2, 0, {{93, 17, 0}} },
        { Vkgc::ResourceMappingNodeType::DescriptorConstBuffer, 4, 2, {{93, 0, 0}} },
        { Vkgc::ResourceMappingNodeType::DescriptorBuffer, 4, 6, {{93, 1, 0}} },
    };
    const uint32_t TraceRayLayoutNodeCount = static_cast<uint32_t>(Util::ArrayLen(TraceRayLayout));

    VK_ASSERT(TraceRayLayoutNodeCount < MaxTraceRayResourceNodeCount);

    Util::FastMemCpy(pStaNode, TraceRayLayout, sizeof(TraceRayLayout));
    *pStaNodeCount += TraceRayLayoutNodeCount;

    pRootNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
    pRootNode->node.offsetInDwords     = offsetInDwords;
    pRootNode->node.sizeInDwords       = sizeInDwords;
    pRootNode->node.tablePtr.pNext     = pStaNode;
    pRootNode->node.tablePtr.nodeCount = TraceRayLayoutNodeCount;
    pRootNode->visibility              = stageMask;

    ++(*pRootNodeCount);
}
#endif

// =====================================================================================================================
// Builds the VKGC resource mapping nodes for debugPrintf argument
void PipelineLayout::BuildLlpcDebugPrintfMapping(
    const uint32_t                 stageMask,
    const uint32_t                 offsetInDwords,
    const uint32_t                 sizeInDwords,
    Vkgc::ResourceMappingRootNode* pRootNode,
    uint32_t*                      pRootNodeCount,
    Vkgc::ResourceMappingNode*     pStaNode,
    uint32_t*                      pStaNodeCount
    ) const
{
    const Vkgc::ResourceMappingNode DebugPrintfLayout[] =
    {
        { Vkgc::ResourceMappingNodeType::DescriptorBuffer, 4, 0, {{Vkgc::InternalDescriptorSetId, 6}}},
    };

    Util::FastMemCpy(pStaNode, DebugPrintfLayout, sizeof(DebugPrintfLayout));
    *pStaNodeCount += static_cast<uint32_t>(Util::ArrayLen(DebugPrintfLayout));

    pRootNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
    pRootNode->node.offsetInDwords     = offsetInDwords;
    pRootNode->node.sizeInDwords       = sizeInDwords;
    pRootNode->node.tablePtr.pNext     = pStaNode;
    pRootNode->node.tablePtr.nodeCount = 1;
    pRootNode->visibility              = stageMask;

    ++(*pRootNodeCount);
}

// =====================================================================================================================
// Populates the resource mapping nodes in compact scheme
VkResult PipelineLayout::BuildCompactSchemeLlpcPipelineMapping(
    const uint32_t             stageMask,
    const VbBindingInfo*       pVbInfo,
    const bool                 appendFetchShaderCb,
#if VKI_RAY_TRACING
    const bool                 appendRtCaptureReplayCb,
#endif
    void*                      pBuffer,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VK_ASSERT(m_info.userDataLayout.scheme == PipelineLayoutScheme::Compact);

    VkResult    result             = VK_SUCCESS;
    const auto& userDataLayout     = m_info.userDataLayout.compact;
    const bool  enableEarlyCompile = m_pDevice->GetRuntimeSettings().enableEarlyCompile;

    Vkgc::ResourceMappingRootNode* pUserDataNodes = static_cast<Vkgc::ResourceMappingRootNode*>(pBuffer);
    Vkgc::ResourceMappingNode* pResourceNodes =
        reinterpret_cast<Vkgc::ResourceMappingNode*>(pUserDataNodes + m_pipelineInfo.numUserDataNodes);
    Vkgc::StaticDescriptorValue* pDescriptorRangeValues =
        reinterpret_cast<Vkgc::StaticDescriptorValue*>(pResourceNodes + m_pipelineInfo.numRsrcMapNodes);

    uint32_t userDataNodeCount    = 0; // Number of consumed ResourceMappingRootNodes
    uint32_t mappingNodeCount     = 0; // Number of consumed ResourceMappingNodes (only sub-nodes)
    uint32_t descriptorRangeCount = 0; // Number of consumed StaticResourceValues

    if (enableEarlyCompile)
    {
        VK_ASSERT(userDataLayout.specConstBufVertexRegBase   == SpecConstBufferVertexOffset);
        VK_ASSERT(userDataLayout.specConstBufFragmentRegBase == SpecConstBufferFragmentOffset);

        if (stageMask & Vkgc::ShaderStageVertexBit)
        {
            BuildLlpcInternalConstantBufferMapping(
                Vkgc::ShaderStageVertexBit,
                userDataLayout.specConstBufVertexRegBase,
                Vkgc::SpecConstInternalBufferBindingId + ShaderStage::ShaderStageVertex,
                &pUserDataNodes[userDataNodeCount],
                &userDataNodeCount);
        }

        if (stageMask & Vkgc::ShaderStageFragmentBit)
        {
            BuildLlpcInternalConstantBufferMapping(
                Vkgc::ShaderStageFragmentBit,
                userDataLayout.specConstBufFragmentRegBase,
                Vkgc::SpecConstInternalBufferBindingId + ShaderStage::ShaderStageFragment,
                &pUserDataNodes[userDataNodeCount],
                &userDataNodeCount);
        }
    }

    // Build descriptor for each set
    for (uint32_t setIndex = 0; setIndex < m_info.setCount; ++setIndex)
    {
        const auto pSetUserData = &GetSetUserData(setIndex);
        const auto pSetLayout   = GetSetLayouts(setIndex);

        const uint32_t visibility =
            (pSetLayout == nullptr) ? 0 : stageMask & VkToVkgcShaderStageMask(pSetLayout->Info().activeStageMask);

        if (visibility != 0)
        {
            // Build the resource mapping nodes for the contents of this set.
            auto     pStaNodes    = &pResourceNodes[mappingNodeCount];
            uint32_t staNodeCount = 0;

            for (uint32_t bindingIndex = 0; bindingIndex < pSetLayout->Info().count; ++bindingIndex)
            {
                const DescriptorSetLayout::BindingInfo& binding = pSetLayout->Binding(bindingIndex);

                if (binding.dyn.dwSize > 0)
                {
                    auto* pDynNode = &pUserDataNodes[userDataNodeCount++];

                    pDynNode->visibility = visibility;

                    BuildLlpcDynamicMapping(
                        setIndex,
                        userDataLayout.setBindingRegBase + pSetUserData->dynDescDataRegOffset,
                        binding,
                        &pDynNode->node);
                }

                if (binding.sta.dwSize > 0)
                {
                    Vkgc::ResourceMappingNode* pNode = nullptr;

                    if ((pSetLayout->Info().flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) &&
                        (pSetUserData->setPtrRegOffset == InvalidReg))
                    {
                        auto* pUserDataNode = &pUserDataNodes[userDataNodeCount++];

                        pUserDataNode->visibility = visibility;

                        pNode = &pUserDataNode->node;
                    }
                    else
                    {
                        pNode = &pStaNodes[staNodeCount++];
                    }

                    BuildLlpcStaticMapping(
                        pSetLayout,
                        visibility,
                        setIndex,
                        binding,
                        pNode,
                        &pDescriptorRangeValues[descriptorRangeCount],
                        &descriptorRangeCount);
                }
            }

            // Increase the number of mapping nodes used by the number of static section nodes added.
            mappingNodeCount += staNodeCount;

            // Add a top-level user data node entry for this set's pointer if there are static nodes.
            if (pSetUserData->setPtrRegOffset != InvalidReg)
            {
                auto pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = userDataLayout.setBindingRegBase +
                                                       pSetUserData->setPtrRegOffset;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pStaNodes;
                pSetPtrNode->visibility              = visibility;
                userDataNodeCount++;
            }
        }
    }

    // TODO: Build the internal push constant resource mapping
    if (userDataLayout.pushConstRegCount > 0)
    {
        auto pPushConstNode = &pUserDataNodes[userDataNodeCount];
        pPushConstNode->node.type = Vkgc::ResourceMappingNodeType::PushConst;
        pPushConstNode->node.offsetInDwords = userDataLayout.pushConstRegBase;
        pPushConstNode->node.sizeInDwords = userDataLayout.pushConstRegCount;
        pPushConstNode->node.srdRange.set = Vkgc::InternalDescriptorSetId;
        pPushConstNode->visibility = stageMask;

        userDataNodeCount += 1;
    }

    if (userDataLayout.transformFeedbackRegCount > 0)
    {
        BuildLlpcTransformFeedbackMapping(
            stageMask,
            userDataLayout.transformFeedbackRegBase,
            userDataLayout.transformFeedbackRegCount,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount);
    }

    if (m_pDevice->GetRuntimeSettings().enableDebugPrintf)
    {
        BuildLlpcDebugPrintfMapping(
            stageMask,
            userDataLayout.debugPrintfRegBase,
            1u,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount,
            &pResourceNodes[mappingNodeCount],
            &mappingNodeCount);
    }

    if (pVbInfo != nullptr)
    {
        const uint32_t tailingVertexBufferRegCount =
            (appendFetchShaderCb && (enableEarlyCompile == false)) ?
            (VbTablePtrRegCount + InternalConstBufferRegCount) : VbTablePtrRegCount;

        if ((m_info.userDataRegCount + tailingVertexBufferRegCount) <=
            m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.maxUserDataEntries)
        {
            BuildLlpcVertexBufferTableMapping(
                pVbInfo,
                m_info.userDataRegCount,
                VbTablePtrRegCount,
                &pUserDataNodes[userDataNodeCount],
                &userDataNodeCount);

            if (appendFetchShaderCb)
            {
                VK_ASSERT((enableEarlyCompile == false) ||
                          (userDataLayout.uberFetchConstBufRegBase == FetchShaderInternalBufferOffset));
                VK_ASSERT((enableEarlyCompile == true) ||
                          (userDataLayout.uberFetchConstBufRegBase == m_info.userDataRegCount + VbTablePtrRegCount));

                // Append node for uber fetch shader constant buffer
                BuildLlpcInternalConstantBufferMapping(
                    Vkgc::ShaderStageVertexBit,
                    userDataLayout.uberFetchConstBufRegBase,
                    Vkgc::FetchShaderInternalBufferBinding,
                    &pUserDataNodes[userDataNodeCount],
                    &userDataNodeCount);
            }
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    if ((stageMask & Vkgc::ShaderStageComputeBit) && (userDataLayout.threadGroupReversalRegBase != InvalidReg))
    {
        BuildLlpcInternalConstantBufferMapping(
            stageMask,
            userDataLayout.threadGroupReversalRegBase,
            Vkgc::ReverseThreadGroupControlBinding,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount);
    }

#if VKI_RAY_TRACING
    if (result == VK_SUCCESS)
    {
        if (m_pipelineInfo.hasRayTracing)
        {
            if (appendRtCaptureReplayCb)
            {
                BuildLlpcInternalConstantBufferMapping(
                    stageMask,
                    userDataLayout.rtCaptureReplayConstBufRegBase,
                    Vkgc::RtCaptureReplayInternalBufferBinding,
                    &pUserDataNodes[userDataNodeCount],
                    &userDataNodeCount);
            }

            BuildLlpcRayTracingDispatchArgumentsMapping(
                stageMask,
                m_info.userDataRegCount,
                MaxTraceRayUserDataRegCount,
                &pUserDataNodes[userDataNodeCount],
                &userDataNodeCount,
                &pResourceNodes[mappingNodeCount],
                &mappingNodeCount);
        }
    }
#endif

    // If you hit these assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(userDataNodeCount    <= m_pipelineInfo.numUserDataNodes);
    VK_ASSERT(mappingNodeCount     <= m_pipelineInfo.numRsrcMapNodes);
    VK_ASSERT(descriptorRangeCount <= m_pipelineInfo.numDescRangeValueNodes);

    pResourceMapping->pUserDataNodes             = pUserDataNodes;
    pResourceMapping->userDataNodeCount          = userDataNodeCount;
    pResourceMapping->pStaticDescriptorValues    = pDescriptorRangeValues;
    pResourceMapping->staticDescriptorValueCount = descriptorRangeCount;

    return result;
}

// =====================================================================================================================
// Populates the resource mapping nodes in indirect scheme
void PipelineLayout::BuildIndirectSchemeLlpcPipelineMapping(
    const uint32_t             stageMask,
    const VbBindingInfo*       pVbInfo,
    const bool                 appendFetchShaderCb,
#if VKI_RAY_TRACING
    const bool                 appendRtCaptureReplayCb,
#endif
    void*                      pBuffer,
    Vkgc::ResourceMappingData* pResourceMapping
    ) const
{
    VK_ASSERT(m_info.userDataLayout.scheme == PipelineLayoutScheme::Indirect);

    constexpr uint32_t VbTablePtrRegCount         = 1; // PAL requires all indirect user data tables to be 1DW
    constexpr uint32_t PushConstPtrRegCount       = 1;
    constexpr uint32_t TransformFeedbackRegCount  = 1;
    constexpr uint32_t ReverseThreadGroupRegCount = 1;
#if VKI_RAY_TRACING
    constexpr uint32_t DispatchRayArgsPtrRegCount = MaxTraceRayUserDataRegCount;
#endif
    constexpr uint32_t DescSetsPtrRegCount        = 2 * SetPtrRegCount * MaxDescriptorSets;

    const auto& userDataLayout = m_info.userDataLayout.indirect;

    const bool uberFetchShaderEnabled     = IsUberFetchShaderEnabled<PipelineLayoutScheme::Indirect>(m_pDevice);
    const bool transformFeedbackEnabled   = m_pDevice->IsExtensionEnabled(DeviceExtensions::EXT_TRANSFORM_FEEDBACK);
    const bool threadGroupReversalEnabled = userDataLayout.threadGroupReversalRegBase != InvalidReg;
#if VKI_RAY_TRACING
    const bool rayTracingEnabled          = m_pipelineInfo.hasRayTracing;
#endif

    uint32_t regBaseOffset = 0;

    const uint32_t vbTablePtrRegBase = regBaseOffset;
    regBaseOffset                   += vbTablePtrRegBase + VbTablePtrRegCount;

    const uint32_t uberFetchCbRegBase = uberFetchShaderEnabled ? regBaseOffset : InvalidReg;
    if (uberFetchCbRegBase != InvalidReg)
    {
        regBaseOffset += InternalConstBufferRegCount;
    }

    const uint32_t pushConstPtrRegBase = regBaseOffset;
    regBaseOffset                     += PushConstPtrRegCount;

    const uint32_t transformFeedbackRegBase = transformFeedbackEnabled ? regBaseOffset : InvalidReg;
    if (transformFeedbackRegBase != InvalidReg)
    {
        regBaseOffset += TransformFeedbackRegCount;
    }

    const uint32_t threadGroupReversalRegBase = threadGroupReversalEnabled ? regBaseOffset : InvalidReg;
    if (threadGroupReversalRegBase != InvalidReg)
    {
        regBaseOffset += ReverseThreadGroupRegCount;
    }

#if VKI_RAY_TRACING
    const uint32_t dispatchRayArgsPtrRegBase = rayTracingEnabled ? regBaseOffset : InvalidReg;
    if (dispatchRayArgsPtrRegBase != InvalidReg)
    {
        regBaseOffset += DispatchRayArgsPtrRegCount;
    }

    const uint32_t rtCaptureReplayCbRegBase = rayTracingEnabled ? regBaseOffset : InvalidReg;
    if (rtCaptureReplayCbRegBase != InvalidReg)
    {
        regBaseOffset += InternalConstBufferRegCount;
    }
#endif
    const uint32_t setBindingPtrRegBase = regBaseOffset;

    Vkgc::ResourceMappingRootNode* pUserDataNodes = static_cast<Vkgc::ResourceMappingRootNode*>(pBuffer);
    Vkgc::ResourceMappingNode* pResourceNodes =
        reinterpret_cast<Vkgc::ResourceMappingNode*>(pUserDataNodes + m_pipelineInfo.numUserDataNodes);
    Vkgc::StaticDescriptorValue* pDescriptorRangeValues =
        reinterpret_cast<Vkgc::StaticDescriptorValue*>(pResourceNodes + m_pipelineInfo.numRsrcMapNodes);

    uint32_t userDataNodeCount    = 0; // Number of consumed ResourceMappingRootNodes
    uint32_t mappingNodeCount     = 0; // Number of consumed ResourceMappingNodes (only sub-nodes)
    uint32_t descriptorRangeCount = 0; // Number of consumed StaticResourceValues

    // Build the internal vertex buffer table mapping
    BuildLlpcVertexBufferTableMapping(
        pVbInfo,
        vbTablePtrRegBase,
        VbTablePtrRegCount,
        &pUserDataNodes[userDataNodeCount],
        &userDataNodeCount);

    if ((pVbInfo != nullptr) && appendFetchShaderCb)
    {
        VK_ASSERT(uberFetchCbRegBase == userDataLayout.uberFetchConstBufRegBase);

        BuildLlpcInternalConstantBufferMapping(
            Vkgc::ShaderStageVertexBit,
            uberFetchCbRegBase,
            Vkgc::FetchShaderInternalBufferBinding,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount);
    }

    // Build push constants mapping
    if (userDataLayout.pushConstSizeInDword > 0)
    {
        // Build mapping for push constant resource
        Vkgc::ResourceMappingNode* pPushConstNode = &pResourceNodes[mappingNodeCount];

        pPushConstNode->type                    = Vkgc::ResourceMappingNodeType::PushConst;
        pPushConstNode->offsetInDwords          = 0;
        pPushConstNode->sizeInDwords            = userDataLayout.pushConstSizeInDword;
        pPushConstNode->srdRange.set            = Vkgc::InternalDescriptorSetId;
        pPushConstNode->srdRange.strideInDwords = 0;

        ++mappingNodeCount;

        // Build mapping for the pointer pointing to push constants buffer
        Vkgc::ResourceMappingRootNode* pPushConstPtrNode = &pUserDataNodes[userDataNodeCount];

        pPushConstPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
        pPushConstPtrNode->node.offsetInDwords     = pushConstPtrRegBase;
        pPushConstPtrNode->node.sizeInDwords       = PushConstPtrRegCount;
        pPushConstPtrNode->node.tablePtr.nodeCount = 1;
        pPushConstPtrNode->node.tablePtr.pNext     = pPushConstNode;
        pPushConstPtrNode->visibility              = stageMask;

        userDataNodeCount += 1;
    }

    // Build transform feedback buffer mapping
    if (transformFeedbackEnabled)
    {
        BuildLlpcTransformFeedbackMapping(
            stageMask,
            transformFeedbackRegBase,
            TransformFeedbackRegCount,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount);
    }

    if (((stageMask & Vkgc::ShaderStageComputeBit) != 0) && threadGroupReversalEnabled)
    {
        VK_ASSERT(threadGroupReversalRegBase == userDataLayout.threadGroupReversalRegBase);

        BuildLlpcInternalConstantBufferMapping(
            stageMask,
            threadGroupReversalRegBase,
            Vkgc::ReverseThreadGroupControlBinding,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount);
    }

#if VKI_RAY_TRACING
    // Build mapping for ray tracing dispatch arguments
    if (rayTracingEnabled)
    {
        BuildLlpcRayTracingDispatchArgumentsMapping(
            stageMask,
            dispatchRayArgsPtrRegBase,
            DispatchRayArgsPtrRegCount,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount,
            &pResourceNodes[mappingNodeCount],
            &mappingNodeCount);

        if (appendRtCaptureReplayCb)
        {
            VK_ASSERT(rtCaptureReplayCbRegBase == userDataLayout.rtCaptureReplayConstBufRegBase);
            BuildLlpcInternalConstantBufferMapping(
                stageMask,
                rtCaptureReplayCbRegBase,
                Vkgc::RtCaptureReplayInternalBufferBinding,
                &pUserDataNodes[userDataNodeCount],
                &userDataNodeCount);
        }
    }
#endif

    if (m_pDevice->GetRuntimeSettings().enableDebugPrintf)
    {
        BuildLlpcDebugPrintfMapping(
            stageMask,
            m_info.userDataRegCount,
            1u,
            &pUserDataNodes[userDataNodeCount],
            &userDataNodeCount,
            &pResourceNodes[mappingNodeCount],
            &mappingNodeCount);
    }

    // Build mapping for each set of descriptors
    VK_ASSERT(setBindingPtrRegBase == userDataLayout.setBindingPtrRegBase);

    for (uint32_t setIndex = 0; setIndex < m_info.setCount; ++setIndex)
    {
        const DescriptorSetLayout* pSetLayout = GetSetLayouts(setIndex);

        const uint32_t visibility = (pSetLayout != nullptr) ?
                (stageMask & VkToVkgcShaderStageMask(pSetLayout->Info().activeStageMask)) : 0;

        if (visibility != 0)
        {
            uint32_t dynNodeCount = 0;
            uint32_t staNodeCount = 0;

            Vkgc::ResourceMappingNode* pDynNodes = &pResourceNodes[mappingNodeCount];

            for (uint32_t bindingIndex = 0; bindingIndex < pSetLayout->Info().count; ++bindingIndex)
            {
                const DescriptorSetLayout::BindingInfo& binding = pSetLayout->Binding(bindingIndex);

                if (binding.dyn.dwSize > 0)
                {
                    auto* pDynNode = &pDynNodes[dynNodeCount++];

                    BuildLlpcDynamicMapping(
                        setIndex,
                        0,
                        binding,
                        pDynNode);
                }
            }

            Vkgc::ResourceMappingNode* pStaNodes = &pResourceNodes[mappingNodeCount + dynNodeCount];

            for (uint32_t bindingIndex = 0; bindingIndex < pSetLayout->Info().count; ++bindingIndex)
            {
                const DescriptorSetLayout::BindingInfo& binding = pSetLayout->Binding(bindingIndex);

                if (binding.sta.dwSize > 0)
                {
                    auto* pStaNode = &pStaNodes[staNodeCount++];

                    BuildLlpcStaticMapping(
                        pSetLayout,
                        visibility,
                        setIndex,
                        binding,
                        pStaNode,
                        &pDescriptorRangeValues[descriptorRangeCount],
                        &descriptorRangeCount);
                }
            }

            // Increase the number of mapping nodes used by the number of static section nodes added.
            mappingNodeCount += (dynNodeCount + staNodeCount);

            // Add a top-level user data node entry for dynamic nodes.
            if (pSetLayout->Info().dyn.numRsrcMapNodes > 0)
            {
                Vkgc::ResourceMappingRootNode* pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = 2 * setIndex * SetPtrRegCount + setBindingPtrRegBase;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = dynNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pDynNodes;
                pSetPtrNode->visibility              = visibility;

                ++userDataNodeCount;
            }

            // Add a top-level user data node entry for static nodes.
            if (pSetLayout->Info().sta.numRsrcMapNodes > 0)
            {
                Vkgc::ResourceMappingRootNode* pSetPtrNode = &pUserDataNodes[userDataNodeCount];

                pSetPtrNode->node.type               = Vkgc::ResourceMappingNodeType::DescriptorTableVaPtr;
                pSetPtrNode->node.offsetInDwords     = (2 * setIndex + 1) * SetPtrRegCount + setBindingPtrRegBase;
                pSetPtrNode->node.sizeInDwords       = SetPtrRegCount;
                pSetPtrNode->node.tablePtr.nodeCount = staNodeCount;
                pSetPtrNode->node.tablePtr.pNext     = pStaNodes;
                pSetPtrNode->visibility              = visibility;

                ++userDataNodeCount;
            }
        }
    }

    // If you hit these assert, we precomputed an insufficient amount of scratch space during layout creation.
    VK_ASSERT(userDataNodeCount    <= m_pipelineInfo.numUserDataNodes);
    VK_ASSERT(mappingNodeCount     <= m_pipelineInfo.numRsrcMapNodes);
    VK_ASSERT(descriptorRangeCount <= m_pipelineInfo.numDescRangeValueNodes);

    pResourceMapping->pUserDataNodes             = pUserDataNodes;
    pResourceMapping->userDataNodeCount          = userDataNodeCount;
    pResourceMapping->pStaticDescriptorValues    = pDescriptorRangeValues;
    pResourceMapping->staticDescriptorValueCount = descriptorRangeCount;
}

// =====================================================================================================================
// This function populates the resource mapping node details to the shader-stage specific pipeline info structure.
VkResult PipelineLayout::BuildLlpcPipelineMapping(
    const uint32_t              stageMask,
    const VbBindingInfo*        pVbInfo,
    const bool                  appendFetchShaderCb,
#if VKI_RAY_TRACING
    const bool                  appendRtCaptureReplayCb,
#endif
    void*                       pBuffer,
    Vkgc::ResourceMappingData*  pResourceMapping,
    Vkgc::ResourceLayoutScheme* pLayoutScheme
    ) const
{
    VkResult result = VK_SUCCESS;

    if (m_info.userDataLayout.scheme == PipelineLayoutScheme::Compact)
    {
        *pLayoutScheme = Vkgc::ResourceLayoutScheme::Compact;

        result = BuildCompactSchemeLlpcPipelineMapping(
            stageMask,
            pVbInfo,
            appendFetchShaderCb,
#if VKI_RAY_TRACING
            appendRtCaptureReplayCb,
#endif
            pBuffer,
            pResourceMapping);
    }
    else if (m_info.userDataLayout.scheme == PipelineLayoutScheme::Indirect)
    {
        *pLayoutScheme = Vkgc::ResourceLayoutScheme::Indirect;

        BuildIndirectSchemeLlpcPipelineMapping(
            stageMask,
            pVbInfo,
            appendFetchShaderCb,
#if VKI_RAY_TRACING
            appendRtCaptureReplayCb,
#endif
            pBuffer,
            pResourceMapping);
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return result;
}

// =====================================================================================================================
// Allocate user data for the thread group reversal state
void PipelineLayout::ReserveAlternatingThreadGroupUserData(
    const Device*                     pDevice,
    const VkPipelineLayoutCreateInfo* pPipelineLayoutInfo,
    uint32_t*                         pUserDataNodeCount,
    uint32_t*                         pUSerDataRegCount,
    uint32_t*                         pThreadGroupReversalRegBase)
{
    bool allocated = false;

    if (pDevice->GetRuntimeSettings().enableAlternatingThreadGroupOrder)
    {
        for (uint32_t setIndex = 0; setIndex < pPipelineLayoutInfo->setLayoutCount; ++setIndex)
        {
            if (pPipelineLayoutInfo->pSetLayouts[setIndex] != VK_NULL_HANDLE)
            {
                const auto pSetLayout = DescriptorSetLayout::ObjectFromHandle(pPipelineLayoutInfo->pSetLayouts[setIndex]);

                // Test if the set layout supports the Compute stage, as thread group reversal is only used for Compute
                if (Util::TestAnyFlagSet(pSetLayout->Info().activeStageMask, VK_SHADER_STAGE_COMPUTE_BIT))
                {
                    *pThreadGroupReversalRegBase = *pUSerDataRegCount;
                    (*pUSerDataRegCount)        += 2;
                    (*pUserDataNodeCount)       += 1;

                    allocated = true;
                    break;
                }
            }
        }
    }

    if (allocated == false)
    {
        *pThreadGroupReversalRegBase = InvalidReg;
    }
}

// =====================================================================================================================
// Destroy pipeline layout object
VkResult PipelineLayout::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    for (uint32_t i = 0; i < m_info.setCount; ++i)
    {
        DescriptorSetLayout* pSetLayout = GetSetLayouts(i);
        if (pSetLayout != nullptr)
        {
            pSetLayout->Destroy(pDevice, pAllocator, false);
        }
    }

    this->~PipelineLayout();

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout,
    const VkAllocationCallbacks*                pAllocator)
{
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        PipelineLayout::ObjectFromHandle(pipelineLayout)->Destroy(pDevice, pAllocCB);
    }
}
} // namespace entry

} // namespace vk
