/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_device.h"
#include "include/vk_render_pass.h"

#include "renderpass/renderpass_builder.h"
#include "renderpass/renderpass_logger.h"

#if ICD_LOG_RENDER_PASSES

#include "palListImpl.h"

namespace vk
{

// =====================================================================================================================
// Assembles a log string in the specified output buffer.  Will return ErrorInvalidMemorySize if the destination buffer
// is not large enough.
static Pal::Result BuildString(
    char*       pOutBuf,   // [out] Output buffer to contain assembled string.
    size_t      bufSize,   // Size of the destination buffer.
    const char* pFormat,   // Printf-style format string.
    va_list     argList)   // Printf-style argument list.
{
    pOutBuf[0] = '\0';

    const size_t length = strlen(pOutBuf);

    Util::Vsnprintf(pOutBuf + length, bufSize - length, pFormat, argList);

    const size_t finalLength = strlen(pOutBuf);

    PAL_ASSERT(finalLength < bufSize);

    // Assume that if the final string length is bufSize-1 then some stuff was truncated.
    return (finalLength == (bufSize - 1)) ? Pal::Result::ErrorInvalidMemorySize : Pal::Result::Success;
}

// =====================================================================================================================
void RenderPassLogger::Log(
    const char* pFormat,
    ...)
{
    if (m_logging == false)
    {
        return;
    }

    va_list argList;
    va_start(argList, pFormat);

    static constexpr size_t BufferLength = 1024;

    char buffer[BufferLength];

    char* pOutputBuf = &buffer[0];
    char* pLargeBuf = nullptr;

    if (BuildString(pOutputBuf, BufferLength, pFormat, argList) != Pal::Result::Success)
    {
        constexpr size_t LargeBufferLength = 128 * 1024;

        pLargeBuf = static_cast<char*>(m_pArena->Alloc(LargeBufferLength));

        if (pLargeBuf != nullptr)
        {
            pOutputBuf = pLargeBuf;

            BuildString(pOutputBuf, LargeBufferLength, pFormat, argList);
        }
    }

    if (m_file.IsOpen())
    {
        m_file.Printf(pOutputBuf);
    }

    va_end(argList);
}

// =====================================================================================================================
RenderPassLogger::RenderPassLogger(
    utils::TempMemArena* pArena,
    const Device*        pDevice)
    :
    m_pArena(pArena),
    m_settings(pDevice->GetRuntimeSettings())
{
    m_logging = true;
}

// =====================================================================================================================
void RenderPassLogger::Begin(
    const RenderPassCreateInfo*   info)
{
    if (m_logging == false)
    {
        return;
    }

    m_pInfo    = info;

    if (OpenLogFile(info->hash))
    {
        Log("= Render Pass Build Log\n\n");

        LogRenderPassCreateInfo(*m_pInfo);

        m_file.Flush();
    }
    else
    {
        m_logging = false;
    }
}

// =====================================================================================================================
static const char* LoadOpString(VkAttachmentLoadOp loadOp)
{
    switch (loadOp)
    {
    case VK_ATTACHMENT_LOAD_OP_LOAD:
        return "LOAD_OP_LOAD";
    case VK_ATTACHMENT_LOAD_OP_CLEAR:
        return "LOAD_OP_CLEAR";
    case VK_ATTACHMENT_LOAD_OP_DONT_CARE:
        return "LOAD_OP_DONT_CARE";
    default:
        VK_NEVER_CALLED();
        return "<unknown load op>";
    }
}

// =====================================================================================================================
static const char* StoreOpString(VkAttachmentStoreOp storeOp)
{
    switch (storeOp)
    {
    case VK_ATTACHMENT_STORE_OP_STORE:
        return "STORE_OP_STORE";
    case VK_ATTACHMENT_STORE_OP_DONT_CARE:
        return "STORE_OP_DONT_CARE";
    default:
        VK_NEVER_CALLED();
        return "<unknown store op>";
    }
}

// =====================================================================================================================
static const char* ImageLayoutString(
    VkImageLayout layout,
    bool          compact)
{
    switch (static_cast<int>(layout) )
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL:
            return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return compact ? "COLOR_OPT" : "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return compact ? "DS_OPT" : "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return compact ? "DS_RD_OPT" : "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
            return compact ? "D_RD_S_OPT" : "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return compact ? "SHADER_RD_OPT" : "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return compact ? "XFER_SRC_OPT" : "TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return compact ? "XFER_DST_OPT" : "TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return compact ? "PREINIT" : "PREINITIALIZED";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return compact ? "PRESENT_SRC" : "PRESENT_SRC_KHR";
        default:
            VK_NEVER_CALLED();
            return "<unknown image layout>";
    }
}

// =====================================================================================================================
void RenderPassLogger::LogInfoAttachmentReference(
    const char*                  pAttachmentArray,
    uint32_t                     element,
    const AttachmentReference&   ref)
{
    Log("   .%s[%d] = ", pAttachmentArray, element);
    LogAttachmentReference(ref);
    Log("\n");
}

// =====================================================================================================================
void RenderPassLogger::LogAttachmentReference(
    const AttachmentReference& reference)
{
    LogAttachment(reference.attachment);
    Log(" in %s", ImageLayoutString(reference.layout, false));
    Log(" aspectMask ");
    LogImageAspectMask(reference.aspectMask, false);
}

// =====================================================================================================================
void RenderPassLogger::LogAttachmentReference(
    const RPAttachmentReference& reference)
{
    LogAttachment(reference.attachment); Log(" in "); LogImageLayout(reference.layout);
}

