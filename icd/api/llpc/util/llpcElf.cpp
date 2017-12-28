/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcElf.cpp
 * @brief LLPC source file: contains implementation of LLPC ELF utilities.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-elf"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llpcElf.h"

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
template<class Elf>
ElfWriter<Elf>::ElfWriter()
    :
    m_header(),
    m_null(),
    m_shStrTab(),
    m_strTab(),
    m_note(),
    m_symtab(),
    m_textPhdr(),
    m_dataPhdr(),
    m_rodataPhdr(),
    m_textSecIdx(InvalidValue),
    m_dataSecIdx(InvalidValue),
    m_rodataSecIdx(InvalidValue)
{
    m_header.e_ident32[EI_MAG0]     = ElfMagic;
    m_header.e_ident[EI_CLASS]      = (sizeof(typename Elf::FormatHeader) == sizeof(Elf32::FormatHeader)) ?
                                      ELFCLASS32 : ELFCLASS64;
    m_header.e_ident[EI_DATA]       = ELFDATA2LSB; // Little endian
    m_header.e_ident[EI_VERSION]    = 1; // ELF version number
    m_header.e_ident[EI_OSABI]      = Util::Abi::ElfOsAbiVersion;
    m_header.e_ident[EI_ABIVERSION] = Util::Abi::ElfAbiMajorVersion;

    m_header.e_type       = ET_DYN;
    m_header.e_entry      = 0;
    m_header.e_machine    = EM_AMDGPU;
    m_header.e_version    = 1;
    m_header.e_ehsize     = sizeof(typename Elf::FormatHeader);
    m_header.e_shentsize  = sizeof(typename Elf::SectionHeader);
    m_header.e_shnum      = ReservedSectionCount;  // NULL, .shstrtab, .note, .strtab, and .symtab sections
    m_header.e_shstrndx   = 1;    // .shstrtab is after the NULL section.
    m_header.e_flags      = 0;
    m_header.e_phentsize  = sizeof(typename Elf::Phdr);
    m_header.e_phnum      = 0;

    m_null.pName = const_cast<char*>("");

    m_shStrTab.secHead.sh_type   =  SHT_STRTAB;
    m_shStrTab.secHead.sh_flags  = SHF_STRINGS;
    m_shStrTab.secHead.sh_offset = sizeof(typename Elf::FormatHeader);
    m_shStrTab.pName = const_cast<char*>(&ShStrTabName[0]);

    m_note.secHead.sh_type = SHT_NOTE;
    m_note.secHead.sh_flags = 0;
    m_note.secHead.sh_addralign = 4;
    m_note.pName = const_cast<char*>(&NoteName[0]);

    m_strTab.secHead.sh_type = SHT_STRTAB;
    m_strTab.secHead.sh_flags = SHF_STRINGS;
    m_strTab.pName = const_cast<char*>(&StrTabName[0]);

    m_symtab.secHead.sh_type = SHT_SYMTAB;
    m_symtab.secHead.sh_addralign = 8;
    m_symtab.secHead.sh_entsize = sizeof(typename Elf::Symbol);
    m_symtab.secHead.sh_link = 3;
    m_symtab.pName = const_cast<char*>(&SymTabName[0]);

    m_sections.push_back(&m_null);
    m_sections.push_back(&m_shStrTab);
    m_sections.push_back(&m_note);
    m_sections.push_back(&m_strTab);
    m_sections.push_back(&m_symtab);

    m_textPhdr.p_type = PT_LOAD;
    m_textPhdr.p_flags = PF_R | PF_X;
    m_textPhdr.p_align = 256;

    m_dataPhdr.p_type = PT_LOAD;
    m_dataPhdr.p_flags = PF_R | PF_W;
    m_dataPhdr.p_align = 32;

    m_rodataPhdr.p_type = PT_LOAD;
    m_rodataPhdr.p_flags = PF_R;
    m_rodataPhdr.p_align = 32;

    ElfSymbol undefSym = {};
    undefSym.pSymName = "";
    m_symbols.push_back(undefSym);
}

