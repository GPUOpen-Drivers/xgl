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
#include "debug_printf.h"
#include "vk_utils.h"
#include "vk_device.h"
#include "vk_cmdbuffer.h"
#include "palPipelineAbi.h"
#include "palPipelineAbiReader.h"
#include "palVectorImpl.h"
#include "palHashBaseImpl.h"
#include "palSysMemory.h"
#include <cinttypes>

using namespace vk;

//=====================================================================================================================
DebugPrintf::DebugPrintf(
    PalAllocator* pAllocator)
    :
    m_state(Uninitialized),
    m_pPipeline(nullptr),
    m_pSettings(nullptr),
    m_parsedFormatStrings(8, pAllocator),
    m_frame(0),
    m_pAllocator(pAllocator)
{
}

//=====================================================================================================================
// Reset DebugPrintf
void DebugPrintf::Reset(
    Device* pDevice)
{
    if ((m_state == MemoryAllocated) && (m_printfMemory.Size() > 0))
    {
        pDevice->MemMgr()->FreeGpuMem(&m_printfMemory);
        m_state = Enabled;
    }
}

//=====================================================================================================================
// Append the string to the output string
void AppendPrintfString(
    PrintfString* pOutput,
    const char*   pSrc,
    unsigned      count)
{
    const char* pPtr = pSrc;
    for (unsigned i = 0; i < count; ++i)
    {
        pOutput->PushBack(*pPtr++);
    }
}

//=====================================================================================================================
// Bind debugprintf to pipeline and create memory
void DebugPrintf::BindPipeline(
    Device*          pDevice,
    const Pipeline*  pPipeline,
    uint32_t         deviceIdx,
    Pal::ICmdBuffer* pCmdBuffer,
    uint32_t         bindPoint,
    uint32_t         userDataOffset)
{
    uint64_t tableVa = 0;
    if ((m_state == Enabled) && (pPipeline->GetFormatStrings().GetNumEntries() > 0))
    {
        const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
        InternalMemCreateInfo allocInfo = {};
        allocInfo.pal.size = Util::Pow2Align(settings.debugPrintfBufferSize, PAL_PAGE_BYTES);
        allocInfo.pal.alignment = PAL_PAGE_BYTES;
        allocInfo.pal.priority = Pal::GpuMemPriority::Normal;
        pDevice->MemMgr()->GetCommonPool(InternalPoolDebugCpuRead, &allocInfo);
        VkResult result = pDevice->MemMgr()->AllocGpuMem(allocInfo,
                                                         &m_printfMemory,
                                                         pDevice->GetPalDeviceMask(),
                                                         VK_OBJECT_TYPE_DEVICE,
                                                         ApiDevice::IntValueFromHandle(
                                                         ApiDevice::FromObject(pDevice)));

        if (result == VK_SUCCESS)
        {
            m_state = MemoryAllocated;
            m_pPipeline = pPipeline;

            const size_t bufferSrdSize =
                pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.srdSizes.bufferView;
            void* pTable = pCmdBuffer->CmdAllocateEmbeddedData(
                bufferSrdSize, bufferSrdSize, &tableVa);

            Pal::BufferViewInfo srdInfo = {};
            srdInfo.gpuAddr = m_printfMemory.GpuVirtAddr(deviceIdx);
            srdInfo.range = m_printfMemory.Size();
            pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &srdInfo, pTable);
            m_frame = 0;
            const Pal::uint32* pEntry = reinterpret_cast<const Pal::uint32*>(&tableVa);
            pCmdBuffer->CmdSetUserData(static_cast<Pal::PipelineBindPoint>(bindPoint), userDataOffset, 1, pEntry);

            m_parsedFormatStrings.Reset();
            for (auto it = pPipeline->GetFormatStrings().Begin(); it.Get() != nullptr; it.Next())
            {
                bool found = true;
                PrintfSubSection* pSubSections = nullptr;
                m_parsedFormatStrings.FindAllocate(it.Get()->key, &found, &pSubSections);
                VK_ASSERT(found == false);
                pSubSections->Reserve(1);
                ParseFormatStringsToSubSection(it.Get()->value.printStr, pSubSections);
            }
        }
    }
}