// =====================================================================================================================
void RenderPassLogger::LogAttachment(uint32_t attachment)
{
    if (attachment != VK_ATTACHMENT_UNUSED)
    {
        Log("%d (", attachment);
        LogFormat(m_pInfo->pAttachments[attachment].format);
        Log("x%us", static_cast<uint32_t>(m_pInfo->pAttachments[attachment].samples));
        Log(")");
    }
    else
    {
        Log("VK_ATTACHMENT_UNUSED");
    }
}

// =====================================================================================================================
void RenderPassLogger::LogImageLayout(
    const RPImageLayout& layout)
{
    Log(ImageLayoutString(layout.layout, false));

    if (layout.extraUsage != 0)
    {
        Log("+0x%x", layout.extraUsage);
    }
}

// =====================================================================================================================
static const char* PipelineStageFlagString(
    VkPipelineStageFlagBits flag,
    bool                    compact)
{
    switch (flag)
    {
    case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
        return compact ? "TOP" : "TOP_OF_PIPE_BIT";
    case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
        return compact ? "DRAW_IND" : "DRAW_INDIRECT_BIT";
    case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
        return compact ? "VTX_IN" : "VERTEX_INPUT_BIT";
    case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
        return compact ? "VS" : "VERTEX_SHADER_BIT";
    case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
        return compact ? "TCS" : "TESSELLATION_CONTROL_SHADER_BIT";
    case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
        return compact ? "TES" : "TESSELLATION_EVALUATION_SHADER_BIT";
    case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
        return compact ? "GS" : "GEOMETRY_SHADER_BIT";
    case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        return compact ? "FS" : "FRAGMENT_SHADER_BIT";
    case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
        return compact ? "EARLY_FRAG" : "EARLY_FRAGMENT_TESTS_BIT";
    case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
        return compact ? "LATE_FRAG" : "LATE_FRAGMENT_TESTS_BIT";
    case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
        return compact ? "COLOR_OUT" : "COLOR_ATTACHMENT_OUTPUT_BIT";
    case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
        return compact ? "CS" : "COMPUTE_SHADER_BIT";
    case VK_PIPELINE_STAGE_TRANSFER_BIT:
        return compact ? "XFER" : "TRANSFER_BIT";
    case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
        return compact ? "BOTTOM" : "BOTTOM_OF_PIPE_BIT";
    case VK_PIPELINE_STAGE_HOST_BIT:
        return compact ? "HOST" : "HOST_BIT";
    case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT:
        return compact ? "ALL_GFX" : "ALL_GRAPHICS_BIT";
    case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
        return compact ? "ALL" : "ALL_COMMANDS_BIT";
    default:
        VK_NEVER_CALLED();
        return "<unknown pipeline stage flag>";
    }
}

