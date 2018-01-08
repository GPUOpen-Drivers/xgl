/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_cmdbuffer.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_conv.h"
#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_memory.h"

#include "palPipeline.h"
#include "palShader.h"

#include "llpc.h"

namespace vk
{

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to internal PAL structure
VkResult ComputePipeline::ConvertComputePipelineInfo(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkComputePipelineCreateInfo*      pIn,
    Pal::ComputePipelineCreateInfo*         pOutInfo,
    ImmedInfo*                              pImmedInfo,
    void**                                  ppTempBuffer,
    void**                                  ppTempShaderBuffer,
    size_t*                                 pPipelineBinarySize,
    const void**                            ppPipelineBinary)
{
    union
    {
        const VkStructHeader*                    pHeader;
        const VkComputePipelineCreateInfo*       pPipelineInfo;
    };
    const RuntimeSettings&    settings           = pDevice->GetRuntimeSettings();
    const PipelineLayout*     pLayout            = nullptr;
    Pal::ResourceMappingNode* pTempBuffer        = nullptr;
    void*                     pPatchMemory[MaxPalDevices] = {};
    size_t                    pipelineBinarySize = 0;
    const void*               pPipelineBinary    = nullptr;

    VkResult result = VK_SUCCESS;
    *ppTempBuffer   = nullptr;

    for (pPipelineInfo = pIn; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
        case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
            {
                const ShaderModule* pShader = ShaderModule::ObjectFromHandle(pPipelineInfo->stage.module);

                bool buildLlpcPipeline = false;
                bool enableLlpc = false;
                Llpc::ComputePipelineBuildInfo pipelineBuildInfo = {};
                Llpc::ComputePipelineBuildOut  pipelineOut = {};
                {
                    buildLlpcPipeline = true;
                }

                if (buildLlpcPipeline)
                {
                    if (result == VK_SUCCESS)
                    {
                        // Fill pipeline create info for LLPC
                        pipelineBuildInfo.pInstance      = pDevice->VkPhysicalDevice()->VkInstance();
                        pipelineBuildInfo.pfnOutputAlloc = AllocateShaderOutput;
                        pipelineBuildInfo.pUserData      = ppTempShaderBuffer;
                        if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCacheType() == PipelineCacheTypeLlpc))
                        {
                            pipelineBuildInfo.pShaderCache = pPipelineCache->GetShaderCache(DefaultDeviceIndex).pLlpcShaderCache;
                        }
                        auto pShaderInfo = &pipelineBuildInfo.cs;

                        pShaderInfo->pModuleData         = pShader->GetLlpcShaderData();
                        pShaderInfo->pSpecializatonInfo  = pPipelineInfo->stage.pSpecializationInfo;
                        pShaderInfo->pEntryTarget        = pPipelineInfo->stage.pName;

                        size_t tempBufferSize = 0;

                        // Reserve space to create the LLPC pipeline resource mapping
                        if (pPipelineInfo->layout != VK_NULL_HANDLE)
                        {
                            pLayout = PipelineLayout::ObjectFromHandle(pPipelineInfo->layout);

                            VK_ASSERT(pLayout != nullptr);

                            tempBufferSize += pLayout->GetPipelineInfo()->tempBufferSize;
                        }

                        // Allocate the temp buffer
                        if (tempBufferSize > 0)
                        {
                            *ppTempBuffer = pDevice->VkInstance()->AllocMem(
                                tempBufferSize,
                                VK_DEFAULT_MEM_ALIGN,
                                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
                        }

                        // Build the resource mapping description for LLPC.  This data contains things about how shader
                        // inputs like descriptor set bindings interact with this pipeline in a form that LLPC can
                        // understand.
                        if (pLayout != nullptr)
                        {
                            result = pLayout->BuildLlpcPipelineMapping(
                                ShaderStageCompute,
                                *ppTempBuffer,
                                nullptr,
                                pShaderInfo,
                                nullptr);
                        }
                    }
                }

                uint64_t pipeHash = 0;
                enableLlpc = true;

                if (result == VK_SUCCESS)
                {
                    if (enableLlpc)
                    {
                        auto llpcResult = pDevice->GetCompiler()->BuildComputePipeline(&pipelineBuildInfo, &pipelineOut);
                        if (llpcResult != Llpc::Result::Success)
                        {
                            {
                                result = VK_ERROR_INITIALIZATION_FAILED;
                            }
                        }
                    }
                    else if (settings.enablePipelineDump)
                    {
                        // LLPC isn't enabled but pipeline dump is required, call LLPC dump interface explicitly
                        pDevice->GetCompiler()->DumpComputePipeline(&pipelineBuildInfo);
                    }
                }

                // Update PAL pipeline create info with LLPC output
                if (enableLlpc)
                {
                    if (result == VK_SUCCESS)
                    {
                        pOutInfo->pPipelineBinary    = static_cast<const uint8_t*>(pipelineOut.pipelineBin.pCode);
                        pOutInfo->pipelineBinarySize = static_cast<uint32_t>(pipelineOut.pipelineBin.codeSize);

                        pipelineBinarySize = pOutInfo->pipelineBinarySize;
                        pPipelineBinary    = pOutInfo->pPipelineBinary;
                    }
                }
                else
                {
                    result = VK_SUCCESS;

                    pDevice->VkInstance()->FreeMem(*ppTempBuffer);

                    *ppTempBuffer = nullptr;
                }

            }
            break;

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    if (result == VK_SUCCESS)
    {
        *pPipelineBinarySize = pipelineBinarySize;
        *ppPipelineBinary    = pPipelineBinary;
    }

    for (uint32_t shaderInst = 0; shaderInst < MaxPalDevices; ++shaderInst)
    {
        if (pPatchMemory[shaderInst] != nullptr)
        {
            pDevice->VkInstance()->FreeMem(pPatchMemory[shaderInst]);
            pPatchMemory[shaderInst] = nullptr;
        }
    }

    if (result != VK_SUCCESS)
    {
        pDevice->VkInstance()->FreeMem(*ppTempBuffer);
    }

    return result;
}

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* const                        pDevice,
    Pal::IPipeline**                     pPalPipeline,
    const PipelineLayout*                pPipelineLayout,
    PipelineBinaryInfo*                  pPipelineBinary,
    const ImmedInfo&                     immedInfo)
    :
    Pipeline(pDevice, pPalPipeline, pPipelineLayout, pPipelineBinary),
    m_info(immedInfo)
{
    CreateStaticState();
}