// =====================================================================================================================
// Init the DebugPrintf
void DebugPrintf::Init(
    const Device* pDevice)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    if ((settings.enableDebugPrintf) && (m_state == Uninitialized))
    {
        m_state = Enabled;
        m_pPipeline = nullptr;
        m_frame = 0;
        m_pSettings = &settings;
        m_parsedFormatStrings.Init();

        if (m_pSettings->debugPrintfToStdout == false)
        {
            Util::MkDirRecursively(m_pSettings->debugPrintfDumpFolder);
        }
    }
}

// =====================================================================================================================
// PostQueue to process executed printf buffer
Pal::Result DebugPrintf::PostQueueProcess(
    Device*  pDevice,
    uint32_t deviceIdx)
{
    if (m_state != MemoryAllocated)
    {
        return Pal::Result::NotReady;
    }
    Util::MutexAuto lock(&m_mutex);
    pDevice->WaitIdle();

    void* pCpuAddr = nullptr;
    Pal::Result palResult = m_printfMemory.Map(deviceIdx, &pCpuAddr);
    uint64_t bufferSize = 0;
    uint32_t* pPrintBuffer = nullptr;
    uint32_t* pPtr = nullptr;
    constexpr uint32_t bufferHeaderSize = 4;
    uint64_t maxBufferDWSize = (m_printfMemory.Size() >> 2) - bufferHeaderSize;
    if (palResult == Pal::Result::Success)
    {
        // Buffer Header is 4 dword {BufferOffset_Loword, BufferOffset_Hiword, rerv0, rerv1};
        pPtr = static_cast<uint32_t*>(pCpuAddr);
        uint32_t bufferSizeLower = *pPtr++;
        uint32_t bufferSizeHigh = *pPtr++;
        pPtr += 2;
        bufferSize = (static_cast<uint64_t>(bufferSizeHigh) << 32) | static_cast<uint64_t>(bufferSizeLower);
        bufferSize = Util::Min(bufferSize, maxBufferDWSize);
        if (bufferSize > 0)
        {
            pPrintBuffer = static_cast<uint32_t*>(pDevice->VkInstance()->AllocMem(
                bufferSize * sizeof(uint32_t), 4, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));

            memcpy(pPrintBuffer, pPtr, bufferSize * 4);
        }

        m_printfMemory.Unmap(deviceIdx);

        if (bufferSize > 0)
        {
            const auto& formatStrings = m_pPipeline->GetFormatStrings();
            const uint32_t entryHeaderSize = 2;
            uint64_t decodeOffset = 0;
            PrintfString outputBufferStr(nullptr);
            outputBufferStr.Reserve(10);
            Vector<PrintfString, 5, GenericAllocator> outputDecodedSpecifiers(nullptr);
            outputDecodedSpecifiers.Reserve(5);
            // Set pPtr point to the head of the system memory
            pPtr = pPrintBuffer;
            while ((bufferSize - decodeOffset) > 1)
            {
                // Decode entry
                uint32_t entryHeaderLow = *pPtr++;
                uint32_t entryHeaderHigh = *pPtr++;
                uint64_t entryHeader = ((uint64_t)(entryHeaderHigh) << 32) | uint64_t(entryHeaderLow);
                // 64 bit header {[0:15], [16:63]} entrySize,hash value for the string
                uint64_t entryValuesSize = (entryHeader & 65535) - entryHeaderSize;
                uint64_t entryHashValue = entryHeader >> 16;

                decodeOffset += entryHeaderSize;
                // Check hash value in the entry valid and if there is space to decoded entry values
                auto pEntry = formatStrings.FindKey(entryHashValue);
                if ((pEntry == nullptr) || ((bufferSize - decodeOffset) < entryValuesSize))
                {
                    break;
                }

                const PrintfString& formatString = pEntry->printStr;
                const PrintfBit& bitPos = pEntry->bit64s;
                PrintfSubSection* pSubSections = m_parsedFormatStrings.FindKey(entryHashValue);
                int initSize = bitPos.size() - outputDecodedSpecifiers.size();
                for (int i = 0; i < initSize; ++i)
                {
                    outputDecodedSpecifiers.PushBack(nullptr);
                }

                // Get printf output variable in dword size
                unsigned outputsInDwords = 0;
                uint64_t outputVar;
                for (uint32_t varIndex = 0; varIndex < bitPos.size(); varIndex++)
                {
                    outputVar = *pPtr++;
                    outputsInDwords++;
                    bool is64bit = bitPos[varIndex];
                    if (is64bit)
                    {
                        uint64_t hiDword = *pPtr++;
                        outputVar = (hiDword << 32) | outputVar;
                        outputsInDwords++;
                    }

                    DecodeSpecifier(formatString,
                                    outputVar,
                                    is64bit,
                                    pSubSections,
                                    varIndex,
                                    &outputDecodedSpecifiers[varIndex]);
                }
                OutputBufferString(formatString, *pSubSections, &outputBufferStr);
                decodeOffset += outputsInDwords;
            }
            WriteToFile(outputBufferStr);
            pDevice->VkInstance()->FreeMem(pPrintBuffer);
            m_frame++;
        }
    }

    return palResult;
}