// =====================================================================================================================
void RenderPassLogger::LogPipelineStageMask(
    VkPipelineStageFlags flags,
    bool                 compact)
{
    if (flags == 0)
    {
        Log("0");

        return;
    }

    uint32_t count = 0;

#define LogFlag(flag) \
    if ((flags & flag) == flag) \
    { \
        if (count >= 1) \
        { \
            Log("|"); \
        } \
        Log("%s", PipelineStageFlagString(flag, compact)); \
        flags &= ~(flag); \
        count++; \
    }

    LogFlag(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    LogFlag(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    LogFlag(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    LogFlag(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    LogFlag(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
    LogFlag(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    LogFlag(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    LogFlag(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    LogFlag(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    LogFlag(VK_PIPELINE_STAGE_TRANSFER_BIT);
    LogFlag(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    LogFlag(VK_PIPELINE_STAGE_HOST_BIT);

    if (flags != 0)
    {
        VK_NEVER_CALLED();

        if (count > 0)
        {
            Log("|");
        }
        Log("0x%x", flags);
    }

#undef LogFlag

}

// =====================================================================================================================
static const char* ImageAspectFlagString(
    VkImageAspectFlagBits   flag,
    bool                    compact)
{
    switch (flag)
    {
    case VK_IMAGE_ASPECT_COLOR_BIT:
        return compact ? "COLOR" : "ASPECT_COLOR_BIT";
    case VK_IMAGE_ASPECT_DEPTH_BIT:
        return compact ? "DEPTH" : "ASPECT_DEPTH_BIT";
    case VK_IMAGE_ASPECT_STENCIL_BIT:
        return compact ? "STENCIL" : "ASPECT_STENCIL_BIT";
    case VK_IMAGE_ASPECT_METADATA_BIT:
        return compact ? "META" : "ASPECT_METADATA_BIT";
    case VK_IMAGE_ASPECT_PLANE_0_BIT:
        return compact ? "PLANE_0" : "ASPECT_PLANE_0_BIT";
    case VK_IMAGE_ASPECT_PLANE_1_BIT:
        return compact ? "PLANE_1" : "ASPECT_PLANE_1_BIT";
    case VK_IMAGE_ASPECT_PLANE_2_BIT:
        return compact ? "PLANE_2" : "ASPECT_PLANE_2_BIT";
    default:
        VK_NEVER_CALLED();
        return "<unknown image aspect flag>";
    }
}

// =====================================================================================================================
void RenderPassLogger::LogImageAspectMask(
    VkImageAspectFlags   flags,
    bool                 compact)
{
    if (flags == 0)
    {
        Log("0");

        return;
    }
    else if (flags == VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM)
    {
        Log(compact ? "MAX" : "ASPECT_FLAG_BITS_MAX_ENUM");

        return;
    }

    uint32_t count = 0;

#define LogFlag(flag) \
    if ((flags & flag) == flag) \
    { \
        if (count >= 1) \
        { \
            Log("|"); \
        } \
        Log("%s", ImageAspectFlagString(flag, compact)); \
        flags &= ~(flag); \
        count++; \
    }

    LogFlag(VK_IMAGE_ASPECT_COLOR_BIT);
    LogFlag(VK_IMAGE_ASPECT_DEPTH_BIT);
    LogFlag(VK_IMAGE_ASPECT_STENCIL_BIT);
    LogFlag(VK_IMAGE_ASPECT_METADATA_BIT);
    LogFlag(VK_IMAGE_ASPECT_PLANE_0_BIT);
    LogFlag(VK_IMAGE_ASPECT_PLANE_1_BIT);
    LogFlag(VK_IMAGE_ASPECT_PLANE_2_BIT);

    if (flags != 0)
    {
        VK_NEVER_CALLED();

        if (count > 0)
        {
            Log("|");
        }
        Log("0x%x", flags);
    }

#undef LogFlag

}

// =====================================================================================================================
static const char* AccessFlagString(VkAccessFlagBits flag, bool compact)
{
    switch (flag)
    {
    case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
        return compact ? "IND_CMD_READ" : "INDIRECT_COMMAND_READ_BIT";
    case VK_ACCESS_INDEX_READ_BIT:
        return compact ? "IDX_RD" : "INDEX_READ_BIT";
    case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
        return compact ? "VTX_ATTR_RD" : "VERTEX_ATTRIBUTE_READ_BIT";
    case VK_ACCESS_UNIFORM_READ_BIT:
        return compact ? "UNIFORM_RD" : "UNIFORM_READ_BIT";
    case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
        return compact ? "INPUT_ATTACH_RD" : "INPUT_ATTACHMENT_READ_BIT";
    case VK_ACCESS_SHADER_READ_BIT:
        return compact ? "SHADER_RD" : "SHADER_READ_BIT";
    case VK_ACCESS_SHADER_WRITE_BIT:
        return compact ? "SHADER_WR" : "SHADER_WRITE_BIT";
    case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
        return compact ? "COLOR_RD" : "COLOR_ATTACHMENT_READ_BIT";
    case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
        return compact ? "COLOR_WR" : "COLOR_ATTACHMENT_WRITE_BIT";
    case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
        return compact ? "DS_RD" : "DEPTH_STENCIL_ATTACHMENT_READ_BIT";
    case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
        return compact ? "DS_WR" : "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT";
    case VK_ACCESS_TRANSFER_READ_BIT:
        return compact ? "XFER_RD" : "TRANSFER_READ_BIT";
    case VK_ACCESS_TRANSFER_WRITE_BIT:
        return compact ? "XFER_WR" : "TRANSFER_WRITE_BIT";
    case VK_ACCESS_HOST_READ_BIT:
        return compact ? "HOST_RD" : "HOST_READ_BIT";
    case VK_ACCESS_HOST_WRITE_BIT:
        return compact ? "HOST_WR" : "HOST_WRITE_BIT";
    case VK_ACCESS_MEMORY_READ_BIT:
        return compact ? "MEM_RD" : "MEMORY_READ_BIT";
    case VK_ACCESS_MEMORY_WRITE_BIT:
        return compact ? "MEM_WR" : "MEMORY_WRITE_BIT";
    default:
        VK_NEVER_CALLED();
        return "<unknown access flag>";
    }
}

// =====================================================================================================================
void RenderPassLogger::LogAccessMask(VkAccessFlags flags, bool compact)
{
    if (flags == 0)
    {
        Log("0");

        return;
    }

    uint32_t count = 0;

#define LogFlag(flag) \
    if ((flags & flag) == flag) \
    { \
        if (count >= 1) \
        { \
            Log("|"); \
        } \
        Log("%s", AccessFlagString(flag, compact)); \
        flags &= ~(flag); \
        count++; \
    }

    LogFlag(VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    LogFlag(VK_ACCESS_INDEX_READ_BIT);
    LogFlag(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    LogFlag(VK_ACCESS_UNIFORM_READ_BIT);
    LogFlag(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
    LogFlag(VK_ACCESS_SHADER_READ_BIT);
    LogFlag(VK_ACCESS_SHADER_WRITE_BIT);
    LogFlag(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    LogFlag(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    LogFlag(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    LogFlag(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    LogFlag(VK_ACCESS_TRANSFER_READ_BIT);
    LogFlag(VK_ACCESS_TRANSFER_WRITE_BIT);
    LogFlag(VK_ACCESS_HOST_READ_BIT);
    LogFlag(VK_ACCESS_HOST_WRITE_BIT);
    LogFlag(VK_ACCESS_MEMORY_READ_BIT);
    LogFlag(VK_ACCESS_MEMORY_WRITE_BIT);

    if (flags != 0)
    {
        VK_NEVER_CALLED();

        if (count > 0)
        {
            Log("|");
        }
        Log("0x%x", flags);
    }

#undef LogFlag
}

// =====================================================================================================================
void RenderPassLogger::LogRenderPassCreateInfo(
    const RenderPassCreateInfo& info)
{
    Log("== Render Pass VkRenderPassCreateInfo:\n");

    LogBeginSource();
    Log("info.flags           = 0x%x\n", info.flags);
    Log("info.attachmentCount = %d\n", info.attachmentCount);

    for (uint32_t i = 0; i < info.attachmentCount; ++i)
    {
        const AttachmentDescription& desc = info.pAttachments[i];

        Log("info.pAttachments[%d] = {\n", i);
        Log("   .flags          = 0x%x\n", desc.flags);
        Log("   .format         = ");    LogFormat(desc.format, false); Log("\n");
        Log("   .samples        = 0x%x\n", desc.samples);
        Log("   .loadOp         = %s\n", LoadOpString(desc.loadOp));
        Log("   .storeOp        = %s\n", StoreOpString(desc.storeOp));
        Log("   .stencilLoadOp  = %s\n", LoadOpString(desc.stencilLoadOp));
        Log("   .stencilStoreOp = %s\n", StoreOpString(desc.stencilStoreOp));
        Log("   .initialLayout  = %s\n", ImageLayoutString(desc.initialLayout, false));
        Log("   .finalLayout    = %s\n", ImageLayoutString(desc.finalLayout, false));
        Log("}\n");
    }

    Log("info.subpassCount = %d\n", info.subpassCount);

    for (uint32_t i = 0; i < info.subpassCount; ++i)
    {
        const SubpassDescription& desc = info.pSubpasses[i];

        Log("info.pSubpasses[%d] = {\n", i);
        Log("   .flags                = 0x%x\n", desc.flags);
        Log("   .pipelineBindPoint    = 0x%x\n", desc.pipelineBindPoint);
        Log("   .viewMask             = 0x%x\n", desc.viewMask);
        Log("   .inputAttachmentCount = %d\n", desc.inputAttachmentCount);
        if (desc.pInputAttachments != nullptr && desc.inputAttachmentCount < info.attachmentCount)
        {
            for (uint32_t j = 0; j < desc.inputAttachmentCount; ++j)
            {
                LogInfoAttachmentReference("pInputAttachments", j, desc.pInputAttachments[j]);
            }
        }
        if (desc.pColorAttachments != nullptr)
        {
            Log("   .colorAttachmentCount = %d\n", desc.colorAttachmentCount);
            for (uint32_t j = 0; j < desc.colorAttachmentCount; ++j)
            {
                LogInfoAttachmentReference("pColorAttachments", j, desc.pColorAttachments[j]);
            }
        }
        if (desc.pResolveAttachments != nullptr)
        {
            for (uint32_t j = 0; j < desc.colorAttachmentCount; ++j)
            {
                LogInfoAttachmentReference("pResolveAttachments", j, desc.pResolveAttachments[j]);
            }
        }
        if (desc.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED)
        {
            LogInfoAttachmentReference("depthStencilAttachment", 0, desc.depthStencilAttachment);
        }
        Log("   .preserveAttachmentCount = %d\n", desc.preserveAttachmentCount);
        for (uint32_t j = 0; j < desc.preserveAttachmentCount; ++j)
        {
            if (j == 0)
            {
                Log("   .preserveAttachments = { ");
            }
            Log("%d", desc.pPreserveAttachments[j]);
            if (j == desc.preserveAttachmentCount - 1)
            {
                Log(" }\n");
            }
            else
            {
                Log(", ");
            }
        }
        Log("}\n");
    }

    Log("info.dependencyCount = %d\n", info.dependencyCount);

    for (uint32_t i = 0; i < info.dependencyCount; ++i)
    {
        const SubpassDependency& dep = info.pDependencies[i];

        Log("info.pDependencies[%d] = {\n", i);

        LogSubpassDependency(dep, true, false);

        Log("}\n");
    }

    Log("info.correlatedViewMaskCount = %d\n", info.correlatedViewMaskCount);
    for (uint32_t j = 0; j < info.correlatedViewMaskCount; ++j)
    {
        if (j == 0)
        {
            Log("   .pCorrelatedViewMasks = { ");
        }
        Log("%d", info.pCorrelatedViewMasks[j]);
        if (j == info.correlatedViewMaskCount - 1)
        {
            Log(" }\n");
        }
        else
        {
            Log(", ");
        }
    }
    Log("}\n");

    LogEndSource();
}

// =====================================================================================================================
void RenderPassLogger::LogSubpassDependency(
    const SubpassDependency&   dep,
    bool                       printSubpasses,
    bool                       label)
{
    const char* pNewLine = (label ? "\\l" : "\n");

    if (printSubpasses)
    {
        if (label)
        {
            Log("Subpass: %d to %d\\l", dep.srcSubpass, dep.dstSubpass);
        }
        else
        {
            if (dep.srcSubpass == VK_SUBPASS_EXTERNAL)
            {
                Log("   .srcSubpass = VK_SUBPASS_EXTERNAL%s", pNewLine);
            }
            else
            {
                Log("   .srcSubpass = %d%s", dep.srcSubpass, pNewLine);
            }

            if (dep.dstSubpass == VK_SUBPASS_EXTERNAL)
            {
                Log("   .dstSubpass = VK_SUBPASS_EXTERNAL%s", pNewLine);
            }
            else
            {
                Log("   .dstSubpass = %d%s", dep.dstSubpass, pNewLine);
            }
        }
    }

    if (label == false || dep.srcStageMask != 0)
    {
        Log(label ? "srcStage: " : "   .srcStageMask = "); LogPipelineStageMask(dep.srcStageMask, label); Log(pNewLine);
    }

    if (label == false || dep.dstStageMask != 0)
    {
        Log(label ? "dstStage: " : "   .dstStageMask = "); LogPipelineStageMask(dep.dstStageMask, label); Log(pNewLine);
    }

    if (label == false || dep.srcAccessMask != 0)
    {
        Log(label ? "srcAccess: " : "   .srcAccessMask = "); LogAccessMask(dep.srcAccessMask, label); Log(pNewLine);
    }

    if (label == false || dep.dstAccessMask != 0)
    {
        Log(label ? "dstAccess: " : "   .dstAccessMask = "); LogAccessMask(dep.dstAccessMask, label); Log(pNewLine);
    }

    if (label == false || dep.dependencyFlags != 0)
    {
        Log("   .dependencyFlags = 0x%x%s", dep.dependencyFlags, pNewLine);
    }

    if (label == false || dep.viewOffset != 0)
    {
        Log("   .viewOffset = 0x%x%s", dep.viewOffset, pNewLine);
    }
}

// =====================================================================================================================
void RenderPassLogger::End()
{
    if (m_logging == false)
    {
        return;
    }

    LogStatistics();

    Log("// end\n");

    m_file.Close();
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteInfo(
    const RenderPassExecuteInfo* pExecute)
{
    if (m_logging == false)
    {
        return;
    }

    m_pExecute = pExecute;

    Log("== Render Pass Execute Info:\n");

    Log("NOTE: This information represents commands that are recorded into a command buffer during a render pass instance "
        "to set up state and perform any other implicit render pass operations.  Please note that this logging code "
        "exists separate to the true code run by the driver and is an approximation.\n\n");

    for (uint32_t subpass = 0; subpass < m_pInfo->subpassCount; ++subpass)
    {
        if (subpass == 0)
        {
            Log("=== vkCmdBeginRenderPass():\n\n");
        }
        else
        {
            Log("=== vkCmdNextSubpass(/* subpass = %d */):\n\n", subpass);
        }

        if (subpass > 0)
        {
            LogExecuteRPEndSubpass(subpass - 1);
        }

        LogExecuteRPBeginSubpass(subpass);
    }

    Log("=== vkCmdEndRenderPass():\n");

    LogExecuteRPEndSubpass(m_pInfo->subpassCount - 1);

    const RPExecuteEndRenderPassInfo& end = pExecute->end;

    Log("==== Execute End State:\n\n");

    if (end.syncEnd.flags.active)
    {
        LogExecuteRPSyncPoint(end.syncEnd, "syncEnd");
    }
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPBeginSubpass(
    uint32_t subpass)
{
    Log("==== CmdBuffer::RPBeginSubpass(%d):\n\n", subpass);

    const RPExecuteBeginSubpassInfo& begin = m_pExecute->pSubpasses[subpass].begin;

    if (begin.syncTop.flags.active)
    {
        LogExecuteRPSyncPoint(begin.syncTop, "syncTop");
    }

    Log("===== Set Sample Pattern for Subpass %d\n\n", subpass);

    if (begin.loadOps.colorClearCount > 0)
    {
        LogExecuteRPLoadOpClear(begin.loadOps.colorClearCount, begin.loadOps.pColorClears,
            "RPLoadOpColorClear", ".loadOps.pColorClears");
    }

    if (begin.loadOps.dsClearCount > 0)
    {
        LogExecuteRPLoadOpClear(begin.loadOps.dsClearCount, begin.loadOps.pDsClears,
            "RPLoadOpDepthStencilClear", ".loadOps.pDsClears");
    }

    LogExecuteRPBindTargets(begin.bindTargets);
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPLoadOpClear(
    uint32_t                 count,
    const RPLoadOpClearInfo* pClears,
    const char*              pName,
    const char*              pVar)
{
    Log("===== %s():\n\n", pName);

    LogBeginSource();

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPLoadOpClearInfo& clear = pClears[i];

        Log("%s[%d]:\n", pVar, i);
        Log("    .attachment = %u\n", clear.attachment);
        Log("    .layout     = "); LogImageLayout(clear.layout); Log("\n");
        Log("    .aspect     = ");

        if (clear.aspect == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            Log("VK_IMAGE_ASPECT_COLOR_BIT");
        }
        else if (clear.aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            Log("VK_IMAGE_ASPECT_DEPTH_BIT");
        }
        else if (clear.aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            Log("VK_IMAGE_ASPECT_STENCIL_BIT");
        }
        else if (clear.aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
        {
            Log("VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT");
        }
        else
        {
            VK_NEVER_CALLED();

            Log("0x%x", clear.aspect);
        }

        Log("\n");
    }

    LogEndSource();
    Log("\n");
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPEndSubpass(
    uint32_t subpass)
{
    Log("==== CmdBuffer::RPEndSubpass(%d):\n\n", subpass);

    const RPExecuteEndSubpassInfo& end = m_pExecute->pSubpasses[subpass].end;

    if (end.syncPreResolve.flags.active)
    {
        LogExecuteRPSyncPoint(end.syncPreResolve, "syncPreResolve");
    }

    if (end.resolveCount > 0)
    {
        LogExecuteRPResolveAttachments(end.resolveCount, end.pResolves);
    }

    if (end.syncBottom.flags.active)
    {
        LogExecuteRPSyncPoint(end.syncBottom, "syncBottom");
    }
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPResolveAttachments(
    uint32_t             count,
    const RPResolveInfo* pResolves)
{
    Log("===== RPResolveAttachments():\n\n");

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPResolveInfo& info = pResolves[i];

        Log(".pResolves[%d]:\n", i);
        Log("    .src = "); LogAttachmentReference(info.src); Log("\n");
        Log("    .dst = "); LogAttachmentReference(info.dst); Log("\n");
    }

    Log("\n");
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPSyncPoint(
    const RPSyncPointInfo& syncPoint,
    const char*            pName)
{
    Log("===== CmdBuffer::RPSyncPoint(%s):\n\n", pName);

    LogBeginSource();
    Log(    "%s.barrier:\n", pName);
    Log(    "    .srcStageMask  = "); LogPipelineStageMask(syncPoint.barrier.srcStageMask, false); Log("\n");
    Log(    "    .dstStageMask  = "); LogPipelineStageMask(syncPoint.barrier.dstStageMask, false); Log("\n");
    Log(    "    .srcAccessMask = "); LogAccessMask(syncPoint.barrier.srcAccessMask, false); Log("\n");
    Log(    "    .dstAccessMask = "); LogAccessMask(syncPoint.barrier.dstAccessMask, false); Log("\n");
    LogFlag("    .flags.needsGlobalTransition    = 1\n", syncPoint.barrier.flags.needsGlobalTransition);
    LogFlag("    .flags.implicitExternalIncoming = 1\n", syncPoint.barrier.flags.implicitExternalIncoming);
    LogFlag("    .flags.implicitExternalOutgoing = 1\n", syncPoint.barrier.flags.implicitExternalOutgoing);
    LogFlag("    .flags.preColorResolveSync      = 1\n", syncPoint.barrier.flags.preColorResolveSync);
    LogFlag("    .flags.preDsResolveSync         = 1\n", syncPoint.barrier.flags.preDsResolveSync);
    LogFlag("    .flags.postResolveSync          = 1\n", syncPoint.barrier.flags.postResolveSync);
    LogFlag("    .flags.preColorClearSync        = 1\n", syncPoint.barrier.flags.preColorClearSync);

    for (uint32_t i = 0; i < syncPoint.transitionCount; ++i)
    {
        if (i == 0)
        {
            Log("\n");
        }

        const auto& tr = syncPoint.pTransitions[i];

        const uint32_t attachment = tr.attachment;
        const VkFormat format = m_pInfo->pAttachments[attachment].format;

        Log(    "%s.pTransitions[%d]:\n", pName, i);
        Log(    "    .attachment = ");  LogAttachment(attachment); Log("\n");
        Log(    "    .prevLayout = "); LogImageLayout(tr.prevLayout); Log("\n");
        Log(    "    .nextLayout = "); LogImageLayout(tr.nextLayout); Log("\n");
    }

    LogEndSource();
    Log("\n");
}

// =====================================================================================================================
void RenderPassLogger::LogFlag(
    const char* pFlag,
    uint32_t    val)
{
    if (val != 0)
    {
        Log(pFlag);
    }
}

// =====================================================================================================================
void RenderPassLogger::LogExecuteRPBindTargets(const RPBindTargetsInfo& info)
{
    Log("===== CmdBuffer::RPBindTargets():\n\n");
    LogBeginSource();
    for (uint32_t t = 0; t < info.colorTargetCount; ++t)
    {
        Log("Color%d: ", t); LogAttachmentReference(info.colorTargets[t]); Log("\n");
    }
    Log("DS:  "); LogAttachmentReference(info.depthStencil); Log("\n");
    LogEndSource();
    Log("\n");
}

// =====================================================================================================================
RenderPassLogger::~RenderPassLogger()
{
    m_file.Close();
}

// =====================================================================================================================
bool RenderPassLogger::OpenLogFile(uint64_t hash)
{
    const char* const pLogDir = m_settings.renderPassLogDirectory;

    char fileName[512];

    Util::Snprintf(fileName, sizeof(fileName), "%s/RenderPass_0x%016llX.adoc", pLogDir, hash);

#if 0
    for (uint32_t suffixNum = 1; Util::File::Exists(fileName); suffixNum++)
    {
        // If the computed file name already exists, append a "_#" to the filename to prevent collisions.  Keep
        // incrementing the number until we succeed.
        Util::Snprintf(fileName, sizeof(fileName), "%s/RenderPass_0x%016llX_%d.adoc",
            pLogDir,
            hash,
            suffixNum);
    }
#endif

    return (m_file.Open(fileName, Util::FileAccessWrite) == Pal::Result::Success);
}

// =====================================================================================================================
void RenderPassLogger::LogStatistics()
{
    Log("== Statistics:\n\n");

    Log("Temporary memory allocated during building: %llu bytes\n", (uint64_t)m_pArena->GetTotalAllocated());
}

// =====================================================================================================================
void RenderPassLogger::LogBeginSource()
{
    Log("[[source,C++]]\n----\n");
}

// =====================================================================================================================
void RenderPassLogger::LogEndSource()
{
    Log("----\n");
}

// =====================================================================================================================
void RenderPassLogger::LogFormat(VkFormat format, bool shortDesc)
{
#define FormatToStringCase(format_suffix) \
    case VK_FORMAT_##format_suffix: \
        Log(shortDesc ? (#format_suffix) : ("VK_FORMAT_" #format_suffix)); \
        break;

    switch (format)
    {
    FormatToStringCase(UNDEFINED)
    FormatToStringCase(R4G4_UNORM_PACK8)
    FormatToStringCase(R4G4B4A4_UNORM_PACK16)
    FormatToStringCase(B4G4R4A4_UNORM_PACK16)
    FormatToStringCase(R5G6B5_UNORM_PACK16)
    FormatToStringCase(B5G6R5_UNORM_PACK16)
    FormatToStringCase(R5G5B5A1_UNORM_PACK16)
    FormatToStringCase(B5G5R5A1_UNORM_PACK16)
    FormatToStringCase(A1R5G5B5_UNORM_PACK16)
    FormatToStringCase(R8_UNORM)
    FormatToStringCase(R8_SNORM)
    FormatToStringCase(R8_USCALED)
    FormatToStringCase(R8_SSCALED)
    FormatToStringCase(R8_UINT)
    FormatToStringCase(R8_SINT)
    FormatToStringCase(R8_SRGB)
    FormatToStringCase(R8G8_UNORM)
    FormatToStringCase(R8G8_SNORM)
    FormatToStringCase(R8G8_USCALED)
    FormatToStringCase(R8G8_SSCALED)
    FormatToStringCase(R8G8_UINT)
    FormatToStringCase(R8G8_SINT)
    FormatToStringCase(R8G8_SRGB)
    FormatToStringCase(R8G8B8_UNORM)
    FormatToStringCase(R8G8B8_SNORM)
    FormatToStringCase(R8G8B8_USCALED)
    FormatToStringCase(R8G8B8_SSCALED)
    FormatToStringCase(R8G8B8_UINT)
    FormatToStringCase(R8G8B8_SINT)
    FormatToStringCase(R8G8B8_SRGB)
    FormatToStringCase(B8G8R8_UNORM)
    FormatToStringCase(B8G8R8_SNORM)
    FormatToStringCase(B8G8R8_USCALED)
    FormatToStringCase(B8G8R8_SSCALED)
    FormatToStringCase(B8G8R8_UINT)
    FormatToStringCase(B8G8R8_SINT)
    FormatToStringCase(B8G8R8_SRGB)
    FormatToStringCase(R8G8B8A8_UNORM)
    FormatToStringCase(R8G8B8A8_SNORM)
    FormatToStringCase(R8G8B8A8_USCALED)
    FormatToStringCase(R8G8B8A8_SSCALED)
    FormatToStringCase(R8G8B8A8_UINT)
    FormatToStringCase(R8G8B8A8_SINT)
    FormatToStringCase(R8G8B8A8_SRGB)
    FormatToStringCase(B8G8R8A8_UNORM)
    FormatToStringCase(B8G8R8A8_SNORM)
    FormatToStringCase(B8G8R8A8_USCALED)
    FormatToStringCase(B8G8R8A8_SSCALED)
    FormatToStringCase(B8G8R8A8_UINT)
    FormatToStringCase(B8G8R8A8_SINT)
    FormatToStringCase(B8G8R8A8_SRGB)
    FormatToStringCase(A8B8G8R8_UNORM_PACK32)
    FormatToStringCase(A8B8G8R8_SNORM_PACK32)
    FormatToStringCase(A8B8G8R8_USCALED_PACK32)
    FormatToStringCase(A8B8G8R8_SSCALED_PACK32)
    FormatToStringCase(A8B8G8R8_UINT_PACK32)
    FormatToStringCase(A8B8G8R8_SINT_PACK32)
    FormatToStringCase(A8B8G8R8_SRGB_PACK32)
    FormatToStringCase(A2R10G10B10_UNORM_PACK32)
    FormatToStringCase(A2R10G10B10_SNORM_PACK32)
    FormatToStringCase(A2R10G10B10_USCALED_PACK32)
    FormatToStringCase(A2R10G10B10_SSCALED_PACK32)
    FormatToStringCase(A2R10G10B10_UINT_PACK32)
    FormatToStringCase(A2R10G10B10_SINT_PACK32)
    FormatToStringCase(A2B10G10R10_UNORM_PACK32)
    FormatToStringCase(A2B10G10R10_SNORM_PACK32)
    FormatToStringCase(A2B10G10R10_USCALED_PACK32)
    FormatToStringCase(A2B10G10R10_SSCALED_PACK32)
    FormatToStringCase(A2B10G10R10_UINT_PACK32)
    FormatToStringCase(A2B10G10R10_SINT_PACK32)
    FormatToStringCase(R16_UNORM)
    FormatToStringCase(R16_SNORM)
    FormatToStringCase(R16_USCALED)
    FormatToStringCase(R16_SSCALED)
    FormatToStringCase(R16_UINT)
    FormatToStringCase(R16_SINT)
    FormatToStringCase(R16_SFLOAT)
    FormatToStringCase(R16G16_UNORM)
    FormatToStringCase(R16G16_SNORM)
    FormatToStringCase(R16G16_USCALED)
    FormatToStringCase(R16G16_SSCALED)
    FormatToStringCase(R16G16_UINT)
    FormatToStringCase(R16G16_SINT)
    FormatToStringCase(R16G16_SFLOAT)
    FormatToStringCase(R16G16B16_UNORM)
    FormatToStringCase(R16G16B16_SNORM)
    FormatToStringCase(R16G16B16_USCALED)
    FormatToStringCase(R16G16B16_SSCALED)
    FormatToStringCase(R16G16B16_UINT)
    FormatToStringCase(R16G16B16_SINT)
    FormatToStringCase(R16G16B16_SFLOAT)
    FormatToStringCase(R16G16B16A16_UNORM)
    FormatToStringCase(R16G16B16A16_SNORM)
    FormatToStringCase(R16G16B16A16_USCALED)
    FormatToStringCase(R16G16B16A16_SSCALED)
    FormatToStringCase(R16G16B16A16_UINT)
    FormatToStringCase(R16G16B16A16_SINT)
    FormatToStringCase(R16G16B16A16_SFLOAT)
    FormatToStringCase(R32_UINT)
    FormatToStringCase(R32_SINT)
    FormatToStringCase(R32_SFLOAT)
    FormatToStringCase(R32G32_UINT)
    FormatToStringCase(R32G32_SINT)
    FormatToStringCase(R32G32_SFLOAT)
    FormatToStringCase(R32G32B32_UINT)
    FormatToStringCase(R32G32B32_SINT)
    FormatToStringCase(R32G32B32_SFLOAT)
    FormatToStringCase(R32G32B32A32_UINT)
    FormatToStringCase(R32G32B32A32_SINT)
    FormatToStringCase(R32G32B32A32_SFLOAT)
    FormatToStringCase(R64_UINT)
    FormatToStringCase(R64_SINT)
    FormatToStringCase(R64_SFLOAT)
    FormatToStringCase(R64G64_UINT)
    FormatToStringCase(R64G64_SINT)
    FormatToStringCase(R64G64_SFLOAT)
    FormatToStringCase(R64G64B64_UINT)
    FormatToStringCase(R64G64B64_SINT)
    FormatToStringCase(R64G64B64_SFLOAT)
    FormatToStringCase(R64G64B64A64_UINT)
    FormatToStringCase(R64G64B64A64_SINT)
    FormatToStringCase(R64G64B64A64_SFLOAT)
    FormatToStringCase(B10G11R11_UFLOAT_PACK32)
    FormatToStringCase(E5B9G9R9_UFLOAT_PACK32)
    FormatToStringCase(D16_UNORM)
    FormatToStringCase(X8_D24_UNORM_PACK32)
    FormatToStringCase(D32_SFLOAT)
    FormatToStringCase(S8_UINT)
    FormatToStringCase(D16_UNORM_S8_UINT)
    FormatToStringCase(D24_UNORM_S8_UINT)
    FormatToStringCase(D32_SFLOAT_S8_UINT)
    FormatToStringCase(BC1_RGB_UNORM_BLOCK)
    FormatToStringCase(BC1_RGB_SRGB_BLOCK)
    FormatToStringCase(BC1_RGBA_UNORM_BLOCK)
    FormatToStringCase(BC1_RGBA_SRGB_BLOCK)
    FormatToStringCase(BC2_UNORM_BLOCK)
    FormatToStringCase(BC2_SRGB_BLOCK)
    FormatToStringCase(BC3_UNORM_BLOCK)
    FormatToStringCase(BC3_SRGB_BLOCK)
    FormatToStringCase(BC4_UNORM_BLOCK)
    FormatToStringCase(BC4_SNORM_BLOCK)
    FormatToStringCase(BC5_UNORM_BLOCK)
    FormatToStringCase(BC5_SNORM_BLOCK)
    FormatToStringCase(BC6H_UFLOAT_BLOCK)
    FormatToStringCase(BC6H_SFLOAT_BLOCK)
    FormatToStringCase(BC7_UNORM_BLOCK)
    FormatToStringCase(BC7_SRGB_BLOCK)
    FormatToStringCase(ETC2_R8G8B8_UNORM_BLOCK)
    FormatToStringCase(ETC2_R8G8B8_SRGB_BLOCK)
    FormatToStringCase(ETC2_R8G8B8A1_UNORM_BLOCK)
    FormatToStringCase(ETC2_R8G8B8A1_SRGB_BLOCK)
    FormatToStringCase(ETC2_R8G8B8A8_UNORM_BLOCK)
    FormatToStringCase(ETC2_R8G8B8A8_SRGB_BLOCK)
    FormatToStringCase(EAC_R11_UNORM_BLOCK)
    FormatToStringCase(EAC_R11_SNORM_BLOCK)
    FormatToStringCase(EAC_R11G11_UNORM_BLOCK)
    FormatToStringCase(EAC_R11G11_SNORM_BLOCK)
    FormatToStringCase(ASTC_4x4_UNORM_BLOCK)
    FormatToStringCase(ASTC_4x4_SRGB_BLOCK)
    FormatToStringCase(ASTC_5x4_UNORM_BLOCK)
    FormatToStringCase(ASTC_5x4_SRGB_BLOCK)
    FormatToStringCase(ASTC_5x5_UNORM_BLOCK)
    FormatToStringCase(ASTC_5x5_SRGB_BLOCK)
    FormatToStringCase(ASTC_6x5_UNORM_BLOCK)
    FormatToStringCase(ASTC_6x5_SRGB_BLOCK)
    FormatToStringCase(ASTC_6x6_UNORM_BLOCK)
    FormatToStringCase(ASTC_6x6_SRGB_BLOCK)
    FormatToStringCase(ASTC_8x5_UNORM_BLOCK)
    FormatToStringCase(ASTC_8x5_SRGB_BLOCK)
    FormatToStringCase(ASTC_8x6_UNORM_BLOCK)
    FormatToStringCase(ASTC_8x6_SRGB_BLOCK)
    FormatToStringCase(ASTC_8x8_UNORM_BLOCK)
    FormatToStringCase(ASTC_8x8_SRGB_BLOCK)
    FormatToStringCase(ASTC_10x5_UNORM_BLOCK)
    FormatToStringCase(ASTC_10x5_SRGB_BLOCK)
    FormatToStringCase(ASTC_10x6_UNORM_BLOCK)
    FormatToStringCase(ASTC_10x6_SRGB_BLOCK)
    FormatToStringCase(ASTC_10x8_UNORM_BLOCK)
    FormatToStringCase(ASTC_10x8_SRGB_BLOCK)
    FormatToStringCase(ASTC_10x10_UNORM_BLOCK)
    FormatToStringCase(ASTC_10x10_SRGB_BLOCK)
    FormatToStringCase(ASTC_12x10_UNORM_BLOCK)
    FormatToStringCase(ASTC_12x10_SRGB_BLOCK)
    FormatToStringCase(ASTC_12x12_UNORM_BLOCK)
    FormatToStringCase(ASTC_12x12_SRGB_BLOCK)
    FormatToStringCase(PVRTC1_2BPP_UNORM_BLOCK_IMG)
    FormatToStringCase(PVRTC1_4BPP_UNORM_BLOCK_IMG)
    FormatToStringCase(PVRTC2_2BPP_UNORM_BLOCK_IMG)
    FormatToStringCase(PVRTC2_4BPP_UNORM_BLOCK_IMG)
    FormatToStringCase(PVRTC1_2BPP_SRGB_BLOCK_IMG)
    FormatToStringCase(PVRTC1_4BPP_SRGB_BLOCK_IMG)
    FormatToStringCase(PVRTC2_2BPP_SRGB_BLOCK_IMG)
    FormatToStringCase(PVRTC2_4BPP_SRGB_BLOCK_IMG)
    default:
        Log("0x%x", static_cast<uint32_t>(format));
    }

#undef FormatToStringCase
}

}; // namespace vk

#endif
