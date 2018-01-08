/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcElf.h
 * @brief LLPC header file: contains declaration of LLPC ELF utilities.
 ***********************************************************************************************************************
 */
#pragma once

#include "llvm/ADT/SmallString.h"

#include <unordered_map>
#include <string>
#include <vector>
#include "llpc.h"
#include "llpcDebug.h"
#include "llpcInternal.h"
#include "palPipelineAbi.h"

namespace Llpc
{

// Represents pseudo hardware registers.
struct HwReg
{
    uint32_t u32All;  // 32-bit register value
};

// Defines PAL metadata entries based on the specified name and type of this metadata
#define DEF_META(_name, _type) \
typedef HwReg reg##_name;  \
static const uint32_t mm##_name(Util::Abi::PipelineMetadataBase | \
                                static_cast<uint32_t>(Util::Abi::PipelineMetadataType::_type));

DEF_META(API_VS_HASH_LO, ApiVsHashDword0)
DEF_META(API_VS_HASH_HI, ApiVsHashDword1)
DEF_META(API_HS_HASH_LO, ApiHsHashDword0)
DEF_META(API_HS_HASH_HI, ApiHsHashDword1)
DEF_META(API_DS_HASH_LO, ApiDsHashDword0)
DEF_META(API_DS_HASH_HI, ApiDsHashDword1)
DEF_META(API_GS_HASH_LO, ApiGsHashDword0)
DEF_META(API_GS_HASH_HI, ApiGsHashDword1)
DEF_META(API_PS_HASH_LO, ApiPsHashDword0)
DEF_META(API_PS_HASH_HI, ApiPsHashDword1)
DEF_META(API_CS_HASH_LO, ApiCsHashDword0)
DEF_META(API_CS_HASH_HI, ApiCsHashDword1)
DEF_META(PIPELINE_HASH_LO, PipelineHashLo)
DEF_META(PIPELINE_HASH_HI, PipelineHashHi)
DEF_META(USER_DATA_LIMIT, UserDataLimit)
DEF_META(HS_MAX_TESS_FACTOR, HsMaxTessFactor)
DEF_META(PS_USES_UAVS, PsUsesUavs)
DEF_META(PS_USES_ROVS, PsUsesRovs)
DEF_META(PS_RUNS_AT_SAMPLE_RATE, PsRunsAtSampleRate)
DEF_META(SPILL_THRESHOLD, SpillThreshold)
DEF_META(LS_NUM_USED_VGPRS, LsNumUsedVgprs)
DEF_META(HS_NUM_USED_VGPRS, HsNumUsedVgprs)
DEF_META(ES_NUM_USED_VGPRS, EsNumUsedVgprs)
DEF_META(GS_NUM_USED_VGPRS, GsNumUsedVgprs)
DEF_META(VS_NUM_USED_VGPRS, VsNumUsedVgprs)
DEF_META(PS_NUM_USED_VGPRS, PsNumUsedVgprs)
DEF_META(CS_NUM_USED_VGPRS, CsNumUsedVgprs)
DEF_META(LS_NUM_USED_SGPRS, LsNumUsedSgprs)
DEF_META(HS_NUM_USED_SGPRS, HsNumUsedSgprs)
DEF_META(ES_NUM_USED_SGPRS, EsNumUsedSgprs)
DEF_META(GS_NUM_USED_SGPRS, GsNumUsedSgprs)
DEF_META(VS_NUM_USED_SGPRS, VsNumUsedSgprs)
DEF_META(PS_NUM_USED_SGPRS, PsNumUsedSgprs)
DEF_META(CS_NUM_USED_SGPRS, CsNumUsedSgprs)
DEF_META(LS_SCRATCH_SIZE, LsScratchByteSize)
DEF_META(HS_SCRATCH_SIZE, HsScratchByteSize)
DEF_META(ES_SCRATCH_SIZE, EsScratchByteSize)
DEF_META(GS_SCRATCH_SIZE, GsScratchByteSize)
DEF_META(VS_SCRATCH_SIZE, VsScratchByteSize)
DEF_META(PS_SCRATCH_SIZE, PsScratchByteSize)
DEF_META(CS_SCRATCH_SIZE, CsScratchByteSize)
DEF_META(INDIRECT_TABLE_ENTRY, IndirectTableEntryLow)
DEF_META(USES_VIEWPORT_ARRAY_INDEX, UsesViewportArrayIndex)
DEF_META(API_HW_SHADER_MAPPING_LO, ApiHwShaderMappingLo)
DEF_META(API_HW_SHADER_MAPPING_HI, ApiHwShaderMappingHi)

