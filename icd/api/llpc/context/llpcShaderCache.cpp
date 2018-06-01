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
 @file llpcShaderCache.cpp
 @brief LLPC source file: contains implementation of class Llpc::ShaderCache.
 ***********************************************************************************************************************
*/
#define DEBUG_TYPE "llpc-shader-cache"

#include <string.h>
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/FileSystem.h"
#include "llpcShaderCache.h"
#include "llvm/Support/DJB.h"

using namespace llvm;

namespace Llpc
{

static const char CacheFileSubPath[] = "/.AMD/LlpcCache/";

static const char ClientStr[] = "LLPC";

static constexpr uint64_t CrcWidth         = sizeof(uint64_t) * 8;
static constexpr uint64_t CrcTopBit        = (static_cast<uint64_t>(1) << (CrcWidth - 1));
static constexpr uint64_t CrcPolynomial    = 0xAD93D23594C935A9;
static constexpr uint64_t CrcInitialValue  = 0xFFFFFFFFFFFFFFFF;

static const uint64_t CrcLookup[256] =
{
    0x0000000000000000, 0xAD93D23594C935A9, 0xF6B4765EBD5B5EFB, 0x5B27A46B29926B52,
    0x40FB3E88EE7F885F, 0xED68ECBD7AB6BDF6, 0xB64F48D65324D6A4, 0x1BDC9AE3C7EDE30D,
    0x81F67D11DCFF10BE, 0x2C65AF2448362517, 0x77420B4F61A44E45, 0xDAD1D97AF56D7BEC,
    0xC10D4399328098E1, 0x6C9E91ACA649AD48, 0x37B935C78FDBC61A, 0x9A2AE7F21B12F3B3,
    0xAE7F28162D3714D5, 0x03ECFA23B9FE217C, 0x58CB5E48906C4A2E, 0xF5588C7D04A57F87,
    0xEE84169EC3489C8A, 0x4317C4AB5781A923, 0x183060C07E13C271, 0xB5A3B2F5EADAF7D8,
    0x2F895507F1C8046B, 0x821A8732650131C2, 0xD93D23594C935A90, 0x74AEF16CD85A6F39,
    0x6F726B8F1FB78C34, 0xC2E1B9BA8B7EB99D, 0x99C61DD1A2ECD2CF, 0x3455CFE43625E766,
    0xF16D8219CEA71C03, 0x5CFE502C5A6E29AA, 0x07D9F44773FC42F8, 0xAA4A2672E7357751,
    0xB196BC9120D8945C, 0x1C056EA4B411A1F5, 0x4722CACF9D83CAA7, 0xEAB118FA094AFF0E,
    0x709BFF0812580CBD, 0xDD082D3D86913914, 0x862F8956AF035246, 0x2BBC5B633BCA67EF,
    0x3060C180FC2784E2, 0x9DF313B568EEB14B, 0xC6D4B7DE417CDA19, 0x6B4765EBD5B5EFB0,
    0x5F12AA0FE39008D6, 0xF281783A77593D7F, 0xA9A6DC515ECB562D, 0x04350E64CA026384,
    0x1FE994870DEF8089, 0xB27A46B29926B520, 0xE95DE2D9B0B4DE72, 0x44CE30EC247DEBDB,
    0xDEE4D71E3F6F1868, 0x7377052BABA62DC1, 0x2850A14082344693, 0x85C3737516FD733A,
    0x9E1FE996D1109037, 0x338C3BA345D9A59E, 0x68AB9FC86C4BCECC, 0xC5384DFDF882FB65,
    0x4F48D60609870DAF, 0xE2DB04339D4E3806, 0xB9FCA058B4DC5354, 0x146F726D201566FD,
    0x0FB3E88EE7F885F0, 0xA2203ABB7331B059, 0xF9079ED05AA3DB0B, 0x54944CE5CE6AEEA2,
    0xCEBEAB17D5781D11, 0x632D792241B128B8, 0x380ADD49682343EA, 0x95990F7CFCEA7643,
    0x8E45959F3B07954E, 0x23D647AAAFCEA0E7, 0x78F1E3C1865CCBB5, 0xD56231F41295FE1C,
    0xE137FE1024B0197A, 0x4CA42C25B0792CD3, 0x1783884E99EB4781, 0xBA105A7B0D227228,
    0xA1CCC098CACF9125, 0x0C5F12AD5E06A48C, 0x5778B6C67794CFDE, 0xFAEB64F3E35DFA77,
    0x60C18301F84F09C4, 0xCD5251346C863C6D, 0x9675F55F4514573F, 0x3BE6276AD1DD6296,
    0x203ABD891630819B, 0x8DA96FBC82F9B432, 0xD68ECBD7AB6BDF60, 0x7B1D19E23FA2EAC9,
    0xBE25541FC72011AC, 0x13B6862A53E92405, 0x489122417A7B4F57, 0xE502F074EEB27AFE,
    0xFEDE6A97295F99F3, 0x534DB8A2BD96AC5A, 0x086A1CC99404C708, 0xA5F9CEFC00CDF2A1,
    0x3FD3290E1BDF0112, 0x9240FB3B8F1634BB, 0xC9675F50A6845FE9, 0x64F48D65324D6A40,
    0x7F281786F5A0894D, 0xD2BBC5B36169BCE4, 0x899C61D848FBD7B6, 0x240FB3EDDC32E21F,
    0x105A7C09EA170579, 0xBDC9AE3C7EDE30D0, 0xE6EE0A57574C5B82, 0x4B7DD862C3856E2B,
    0x50A1428104688D26, 0xFD3290B490A1B88F, 0xA61534DFB933D3DD, 0x0B86E6EA2DFAE674,
    0x91AC011836E815C7, 0x3C3FD32DA221206E, 0x671877468BB34B3C, 0xCA8BA5731F7A7E95,
    0xD1573F90D8979D98, 0x7CC4EDA54C5EA831, 0x27E349CE65CCC363, 0x8A709BFBF105F6CA,
    0x9E91AC0C130E1B5E, 0x33027E3987C72EF7, 0x6825DA52AE5545A5, 0xC5B608673A9C700C,
    0xDE6A9284FD719301, 0x73F940B169B8A6A8, 0x28DEE4DA402ACDFA, 0x854D36EFD4E3F853,
    0x1F67D11DCFF10BE0, 0xB2F403285B383E49, 0xE9D3A74372AA551B, 0x44407576E66360B2,
    0x5F9CEF95218E83BF, 0xF20F3DA0B547B616, 0xA92899CB9CD5DD44, 0x04BB4BFE081CE8ED,
    0x30EE841A3E390F8B, 0x9D7D562FAAF03A22, 0xC65AF24483625170, 0x6BC9207117AB64D9,
    0x7015BA92D04687D4, 0xDD8668A7448FB27D, 0x86A1CCCC6D1DD92F, 0x2B321EF9F9D4EC86,
    0xB118F90BE2C61F35, 0x1C8B2B3E760F2A9C, 0x47AC8F555F9D41CE, 0xEA3F5D60CB547467,
    0xF1E3C7830CB9976A, 0x5C7015B69870A2C3, 0x0757B1DDB1E2C991, 0xAAC463E8252BFC38,
    0x6FFC2E15DDA9075D, 0xC26FFC20496032F4, 0x9948584B60F259A6, 0x34DB8A7EF43B6C0F,
    0x2F07109D33D68F02, 0x8294C2A8A71FBAAB, 0xD9B366C38E8DD1F9, 0x7420B4F61A44E450,
    0xEE0A5304015617E3, 0x43998131959F224A, 0x18BE255ABC0D4918, 0xB52DF76F28C47CB1,
    0xAEF16D8CEF299FBC, 0x0362BFB97BE0AA15, 0x58451BD25272C147, 0xF5D6C9E7C6BBF4EE,
    0xC1830603F09E1388, 0x6C10D43664572621, 0x3737705D4DC54D73, 0x9AA4A268D90C78DA,
    0x8178388B1EE19BD7, 0x2CEBEABE8A28AE7E, 0x77CC4ED5A3BAC52C, 0xDA5F9CE03773F085,
    0x40757B122C610336, 0xEDE6A927B8A8369F, 0xB6C10D4C913A5DCD, 0x1B52DF7905F36864,
    0x008E459AC21E8B69, 0xAD1D97AF56D7BEC0, 0xF63A33C47F45D592, 0x5BA9E1F1EB8CE03B,
    0xD1D97A0A1A8916F1, 0x7C4AA83F8E402358, 0x276D0C54A7D2480A, 0x8AFEDE61331B7DA3,
    0x91224482F4F69EAE, 0x3CB196B7603FAB07, 0x679632DC49ADC055, 0xCA05E0E9DD64F5FC,
    0x502F071BC676064F, 0xFDBCD52E52BF33E6, 0xA69B71457B2D58B4, 0x0B08A370EFE46D1D,
    0x10D4399328098E10, 0xBD47EBA6BCC0BBB9, 0xE6604FCD9552D0EB, 0x4BF39DF8019BE542,
    0x7FA6521C37BE0224, 0xD2358029A377378D, 0x891224428AE55CDF, 0x2481F6771E2C6976,
    0x3F5D6C94D9C18A7B, 0x92CEBEA14D08BFD2, 0xC9E91ACA649AD480, 0x647AC8FFF053E129,
    0xFE502F0DEB41129A, 0x53C3FD387F882733, 0x08E45953561A4C61, 0xA5778B66C2D379C8,
    0xBEAB1185053E9AC5, 0x1338C3B091F7AF6C, 0x481F67DBB865C43E, 0xE58CB5EE2CACF197,
    0x20B4F813D42E0AF2, 0x8D272A2640E73F5B, 0xD6008E4D69755409, 0x7B935C78FDBC61A0,
    0x604FC69B3A5182AD, 0xCDDC14AEAE98B704, 0x96FBB0C5870ADC56, 0x3B6862F013C3E9FF,
    0xA142850208D11A4C, 0x0CD157379C182FE5, 0x57F6F35CB58A44B7, 0xFA6521692143711E,
    0xE1B9BB8AE6AE9213, 0x4C2A69BF7267A7BA, 0x170DCDD45BF5CCE8, 0xBA9E1FE1CF3CF941,
    0x8ECBD005F9191E27, 0x235802306DD02B8E, 0x787FA65B444240DC, 0xD5EC746ED08B7575,
    0xCE30EE8D17669678, 0x63A33CB883AFA3D1, 0x388498D3AA3DC883, 0x95174AE63EF4FD2A,
    0x0F3DAD1425E60E99, 0xA2AE7F21B12F3B30, 0xF989DB4A98BD5062, 0x541A097F0C7465CB,
    0x4FC6939CCB9986C6, 0xE25541A95F50B36F, 0xB972E5C276C2D83D, 0x14E137F7E20BED94
};

static constexpr uint32_t ShaderCacheTimeout = 500;

// =====================================================================================================================
ShaderCache::ShaderCache()
    :
    m_onDiskFile(),
    m_disableCache(true),
    m_shaderDataEnd(sizeof(ShaderCacheSerializedHeader)),
    m_totalShaders(0),
    m_serializedSize(sizeof(ShaderCacheSerializedHeader)),
    m_pfnGetValueFunc(nullptr),
    m_pfnStoreValueFunc(nullptr)
{
    memset(m_fileFullPath, 0, MaxFilePathLen);
    memset(&m_gfxIp, 0, sizeof(m_gfxIp));
}

// =====================================================================================================================
ShaderCache::~ShaderCache()
{
     Destroy();
}

// =====================================================================================================================
// Destruction, does clean-up work.
void ShaderCache::Destroy()
{
    if (m_onDiskFile.IsOpen())
    {
        m_onDiskFile.Close();
    }
    ResetRuntimeCache();
}

// =====================================================================================================================
// Resets the runtime shader cache to an empty state. Releases all allocator memory and decommits it back to the OS.
void ShaderCache::ResetRuntimeCache()
{
    for (auto indexMap : m_shaderIndexMap)
    {
        delete indexMap.second;
    }
    m_shaderIndexMap.clear();

    for (auto allocIt : m_allocationList)
    {
        delete[] allocIt.first;
    }
    m_allocationList.clear();

    m_totalShaders   = 0;
    m_shaderDataEnd  = sizeof(ShaderCacheSerializedHeader);
    m_serializedSize = sizeof(ShaderCacheSerializedHeader);
}

// =====================================================================================================================
// Copies the shader cache data to the memory blob provided by the calling function.
//
// NOTE: It is expected that the calling function has not used this shader cache since querying the size
Result ShaderCache::Serialize(
    void*   pBlob,    // [out] System memory pointer where the serialized data should be placed
    size_t* pSize)    // [in,out] Size of the memory pointed to by pBlob. If the value stored in pSize is zero then no
                      // data will be copied and instead the size required for serialization will be returned in pSize
{
    Result result = Result::Success;

    if (*pSize == 0)
    {
        // Query shader cache serailzied size
        (*pSize) = m_serializedSize;
    }
    else
    {
        // Do serialize
        LLPC_ASSERT(m_shaderDataEnd == m_serializedSize || (m_shaderDataEnd == sizeof(ShaderCacheSerializedHeader)));

        if (m_serializedSize >= sizeof(ShaderCacheSerializedHeader))
        {
            if ((pBlob != nullptr) && ((*pSize) >= m_serializedSize))
            {
                // First construct the header and copy it into the memory provided
                ShaderCacheSerializedHeader header = {};
                header.headerSize    = sizeof(ShaderCacheSerializedHeader);
                header.shaderCount   = m_totalShaders;
                header.shaderDataEnd = m_shaderDataEnd;
                GetBuildTime(&header.buildId);

                memcpy(pBlob, &header, sizeof(ShaderCacheSerializedHeader));

                void* pDataDst = VoidPtrInc(pBlob, sizeof(ShaderCacheSerializedHeader));

                // Then iterate through all allocators (which hold the backing memory for the shader data)
                // and copy their contents to the blob.
                for (auto it : m_allocationList)
                {
                    LLPC_ASSERT(it.first != nullptr);

                    const size_t copySize = it.second;
                    if (VoidPtrDiff(pDataDst, pBlob) + copySize > (*pSize))
                    {
                        result = Result::ErrorUnknown;
                        break;
                    }

                    memcpy(pDataDst, it.first, copySize);
                    pDataDst = VoidPtrInc(pDataDst, copySize);
                }
            }
            else
            {
                LLPC_NEVER_CALLED();
                result = Result::ErrorUnknown;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Merges the shader data of source shader caches into this shader cache.
Result ShaderCache::Merge(
    uint32_t             srcCacheCount,  // Count of input source shader caches
    const IShaderCache** ppSrcCaches)    // [in] Input shader caches
{
    // Merge function is supposed to be called by client created shader caches, which are always runtime mode.
    LLPC_ASSERT(m_fileFullPath[0] == '\0');

    Result result = Result::Success;

    LockCacheMap(false);

    for (uint32_t i = 0; i < srcCacheCount; i++)
    {
        ShaderCache* pSrcCache = static_cast<ShaderCache*>(const_cast<IShaderCache*>(ppSrcCaches[i]));
        pSrcCache->LockCacheMap(true);

        for (auto it : pSrcCache->m_shaderIndexMap)
        {
            ShaderHash key = it.first;

            auto indexMap = m_shaderIndexMap.find(key);
            if (indexMap == m_shaderIndexMap.end())
            {
                ShaderIndex* pIndex = nullptr;
                void* pMem = GetCacheSpace(it.second->header.size);
                memcpy(pMem, it.second->pDataBlob, it.second->header.size);

                pIndex = new ShaderIndex;
                pIndex->pDataBlob = pMem;
                pIndex->state = ShaderEntryState::Ready;
                pIndex->header = it.second->header;

                m_shaderIndexMap[key] = pIndex;
            }
        }
        pSrcCache->UnlockCacheMap(true);
    }

    UnlockCacheMap(false);

    return result;
}

// =====================================================================================================================
// Initializes the Shader Cache in late stage.
Result ShaderCache::Init(
    const ShaderCacheCreateInfo*    pCreateInfo,    // [in] Shader cache create info
    const ShaderCacheAuxCreateInfo* pAuxCreateInfo) // [in] Shader cache auxiliary info (static fields)
{
    Result result = Result::Success;

    if (pAuxCreateInfo->shaderCacheMode != ShaderCacheDisable)
    {
        m_disableCache = false;
        m_pClientData       = pCreateInfo->pClientData;
        m_pfnGetValueFunc   = pCreateInfo->pfnGetValueFunc;
        m_pfnStoreValueFunc = pCreateInfo->pfnStoreValueFunc;
        m_gfxIp             = pAuxCreateInfo->gfxIp;
        m_hash              = pAuxCreateInfo->hash;

        LockCacheMap(false);

        // If we're in runtime mode and the caller provided a data blob, try to load the from that blob.
        if ((pAuxCreateInfo->shaderCacheMode == ShaderCacheEnableRuntime) && (pCreateInfo->initialDataSize > 0))
        {
            if (LoadCacheFromBlob(pCreateInfo->pInitialData, pCreateInfo->initialDataSize) != Result::Success)
            {
                ResetRuntimeCache();
            }
        }
        // If we're in on-disk mode try to load the cache from file.
        else if ((pAuxCreateInfo->shaderCacheMode == ShaderCacheEnableOnDisk) ||
                 (pAuxCreateInfo->shaderCacheMode == ShaderCacheForceInternalCacheOnDisk) ||
                 (pAuxCreateInfo->shaderCacheMode == ShaderCacheEnableOnDiskReadOnly))
        {
            // Default to false because the cache file is invalid if it's brand new
            bool cacheFileExists = false;

            // Build the cache file name and make required directories if necessary
            // cacheFileValid gets initially set based on whether the file exists.
            result = BuildFileName(pAuxCreateInfo->pExecutableName,
                                   pAuxCreateInfo->pCacheFilePath,
                                   pAuxCreateInfo->gfxIp,
                                   &cacheFileExists);

            if (result == Result::Success)
            {
                // Open the storage file if it exists
                if (cacheFileExists)
                {
                    if (pAuxCreateInfo->shaderCacheMode == ShaderCacheEnableOnDiskReadOnly)
                    {
                        result = m_onDiskFile.Open(m_fileFullPath, (FileAccessRead | FileAccessBinary));
                    }
                    else
                    {
                        result = m_onDiskFile.Open(m_fileFullPath, (FileAccessReadUpdate | FileAccessBinary));
                    }
                }
                else
                // Create the storage file if it does not exist
                {
                    result = m_onDiskFile.Open(m_fileFullPath, (FileAccessRead | FileAccessAppend | FileAccessBinary));
                }
            }

            Result loadResult = Result::ErrorUnknown;
            // If the cache file already existed, then we can try loading the data from it
            if (result == Result::Success)
            {
                if (cacheFileExists)
                {
                    loadResult = LoadCacheFromFile();
                    if ((pAuxCreateInfo->shaderCacheMode == ShaderCacheEnableOnDiskReadOnly) &&
                        (loadResult == Result::Success))
                    {
                        m_onDiskFile.Close();
                    }
                }
                else
                {
                    ResetCacheFile();
                }
            }

            // Either the file is new or had invalid data so we need to reset the index hash map and release
            // any memory allocated
            if (loadResult != Result::Success)
            {
                ResetRuntimeCache();
            }
        }

        UnlockCacheMap(false);
    }
    else
    {
        m_disableCache = true;
    }

    return result;
}

// =====================================================================================================================
// Constructs the on disk cache file name and path and puts it in m_fileFullPath. This function also creates any
// any missing directories in the full path to the cache file.
Result ShaderCache::BuildFileName(
    const char*  pExecutableName,     // [in] Name of Executable file
    const char*  pCacheFilePath,      // [in] Root directory of cache file
    GfxIpVersion gfxIp,               // Graphics IP version info
    bool*        pCacheFileExists)    // [out] Whether cache file exists
{
    // The file name is constructed by taking the executable file name, appending the client string, device ID and
    // GPU index then hashing the result.
    char hashedFileName[MaxFilePathLen];
    int32_t length = snprintf(hashedFileName, MaxFilePathLen, "%s.%s.%u.%u.%u", pExecutableName,
             ClientStr,
             gfxIp.major,
             gfxIp.minor,
             gfxIp.stepping);

    const uint32_t nameHash = djbHash(hashedFileName, 0);
    length = snprintf(hashedFileName, MaxFilePathLen, "%08x.bin", nameHash);

    // Combine the base path, the sub-path and the file name to get the fully qualified path to the cache file
    length = snprintf(m_fileFullPath, MaxFilePathLen, "%s%s%s", pCacheFilePath, CacheFileSubPath, hashedFileName);

    LLPC_ASSERT(pCacheFileExists != nullptr);
    *pCacheFileExists = File::Exists(m_fileFullPath);
    Result result = Result::Success;
    if ((*pCacheFileExists) == false)
    {
        length = snprintf(hashedFileName, MaxFilePathLen, "%s%s", pCacheFilePath, CacheFileSubPath);
        std::error_code errCode = sys::fs::create_directories(hashedFileName);
        if (!errCode)
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Resets the contents of the cache file, assumes the shader cache has been locked for writes.
void ShaderCache::ResetCacheFile()
{
    m_onDiskFile.Close();
    Result fileResult = m_onDiskFile.Open(m_fileFullPath, (FileAccessRead | FileAccessWrite | FileAccessBinary));
    LLPC_ASSERT(fileResult == Result::Success);

    ShaderCacheSerializedHeader header = {};
    header.headerSize    = sizeof(ShaderCacheSerializedHeader);
    header.shaderCount   = 0;
    header.shaderDataEnd = header.headerSize;
    GetBuildTime(&header.buildId);

    m_onDiskFile.Write(&header, header.headerSize);
}

// =====================================================================================================================
// Searches the shader cache for a shader with the matching key, allocating a new entry if it didn't already exist.
//
// Returns:
//    Ready       - if a matching shader was found and is ready for use
//    Compiling   - if an entry was created and must be compiled/populated by the caller
//    Unavailable - if an unrecoverable error was encountered
ShaderEntryState ShaderCache::FindShader(
    MetroHash::Hash   hash,                    // Hash code of shader
    bool              allocateOnMiss,          // Whether allocate a new entry for new hash
    CacheEntryHandle* phEntry)                 // [out] Handle of shader cache entry
{
    // Early return if shader cache is disabled
    if (m_disableCache)
    {
        *phEntry = nullptr;
        return ShaderEntryState::Compiling;
    }

    ShaderEntryState result    = ShaderEntryState::Unavailable;
    bool             existed   = false;
    ShaderIndex*     pIndex    = nullptr;
    Result           mapResult = Result::Success;
    LLPC_ASSERT(phEntry != nullptr);

    bool readOnlyLock = (allocateOnMiss == false);
    LockCacheMap(readOnlyLock);
    ShaderHash hashKey = MetroHash::Compact64(&hash);
    auto indexMap = m_shaderIndexMap.find(hashKey);
    if (indexMap != m_shaderIndexMap.end())
    {
        existed = true;
        pIndex = indexMap->second;
    }
    else if (allocateOnMiss)
    {
        pIndex = new ShaderIndex;
        m_shaderIndexMap[hashKey] = pIndex;
    }

    if (pIndex == nullptr)
    {
        mapResult = Result::ErrorUnavailable;
    }

    if (mapResult == Result::Success)
    {
        if (existed)
        {
            // We don't need to hold on to the write lock if we're not the one doing the compile
            if (readOnlyLock == false)
            {
                UnlockCacheMap(readOnlyLock);
                readOnlyLock = true;
                LockCacheMap(readOnlyLock);
            }
        }
        else
        {
            bool needsInit = true;

            // We didn't find the entry in our own hash map, now search the external cache if available
            if (UseExternalCache())
            {
                // The first call to the external cache queries the existence and the size of the cached shader.
                Result extResult = m_pfnGetValueFunc(m_pClientData, hashKey, nullptr, &pIndex->header.size);
                if (extResult == Result::Success)
                {
                    // An entry was found matching our hash, we should allocate memory to hold the data and call again
                    LLPC_ASSERT(pIndex->header.size > 0);
                    pIndex->pDataBlob = GetCacheSpace(pIndex->header.size);

                    if (pIndex->pDataBlob == nullptr)
                    {
                        extResult = Result::ErrorOutOfMemory;
                    }
                    else
                    {
                        extResult =
                            m_pfnGetValueFunc(m_pClientData, hashKey, pIndex->pDataBlob, &pIndex->header.size);
                    }
                }

                if (extResult == Result::Success)
                {
                    // We now have a copy of the shader data from the external cache, just need to update the
                    // ShaderIndex. The first item in the data blob is a ShaderHeader, followed by the serialized
                    // data blob for the shader.
                    const auto*const pHeader = static_cast<const ShaderHeader*>(pIndex->pDataBlob);
                    LLPC_ASSERT(pIndex->header.size == pHeader->size);

                    pIndex->header = (*pHeader);
                    pIndex->state  = ShaderEntryState::Ready;
                    needsInit      = false;
                }
                else if (extResult == Result::ErrorUnavailable)
                {
                    // This means the external cache is unavailable and we shouldn't bother using it anymore. To
                    // prevent useless calls we'll zero out the function pointers.
                    m_pfnGetValueFunc   = nullptr;
                    m_pfnStoreValueFunc = nullptr;
                }
                else
                {
                    // extResult should never be ErrorInvalidMemorySize since Cache space is always allocated based
                    // on 1st m_pfnGetValueFunc call.
                    LLPC_ASSERT(extResult != Result::ErrorOutOfMemory);

                    // Any other result means we just need to continue with initializing the new index/compiling.
                }
            }

            if (needsInit)
            {
                // This is a brand new cache entry so we need to initialize the ShaderIndex.
                memset(pIndex, 0, sizeof(*pIndex));
                pIndex->header.key = hashKey;
                pIndex->state      = ShaderEntryState::New;
            }
        }   // End if (existed == false)

        if (pIndex->state == ShaderEntryState::Compiling)
        {
            // The shader is being compiled by another thread, we should release the lock and wait for it to complete
            while (pIndex->state == ShaderEntryState::Compiling)
            {
                UnlockCacheMap(readOnlyLock);
                {
                    std::unique_lock<std::mutex> lock(m_conditionMutex);

                    m_conditionVariable.wait_for(lock, std::chrono::seconds(1));
                }
                LockCacheMap(readOnlyLock);
            }
            // At this point the shader entry is either Ready, New or something failed. We've already
            // initialized our result code to an error code above, the Ready and New cases are handled below so
            // nothing else to do here.
        }

        if (pIndex->state == ShaderEntryState::Ready)
        {
            // The shader has been compiled, just verify it has valid data and then return success.
            LLPC_ASSERT((pIndex->pDataBlob != nullptr) && (pIndex->header.size != 0));
        }
        else if (pIndex->state == ShaderEntryState::New)
        {
            // The shader entry is new (or previously failed compilation) and we're the first thread to get a
            // crack at it, move it into the Compiling state
            pIndex->state = ShaderEntryState::Compiling;
        }

        // Return the ShaderIndex as a handle so subsequent calls into the cache can avoid the hash map lookup.
        (*phEntry) = pIndex;
        result     = pIndex->state;
    }

    UnlockCacheMap(readOnlyLock);

    return result;
}

// =====================================================================================================================
// Inserts a new shader into the cache. The new shader is written to the cache file if it is in-use, and will also
// upload it to the client's external cache if it is in-use.
void ShaderCache::InsertShader(
    CacheEntryHandle         hEntry,                 // [in] Handle of shader cache entry
    const void*              pBlob,                  // [in] Shader data
    size_t                   shaderSize)             // size of shader data in bytes
{
    auto*const pIndex = static_cast<ShaderIndex*>(hEntry);
    LLPC_ASSERT(m_disableCache == false);
    LLPC_ASSERT((pIndex != nullptr) && (pIndex->state == ShaderEntryState::Compiling));

    LockCacheMap(false);

    Result result = Result::Success;

    if (result == Result::Success)
    {
        // Allocate space to store the serialized shader and a copy of the header. The header is duplicated in the
        // data to simplify serialize/load.
        pIndex->header.size = (shaderSize + sizeof(ShaderHeader));
        pIndex->pDataBlob   = GetCacheSpace(pIndex->header.size);

        if (pIndex->pDataBlob == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            ++m_totalShaders;

            auto*const pHeader   = static_cast<ShaderHeader*>(pIndex->pDataBlob);
            void*const pDataBlob = (pHeader + 1);

            // Serialize the shader into an opaque blob of data.
            memcpy(pDataBlob, pBlob, shaderSize);

            // Compute a CRC for the serialized data (useful for detecting data corruption), and copy the index's
            // header into the data's header.
            pIndex->header.crc = CalculateCrc(static_cast<uint8_t*>(pDataBlob), shaderSize);
            (*pHeader)         = pIndex->header;

            if (UseExternalCache())
            {
                // If we're making use of the external shader cache then we need to store the compiled shader data here.
                Result externalResult = m_pfnStoreValueFunc(m_pClientData,
                                                            pIndex->header.key,
                                                            pIndex->pDataBlob,
                                                            pIndex->header.size);
                if (externalResult == Result::ErrorUnavailable)
                {
                    // This is the only return code we can do anything about. In this case it means the external cache
                    // is not available and we should zero out the function pointers to avoid making useless calls on
                    // subsequent shader compiles.
                    m_pfnGetValueFunc   = nullptr;
                    m_pfnStoreValueFunc = nullptr;
                }
                else
                {
                    // Otherwise the store either succeeded (yay!) or failed in some other transient way. Either way,
                    // we will just continue, there's nothing to be done.
                }
            }

            // Mark this entry as ready, we'll wake the waiting threads once we release the lock
            pIndex->state = ShaderEntryState::Ready;

            // Finally, update the file if necessary.
            if (m_onDiskFile.IsOpen())
            {
                AddShaderToFile(pIndex);
            }
        }
    }

    if (result != Result::Success)
    {
        // Something failed while attempting to add the shader, most likely memory allocation. There's not much we
        // can do here except give up on adding data. This means we need to set the entry back to New so if another
        // thread is waiting it will be allowed to continue (it will likely just get to this same point, but at least
        // we won't hang or crash).
        pIndex->state       = ShaderEntryState::New;
        pIndex->header.size = 0;
        pIndex->pDataBlob   = nullptr;
    }

    UnlockCacheMap(false);
    m_conditionVariable.notify_all();
}

// =====================================================================================================================
// Resets cache entry state to new. It is used when shader compile fails.
void ShaderCache::ResetShader(
    CacheEntryHandle         hEntry)                 // [in] Handle of shader cache entry
{
    auto*const pIndex = static_cast<ShaderIndex*>(hEntry);
    LLPC_ASSERT(m_disableCache == false);
    LLPC_ASSERT((pIndex != nullptr) && (pIndex->state == ShaderEntryState::Compiling));
    LockCacheMap(false);
    pIndex->state       = ShaderEntryState::New;
    pIndex->header.size = 0;
    pIndex->pDataBlob   = nullptr;
    UnlockCacheMap(false);
    m_conditionVariable.notify_all();
}

// =====================================================================================================================
// Retrieves the shader from the cache which is identified by the specified entry handle.
Result ShaderCache::RetrieveShader(
    CacheEntryHandle   hEntry,   // [in] Handle of shader cache entry
    const void**       ppBlob,   // [out] Shader data
    size_t*            pSize)    // [out] size of shader data in bytes
{
    Result result = Result::Success;
    const auto*const pIndex = static_cast<ShaderIndex*>(hEntry);

    LLPC_ASSERT(m_disableCache == false);
    LLPC_ASSERT(pIndex != nullptr);
    LLPC_ASSERT(pIndex->header.size >= sizeof(ShaderHeader));

    LockCacheMap(true);

    *ppBlob = VoidPtrInc(pIndex->pDataBlob, sizeof(ShaderHeader));
    *pSize = pIndex->header.size -  sizeof(ShaderHeader);

    UnlockCacheMap(true);

    return (*pSize > 0) ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Adds data for a new shader to the on-disk file
void ShaderCache::AddShaderToFile(
    const ShaderIndex* pIndex)    // [in] A new shader
{
    LLPC_ASSERT(m_onDiskFile.IsOpen());

    // We only need to update the parts of the file that changed, which is the number of shaders, the new data section,
    // and the shaderDataEnd.

    // Calculate the header offsets, then write the relavent data to the file.
    const uint32_t shaderCountOffset = offsetof(struct ShaderCacheSerializedHeader, shaderCount);
    const uint32_t dataEndOffset     = offsetof(struct ShaderCacheSerializedHeader, shaderDataEnd);

    m_onDiskFile.Seek(shaderCountOffset, true);
    m_onDiskFile.Write(&m_totalShaders, sizeof(size_t));

    // Write the new shader data at the current end of the data section
    m_onDiskFile.Seek(static_cast<uint32_t>(m_shaderDataEnd), true);
    m_onDiskFile.Write(pIndex->pDataBlob, pIndex->header.size);

    // Then update the data end value and write it out to the file.
    m_shaderDataEnd += pIndex->header.size;
    m_onDiskFile.Seek(dataEndOffset, true);
    m_onDiskFile.Write(&m_shaderDataEnd, sizeof(size_t));

    m_onDiskFile.Flush();
}

// =====================================================================================================================
// Loads all shader data from the cache file into the local cache copy. Returns true if the file contents were loaded
// successfully or false if invalid data was found.
//
// NOTE: This function assumes that a write lock has already been taken by the calling function and that the on-disk
// file has been successfully opened and the file position is the beginning of the file.
Result ShaderCache::LoadCacheFromFile()
{
    LLPC_ASSERT(m_onDiskFile.IsOpen());

    // Read the header from the file and validate it
    ShaderCacheSerializedHeader header = {};
    m_onDiskFile.Rewind();
    m_onDiskFile.Read(&header, sizeof(ShaderCacheSerializedHeader), nullptr);

    const size_t fileSize = File::GetFileSize(m_fileFullPath);
    const size_t dataSize = fileSize - sizeof(ShaderCacheSerializedHeader);
    Result result = ValidateAndLoadHeader(&header, fileSize);

    void* pDataMem = nullptr;
    if (result == Result::Success)
    {
        // The header is valid, so allocate space to fit all of the shader data.
        pDataMem = GetCacheSpace(dataSize);
    }

    if (result == Result::Success)
    {
        if (pDataMem != nullptr)
        {
            // Read the shader data into the allocated memory.
            m_onDiskFile.Seek(sizeof(ShaderCacheSerializedHeader), true);
            size_t bytesRead = 0;
            result = m_onDiskFile.Read(pDataMem, dataSize, &bytesRead);

            // If we didn't read the correct number of bytes then something went wrong and we should return a failure
            if (bytesRead != dataSize)
            {
                result = Result::ErrorUnknown;
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        // Now setup the shader index hash map.
        result = PopulateIndexMap(pDataMem, dataSize);
    }

    if (result != Result::Success)
    {
        // Something went wrong in loading the file, so reset it
        ResetCacheFile();
    }

    return result;
}

// =====================================================================================================================
// Loads all shader data from a client provided initial data blob. Returns true if the file contents were loaded
// successfully or false if invalid data was found.
//
// NOTE: This function assumes that a write lock has already been taken by the calling function.
Result ShaderCache::LoadCacheFromBlob(
    const void* pInitialData,       // [in] Initial data of the shader cache
    size_t      initialDataSize)    // Size of initial data
{
    const auto* pHeader = static_cast<const ShaderCacheSerializedHeader*>(pInitialData);
    LLPC_ASSERT(pInitialData != nullptr);

    // First verify that the header data is valid
    Result result = ValidateAndLoadHeader(pHeader, initialDataSize);

    if (result == Result::Success)
    {
        // The header appears valid so allocate space for the shader data.
        const size_t dataSize = initialDataSize - pHeader->headerSize;
        void* pDataMem = GetCacheSpace(dataSize);

        if (pDataMem != nullptr)
        {
            // Then copy the data and setup the shader index hash map.
            memcpy(pDataMem, VoidPtrInc(pInitialData, pHeader->headerSize), dataSize);
            result = PopulateIndexMap(pDataMem, dataSize);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Validates shader data (from a file or a blob) by checking the CRCs and adding index hash map entries if successful.
// Will return a failure if any of the shader data is invalid.
Result ShaderCache::PopulateIndexMap(
    void*  pDataStart,    // [in] Start pointer of cached shader data
    size_t dataSize)      // Shader data size in bytes
{
    Result result = Result::Success;

    // Iterate through all of the entries to verify the data CRC, zero out the GPU memory pointer/offset and add to the
    // hashmap. We zero out the GPU memory data here because we're already iterating through each entry, rather than
    // take the hit each time we add shader data to the file.
    auto* pHeader = static_cast<ShaderHeader*>(pDataStart);

    for (uint32_t shader = 0; ((shader < m_totalShaders) && (result == Result::Success)); ++shader)
    {
        // Guard against buffer overruns.
        LLPC_ASSERT(VoidPtrDiff(pHeader, pDataStart) <= dataSize);

        // TODO: Add a static function to RelocatableShader to validate the input data.

        // The serialized data blob representing each RelocatableShader object immediately follows the header.
        void*const pDataBlob = (pHeader + 1);

        // Verify the CRC
        const uint64_t crc = CalculateCrc(static_cast<uint8_t*>(pDataBlob), (pHeader->size - sizeof(ShaderHeader)));

        if (crc == pHeader->crc)
        {
            // It all checks out, so add this shader to the hash map!
            ShaderIndex* pIndex = nullptr;
            bool existed = false;
            auto indexMap = m_shaderIndexMap.find(pHeader->key);
            if (indexMap == m_shaderIndexMap.end())
            {
                pIndex = new ShaderIndex;
                pIndex->header    = (*pHeader);
                pIndex->pDataBlob = pHeader;
                pIndex->state     = ShaderEntryState::Ready;
                m_shaderIndexMap[pHeader->key] = pIndex;
            }
        }
        else
        {
            result = Result::ErrorUnknown;
        }

        // Move to next entry in cache
        pHeader = static_cast<ShaderHeader*>(VoidPtrInc(pHeader, pHeader->size));
    }

    return result;
}

// =====================================================================================================================
// Caclulates a 64-bit CRC of the data provided
uint64_t ShaderCache::CalculateCrc(
    const uint8_t* pData,         // [in]  Data need generate CRC
    size_t         numBytes)      // Data size in bytes
{
    uint64_t crc = CrcInitialValue;
    for (uint32_t byte = 0; byte < numBytes; ++byte)
    {
        uint8_t tableIndex = static_cast<uint8_t>(crc >> (CrcWidth - 8)) & 0xFF;
        crc = (crc << 8) ^ CrcLookup[tableIndex] ^ pData[byte];
    }

    return crc;
}

// =====================================================================================================================
// Validates the provided header and stores the data contained within it if valid.
Result ShaderCache::ValidateAndLoadHeader(
    const ShaderCacheSerializedHeader* pHeader,            // [in] Cache file header
    size_t                             dataSourceSize)     // Data size in byte
{
    LLPC_ASSERT(pHeader != nullptr);

    BuildUniqueId buildId;
    GetBuildTime(&buildId);

    Result result = Result::Success;

    if (pHeader->headerSize == sizeof(ShaderCacheSerializedHeader) &&
        (memcmp(pHeader->buildId.buildDate, buildId.buildDate, sizeof(buildId.buildDate)) == 0) &&
        (memcmp(pHeader->buildId.buildTime, buildId.buildTime, sizeof(buildId.buildTime)) == 0) &&
        (memcmp(&pHeader->buildId.gfxIp, &buildId.gfxIp, sizeof(buildId.gfxIp)) == 0) &&
        (memcmp(&pHeader->buildId.hash, &buildId.hash, sizeof(buildId.hash)) == 0))
    {
        // The header appears valid so copy the header data to the runtime cache
        m_totalShaders  = pHeader->shaderCount;
        m_shaderDataEnd = pHeader->shaderDataEnd;
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    // Make sure the shader data end value is correct. It's ok for there to be unused space at the end of the file, but
    // if the shaderDataEnd is beyond the end of the file we have a problem.
    if ((result == Result::Success) && (m_shaderDataEnd > dataSourceSize))
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Allocates memory from the shader cache's linear allocator. This function assumes that a write lock has been taken by
// the calling function.
void* ShaderCache::GetCacheSpace(
    size_t numBytes)    // Allocation size in bytes
{
    auto p = new uint8_t[numBytes];
    m_allocationList.push_back(std::pair<uint8_t*, size_t>(p, numBytes));
    m_serializedSize += numBytes;
    return p;
}

// =====================================================================================================================
// Returns the time & date that pipeline.cpp was compiled.
void ShaderCache::GetBuildTime(
    BuildUniqueId *pBuildId)  // [out] Unique ID of build info
{
    memset(pBuildId, 0, sizeof(pBuildId[0]));
    memcpy(&pBuildId->buildDate, __DATE__, std::min(strlen(__DATE__), sizeof(pBuildId->buildDate)));
    memcpy(&pBuildId->buildTime, __TIME__, std::min(strlen(__TIME__), sizeof(pBuildId->buildTime)));
    memcpy(&pBuildId->gfxIp, &m_gfxIp, sizeof(m_gfxIp));
    memcpy(&pBuildId->hash, &m_hash, sizeof(m_hash));
}

// =====================================================================================================================
// Check if the shader cache creation info is compatible
bool ShaderCache::IsCompatible(
    const ShaderCacheCreateInfo*    pCreateInfo,    // [in] Shader cache create info
    const ShaderCacheAuxCreateInfo* pAuxCreateInfo) // [in] Shader cache auxiliary info (static fields)
{
    // Check hash first
    bool isCompatible = (memcmp(&(pAuxCreateInfo->hash), &m_hash, sizeof(m_hash)) == 0);

    return isCompatible && (m_gfxIp.major == pAuxCreateInfo->gfxIp.major) &&
        (m_gfxIp.minor == pAuxCreateInfo->gfxIp.minor) &&
        (m_gfxIp.stepping == pAuxCreateInfo->gfxIp.stepping);
}

} // Llpc