// =====================================================================================================================
// Creates instances of static pipeline state.  Much of this information can be cached at the device-level to help speed
// up pipeline-bind operations.
void ComputePipeline::CreateStaticState()
{
}

// =====================================================================================================================
// Destroys static pipeline state.
void ComputePipeline::DestroyStaticState(
    const VkAllocationCallbacks* pAllocator)
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();

    pCache->DestroyComputeWaveLimits(m_info.computeWaveLimitParams,
        m_info.staticTokens.waveLimits);
}

// =====================================================================================================================
VkResult ComputePipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    DestroyStaticState(pAllocator);

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
// Create a compute pipeline object.
VkResult ComputePipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkComputePipelineCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    // Setup PAL create info from Vulkan inputs
    Pal::ComputePipelineCreateInfo palCreateInfo              = {};
    ImmedInfo                      immedInfo                  = {};
    void*                          pTempBuffer                = nullptr;
    void*                          pTempShaderBuffer          = nullptr;
    size_t                         pipelineBinarySize         = 0;
    const void*                    pPipelineBinary            = nullptr;

    const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    VkResult result = ConvertComputePipelineInfo(
        pDevice,
        pPipelineCache,
        pCreateInfo,
        &palCreateInfo,
        &immedInfo,
        &pTempBuffer,
        &pTempShaderBuffer,
        &pipelineBinarySize,
        &pPipelineBinary);

    if (result != VK_SUCCESS)
    {
        return result;
    }

    size_t pipelineSize = 0;
    void*  pSystemMem   = nullptr;

    if (result == VK_SUCCESS)
    {
        // Get the pipeline and shader size from PAL and allocate memory.
        pipelineSize = pDevice->PalDevice()->GetComputePipelineSize(palCreateInfo, nullptr);

        pSystemMem = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(ComputePipeline) + (pipelineSize * pDevice->NumPalDevices()),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Create the PAL pipeline object.
    Pal::IPipeline* pPalPipeline[MaxPalDevices] = {};

    if (result == VK_SUCCESS)
    {
        Pal::Result palResult = Pal::Result::Success;
        void*  pPalMem = Util::VoidPtrInc(pSystemMem, sizeof(ComputePipeline));

        for (uint32_t deviceIdx = 0;
            ((deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
            deviceIdx++)
        {
            pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                palCreateInfo,
                Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                &pPalPipeline[deviceIdx]);
        }

        result = PalToVkResult(palResult);
    }

    // Retain a copy of the pipeline binary if an extension that can query it is enabled
    PipelineBinaryInfo* pBinary = nullptr;

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) && (result == VK_SUCCESS))
    {
        // The CreateLegacyPathElfBinary() function is temporary.
        void* pLegacyBinary = nullptr;

        if (pPipelineBinary == nullptr)
        {
            bool graphicsPipeline = false;
            Pipeline::CreateLegacyPathElfBinary(pDevice,
                                                graphicsPipeline,
                                                pPalPipeline[DefaultDeviceIndex],
                                                &pipelineBinarySize,
                                                &pLegacyBinary);
            pPipelineBinary = pLegacyBinary;
        }

        // (This call is not temporary)
        pBinary = PipelineBinaryInfo::Create(pipelineBinarySize, pPipelineBinary, pAllocator);

        if (pLegacyBinary != nullptr)
        {
            pDevice->VkInstance()->FreeMem(pLegacyBinary);
        }
    }

    if (result == VK_SUCCESS)
    {
        // On success, wrap it up in a Vulkan object and return.
        VK_PLACEMENT_NEW(pSystemMem) ComputePipeline(pDevice, pPalPipeline, pLayout, pBinary, immedInfo);

        *pPipeline = ComputePipeline::HandleFromVoidPointer(pSystemMem);
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            // Internal memory allocation failed, free PAL event object if it gets created
            if (pPalPipeline[deviceIdx] != nullptr)
            {
                pPalPipeline[deviceIdx]->Destroy();
            }
        }
    }

    // Destroy PAL shader object and temp memory
    if (pTempBuffer != nullptr)
    {
        pDevice->VkInstance()->FreeMem(pTempBuffer);
    }

    if (pTempShaderBuffer != nullptr)
    {
        pDevice->VkInstance()->FreeMem(pTempShaderBuffer);
    }

    // Something went wrong with creating the PAL object. Free memory and return error.
    if (result != VK_SUCCESS)
    {
        // Free system memory for pipeline object
        pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);

        if (pBinary != nullptr)
        {
            pBinary->Destroy(pAllocator);
        }
    }

    return result;
}

// =====================================================================================================================
void ComputePipeline::BindToCmdBuffer(CmdBuffer* pCmdBuffer) const
{
    BindToCmdBuffer(pCmdBuffer, m_info.computeWaveLimitParams);
}

// =====================================================================================================================
void ComputePipeline::BindToCmdBuffer(
    CmdBuffer*                           pCmdBuffer,
    const Pal::DynamicComputeShaderInfo& computeShaderInfo) const
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint = Pal::PipelineBindPoint::Compute;
    params.cs = computeShaderInfo;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        params.pPipeline = m_pPalPipeline[deviceIdx];

        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

// =====================================================================================================================
void ComputePipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint       = Pal::PipelineBindPoint::Compute;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

} // namespace vk