// LLVM backend compiler pseudo hardware registers
#define mmSPILLED_SGPRS                         0x0001
#define mmSPILLED_VGPRS                         0x0002

// LLVM backend special section name
static const char   AmdGpuDisasmName[] = ".AMDGPU.disasm"; // Name of ".AMDGPU.disasm" section
static const char   AmdGpuCsdataName[] = ".AMDGPU.csdata"; // Name of ".AMDGPU.csdata" section
static const char   AmdGpuConfigName[] = ".AMDGPU.config"; // Name of ".AMDGPU.config" section

// Pal pipeline ABI debug symbol names
namespace DebugSymNames
{
    static const char LsDisasm[] = "_amdgpu_ls_disasm";
    static const char HsDisasm[] = "_amdgpu_hs_disasm";
    static const char EsDisasm[] = "_amdgpu_es_disasm";
    static const char GsDisasm[] = "_amdgpu_gs_disasm";
    static const char VsDisasm[] = "_amdgpu_vs_disasm";
    static const char PsDisasm[] = "_amdgpu_ps_disasm";
    static const char CsDisasm[] = "_amdgpu_cs_disasm";

    static const char LsCsdata[] = "_amdgpu_ls_csdata";
    static const char HsCsdata[] = "_amdgpu_hs_csdata";
    static const char EsCsdata[] = "_amdgpu_es_csdata";
    static const char GsCsdata[] = "_amdgpu_gs_csdata";
    static const char VsCsdata[] = "_amdgpu_vs_csdata";
    static const char PsCsdata[] = "_amdgpu_ps_csdata";
    static const char CsCsdata[] = "_amdgpu_cs_csdata";
};

// e_ident size and indices
enum
{
    EI_MAG0         = 0,      // File identification index
    EI_MAG1         = 1,      // File identification index
    EI_MAG2         = 2,      // File identification index
    EI_MAG3         = 3,      // File identification index
    EI_CLASS        = 4,      // File class
    EI_DATA         = 5,      // Data encoding
    EI_VERSION      = 6,      // File version
    EI_OSABI        = 7,      // OS/ABI identification
    EI_ABIVERSION   = 8,      // ABI version
    EI_PAD          = 9,      // Start of padding bytes
    EI_NIDENT       = 16      // Number of bytes in e_ident
};

// Object file classes
enum
{
    ELFCLASSNONE    = 0,      // Invalid object file
    ELFCLASS32      = 1,      // 32-bit object file
    ELFCLASS64      = 2,      // 64-bit object file
};

// Object file byte orderings
enum
{
    ELFDATANONE     = 0,      // Invalid data encoding
    ELFDATA2LSB     = 1,      // Little-endian object file
    ELFDATA2MSB     = 2,      // Big-endian object file
};

// Program header table type
enum
{
    PT_LOAD         = 1,     // Loadable segment
};

// Machine architectures
enum
{
    EM_AMDGPU       = 224,    // AMD GPU architecture
};

// Segment flag bits.
enum
{
    PF_X = 0x1,  // Execute
    PF_W = 0x2,  // Write
    PF_R = 0x4,  // Read
};

// ELF file type
enum
{
    ET_DYN     = 3, // Shared object file
};

