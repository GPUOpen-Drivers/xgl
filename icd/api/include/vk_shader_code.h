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

#pragma once

#include "khronos/vulkan.h"
#include "palPipeline.h"
#include "vk_instance.h"

namespace vk
{

/// Enumerates various shader stages.
enum ShaderStage : uint32_t
{
    ShaderStageVertex = 0,        ///< Vertex shader
    ShaderStageTessControl,       ///< Tessellation control shader
    ShaderStageTessEvaluation,    ///< Tessellation evaluation shader
    ShaderStageGeometry,          ///< Geomtry shader
    ShaderStageFragment,          ///< Fragment shader
    ShaderStageCompute,           ///< Compute shader

    ShaderGfxStageCount = ShaderStageFragment + 1,  ///< Count of graphics shader stages
    ShaderStageCount    = ShaderStageCompute  + 1,  ///< Count of shader stages
};

/// Translate shader stage flag bits to corresponding shader stage.
VK_INLINE ShaderStage ShaderFlagBitToStage(const VkShaderStageFlagBits& shaderBits)
{
    ShaderStage stage = ShaderStageCount; // Initialize it with an invalid value
    if (shaderBits & VK_SHADER_STAGE_VERTEX_BIT)
    {
        stage = ShaderStageVertex;
    }
    else if(shaderBits & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        stage = ShaderStageTessControl;
    }
    else if (shaderBits & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
    {
        stage = ShaderStageTessEvaluation;
    }
    else if (shaderBits & VK_SHADER_STAGE_GEOMETRY_BIT)
    {
        stage = ShaderStageGeometry;
    }
    else if (shaderBits & VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        stage = ShaderStageFragment;
    }
    else if (shaderBits & VK_SHADER_STAGE_COMPUTE_BIT)
    {
        stage = ShaderStageCompute;
    }

    return stage;
}

} // namespace vk