// =====================================================================================================================
template<class Elf>
ElfWriter<Elf>::~ElfWriter()
{
    delete[] m_shStrTab.pData;
    delete[] m_strTab.pData;
    delete[] m_note.pData;
    delete[] m_symtab.pData;

    for (size_t i = ReservedSectionCount; i < m_sections.size(); ++i)
    {
        auto pSection = m_sections[i];
       if (pSection != nullptr)
       {
            delete[] pSection->pName;
            delete[] pSection->pData;
            delete pSection;
       }
    }

    for (auto& note : m_notes)
    {
        delete[] note.pData;
        note.pData = nullptr;
    }

    // NOTE: The first symbol is a reserved one, clear its name to avoid memory free operation.
    m_symbols[0].pSymName = nullptr;

    for (auto& symbol : m_symbols)
    {
        delete[] symbol.pSecName;
        delete[] symbol.pSymName;
    }

    m_sections.clear();
}

// =====================================================================================================================
// Generates a new section header for the binary section, and then add it to the linked list.
template<class Elf>
Result ElfWriter<Elf>::AddBinarySection(
    const char* pName,         // [in] Name of the section to add
    const void* pData,         // [in] Pointer to the binary data to store
    size_t      dataLength,    // Length of the data buffer
    uint32_t*   pSecIndex)     //[out] Index of this section
{
    LLPC_ASSERT(pName != nullptr);
    LLPC_ASSERT(pData != nullptr);
    LLPC_ASSERT(dataLength > 0);

    Result result = Result::Success;

    auto pSectionBuf = new ElfWriteSectionBuffer<typename Elf::SectionHeader>;
    result = (pSectionBuf != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        memset(pSectionBuf, 0, sizeof(ElfWriteSectionBuffer<typename Elf::SectionHeader>));
        // Extra space for the null terminator
        const size_t nameLen = strlen(pName) + 1;
        pSectionBuf->pName = new char[nameLen];

        result = (pSectionBuf->pName != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        pSectionBuf->pData = new uint8_t[dataLength];
        result = (pSectionBuf->pData != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        strcpy(pSectionBuf->pName, pName);
        memcpy(pSectionBuf->pData, pData, dataLength);

        pSectionBuf->secHead.sh_size      = static_cast<uint32_t>(dataLength);
        pSectionBuf->secHead.sh_type      = SHT_PROGBITS;
        pSectionBuf->secHead.sh_addralign = 1;

        if (strcmp(TextName, pName) == 0)
        {
            pSectionBuf->secHead.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
            pSectionBuf->secHead.sh_addralign = 256;
            m_textSecIdx = m_header.e_shnum;
            ++m_header.e_phnum;
        }
        else if (strcmp(DataName, pName) == 0)
        {
            pSectionBuf->secHead.sh_flags = SHF_ALLOC | SHF_WRITE;
            pSectionBuf->secHead.sh_addralign = 32;
            m_dataSecIdx = m_header.e_shnum;
            ++m_header.e_phnum;
        }
        else if (strcmp(RoDataName, pName) == 0)
        {
            pSectionBuf->secHead.sh_flags = SHF_ALLOC;
            pSectionBuf->secHead.sh_addralign = 32;
            m_rodataSecIdx = m_header.e_shnum;
            ++m_header.e_phnum;
        }

        m_sections.push_back(pSectionBuf);
        *pSecIndex = m_header.e_shnum++;
    }

    return result;
}

// =====================================================================================================================
// Adds one ELF note to the note list.
template<class Elf>
void ElfWriter<Elf>::AddNote(
    Util::Abi::PipelineAbiNoteType    type,      // Note type
    uint32_t                          descSize,  // The size of note description
    const void*                       pDesc)     // [in] Note description
{
    ElfNote note = {};

    static_assert(sizeof(note.hdr.name) == sizeof(Util::Abi::AmdGpuVendorName), "");
    static_assert(sizeof(note.hdr) % 4 == 0, "");
    LLPC_ASSERT(descSize % 4 == 0);

    // Fill the note's header
    note.hdr.nameSize = sizeof(Util::Abi::AmdGpuVendorName);
    note.hdr.descSize = descSize;
    note.hdr.type = type;
    memcpy(note.hdr.name, Util::Abi::AmdGpuVendorName, sizeof(Util::Abi::AmdGpuVendorName));

    // Copy note content
    note.pData = new uint32_t[descSize / 4];
    memcpy(note.pData, pDesc, descSize);

    // Add note to list
    m_notes.push_back(note);
}

// =====================================================================================================================
// Adds one ELF symbol to the symbol list.
template<class Elf>
void ElfWriter<Elf>::AddSymbol(
    ElfSymbol* pSymbol)    // [in] ELF symbol
{
     ElfSymbol symbol;
     symbol = *pSymbol;
     if (pSymbol->pSecName != nullptr)
     {
         // NOTE: Section name "pSecName" is only used in read ELF symbol, to insert ELF symbol, client must set
         // section index explicitly.
         LLPC_NEVER_CALLED();
     }

     if (pSymbol->pSymName)
     {
         char* pSymName = new char[strlen(pSymbol->pSymName) + 1];
         strcpy(pSymName, pSymbol->pSymName);
         symbol.pSymName = pSymName;
     }

     m_symbols.push_back(symbol);
}

// =====================================================================================================================
// Determines the size needed for a memory buffer to store this ELF.
template<class Elf>
size_t ElfWriter<Elf>::GetRequiredBufferSizeBytes()
{
    // Update offsets and size values
    CalcReservedSectionSize();
    CalcSectionHeaderOffset();

    size_t totalBytes = sizeof(typename Elf::FormatHeader);

    // Iterate through the section list
    for (auto pSection : m_sections)
    {
        totalBytes += pSection->secHead.sh_size;
    }

    totalBytes += m_header.e_shentsize * m_header.e_shnum;
    totalBytes += m_header.e_phentsize * m_header.e_phnum;

    return totalBytes;
}

// =====================================================================================================================
// Calculates total required size that is reserved to add expected sections in the future.
template<class Elf>
void ElfWriter<Elf>::CalcReservedSectionSize()
{
    // Calculate size for .shstrtab
    uint32_t dataLen = 0;
    for (auto pSection : m_sections)
    {
        dataLen += strlen(pSection->pName) + 1;
    }
    dataLen += 1; // Final null terminator
    m_shStrTab.secHead.sh_size = dataLen;

    // Calculate size for .strtab
    dataLen = 0;
    for (auto& sym : m_symbols)
    {
        dataLen += strlen(sym.pSymName) + 1;
    }
    dataLen += 1; // Final null terminator
    m_strTab.secHead.sh_size = dataLen;

    // Calculate size for .note
    dataLen = 0;
    const uint32_t noteHeaderSize = sizeof(NoteHeader);
    for (auto& note : m_notes)
    {
        dataLen += noteHeaderSize;
        dataLen += note.hdr.descSize;
    }
    m_note.secHead.sh_size = dataLen;

    // Calculate size for .symtab
    dataLen = m_symbols.size() * sizeof(typename Elf::Symbol);
    m_symtab.secHead.sh_size = dataLen;

}
// =====================================================================================================================
// Assembles the names of sections into a buffer and stores the size in the ".shstrtab" section header. Each section
// header stores the offset to its name string into the shared string table in its "secHead.sh_name" field.
template<class Elf>
void ElfWriter<Elf>::AssembleSharedStringTable()
{
    // Assemble .shstrtab
    LLPC_ASSERT(m_shStrTab.pData == nullptr);
    LLPC_ASSERT(m_shStrTab.secHead.sh_size > 0);

    char* pShStrTabString = new char[m_shStrTab.secHead.sh_size];
    LLPC_ASSERT(pShStrTabString != nullptr);

    char* pStringPtr = &pShStrTabString[0];

    for (auto pSection : m_sections)
    {
        strcpy(pStringPtr, pSection->pName);
        pSection->secHead.sh_name = static_cast<uint32_t>(pStringPtr - pShStrTabString);
        pStringPtr += strlen(pSection->pName) + 1;
    }

    *pStringPtr++ = 0; // Table ends with a double null terminator

    LLPC_ASSERT(m_shStrTab.secHead.sh_size == static_cast<uint32_t>(pStringPtr - pShStrTabString));
    m_shStrTab.pData = reinterpret_cast<uint8_t*>(pShStrTabString);

    // Assemble .strtab
    LLPC_ASSERT(m_strTab.pData == nullptr);
    LLPC_ASSERT(m_strTab.secHead.sh_size > 0);

    char* pStrTabString = new char[m_strTab.secHead.sh_size];
    pStringPtr = &pStrTabString[0];

    for (auto& sym : m_symbols)
    {
        strcpy(pStringPtr, sym.pSymName);
        sym.nameOffset = static_cast<uint32_t>(pStringPtr - pStrTabString);
        pStringPtr += strlen(sym.pSymName) + 1;
    }

    *pStringPtr++ = 0; // Table ends with a double null terminator

    LLPC_ASSERT(m_strTab.secHead.sh_size == static_cast<uint32_t>(pStringPtr - pStrTabString));
    m_strTab.pData = reinterpret_cast<uint8_t*>(pStrTabString);
}

// =====================================================================================================================
// Assembles ELF notes and add them to .note section
template<class Elf>
void ElfWriter<Elf>::AssembleNotes()
{
    LLPC_ASSERT(m_note.pData == nullptr);
    LLPC_ASSERT(m_note.secHead.sh_size > 0);
    const uint32_t noteHeaderSize = sizeof(NoteHeader);
    m_note.pData = new uint8_t[m_note.secHead.sh_size   + 16];
    uint8_t* pData = m_note.pData;
    for (auto& note : m_notes)
    {
        memcpy(pData, &note.hdr, noteHeaderSize);
        pData += noteHeaderSize;
        memcpy(pData, note.pData, note.hdr.descSize);
        pData += note.hdr.descSize;
    }
    LLPC_ASSERT(m_note.secHead.sh_size == static_cast<uint32_t>(pData - m_note.pData));
}

// =====================================================================================================================
// Assembles ELF symbols and symbol info to .symtab section
template<class Elf>
void ElfWriter<Elf>::AssembleSymbols()
{
    LLPC_ASSERT(m_symtab.pData == nullptr);
    LLPC_ASSERT(m_symtab.secHead.sh_size > 0);

    m_symtab.pData = new uint8_t[m_symtab.secHead.sh_size];
    auto pSymbol = reinterpret_cast<typename Elf::Symbol*>(m_symtab.pData);

    for (auto& symbol : m_symbols)
    {
        pSymbol->st_name  = symbol.nameOffset;
        pSymbol->st_info  = 0;
        pSymbol->st_other = 0;
        pSymbol->st_shndx = symbol.secIdx;
        pSymbol->st_value = symbol.value;
        pSymbol->st_size  = symbol.size;
        ++pSymbol;
    }

    LLPC_ASSERT(m_symtab.secHead.sh_size ==
                static_cast<uint32_t>(reinterpret_cast<uint8_t*>(pSymbol) - m_symtab.pData));
}

// =====================================================================================================================
// Determines the offset of the section header table by totaling the sizes of each binary chunk written to the ELF file,
// accounting for alignment.
template<class Elf>
void ElfWriter<Elf>::CalcSectionHeaderOffset()
{
    uint32_t sharedHdrOffset = 0;

    const uint32_t elfHdrSize = sizeof(typename Elf::FormatHeader);
    const uint32_t secHdrSize = sizeof(typename Elf::SectionHeader);
    const uint32_t pHdrSize = sizeof(typename Elf::Phdr);

    sharedHdrOffset += elfHdrSize;
    sharedHdrOffset += m_header.e_phnum * pHdrSize;

    for (auto pSection : m_sections)
    {
        const uint32_t secSzBytes = pSection->secHead.sh_size;
        sharedHdrOffset += secSzBytes;
    }

    m_header.e_phoff = m_header.e_phnum > 0 ? elfHdrSize : 0;
    m_header.e_shoff = sharedHdrOffset;
}

// =====================================================================================================================
// Writes the data out to the given buffer in ELF format. Assumes the buffer has been pre-allocated with adequate
// space, which can be determined with a call to "GetRequireBufferSizeBytes()".
//
// ELF data is stored in the buffer like so:
//
//
// + ELF header
// + Section Buffer (b0) [NULL]
// + Section Buffer (b1) [.shstrtab]
// + Section Buffer (b2) [.note]
// + Section Buffer (b3) [.strtab]
// + Section Buffer (b4) [.symtab]
// + ...            (b#) [???]
//
// + Section Header (h0) [NULL]
// + Section Header (h1) [.shstrtab]
// + Section Header (h2) [.note]
// + Section Header (h3) [.strtab]
// + Section Header (h4) [.symtab]
// + Section Header (h#) [???]
//
// + Program Segments (p0) [.text]
// + Program Segments (p1) [???]

template<class Elf>
void ElfWriter<Elf>::WriteToBuffer(
    char*  pBuffer,   // [in] Output buffer to write ELF data
    size_t bufSize)   // [in] Size of the given write buffer
{
    LLPC_ASSERT(pBuffer != nullptr);

    const size_t reqSize = GetRequiredBufferSizeBytes();
    LLPC_ASSERT(bufSize >= reqSize);

    // Update offsets and size values
    AssembleSharedStringTable();
    AssembleNotes();
    AssembleSymbols();

    memset(pBuffer, 0, reqSize);

    char* pWrite = static_cast<char*>(pBuffer);

    // ELF header comes first
    const uint32_t elfHdrSize = sizeof(typename Elf::FormatHeader);
    memcpy(pWrite, &m_header, elfHdrSize);
    pWrite += elfHdrSize;

    // Skip program header table, since the section offset isn't calculated yet
    LLPC_ASSERT(m_header.e_phoff == elfHdrSize);
    const uint32_t phdrSize = sizeof(typename Elf::Phdr);
    pWrite += phdrSize * m_header.e_phnum;

    // Write each section buffer
    for (auto pSection : m_sections)
    {
        pSection->secHead.sh_offset = static_cast<uint32_t>(pWrite - pBuffer);
        const uint32_t sizeBytes = pSection->secHead.sh_size;
        memcpy(pWrite, pSection->pData, sizeBytes);
        pWrite += sizeBytes;
    }

    LLPC_ASSERT(m_header.e_shoff == static_cast<uint32_t>(pWrite - pBuffer));

    const uint32_t secHdrSize = sizeof(typename Elf::SectionHeader);

    for (auto pSection : m_sections)
    {
        memcpy(pWrite, &pSection->secHead, secHdrSize);
        pWrite += secHdrSize;
    }

    LLPC_ASSERT((pWrite -pBuffer) == reqSize);

    // Add program header table
    pWrite = pBuffer + m_header.e_phoff;
    if (m_textSecIdx >= 0)
    {
        m_textPhdr.p_offset = m_sections[m_textSecIdx]->secHead.sh_offset;
        m_textPhdr.p_filesz = m_sections[m_textSecIdx]->secHead.sh_size;
        m_textPhdr.p_memsz = m_textPhdr.p_filesz;
        memcpy(pWrite, &m_textPhdr, phdrSize);
        pWrite += phdrSize;
    }

    if (m_dataSecIdx >= 0)
    {
        m_dataPhdr.p_offset = m_sections[m_dataSecIdx]->secHead.sh_offset;
        m_dataPhdr.p_filesz = m_sections[m_dataSecIdx]->secHead.sh_size;
        m_dataPhdr.p_memsz = m_dataPhdr.p_filesz;
        memcpy(pWrite, &m_dataPhdr, phdrSize);
        pWrite += phdrSize;
    }

    if (m_rodataSecIdx >= 0)
    {
        m_rodataPhdr.p_offset = m_sections[m_rodataSecIdx]->secHead.sh_offset;
        m_rodataPhdr.p_filesz = m_sections[m_rodataSecIdx]->secHead.sh_size;
        m_rodataPhdr.p_memsz = m_rodataPhdr.p_filesz;
        memcpy(pWrite, &m_rodataPhdr, phdrSize);
        pWrite += phdrSize;
    }
}

// =====================================================================================================================
template<class Elf>
ElfReader<Elf>::ElfReader(
    GfxIpVersion gfxIp) // Graphics IP version info
    :
    m_gfxIp(gfxIp),
    m_header(),
    m_symSecIdx(InvalidValue),
    m_relocSecIdx(InvalidValue),
    m_strtabSecIdx(InvalidValue)
{
}

// =====================================================================================================================
template<class Elf>
ElfReader<Elf>::~ElfReader()
{
    for (auto pSection : m_sections)
    {
        delete pSection;
    }
    m_sections.clear();
}

// =====================================================================================================================
// Reads ELF data in from the given buffer into the context.
//
// ELF data is stored in the buffer like so:
//
// + ELF header
// + Section Header String Table
//
// + Section Buffer (b0) [NULL]
// + Section Buffer (b1) [.shstrtab]
// + ...            (b#) [...]
//
// + Section Header (h0) [NULL]
// + Section Header (h1) [.shstrtab]
// + ...            (h#) [...]
template<class Elf>
Result ElfReader<Elf>::ReadFromBuffer(
    const void* pBuffer,   // [in] Input ELF data buffer
    size_t*     pBufSize)  // [out] Size of the given read buffer (determined from the ELF header)
{
    LLPC_ASSERT(pBuffer != nullptr);

    Result result = Result::Success;

    const uint8_t* pData = static_cast<const uint8_t*>(pBuffer);

    // ELF header is always located at the beginning of the file
    auto pHeader = static_cast<const typename Elf::FormatHeader*>(pBuffer);

    // If the identification info isn't the magic number, this isn't a valid file.
    result = (pHeader->e_ident32[EI_MAG0] == ElfMagic) ?  Result::Success : Result::ErrorInvalidValue;

    if (result == Result::Success)
    {
        result = (pHeader->e_machine == EM_AMDGPU) ? Result::Success : Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        m_header = *pHeader;
        size_t readSize = sizeof(typename Elf::FormatHeader);

        // Section header location information.
        const uint32_t sectionHeaderOffset = pHeader->e_shoff;
        const uint32_t sectionHeaderNum    = pHeader->e_shnum;
        const uint32_t sectionHeaderSize   = pHeader->e_shentsize;

        const uint32_t sectionStrTableHeaderOffset = sectionHeaderOffset + (pHeader->e_shstrndx * sectionHeaderSize);
        auto pSectionStrTableHeader = reinterpret_cast<const typename Elf::SectionHeader*>(pData + sectionStrTableHeaderOffset);
        const uint32_t sectionStrTableOffset = pSectionStrTableHeader->sh_offset;

        for (uint32_t section = 0; section < sectionHeaderNum; section++)
        {
            // Where the header is located for this section
            const uint32_t sectionOffset = sectionHeaderOffset + (section * sectionHeaderSize);
            auto pSectionHeader = reinterpret_cast<const typename Elf::SectionHeader*>(pData + sectionOffset);
            readSize += sizeof(typename Elf::SectionHeader);

            // Where the name is located for this section
            const uint32_t sectionNameOffset = sectionStrTableOffset + pSectionHeader->sh_name;
            const char* pSectionName = reinterpret_cast<const char*>(pData + sectionNameOffset);

            // Where the data is located for this section
            const uint32_t sectionDataOffset = pSectionHeader->sh_offset;
            auto pBuf =  new ElfReadSectionBuffer<typename Elf::SectionHeader>;

            result = (pBuf != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

            if (result == Result::Success)
            {
                pBuf->secHead = *pSectionHeader;
                pBuf->pName   = pSectionName;
                pBuf->pData   = (pData + sectionDataOffset);

                readSize += pSectionHeader->sh_size;

                m_sections.push_back(pBuf);
                m_map[pSectionName] = section;
            }
        }

        *pBufSize = readSize;
    }

    // Get section index
    m_symSecIdx    = GetSectionIndex(SymTabName);
    m_relocSecIdx  = GetSectionIndex(RelocName);
    m_strtabSecIdx = GetSectionIndex(StrTabName);

    return result;
}

// =====================================================================================================================
// Retrieves the section data for the specified section name, if it exists.
template<class Elf>
Result ElfReader<Elf>::GetSectionData(
    const char*  pName,       // [in] Name of the section to look for
    const void** pData,       // [out] Pointer to section data
    size_t*      pDataLength  // [out] Size of the section data
    ) const
{
    Result result = Result::ErrorInvalidValue;

    auto pEntry = m_map.find(pName);

    if (pEntry != m_map.end())
    {
        *pData = m_sections[pEntry->second]->pData;
        *pDataLength = m_sections[pEntry->second]->secHead.sh_size;
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Gets the count of symbols in the symbol table section.
template<class Elf>
uint32_t ElfReader<Elf>::GetSymbolCount()
{
    uint32_t symCount = 0;
    if (m_symSecIdx >= 0)
    {
        auto& pSection = m_sections[m_symSecIdx];
        symCount = pSection->secHead.sh_size / pSection->secHead.sh_entsize;
    }
    return symCount;
}

// =====================================================================================================================
// Gets info of the symbol in the symbol table section according to the specified index.
template<class Elf>
void ElfReader<Elf>::GetSymbol(
    uint32_t   idx,       // Symbol index
    ElfSymbol* pSymbol)   // [out] Info of the symbol
{
    auto& pSection = m_sections[m_symSecIdx];
    const char* pStrTab = reinterpret_cast<const char*>(m_sections[m_strtabSecIdx]->pData);

    auto symbols = reinterpret_cast<const typename Elf::Symbol*>(pSection->pData);
    pSymbol->secIdx   = symbols[idx].st_shndx;
    pSymbol->pSecName = m_sections[pSymbol->secIdx]->pName;
    pSymbol->pSymName = pStrTab + symbols[idx].st_name;
    pSymbol->size     = symbols[idx].st_size;
    pSymbol->value    = symbols[idx].st_value;
}

// =====================================================================================================================
// Gets the count of relocations in the relocation section.
template<class Elf>
uint32_t ElfReader<Elf>::GetRelocationCount()
{
    uint32_t relocCount = 0;
    if (m_relocSecIdx >= 0)
    {
        auto& pSection = m_sections[m_relocSecIdx];
        relocCount = pSection->secHead.sh_size / pSection->secHead.sh_entsize;
    }
    return relocCount;
}

// =====================================================================================================================
// Gets info of the relocation in the relocation section according to the specified index.
template<class Elf>
void ElfReader<Elf>::GetRelocation(
    uint32_t  idx,      // Relocation index
    ElfReloc* pReloc)   // [out] Info of the relocation
{
    auto& pSection = m_sections[m_relocSecIdx];

    auto relocs = reinterpret_cast<const typename Elf::Reloc*>(pSection->pData);
    pReloc->offset = relocs[idx].r_offset;
    pReloc->symIdx = relocs[idx].r_symbol;
}

// =====================================================================================================================
// Gets the count of Elf section.
template<class Elf>
uint32_t ElfReader<Elf>::GetSectionCount()
{
    return m_sections.size();
}

// =====================================================================================================================
// Gets section data by section index.
template<class Elf>
Result ElfReader<Elf>::GetSectionDataBySectionIndex(
    uint32_t           secIdx,          // Section index
    ElfSectionBuffer** ppSectionData    // [out] Section data
    ) const
{
    Result result = Result::ErrorInvalidValue;
    if (secIdx < m_sections.size())
    {
        *ppSectionData = m_sections[secIdx];
        result = Result::Success;
    }
    return result;
}

// =====================================================================================================================
// Gets all associated symbols by section index.
template<class Elf>
void ElfReader<Elf>::GetSymbolsBySectionIndex(
    uint32_t                secIdx,         // Section index
    std::vector<ElfSymbol>& secSymbols)     // [out] ELF symbols
{
    if ((secIdx < m_sections.size()) && (m_symSecIdx >= 0))
    {
        auto& pSection = m_sections[m_symSecIdx];
        const char* pStrTab = reinterpret_cast<const char*>(m_sections[m_strtabSecIdx]->pData);

        auto symbols = reinterpret_cast<const typename Elf::Symbol*>(pSection->pData);
        uint32_t symCount = GetSymbolCount();
        ElfSymbol symbol = {};

        for (uint32_t idx = 0; idx < symCount; ++idx)
        {
            if (symbols[idx].st_shndx == secIdx)
            {
                symbol.secIdx   = symbols[idx].st_shndx;
                symbol.pSecName = m_sections[symbol.secIdx]->pName;
                symbol.pSymName = pStrTab + symbols[idx].st_name;
                symbol.size     = symbols[idx].st_size;
                symbol.value    = symbols[idx].st_value;

                secSymbols.push_back(symbol);
            }
        }

        sort(secSymbols.begin(), secSymbols.end(),
             [](const ElfSymbol& a, const ElfSymbol& b)
             {
                 return a.value < b.value;
             });
    }
}

// Explicit instantiations for ELF utilities
template class ElfWriter<Elf64>;
template class ElfReader<Elf64>;

} // Llpc
