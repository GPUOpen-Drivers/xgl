/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#ifndef __RENDERPASS_RENDERPASS_LOGGER_H__
#define __RENDERPASS_RENDERPASS_LOGGER_H__
#pragma once

#include "include/khronos/vulkan.h"

#include "renderpass/renderpass_builder.h"
#include "renderpass/renderpass_types.h"
#include "utils/temp_mem_arena.h"

#include "palFile.h"

#if ICD_LOG_RENDER_PASSES
#define RenderPassLogBegin(logger, apiInfo, createInfo) { logger->Begin(apiInfo, createInfo); }
#define RenderPassLogEnd(logger) { logger->End(); }
#define RenderPassLogExecuteInfo(logger, execute) { logger->LogExecuteInfo(execute); }
#else
#define RenderPassLogBegin(logger, apiInfo, createInfo) {}
#define RenderPassLogEnd(logger) {}
#define RenderPassLogExecuteInfo(logger, execute) {}
#endif

namespace vk
{

class RenderPassBuilder;

// =====================================================================================================================
// This class dumps render passes in .asciidoc format as they are created.
class RenderPassLogger
{
public:
    RenderPassLogger(utils::TempMemArena* pArena, const Device* pDevice);
    ~RenderPassLogger();

    void Begin(const VkRenderPassCreateInfo& apiInfo, const RenderPassCreateInfo& info);
    void LogExecuteInfo(const RenderPassExecuteInfo* pExecute);
    void End();

private:
    bool OpenLogFile(uint64_t hash);
    void Log(const char* pFormat, ...);
    void LogRenderPassCreateInfo(const VkRenderPassCreateInfo& apiInfo);
    void LogExecuteRPBeginSubpass(uint32_t subpass);
    void LogExecuteRPEndSubpass(uint32_t subpass);
    void LogExecuteRPSyncPoint(const RPSyncPointInfo& syncPoint, const char* pName);
    void LogFlag(const char* pFlag, uint32_t val);
    void LogExecuteRPLoadOpClear(uint32_t count, const RPLoadOpClearInfo* pClears, const char* pName, const char* pVar);
    void LogExecuteRPResolveAttachments(uint32_t count, const RPResolveInfo* pResolves);

    void LogInfoAttachmentReference(const char* pAttachmentArray, uint32_t element, const VkAttachmentReference& ref);
    void LogAttachmentReference(const VkAttachmentReference& reference);
    void LogAttachmentReference(const RPAttachmentReference& reference);
    void LogAttachment(uint32_t attachment);
    void LogImageLayout(const RPImageLayout& layout);
    void LogFormat(VkFormat format, bool shortDesc = true);
    void LogPipelineStageMask(VkPipelineStageFlags flags, bool compact);
    void LogAccessMask(VkAccessFlags flags, bool compact);
    void LogSubpassDependency(const VkSubpassDependency& dep, bool printSubpasses, bool label);
    void LogStatistics();
    void LogBeginSource();
    void LogEndSource();
    void LogIntermSubpass(uint32_t subpass);
    void LogIntermDependency(uint32_t depIdx, bool incoming);
    void LogIntermWorkItem(uint32_t item);
    void LogIntermTransition(const RPTransitionInfo& transition);
    void LogExecuteRPBindTargets(const RPBindTargetsInfo& info);
    void LogClearInfo(const RPLoadOpClearInfo& info);

    utils::TempMemArena*           m_pArena;
    const RuntimeSettings&         m_settings;
    const VkRenderPassCreateInfo*  m_pApiInfo;
    const RenderPassCreateInfo*    m_pInfo;
    const RenderPassExecuteInfo*   m_pExecute;
    Util::File                     m_file;
    bool                           m_logging;
};

} // namespace vk

#endif /* __RENDERPASS_RENDERPASS_LOGGER_H__ */
