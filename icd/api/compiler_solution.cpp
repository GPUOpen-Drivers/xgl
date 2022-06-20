/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  compiler_solution.cpp
* @brief Contains implementation of CompilerSolution
***********************************************************************************************************************
*/
#include "include/compiler_solution.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"

#include <climits>
#include <cmath>

namespace vk
{
    // =====================================================================================================================
CompilerSolution::CompilerSolution(
    PhysicalDevice* pPhysicalDevice)
    : m_pPhysicalDevice(pPhysicalDevice)
{

}

// =====================================================================================================================
CompilerSolution::~CompilerSolution()
{

}

// =====================================================================================================================
// Initialize CompilerSolution class
VkResult CompilerSolution::Initialize(
    Vkgc::GfxIpVersion gfxIp,
    Pal::GfxIpLevel    gfxIpLevel,
    Vkgc::ICache*      pCache)
{
    m_gfxIp      = gfxIp;
    m_gfxIpLevel = gfxIpLevel;

    return VK_SUCCESS;
}

// =====================================================================================================================
// Gets the name string of shader stage.
const char* CompilerSolution::GetShaderStageName(
    ShaderStage shaderStage)
{
    const char* pName = nullptr;

    VK_ASSERT(shaderStage < ShaderStageCount);

    static const char* ShaderStageNames[] =
    {
        "Vertex  ",
        "Tessellation control",
        "Tessellation evaluation",
        "Geometry",
        "Fragment",
        "Compute ",
    };

    pName = ShaderStageNames[static_cast<uint32_t>(shaderStage)];

    return pName;
}

// =====================================================================================================================
// Helper to disable all NGG culling options
void CompilerSolution::DisableNggCulling(
    Vkgc::NggState* pNggState)
{
    pNggState->enableBackfaceCulling     = false;
    pNggState->enableFrustumCulling      = false;
    pNggState->enableBoxFilterCulling    = false;
    pNggState->enableSphereCulling       = false;
    pNggState->enableSmallPrimFilter     = false;
    pNggState->enableCullDistanceCulling = false;
}

}
