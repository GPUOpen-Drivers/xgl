/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_indirect_commands_layout.h
 * @brief Functionality related to Vulkan indirect commands layout objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_INDIRECT_COMMANDS_LAYOUT_H__
#define __VK_INDIRECT_COMMANDS_LAYOUT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"
#include "include/vk_pipeline_layout.h"

#include "palIndirectCmdGenerator.h"

namespace Pal
{

class  IIndirectCmdGenerator;
struct IndirectCmdGeneratorCreateInfo;
struct IndirectParam;

};

namespace vk
{

enum IndirectCommandsActionType
{
    Draw = 0,
    DrawIndexed,
    Dispatch,
    MeshTask
};

struct IndirectCommandsInfo
{
    IndirectCommandsActionType  actionType;
};

 // =====================================================================================================================
 // API implementation of Vulkan indirect commands layout
 //
 // Indirect commands layout objects describe the information of indirect commands, as well as how to interpret and
 // process indirect buffers.
class IndirectCommandsLayout final : public NonDispatchable<VkIndirectCommandsLayoutNV, IndirectCommandsLayout>
{
public:
    static VkResult Create(
        const Device*                               pDevice,
        const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkIndirectCommandsLayoutNV*                 pLayout);

    void CalculateMemoryRequirements(
        const Device*                               pDevice,
        VkMemoryRequirements2*                      pMemoryRequirements) const;

    VkResult Destroy(
        Device*                                     pDevice,
        const VkAllocationCallbacks*                pAllocator);

    const Pal::IIndirectCmdGenerator* PalIndirectCmdGenerator(uint32_t deviceIdx) const
    {
        return m_perGpu[deviceIdx].pGenerator;
    }

    IndirectCommandsInfo GetIndirectCommandsInfo() const
    {
        return m_info;
    }

private:

    PAL_DISALLOW_COPY_AND_ASSIGN(IndirectCommandsLayout);

    struct PerGpuInfo
    {
        Pal::IIndirectCmdGenerator* pGenerator;
        Pal::IGpuMemory*            pGpuMemory;
    };

    IndirectCommandsLayout(
        const Device*                               pDevice,
        const IndirectCommandsInfo&                 info,
        Pal::IIndirectCmdGenerator**                pGenerators,
        Pal::IGpuMemory**                           pGpuMemory,
        const Pal::IndirectCmdGeneratorCreateInfo&  palCreateInfo);

    static size_t ObjectSize(const Device*  pDevice)
    {
        return sizeof(IndirectCommandsLayout) + ((pDevice->NumPalDevices() - 1) * sizeof(PerGpuInfo));
    }

    static void BuildPalCreateInfo(
        const Device*                               pDevice,
        const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo,
        Pal::IndirectParam*                         pIndirectParams,
        Pal::IndirectCmdGeneratorCreateInfo*        pPalCreateInfo);

    static VkResult BindGpuMemory(
        const Device*                               pDevice,
        const VkAllocationCallbacks*                pAllocator,
        Pal::IIndirectCmdGenerator**                pGenerators,
        Pal::IGpuMemory**                           pGpuMemory);

    IndirectCommandsInfo                    m_info;
    Pal::IndirectCmdGeneratorCreateInfo     m_palCreateInfo;
    PerGpuInfo                              m_perGpu[1];
};

// Max usage is the situation where indirect commands layout drains push constants size plus uses indirect index & vertex
// buffer binding and ends with a draw indexed.
constexpr uint32_t MaxIndirectTokenCount  = MaxPushConstRegCount + 3;
constexpr uint32_t MaxIndirectTokenOffset = MaxPushConstants +
                                            sizeof(VkBindIndexBufferIndirectCommandNV) +
                                            sizeof(VkBindVertexBufferIndirectCommandNV) +
                                            sizeof(VkDrawIndexedIndirectCommand);
} // namespace vk

#endif /* __VK_INDIRECT_COMMANDS_LAYOUT_H__ */