// =====================================================================================================================
// Write the outputBuffer to the file
void DebugPrintf::WriteToFile(
    const PrintfString& outputBuffer)
{
    if (outputBuffer.size() == 0)
    {
        return;
    }
    Util::File file;
    PrintfString fileName = GetFileName(m_pPipeline->PalPipelineHash(),
                                        ConvertVkPipelineType(m_pPipeline->GetType()),
                                        m_frame,
                                        m_pSettings->debugPrintfDumpFolder);
    const char* pOutputName = m_pSettings->debugPrintfToStdout ? "-" : fileName.Data();
    Util::Result result = file.Open(pOutputName, Util::FileAccessMode::FileAccessAppend);
    if (result == Util::Result::Success)
    {
        const char* fileBeginPrefix = "========================= ";
        const char* fileBeginPostfix =" Begin ========================\n";
        const char* fileEnd = "========================= Session End ========================\n";
        file.Write(fileBeginPrefix, strlen(fileBeginPrefix));
        file.Write(fileName.Data(), strlen(fileName.Data()));
        file.Write(fileBeginPostfix, strlen(fileBeginPostfix));
        result = file.Write(outputBuffer.Data(), outputBuffer.size());
        if (result == Util::Result::Success)
        {
            file.Write(fileEnd, strlen(fileEnd));
            file.Flush();
            file.Close();
        }
    }
}

// =====================================================================================================================
// Get output file name
PrintfString DebugPrintf::GetFileName(
    uint64_t    pipelineHash,
    uint32_t    pipelineType,
    uint32_t    frameNumber,
    const char* pDumpFolder)
{
    char fileName[255] = {};
    const char* strPipelineTypes[] = {"Cs", "Graphics"
#if VKI_RAY_TRACING
        , "Rays"
#endif
        };
    sprintf(fileName, "Pipeline%s_0x%016" PRIx64 "_%u", strPipelineTypes[pipelineType],
            pipelineHash, frameNumber);
    PrintfString fName(nullptr);
    fName.Reserve(10);

    if (m_pSettings->debugPrintfToStdout)
    {
        AppendPrintfString(&fName, fileName, strlen(fileName));
    }
    else
    {
        AppendPrintfString(&fName, pDumpFolder, strlen(pDumpFolder));
        AppendPrintfString(&fName, "/", 1);
        AppendPrintfString(&fName, fileName, strlen(fileName));
        AppendPrintfString(&fName, ".txt\0", 5);
    }
    return fName;
}

// =====================================================================================================================
// Convert the vulkan pipeline type the internal pipeline type
uint32_t DebugPrintf::ConvertVkPipelineType(
    uint32_t vkPipelineType)
{
    unsigned pipelineType = DebugPrintfCompute;
    switch (vkPipelineType)
    {
    case VK_PIPELINE_BIND_POINT_GRAPHICS:
        pipelineType = DebugPrintfGraphics;
        break;
    case VK_PIPELINE_BIND_POINT_COMPUTE:
        pipelineType = DebugPrintfCompute;
        break;
#if VKI_RAY_TRACING
    case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        pipelineType = DebugPrintfRayTracing;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        pipelineType = DebugPrintfCompute;
        break;
    }
    return pipelineType;
}