// Enumerates ELF Constants from GNU readelf indicating section type.
enum ElfSectionHeaderTypes : uint32_t
{
    SHT_NULL     = 0,                // No associated section (inactive entry)
    SHT_PROGBITS = 1,                // Program-defined contents
    SHT_SYMTAB   = 2,                // Symbol table
    SHT_STRTAB   = 3,                // String table
    SHT_RELA     = 4,                // Relocation entries; explicit addends
    SHT_HASH     = 5,                // Symbol hash table
    SHT_DYNAMIC  = 6,                // Information for dynamic linking
    SHT_NOTE     = 7,                // Information about the file
};

// Enumerates ELF Section flags.
enum ElfSectionHeaderFlags : uint32_t
{
    SHF_WRITE       = 0x1,   // Section data should be writable during execution
    SHF_ALLOC       = 0x2,   // Section occupies memory during program execution
    SHF_EXECINSTR   = 0x4,   // Section contains executable machine instructions
    SHF_MERGE       = 0x10,  // The data in this section may be merged
    SHF_STRINGS     = 0x20,  // The data in this section is null-terminated strings
};

static const uint32_t ElfMagic         = 0x464C457F;   // "/177ELF" in little-endian

// Section names used in PAL pipeline and LLVM back-end compiler
static const char   TextName[]         = ".text";      // Name of ".text" section (GPU ISA codes)
static const char   DataName[]         = ".data";      // Name of ".data" section
static const char   RoDataName[]       = ".rodata";    // Name of ".rodata" section
static const char   ShStrTabName[]     = ".shstrtab";  // Name of ".shstrtab" section
static const char   StrTabName[]       = ".strtab";    // Name of ".strtab" section
static const char   SymTabName[]       = ".symtab";    // Name of ".symtab" section
static const char   NoteName[]         = ".note";      // Name of ".note" section
static const char   RelocName[]        = ".reloc";     // Name of ".reloc" section

// Represents the layout of standard note header
struct NoteHeader
{
    uint32_t                       nameSize;                                   // Byte size of note name
    uint32_t                       descSize;                                   // Descriptor size in byte
    Util::Abi::PipelineAbiNoteType type;                                       // Note type
    char                           name[sizeof(Util::Abi::AmdGpuVendorName)];  // Note name, include padding
};
static_assert(sizeof(Util::Abi::AmdGpuVendorName) == 4, "");

#pragma pack (push, 1)
// Represents the layout of 32-bit ELF
struct Elf32
{
    // ELF file header
    struct FormatHeader
    {
        union
        {
            uint8_t  e_ident[EI_NIDENT];        // ELF identification info
            uint32_t e_ident32[EI_NIDENT / 4];  // Bytes grouped for easy magic number setting
        };

        uint16_t  e_type;       // 1 = relocatable, 3 = shared
        uint16_t  e_machine;    // Machine architecture constant, 0x3FD = AMD GPU, 0xE0 = LLVM AMD GCN
        uint32_t  e_version;    // ELF format version (1)
        uint32_t  e_entry;      // Entry point if executable (0)
        uint32_t  e_phoff;      // File offset of program header (unused, 0)
        uint32_t  e_shoff;      // File offset of section header
        uint32_t  e_flags;      // Architecture-specific flags
        uint16_t  e_ehsize;     // Size of this ELF header
        uint16_t  e_phentsize;  // Size of an entry in program header (unused, 0)
        uint16_t  e_phnum;      // # of entries in program header (0)
        uint16_t  e_shentsize;  // Size of an entry in section header
        uint16_t  e_shnum;      // # of entries in section header
        uint16_t  e_shstrndx;   // Section # that contains section name strings
    };

