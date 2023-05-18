/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  log.h
* @brief wrap sanity print methods for xgl, the log file is in /var/tmp/palLog.txt.
***********************************************************************************************************************
*/
#ifndef __LOG_H__
#define __LOG_H__
#pragma once

#include "include/vk_utils.h"

#include "palDbgPrint.h"

namespace vk
{

enum LogTagId : uint32_t {
    GeneralPrint,
    PipelineCompileTime,
    LogTagIdCount
};

static const char* LogTag[LogTagIdCount] =
{
    "GeneralPrint",
    "PipelineCompileTime",
};

static void AmdvlkLog(
    uint64_t          logTagIdMask,
    LogTagId          tagId,
    const char*       pFormatStr,
    ...)
{
    VK_ASSERT(tagId < LogTagIdCount);
    if ((logTagIdMask & (1LLU << tagId)) == 0)
    {
      return;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    va_list argList;
    Util::DbgPrintf(Util::DbgPrintCatMsgFile, Util::DbgPrintStyleNoPrefixNoCrLf, "%s-", LogTag[tagId]);
    va_start(argList, pFormatStr);
    Util::DbgVPrintf(Util::DbgPrintCatMsgFile, Util::DbgPrintStyleNoPrefixNoCrLf, pFormatStr, argList);
    va_end(argList);
    Util::DbgPrintf(Util::DbgPrintCatMsgFile, Util::DbgPrintStyleNoPrefixNoCrLf, "\n");
#endif
}
} // namespace vk

#endif /* __LOG_H__ */