// =====================================================================================================================
// Classify the printf specifier
void DebugPrintf::DecodeSpecifier(
    const PrintfString& formatString,
    const uint64_t&     outputVar,
    bool                is64bit,
    PrintfSubSection*   pSections,
    uint32_t            varidx,
    PrintfString*       pOutString)
{

    if ((pSections->NumElements() == 0) || (varidx >= pSections->NumElements()))
    {
        return;
    }
    SubStrSection* pSection = &((*pSections)[varidx]);
    union
    {
        int iValue;
        float fValue;
        int64_t ilValue;
        double flValue;
    } value;

    pOutString->Resize(255, ' ');
    char specifierStr[255];
    strncpy(specifierStr, &formatString[pSection->beginPos], pSection->count);
    specifierStr[pSection->count] = '\0';
    if (is64bit)
    {
        switch (pSection->specifierType)
        {
        case SpecifierUnsigned:
            sprintf(pOutString->Data(), specifierStr, outputVar);
            break;
        case SpecifierInteger:
            value.ilValue = static_cast<int64_t>(outputVar);
            sprintf(pOutString->Data(), specifierStr, value.ilValue);
            break;
        case SpecifierFloat:
            value.flValue = *(reinterpret_cast<const double*>(&outputVar));
            sprintf(pOutString->Data(), specifierStr, value.flValue);
            break;
        default:
            VK_NEVER_CALLED();
            break;
        }
    }
    else
    {
        // trunc 64bit to 32bit
        uint32_t dwValue = static_cast<uint32_t>(outputVar);
        switch (pSection->specifierType)
        {
        case SpecifierUnsigned:
            sprintf(pOutString->Data(), specifierStr, dwValue);
            break;
        case SpecifierInteger:
            value.iValue = static_cast<int>(dwValue);
            sprintf(pOutString->Data(), specifierStr, value.iValue);
            break;
        case SpecifierFloat:
            value.fValue = *(reinterpret_cast<float*>(&dwValue));
            sprintf(pOutString->Data(), specifierStr, value.fValue);
            break;
        default:
            VK_NEVER_CALLED();
            break;
        }
    }
    pSection->decodedStr = pOutString->Data();
}

// =====================================================================================================================
// static function called after QueueSubmit
void DebugPrintf::PostQueueSubmit(
    Device*          pDevice,
    VkCommandBuffer* pCmdBuffers,
    uint32_t         cmdBufferCount)
{
    Pal::Result palResult = Pal::Result::Success;
    for (uint32_t deviceIdx = 0;
         deviceIdx < pDevice->NumPalDevices() && (palResult == Pal::Result::Success);
         deviceIdx++)
    {
        for (uint32_t j = 0; j < cmdBufferCount; ++j)
        {
            CmdBuffer* pCmdBuf = ApiCmdBuffer::ObjectFromHandle(pCmdBuffers[j]);
            palResult = pCmdBuf->GetDebugPrintf()->PostQueueProcess(pDevice, deviceIdx);
        }
    }
}

// =====================================================================================================================
// Function called before QueueSubmit
void DebugPrintf::PreQueueSubmit(
    Device*  pDevice,
    uint32_t deviceIdx)
{
    if (m_state != MemoryAllocated)
    {
        return;
    }
    Util::MutexAuto lock(&m_mutex);
    pDevice->WaitIdle();

    void* pCpuAddr = nullptr;

    if (m_printfMemory.Map(deviceIdx, &pCpuAddr) == Pal::Result::Success)
    {
        // Buffer Header is 4 dword {BufferOffset_Loword, BufferOffset_Hiword, rerv0, rerv1};
        memset(pCpuAddr, 0, sizeof(uint32_t) * 4);
        m_printfMemory.Unmap(deviceIdx);
    }
}

