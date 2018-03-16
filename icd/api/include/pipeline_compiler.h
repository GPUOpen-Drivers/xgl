/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  pipeline_compiler.h
 * @brief Contains declaration of Vulkan pipeline compiler
 ***********************************************************************************************************************
 */

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_shader_code.h"

#include "llpc.h"

namespace Bil
{

struct BilConvertOptions;
struct BilShaderPatchOutput;
enum BilDescriptorType : uint32_t;

}

namespace vk
{

class PhysicalDevice;
class PipelineLayout;
class PipelineCache;
struct VbBindingInfo;

// =====================================================================================================================
// Represents Vulkan pipeline compiler, it wraps LLPC and SCPC, and hides the differences.
class PipelineCompiler
{
public:
    // Creation info parameters for all the necessary LLPC/SCPC state objects encapsulated
    // by the Vulkan graphics pipeline.
    struct GraphicsPipelineCreateInfo
    {
        Llpc::GraphicsPipelineBuildInfo        pipelineInfo;
        const PipelineLayout*                  pLayout;
        const VkPipelineShaderStageCreateInfo* pStages[ShaderGfxStageCount];
        VkPipelineCreateFlags                  flags;
        void*                                  pMappingBuffer;
        VkFormat                               dbFormat;
    };

    // Creation info parameters for all the necessary LLPC/SCPC state objects encapsulated
    // by the Vulkan compute pipeline.
    struct ComputePipelineCreateInfo
    {
        Llpc::ComputePipelineBuildInfo         pipelineInfo;
        const PipelineLayout*                  pLayout;
        const VkPipelineShaderStageCreateInfo* pStage;
        VkPipelineCreateFlags                  flags;
        void*                                  pMappingBuffer;
    };

    PipelineCompiler(PhysicalDevice* pPhysicalDevice);
    ~PipelineCompiler();
    VkResult Initialize();
    void Destroy();

    VkResult CreateGraphicsPipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        GraphicsPipelineCreateInfo*         pCreateInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary);

    VkResult CreateComputePipelineBinary(
        Device*                             pDevice,
        uint32_t                            deviceIndex,
        PipelineCache*                      pPipelineCache,
        ComputePipelineCreateInfo*          pInfo,
        size_t*                             pPipelineBinarySize,
        const void**                        ppPipelineBinary);

    VkResult ConvertGraphicsPipelineInfo(
        Device*                             pDevice,
        const VkGraphicsPipelineCreateInfo* pIn,
        GraphicsPipelineCreateInfo*         pInfo,
        VbBindingInfo*                      pVbInfo);

    VkResult ConvertComputePipelineInfo(
        const VkComputePipelineCreateInfo*  pIn,
        ComputePipelineCreateInfo*          pInfo);

    void FreeComputePipelineBinary(
        ComputePipelineCreateInfo* pCreateInfo,
        const void*                pPipelineBinary,
        size_t                     binarySize);

    void FreeGraphicsPipelineBinary(
        GraphicsPipelineCreateInfo* pCreateInfo,
        const void*                pPipelineBinary,
        size_t                     binarySize);

    void FreeComputePipelineCreateInfo(ComputePipelineCreateInfo* pCreateInfo);

    void FreeGraphicsPipelineCreateInfo(GraphicsPipelineCreateInfo* pCreateInfo);
    // Get LLPC compiler explicitly.
    // TODO: Should be removed in the future
    Llpc::ICompiler* GetLlpcCompiler() { return m_pLlpc; }

private:
    VkResult CreateLlpcCompiler();

    static bool IsDualSourceBlend(VkBlendFactor blend);

    // -----------------------------------------------------------------------------------------------------------------

    PhysicalDevice*    m_pPhysicalDevice;      // Vulkan physical device object
    Llpc::GfxIpVersion m_gfxIp;                // Graphics IP version info, used by LLPC
    Pal::GfxIpLevel    m_gfxIpLevel;           // Graphics IP Level, used by SCPC

    Llpc::ICompiler*    m_pLlpc;               // LLPC compiler object

}; // class PipelineCompiler

} // namespce vk
