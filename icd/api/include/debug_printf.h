/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef __DEBUG_PRINTF_H__
#define __DEBUG_PRINTF_H__

#include "include/khronos/vulkan.h"
#include "palVector.h"
#include <regex>
#include "internal_mem_mgr.h"

namespace Pal
{
class ICmdBuffer;
}

namespace vk
{
class CmdBuffer;
class Device;
class Queue;
class Pipeline;
struct RuntimeSettings;

typedef Util::Vector<char, 8, Util::GenericAllocator> PrintfString;
typedef Util::Vector<bool, 8, Util::GenericAllocator> PrintfBit;

// Printf Elf string and bits position
struct PrintfElfString
{
    PrintfString printStr; // Printf format string
    PrintfBit    bit64s;   // Bit positions of output variables
    PrintfElfString() : printStr(nullptr), bit64s(nullptr)
    {
    }
};

typedef Util::HashMap<uint64_t, PrintfElfString, PalAllocator> PrintfFormatMap;

// =====================================================================================================================
// Support the printf like solution in driver
class DebugPrintf
{
    // Specifier type
    enum SpecifierType : uint32_t
    {
        SpecifierUnsigned = 0, // Unsigned specifier
        SpecifierInteger,      // Integer specifier
        SpecifierFloat         // Float specifier
    };
    // The format string subsection
    struct SubStrSection
    {
        uint32_t      beginPos;      // Specifier begin position in the format string
        uint32_t      count;         // Specifier string length
        SpecifierType specifierType; // Specifier type
        const char*   decodedStr;    // Decoded String
    };
    typedef Util::Vector<SubStrSection, 3, Util::GenericAllocator> PrintfSubSection;
    typedef Util::HashMap<uint64_t, PrintfSubSection, PalAllocator> PrintfSubsectMap;
    // Debug printf class state
    enum DebugPrintfState
    {
        Uninitialized,  // DebugPrintf is not initialized
        Enabled,        // If runtime setting enable debug print
        MemoryAllocated // If the debugprintf memory buffer created
    };

    // Pipeline type
    enum PipelineType : uint32_t
    {
        DebugPrintfCompute = 0, // Compute pipeline type
        DebugPrintfGraphics,    // Graphics pipeline type
#if VKI_RAY_TRACING
        DebugPrintfRayTracing,  // Raytracing pipeline type
#endif
    };

public:
    DebugPrintf(PalAllocator* pAllocator);
    void Init(const Device* pDevice);
    void Reset(Device* pDevice);

    void BindPipeline(
        Device*          pDevice,
        const Pipeline*  pPipeline,
        uint32_t         deviceIdx,
        Pal::ICmdBuffer* pCmdBuffer,
        uint32_t         bindPoint,
        uint32_t         userDataOffset);

    void PreQueueSubmit(Device* pDevice, uint32_t deviceIdx);
    Pal::Result PostQueueProcess(Device* pDevice, uint32_t deviceIdx);
    static void PostQueueSubmit(Device* pDevice, VkCommandBuffer* pCmdBuffers, uint32_t cmdBufferCount);

    static void DecodeFormatStringsFromElf(
        const Device*    pDevice,
        uint32_t         code,
        const char*      pCode,
        PrintfFormatMap* pFormatStrings);

    static uint32_t ConvertVkPipelineType(uint32_t);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DebugPrintf);
    void ParseFormatStringsToSubSection(const PrintfString& formatString, PrintfSubSection* pOutputSections);
    void ParseSpecifier(const std::cmatch& matchedFormatString, SubStrSection* pSection);

    void DecodeSpecifier(
        const PrintfString& formatString,
        const uint64_t&     outputVar,
        bool                is64bit,
        PrintfSubSection*   pSections,
        uint32_t            varidx,
        PrintfString*       pOutString);

    void OutputBufferString(
        const PrintfString&     formatString,
        const PrintfSubSection& subSections,
        PrintfString*           pOutputStr);

    void WriteToFile(const PrintfString& outputBuffer);

    PrintfString GetFileName(
        uint64_t    pipelineHash,
        uint32_t    pipelineType,
        uint32_t    frameNumber,
        const char* dumpFolder);

    DebugPrintfState        m_state;
    InternalMemory          m_printfMemory;
    const Pipeline*         m_pPipeline;
    const RuntimeSettings*  m_pSettings;
    PrintfSubsectMap        m_parsedFormatStrings;
    uint32_t                m_frame;
    Util::Mutex             m_mutex;
    PalAllocator*           m_pAllocator;
};
}
#endif