// =====================================================================================================================
// Assemble the sub sections to the complete the string
void DebugPrintf::OutputBufferString(
    const PrintfString&     formatString,
    const PrintfSubSection& subSections,
    PrintfString*           pOutputStr)
{
    if (subSections.size() == 0)
    {
        AppendPrintfString(pOutputStr, formatString.Data(), formatString.size());
        return;
    }
    auto it = subSections.begin();
    if (it->beginPos > 0)
    {
        AppendPrintfString(pOutputStr, formatString.Data(), it->beginPos);
    }
    AppendPrintfString(pOutputStr, it->decodedStr, strlen(it->decodedStr));
    auto lastIt = it;
    it++;
    for (; it != subSections.end(); ++it)
    {
        auto lastEndPos = lastIt->beginPos + lastIt->count;
        if (lastEndPos < it->beginPos)
        {
            //outputStr += formatString.substr(lastEndPos, it->beginPos - lastEndPos);
            AppendPrintfString(pOutputStr, formatString.Data() + lastEndPos, it->beginPos - lastEndPos);
        }
        AppendPrintfString(pOutputStr, it->decodedStr, strlen(it->decodedStr));
        lastIt = it;
    }

    const SubStrSection& lastSect = subSections[subSections.size() - 1];
    if (lastSect.beginPos + lastSect.count < formatString.size())
    {
        uint32_t lastBegin = lastSect.beginPos + lastSect.count;
        uint32_t lastCount = formatString.size() - lastBegin;
        // outputStr += formatString.substr(lastBegin, lastCount);
        AppendPrintfString(pOutputStr, formatString.Data() + lastBegin, lastCount);
    }
}

// =====================================================================================================================
// Parse the format string to the sub sections
void DebugPrintf::ParseFormatStringsToSubSection(
    const PrintfString& formatString,
    PrintfSubSection*   pOutputSections)
{
    // %[flag][width][Precision][vector][length][Specifier]
    static std::regex specifierPattern(
        "(%){1}[-+#0]*[0-9]*((.)[0-9]+){0,1}(v[2-4])*((h)+|(l)+|j|z|t|L)*([diuoxXfFeEgGaAc]){1}");

    const char* pStrBegin = formatString.Data();
    const char* pStrEnd = pStrBegin + formatString.NumElements();

    auto specifierBegin =
        std::cregex_iterator(pStrBegin, pStrEnd, specifierPattern);
    auto specifierEnd = std::cregex_iterator();
    if (specifierBegin != specifierEnd)
    {
        for (auto it = specifierBegin; it != specifierEnd; ++it)
        {
            SubStrSection section = {};
            ParseSpecifier(*it, &section);
            pOutputSections->PushBack(section);
        }
    }
}