    // ELF section header (used to locate each data section)
    struct SectionHeader
    {
        uint32_t  sh_name;      // Name (index into string table)
        uint32_t  sh_type;      // Section type
        uint32_t  sh_flags;     // Flag bits (SectionHeaderFlags enum)
        uint32_t  sh_addr;      // Base memory address if loadable (0)
        uint32_t  sh_offset;    // File position of start of section
        uint32_t  sh_size;      // Size of section in bytes
        uint32_t  sh_link;      // Section # with related info (unused, 0)
        uint32_t  sh_info;      // More section-specific info
        uint32_t  sh_addralign; // Alignment granularity in power of 2 (1)
        uint32_t  sh_entsize;   // Size of entries if section is array
    };

    // ELF symbol table entry
    struct Symbol
    {
        uint32_t  st_name;      // Symbol name (index into string table)
        uint32_t  st_value;     // Value or address associated with the symbol
        uint32_t  st_size;      // Size of the symbol
        uint8_t   st_info;      // Symbol's type and binding attributes
        uint8_t   st_other;     // Must be zero, reserved
        uint16_t  st_shndx;     // Which section (header table index) it's defined in
    };

    // ELF relocation entry (without explicit append)
    struct Reloc
    {
        uint32_t r_offset;     // Location (file byte offset, or program virtual address)
        union
        {
            uint32_t r_info;   // Symbol table index and type of relocation to apply
            struct
            {
                uint32_t  r_type    : 8;  // Type of relocation
                uint32_t  r_symbol  : 24; // Index of the symbol in the symbol table
            };
        };
    };

    // ELF program header
    struct Phdr{
        uint32_t  p_type;     // Type of segment
        uint32_t  p_offset;   // File offset where segment is located, in bytes
        uint32_t  p_vaddr;    // Virtual address of beginning of segment
        uint32_t  p_paddr;    // Physical address of beginning of segment (OS-specific)
        uint32_t  p_filesz;   // Num. of bytes in file image of segment (may be zero)
        uint32_t  p_memsz;    // Num. of bytes in mem image of segment (may be zero)
        uint32_t  p_flags;    // Segment flags
        uint32_t  p_align;     // Segment alignment constraint
    };
};

// Represents the layout of 64-bit ELF
struct Elf64
{
    // ELF file header
    struct FormatHeader
    {
        union
        {
            uint8_t  e_ident[EI_NIDENT];       // ELF identification info
            uint32_t e_ident32[EI_NIDENT / 4]; // Bytes grouped for easy magic number setting
        };

        uint16_t  e_type;       // 1 = relocatable, 3 = shared
        uint16_t  e_machine;    // Machine architecture constant, 0x3FD = AMD GPU, 0xE0 = LLVM AMD GCN
        uint32_t  e_version;    // ELF format version (1)
        uint64_t  e_entry;      // Entry point if executable (0)
        uint64_t  e_phoff;      // File offset of program header (unused, 0)
        uint64_t  e_shoff;      // File offset of section header
        uint32_t  e_flags;      // Architecture-specific flags
        uint16_t  e_ehsize;     // Size of this ELF header
        uint16_t  e_phentsize;  // Size of an entry in program header (unused, 0)
        uint16_t  e_phnum;      // # of entries in program header (0)
        uint16_t  e_shentsize;  // Size of an entry in section header
        uint16_t  e_shnum;      // # of entries in section header
        uint16_t  e_shstrndx;   // Section # that contains section name strings
    };

    // ELF section header (used to locate each data section)
    struct SectionHeader
    {
        uint32_t  sh_name;      // Name (index into string table)
        uint32_t  sh_type;      // Section type
        uint64_t  sh_flags;     // Flag bits (SectionHeaderFlags enum)
        uint64_t  sh_addr;      // Base memory address if loadable (0)
        uint64_t  sh_offset;    // File position of start of section
        uint64_t  sh_size;      // Size of section in bytes
        uint32_t  sh_link;      // Section # with related info (unused, 0)
        uint32_t  sh_info;      // More section-specific info
        uint64_t  sh_addralign; // Alignment granularity in power of 2 (1)
        uint64_t  sh_entsize;   // Size of entries if section is array
    };