// =====================================================================================================================
// Classify the specifier
void DebugPrintf::ParseSpecifier(
    const std::cmatch& matchedFormatString,
    SubStrSection*     pSection)
{
    const auto& formatStr = matchedFormatString.str();
    pSection->beginPos = matchedFormatString.position();
    pSection->count = matchedFormatString.length();
    char specifier = formatStr[formatStr.length() - 1];
    switch (specifier)
    {
    case 'd':
    case 'i':
    case 'c':
        pSection->specifierType = SpecifierInteger;
        break;

    case 'u':
    case 'o':
    case 'x':
    case 'X':
    {
        pSection->specifierType = SpecifierUnsigned;
        break;
    }
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
    {
        pSection->specifierType = SpecifierFloat;
        break;
    }
    default:
        VK_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Get elf meta data
const void* GetMetaData(
    const ElfReader::Notes& notes,
    uint32_t                noteType,
    uint32_t*               pDataLength)
{
    for (ElfReader::NoteIterator note = notes.Begin(); note.IsValid(); note.Next())
    {
        if (note.GetHeader().n_type == noteType)
        {
            *pDataLength = note.GetHeader().n_descsz;
            return note.GetDescriptor();
        }
    }
    return nullptr;
}

// =====================================================================================================================
// Retrieve the formatstring section from elf
void DebugPrintf::DecodeFormatStringsFromElf(
    const Device*    pDevice,
    uint32_t         code,
    const char*      pCode,
    PrintfFormatMap* pFormatStrings)
{
    Util::Abi::PipelineAbiReader abiReader(pDevice->VkInstance()->Allocator(), pCode);
    auto& elfReader = abiReader.GetElfReader();
    auto noteId = abiReader.GetElfReader().FindSection(".note");
    auto& noteSection = abiReader.GetElfReader().GetSection(noteId);
    VK_ASSERT(noteId != 0);
    VK_ASSERT(noteSection.sh_type == static_cast<uint32_t>(Elf::SectionHeaderType::Note));
    ElfReader::Notes notes(elfReader, noteId);
    unsigned noteLength = 0;
    auto noteData = GetMetaData(notes, Abi::MetadataNoteType, &noteLength);
    MsgPackReader docReader;
    Result result = docReader.InitFromBuffer(noteData, noteLength);
    VK_ASSERT(docReader.Type() == CWP_ITEM_MAP);
    const auto hashFormatStr = HashLiteralString("amdpal.format_strings");
    const auto hashIndex = HashLiteralString(".index");
    const auto hashString = HashLiteralString(".string");
    const auto hashVarsCount = HashLiteralString(".argument_count");
    const auto hashBitsPos = HashLiteralString(".64bit_arguments");
    const auto hashStrings = HashLiteralString(".strings");

    Util::StringView<char> key;
    uint32_t palmetaSize = docReader.Get().as.map.size;
    for (uint32 i = 0; i < palmetaSize; ++i)
    {
        result = docReader.Next(CWP_ITEM_STR);
        const char* itemString = static_cast<const char*>(docReader.Get().as.str.start);
        if (Util::HashString(itemString, docReader.Get().as.str.length) == hashFormatStr)
        {
            result = docReader.Next(CWP_ITEM_MAP);
            VK_ASSERT(docReader.Get().as.map.size == 2);
            uint32_t formatStringsMap = docReader.Get().as.map.size;
            for (uint32 j = 0; j < formatStringsMap; ++j)
            {
                result = docReader.UnpackNext(&key);
                itemString = static_cast<const char*>(docReader.Get().as.str.start);
                if (Util::HashString(key) == hashStrings)
                {
                    result = docReader.Next(CWP_ITEM_ARRAY);
                    uint32_t stringsSize = docReader.Get().as.array.size;
                    for (uint32 k = 0; k < stringsSize; ++k)
                    {
                        result = docReader.Next(CWP_ITEM_MAP);
                        uint64_t hashValue = 0;
                        uint64_t outputCount = 0;
                        StringView<char> formatString;
                        Vector<uint64_t, 4, GenericAllocator> bitPos(nullptr);
                        uint32_t stringMap = docReader.Get().as.map.size;
                        for (uint32 l = 0; l < stringMap; ++l)
                        {
                            result = docReader.UnpackNext(&key);
                            auto hashKey = Util::HashString(key);
                            switch (hashKey)
                            {
                            case hashIndex:
                                docReader.UnpackNext(&hashValue);
                                break;
                            case hashString:
                                docReader.UnpackNext(&formatString);
                                break;
                            case hashVarsCount:
                                docReader.UnpackNext(&outputCount);
                                break;
                            default:
                            {
                                VK_ASSERT(hashKey == hashBitsPos);
                                docReader.UnpackNext(&bitPos);
                                break;
                            }
                            }
                        }
                        bool found = true;
                        PrintfElfString* pElfString = nullptr;
                        result = pFormatStrings->FindAllocate(hashValue, &found, &pElfString);
                        if ((result == Pal::Result::Success) && (found == false))
                        {
                            pElfString->printStr.Reserve(formatString.Length());
                            for (auto& elem : formatString)
                            {
                                pElfString->printStr.PushBack(elem);
                            }
                            pElfString->bit64s.Reserve(outputCount);
                            for (uint32 bitIndex = 0; bitIndex < outputCount; ++bitIndex)
                            {
                                bool bitValue = (bitPos[bitIndex / 64] >> (bitIndex % 64)) & 1;
                                pElfString->bit64s.PushBack(bitValue);
                            }
                        }
                    }
                }
                else
                {
                    docReader.Skip(1);
                }
            }
        }
        else
        {
            docReader.Skip(1);
        }
    }
}