    // ELF symbol table entry
    struct Symbol
    {
        uint32_t  st_name;      // Symbol name (index into string table)
        uint8_t	  st_info;      // Symbol's type and binding attributes
        uint8_t   st_other;     // Must be zero, reserved
        uint16_t  st_shndx;     // Which section (header table index) it's defined in
        uint64_t  st_value;     // Value or address associated with the symbol
        uint64_t  st_size;      // Size of the symbol
    };

    // ELF relocation entry
    struct Reloc
    {
        uint64_t r_offset;     // Location (file byte offset, or program virtual address)
        union
        {
            uint64_t r_info;   // Symbol table index and type of relocation to apply
            struct
            {
                uint32_t  r_type;   // Type of relocation
                uint32_t  r_symbol; // Index of the symbol in the symbol table
            };
        };
    };

    // ELF program header
    struct Phdr{
        uint32_t  p_type;       // Type of segment
        uint32_t  p_flags;      // Segment flags
        uint64_t  p_offset;     // File offset where segment is located, in bytes
        uint64_t  p_vaddr;      // Virtual address of beginning of segment
        uint64_t  p_paddr;      // Physical addr of beginning of segment (OS-specific)
        uint64_t  p_filesz;     // Num. of bytes in file image of segment (may be zero)
        uint64_t  p_memsz;      // Num. of bytes in mem image of segment (may be zero)
        uint64_t  p_align;     // Segment alignment constraint
    };
};
#pragma pack (pop)

// Represents a named buffer to hold section data and metadata.
template<class ElfSectionHeader>
struct ElfWriteSectionBuffer
{
    uint8_t*          pData;      // Pointer to binary data buffer
    char*             pName;      // Section name
    ElfSectionHeader  secHead;    // Section metadata
};

// Represents a named buffer to hold constant section data and metadata.
template<class ElfSectionHeader>
struct ElfReadSectionBuffer
{
    const uint8_t*    pData;      // Pointer to binary data buffer
    const char*       pName;      // Section name
    ElfSectionHeader  secHead;    // Section metadata
};

// Represents info of ELF symbol
struct ElfSymbol
{
    const char*       pSecName;   // Name of the section this symbol's defined in
    uint32_t          secIdx;     // Index of the section this symbol's defined in
    const char*       pSymName;   // Name of this symbol
    uint32_t          nameOffset; // Symbol name offset in .strtab
    uint64_t          size;       // Size of this symbol
    uint64_t          value;      // Value associated with this symbol
};

// Represents info of ELF relocation
struct ElfReloc
{
    uint64_t          offset;     // Location
    uint32_t          symIdx;     // Index of this symbol in the symbol table
};

// Represents info of ELF note
struct ElfNote
{
    NoteHeader hdr;       // Note header
    uint32_t*  pData;     // The content of the note
};

// ELF package
typedef llvm::SmallString<1024> ElfPackage;

// =====================================================================================================================
// Represents a writer for storing data to an [Executable and Linkable Format (ELF)](http://tinyurl.com/2toj8) buffer.
//
// NOTE: The client should call "AddBinarySection()" as necessary to add one or more named sections to the ELF. After
// all sections are added, the client should call "GetRequiredBufferSizeBytes()", allocate the specified amount of
// memory, then call "WriteToBuffer()" to get the final ELF binary.
template<class Elf>
class ElfWriter
{
public:
    typedef ElfWriteSectionBuffer<typename Elf::SectionHeader> ElfSectionBuffer;

    ElfWriter();
    ~ElfWriter();

    // Sets architecture-specific flags
    void SetFlags(uint32_t flags) { m_header.e_flags = flags; }

    Result AddBinarySection(const char* pName, const void* pData, size_t dataLength, uint32_t* pSecIndex);

    size_t GetRequiredBufferSizeBytes();

    void WriteToBuffer(char* pBuffer, size_t bufSize);

    void AddNote(Util::Abi::PipelineAbiNoteType type, uint32_t descSize, const void* pDesc);

    void AddSymbol(ElfSymbol* pSymbol);
private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(ElfWriter);

    void CalcReservedSectionSize();
    void CalcSectionHeaderOffset();
    void AssembleSharedStringTable();
    void AssembleNotes();
    void AssembleSymbols();

    typename Elf::FormatHeader m_header;           // ELF header
    ElfSectionBuffer           m_null;             // Section header for null section
    ElfSectionBuffer           m_shStrTab;         // Section header for section .shstrtab
    ElfSectionBuffer           m_strTab;           // Section header for section .strtab
    ElfSectionBuffer           m_note;             // Section header for section .note
    ElfSectionBuffer           m_symtab;           // Section header for section .symtab
    typename Elf::Phdr         m_textPhdr;         // Program header for section .text
    typename Elf::Phdr         m_dataPhdr;         // Program header for section .data
    typename Elf::Phdr         m_rodataPhdr;       // Program header for section .rodata

    std::vector<ElfSectionBuffer*>  m_sections;    // List of section data and headers
    std::vector<ElfNote>            m_notes;       // List of Elf notes
    std::vector<ElfSymbol>          m_symbols;     // List of Elf symbols
    int32_t m_textSecIdx;                          // Section index of .text section
    int32_t m_dataSecIdx;                          // Section index of .data section
    int32_t m_rodataSecIdx;                        // Section index of .rodata section
    static const uint32_t            ReservedSectionCount = 5; // Reserved section count
};

// =====================================================================================================================
// Represents a reader for loading data from an [Executable and Linkable Format (ELF)](http://tinyurl.com/2toj8) buffer.
//
// The client should call "ReadFromBuffer()" to initialize the context with the contents of an ELF, then
// "GetSectionData()" to retrieve the contents of a particular named section.
template<class Elf>
class ElfReader
{
public:
    typedef ElfReadSectionBuffer<typename Elf::SectionHeader> ElfSectionBuffer;
    ElfReader(GfxIpVersion gfxIp);
    ~ElfReader();

    // Gets architecture-specific flags
    uint32_t GetFlags() const { return m_header.e_flags; }

    // Gets graphics IP version info (used by ELF dump only)
    GfxIpVersion GetGfxIpVersion() const { return m_gfxIp; }

    Result ReadFromBuffer(const void* pBuffer, size_t* pBufSize);

    Result GetSectionData(const char* pName, const void** ppData, size_t* pDataLength) const;

    uint32_t GetSectionCount();
    Result GetSectionDataBySectionIndex(uint32_t secIdx, ElfSectionBuffer** ppSectionData) const;

    // Determine if a section with the specified name is present in this ELF.
    bool IsSectionPresent(const char* pName) const { return (m_map.find(pName) != m_map.end()); }

    uint32_t GetSymbolCount();
    void GetSymbol(uint32_t idx, ElfSymbol* pSymbol);
    void GetSymbolsBySectionIndex(uint32_t secIndx, std::vector<ElfSymbol>& secSymbols);

    uint32_t GetRelocationCount();
    void GetRelocation(uint32_t idx, ElfReloc* pReloc);

    // Gets the section index for the specified section name.
    int32_t GetSectionIndex(const char* pName) const
    {
        auto pEntry = m_map.find(pName);
        return (pEntry != m_map.end()) ? pEntry->second : InvalidValue;
    }

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(ElfReader);

    // -----------------------------------------------------------------------------------------------------------------

    GfxIpVersion    m_gfxIp;    // Graphics IP version info (used by ELF dump only)

    typename Elf::FormatHeader                  m_header;     // ELF header
    std::unordered_map<std::string, uint32_t>   m_map;        // Map between section name and section index

    std::vector<ElfReadSectionBuffer<typename Elf::SectionHeader>*> m_sections; // List of section data and headers

    int32_t   m_symSecIdx;      // Index of symbol section
    int32_t   m_relocSecIdx;    // Index of relocation section
    int32_t   m_strtabSecIdx;   // Index of string table section
};

} // Llpc
