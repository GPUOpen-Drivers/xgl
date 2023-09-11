/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  pipeline_binary_cache.cpp
* @brief Implementation of the Vulkan interface for PAL layered caching.
***********************************************************************************************************************
*/
#include "include/pipeline_binary_cache.h"
#include "include/binary_cache_serialization.h"

#include "palArchiveFile.h"
#include "palAutoBuffer.h"
#include "palPlatformKey.h"
#include "palSysMemory.h"
#include "palVectorImpl.h"
#include "palHashMapImpl.h"
#include "palFile.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "palPipelineAbiReader.h"
#include "devmode/devmode_mgr.h"
#endif
#include <string.h>

namespace vk
{
constexpr char   PipelineBinaryCache::EnvVarPath[];
constexpr char   PipelineBinaryCache::EnvVarFileName[];
constexpr char   PipelineBinaryCache::EnvVarReadOnlyFileName[];

static constexpr char   ArchiveTypeString[]  = "VK_SHADER_PIPELINE_CACHE";
static constexpr size_t ArchiveTypeStringLen = sizeof(ArchiveTypeString);
static constexpr char   ElfTypeString[]      = "VK_PIPELINE_ELF";
static constexpr size_t ElfTypeStringLen     = sizeof(ElfTypeString);

const uint32_t PipelineBinaryCache::ArchiveType = Util::HashString(ArchiveTypeString, ArchiveTypeStringLen);
const uint32_t PipelineBinaryCache::ElfType     = Util::HashString(ElfTypeString, ElfTypeStringLen);

#if ICD_GPUOPEN_DEVMODE_BUILD
static Util::Hash128 ParseHash128(const char* str);
#endif

bool PipelineBinaryCache::IsValidBlob(
    VkAllocationCallbacks* pAllocationCallbacks,
    Util::IPlatformKey*    pKey,
    size_t                 dataSize,
    const void*            pData)
{
    VK_ASSERT(pData != nullptr);

    bool     isValid            = false;
    size_t   blobSize           = dataSize;
    auto pBinaryPrivateHeader   = static_cast<const PipelineBinaryCachePrivateHeader*>(pData);
    uint8_t  hashId[SHA_DIGEST_LENGTH];

    if ((pKey != nullptr) && (dataSize > sizeof(PipelineBinaryCachePrivateHeader)))
    {
        pData         = Util::VoidPtrInc(pData, sizeof(PipelineBinaryCachePrivateHeader));
        blobSize     -= sizeof(PipelineBinaryCachePrivateHeader);

        Util::Result result = CalculatePipelineBinaryCacheHashId(
                                pAllocationCallbacks,
                                pKey,
                                pData,
                                blobSize,
                                hashId);

        if (result == Util::Result::Success)
        {
            isValid = (memcmp(hashId, pBinaryPrivateHeader->hashId, SHA_DIGEST_LENGTH) == 0);
        }
    }

    return isValid;
}

// =====================================================================================================================
// Allocate and initialize a PipelineBinaryCache object
PipelineBinaryCache* PipelineBinaryCache::Create(
    VkAllocationCallbacks*    pAllocationCallbacks,
    Util::IPlatformKey*       pKey,
    const Vkgc::GfxIpVersion& gfxIp,
    const RuntimeSettings&    settings,
    const char*               pDefaultCacheFilePath,
#if ICD_GPUOPEN_DEVMODE_BUILD
    vk::DevModeMgr*           pDevModeMgr,
#endif
    uint32_t                  expectedEntries,
    size_t                    initDataSize,
    const void*               pInitData,
    bool                      createArchiveLayers
    )
{
    VK_ASSERT(pAllocationCallbacks != nullptr);

    PipelineBinaryCache* pObj = nullptr;
    void*                pMem = pAllocationCallbacks->pfnAllocation(pAllocationCallbacks->pUserData,
                                                                    sizeof(PipelineBinaryCache),
                                                                    VK_DEFAULT_MEM_ALIGN,
                                                                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (pMem != nullptr)
    {
        pObj = VK_PLACEMENT_NEW(pMem) PipelineBinaryCache(pAllocationCallbacks, gfxIp, expectedEntries);

#if ICD_GPUOPEN_DEVMODE_BUILD
        pObj->m_pDevModeMgr = pDevModeMgr;
#endif

        if (pObj->Initialize(settings, createArchiveLayers, pDefaultCacheFilePath, pKey) != VK_SUCCESS)
        {
            pObj->Destroy();
            pObj = nullptr;
        }
        else if ((pInitData != nullptr) &&
                 (initDataSize > (sizeof(BinaryCacheEntry) + sizeof(PipelineBinaryCachePrivateHeader))))
        {
            // The pipeline binary cache data format is as follows:
            // ```
            // | Public Header | Private Header (20B) | BinaryCacheEntry (24B) | Blob (n) | BinaryCacheEntry (24B) | ...
            // ```
            //
            const void* pBlob           = pInitData;
            size_t   blobSize           = initDataSize;
            constexpr size_t EntrySize  = sizeof(BinaryCacheEntry);

            pBlob         = Util::VoidPtrInc(pBlob, sizeof(PipelineBinaryCachePrivateHeader));
            blobSize     -= sizeof(PipelineBinaryCachePrivateHeader);
            while (blobSize > EntrySize)
            {
                // `BinaryCacheEntry` headers require the alignment of 8 bytes, which is not guaranteed with this data
                // format. Therefore we cannot use `reinterpret_cast` to read cache entry headers. We use memcpy to
                // avoid unaligned memory accesses.
                BinaryCacheEntry entry;
                memcpy(&entry, pBlob, EntrySize);
                const void*             pData   = Util::VoidPtrInc(pBlob, EntrySize);
                const size_t entryAndDataSize   = entry.dataSize + EntrySize;

                if (blobSize >= entryAndDataSize)
                {
                    //add to cache
                    Util::Result result = pObj->StorePipelineBinary(&entry.hashId, entry.dataSize, pData);
                    if (result != Util::Result::Success)
                    {
                        break;
                    }
                    pBlob = Util::VoidPtrInc(pBlob, entryAndDataSize);
                    blobSize -= entryAndDataSize;
                }
                else
                {
                    break;
                }
            }
        }
    }
    return pObj;
}

// =====================================================================================================================
PipelineBinaryCache::PipelineBinaryCache(
    VkAllocationCallbacks*    pAllocationCallbacks,
    const Vkgc::GfxIpVersion& gfxIp,
    uint32_t                  expectedEntries)
    :
    m_pAllocationCallbacks { pAllocationCallbacks },
    m_palAllocator         { pAllocationCallbacks },
    m_pPlatformKey         { nullptr },
    m_pTopLayer            { nullptr },
#if ICD_GPUOPEN_DEVMODE_BUILD
    m_pDevModeMgr          { nullptr },
    m_pReinjectionLayer    { nullptr },
    m_hashMapping          { 32, &m_palAllocator },
#endif
    m_pMemoryLayer         { nullptr },
    m_pCompressingLayer    { nullptr },
    m_expectedEntries      { expectedEntries },
    m_pArchiveLayer        { nullptr },
    m_openFiles            { &m_palAllocator },
    m_archiveLayers        { &m_palAllocator },
    m_pCacheAdapter        { nullptr }
{
    // Without copy constructor, a class type variable can't be initialized in initialization list with gcc 4.8.5.
    // Initialize m_gfxIp here instead to make gcc 4.8.5 work.
    m_gfxIp = gfxIp;
}

// =====================================================================================================================
PipelineBinaryCache::~PipelineBinaryCache()
{
    if (m_pCacheAdapter != nullptr)
    {
        m_pCacheAdapter->Destroy();
        m_pCacheAdapter = nullptr;
    }

    for (FileVector::Iter i = m_openFiles.Begin(); i.IsValid(); i.Next())
    {
        i.Get()->Destroy();
        FreeMem(i.Get());
    }

    m_openFiles.Clear();

    for (LayerVector::Iter i = m_archiveLayers.Begin(); i.IsValid(); i.Next())
    {
        i.Get()->Destroy();
        FreeMem(i.Get());
    }

    m_archiveLayers.Clear();

    if (m_pMemoryLayer != nullptr)
    {
        m_pMemoryLayer->Destroy();
        FreeMem(m_pMemoryLayer);
    }

    if (m_pCompressingLayer != nullptr)
    {
        m_pCompressingLayer->Destroy();
        FreeMem(m_pCompressingLayer);
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    if (m_pReinjectionLayer != nullptr)
    {
        m_pReinjectionLayer->Destroy();
    }
#endif
}

// =====================================================================================================================
// Allocates memory using the internal VkAllocationCallbacks. Uses the default memory alignment and the object system
// allocation scope.
// Any memory returned by this function must be freed with PipelineBinaryCache::FreeMem.
void* PipelineBinaryCache::AllocMem(
    size_t memSize) const
{
        VK_ASSERT(memSize > 0);
        return m_pAllocationCallbacks->pfnAllocation(m_pAllocationCallbacks->pUserData,
                                                     memSize,
                                                     VK_DEFAULT_MEM_ALIGN,
                                                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
}

// =====================================================================================================================
// Frees memory allocated by PipelineBinaryCache::AllocMem. No-op when passed a null pointer.
void PipelineBinaryCache::FreeMem(
    void* pMem) const
{
    if (pMem != nullptr)
    {
        m_pAllocationCallbacks->pfnFree(m_pAllocationCallbacks->pUserData, pMem);
    }
}

// =====================================================================================================================
// Query if a pipeline binary exists in cache
// Must call ReleaseCacheRef() when the flags contains AcquireEntryRef
Util::Result PipelineBinaryCache::QueryPipelineBinary(
    const CacheId*     pCacheId,
    uint32_t           flags,
    Util::QueryResult* pQuery)
{
    VK_ASSERT(m_pTopLayer != nullptr);

    uint32_t policy = Util::ICacheLayer::LinkPolicy::LoadOnQuery;
    // We have to make sure the Query is atomic, otherwise we could get unexpected result while running multi-thread
    // test case.
    m_entriesMutex.Lock();
    Util::Result result = m_pTopLayer->Query(pCacheId, policy, flags, pQuery);
    m_entriesMutex.Unlock();

    return result;
}

// =====================================================================================================================
// Query if a pipeline binary exists in cache
// Must call ReleaseCacheRef() when the flags contains AcquireEntryRef
Util::Result PipelineBinaryCache::WaitPipelineBinary(
    const CacheId*     pCacheId)
{
    VK_ASSERT(m_pTopLayer != nullptr);

    return m_pTopLayer->WaitForEntry(pCacheId);
}
// =====================================================================================================================
// Attempt to load a graphics pipeline binary from cache
Util::Result PipelineBinaryCache::LoadPipelineBinary(
    const CacheId* pCacheId,
    size_t*        pPipelineBinarySize,
    const void**   ppPipelineBinary) const
{
    VK_ASSERT(m_pTopLayer != nullptr);

    Util::QueryResult query  = {};
    Util::Result      result = m_pTopLayer->Query(pCacheId, 0, 0, &query);

    if (result == Util::Result::Success)
    {
        void* pOutputMem = AllocMem(query.dataSize);
        if (pOutputMem != nullptr)
        {
            result = m_pTopLayer->Load(&query, pOutputMem);

            if (result == Util::Result::Success)
            {
                *pPipelineBinarySize = query.dataSize;
                *ppPipelineBinary    = pOutputMem;
            }
            else
            {
                FreeMem(pOutputMem);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Attempt to store a binary into a cache chain
Util::Result PipelineBinaryCache::StorePipelineBinary(
    const CacheId*  pCacheId,
    size_t          pipelineBinarySize,
    const void*     pPipelineBinary)
{
    VK_ASSERT(m_pTopLayer != nullptr);

    Util::StoreFlags storeFlags  = {};
    storeFlags.enableFileCache   = true;
    storeFlags.enableCompression = true;

    return m_pTopLayer->Store(storeFlags, pCacheId, pPipelineBinary, pipelineBinarySize);
}

// =====================================================================================================================
Util::Result PipelineBinaryCache::ReleaseCacheRef(
    const Util::QueryResult* pQuery) const
{
    VK_ASSERT(m_pTopLayer != nullptr);
    return m_pTopLayer->ReleaseCacheRef(pQuery);
}

// =====================================================================================================================
Util::Result PipelineBinaryCache::GetCacheDataPtr(
    const Util::QueryResult* pQuery,
    const void**             ppData) const
{
    VK_ASSERT(m_pTopLayer != nullptr);
    return m_pTopLayer->GetCacheData(pQuery, ppData);
}

// =====================================================================================================================
Util::Result PipelineBinaryCache::EvictEntry(
    const Util::QueryResult* pQuery) const
{
    VK_ASSERT(m_pTopLayer != nullptr);

    return m_pTopLayer->Evict(&pQuery->hashId);
}

// =====================================================================================================================
Util::Result PipelineBinaryCache::MarkEntryBad(
    const Util::QueryResult* pQuery) const
{
    VK_ASSERT(m_pTopLayer != nullptr);

    return m_pTopLayer->MarkEntryBad(&pQuery->hashId);
}

// =====================================================================================================================
Util::Result PipelineBinaryCache::GetPipelineBinary(
    const Util::QueryResult* pQeuryId,
    void*                    pPipelineBinary) const
{
    VK_ASSERT(m_pTopLayer != nullptr);

    return m_pTopLayer->Load(pQeuryId, pPipelineBinary);
}

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// Introduces a mapping from an internal pipeline hash to a cache ID
void PipelineBinaryCache::RegisterHashMapping(
    const Pal::PipelineHash* pInternalPipelineHash,
    const CacheId*           pCacheId)
{
    VK_ASSERT(pInternalPipelineHash != nullptr);
    VK_ASSERT(pCacheId              != nullptr);

    if (m_pReinjectionLayer != nullptr)
    {
        Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> readWriteLock(&m_hashMappingLock);

        m_hashMapping.Insert(*pInternalPipelineHash, *pCacheId);
    }
}

// =====================================================================================================================
// Retrieves the cache ID that maps to the given internal pipeline hash, pCacheId is unchanged if no mapping found
PipelineBinaryCache::CacheId* PipelineBinaryCache::GetCacheIdForPipeline(
    const Pal::PipelineHash* pInternalPipelineHash)
{
    VK_ASSERT(pInternalPipelineHash != nullptr);

    CacheId* pCacheId = nullptr;

    if (m_pReinjectionLayer != nullptr)
    {
        Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> readWriteLock(&m_hashMappingLock);

        pCacheId = m_hashMapping.FindKey(*pInternalPipelineHash);
    }

    return pCacheId;
}

// =====================================================================================================================
// Attempt to load a binary from the reinjection cache layer
Util::Result PipelineBinaryCache::LoadReinjectionBinary(
    const CacheId* pInternalPipelineHash,
    size_t*        pPipelineBinarySize,
    const void**   ppPipelineBinary)
{
    Util::Result result = Util::Result::ErrorUnavailable;

    if (m_pReinjectionLayer != nullptr)
    {
        Util::QueryResult query = {};
        result = m_pReinjectionLayer->Query(pInternalPipelineHash, 0, 0, &query);

        if (result == Util::Result::Success)
        {
            void* pOutputMem = AllocMem(query.dataSize);

            if (pOutputMem != nullptr)
            {
                result = m_pReinjectionLayer->Load(&query, pOutputMem);

                if (result == Util::Result::Success)
                {
                    *pPipelineBinarySize = query.dataSize;
                    *ppPipelineBinary = pOutputMem;
                }
                else
                {
                    FreeMem(pOutputMem);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Attempt to store a binary into the reinjection cache layer
Util::Result PipelineBinaryCache::StoreReinjectionBinary(
    const CacheId*  pInternalPipelineHash,
    size_t          pipelineBinarySize,
    const void*     pPipelineBinary)
{
    Util::Result result = Util::Result::ErrorUnavailable;

    if (m_pReinjectionLayer != nullptr)
    {
        uint32_t gfxIpMajor = 0u;
        uint32_t gfxIpMinor = 0u;
        uint32_t gfxIpStepping = 0u;

        Util::Abi::PipelineAbiReader reader(&m_palAllocator, pPipelineBinary);
        reader.GetGfxIpVersion(&gfxIpMajor, &gfxIpMinor, &gfxIpStepping);

        if (gfxIpMajor == m_gfxIp.major &&
            gfxIpMinor == m_gfxIp.minor &&
            gfxIpStepping == m_gfxIp.stepping)
        {
            Util::StoreFlags storeFlags  = {};
            storeFlags.enableFileCache   = true;
            storeFlags.enableCompression = true;
            result = m_pReinjectionLayer->Store(storeFlags, pInternalPipelineHash, pPipelineBinary, pipelineBinarySize);
        }
        else
        {
            result = Util::Result::ErrorIncompatibleDevice;
        }
    }

    return result;
}

#endif
// =====================================================================================================================
// Free memory allocated by our allocator
void PipelineBinaryCache::FreePipelineBinary(
    const void* pPipelineBinary)
{
    FreeMem(const_cast<void*>(pPipelineBinary));
}

// =====================================================================================================================
// Destroy PipelineBinaryCache itself
void PipelineBinaryCache::Destroy()
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    if (m_pDevModeMgr != nullptr)
    {
        m_pDevModeMgr->DeregisterPipelineCache(this);
    }
#endif

    VkAllocationCallbacks* pAllocationCallbacks = m_pAllocationCallbacks;
    void* pMem = this;
    Util::Destructor(this);
    pAllocationCallbacks->pfnFree(pAllocationCallbacks->pUserData, pMem);
}

// =====================================================================================================================
// Build the cache layer chain
VkResult PipelineBinaryCache::Initialize(
    const RuntimeSettings&    settings,
    bool                      createArchiveLayers,
    const char*               pDefaultCacheFilePath,
    const Util::IPlatformKey* pKey)
{
    VkResult result = VK_SUCCESS;

    if (pKey != nullptr)
    {
        m_pPlatformKey = pKey;
    }
    else
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    if (result == VK_SUCCESS)
    {
        result = InitLayers(pDefaultCacheFilePath, createArchiveLayers, settings);
    }

    if (result == VK_SUCCESS)
    {
        result = OrderLayers(settings);
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    if ((result == VK_SUCCESS) &&
        (m_pReinjectionLayer != nullptr))
    {
        Util::Result palResult = m_pDevModeMgr->RegisterPipelineCache(
            this,
            settings.devModePipelineUriServicePostSizeLimit);

        if (palResult == Util::Result::Success)
        {
            palResult = m_hashMapping.Init();
        }

        if (palResult != Util::Result::Success)
        {
            m_pReinjectionLayer->Destroy();
            m_pReinjectionLayer = nullptr;

            // Fail silently so that the pipeline cache may still be used for other purposes.
            PAL_ASSERT_ALWAYS();
        }
    }
#endif

    if (result == VK_SUCCESS)
    {
        m_pCacheAdapter = CacheAdapter::Create(this);
        VK_ALERT(m_pCacheAdapter == nullptr);
    }

    return result;
}

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// Initialize reinjection cache layer
VkResult PipelineBinaryCache::InitReinjectionLayer(
    const RuntimeSettings& settings)
{
    VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;

    if (m_pDevModeMgr != nullptr)
    {
        Util::MemoryCacheCreateInfo info = {};
        Util::AllocCallbacks        allocCbs = {
            m_pAllocationCallbacks,
            allocator::PalAllocFuncDelegator,
            allocator::PalFreeFuncDelegator
        };

        info.baseInfo.pCallbacks = &allocCbs;
        info.maxObjectCount = SIZE_MAX;
        info.maxMemorySize = SIZE_MAX;
        info.evictOnFull = false;
        info.evictDuplicates = true;

        size_t memSize = Util::GetMemoryCacheLayerSize(&info);
        void*  pMem    = AllocMem(memSize);

        if (pMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            result = PalToVkResult(
                Util::CreateMemoryCacheLayer(
                    &info,
                    pMem,
                    &m_pReinjectionLayer));

            if (result != VK_SUCCESS)
            {
                FreeMem(pMem);
            }
        }

        if (result == VK_SUCCESS)
        {
            result = PalToVkResult(InjectBinariesFromDirectory(settings));
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function for converting a 32-digit hexadecimal (in C string format) to a Hash128 object
Util::Hash128 ParseHash128(const char* str)
{
    Util::Hash128   hash;
    const uint32_t  stride = 2u; // 1 byte = 2 hex digits
    const uint32_t  byteCount = (sizeof(hash.bytes) / sizeof(*hash.bytes));
    uint32_t        stringIndex = 0u;
    char            buffer[stride + 1u];
    buffer[stride] = '\n';

    // Using little-endian byte order
    for (uint32_t byteIndex = 0u; byteIndex < byteCount; byteIndex++)
    {
        stringIndex = (byteCount - byteIndex - 1) * stride;
        memcpy(buffer, &str[stringIndex], stride);
        hash.bytes[byteIndex] = static_cast<uint8_t>(strtoul(buffer, nullptr, 16));
    }

    return hash;
}

// =====================================================================================================================
// Adds binaries to reinjection cache layer from a directory source
Util::Result PipelineBinaryCache::InjectBinariesFromDirectory(
    const RuntimeSettings& settings)
{
    Util::Result result = Util::Result::Success;

    if (settings.devModeElfReplacementDirectoryEnable)
    {
        Util::File      file;
        char            filePath[Util::PathBufferLen] = {};
        uint32_t        fileCount                     = 0u;
        const char**    ppFileNames                   = nullptr;
        size_t          fileNameBufferSize            = 0u;
        void*           pFileNameBuffer               = nullptr;
        size_t          dirLength                     = strlen(settings.devModeElfReplacementDirectory) + 1u;

        Util::Hash128   pipelineHash                  = {};
        size_t          pipelineBinarySize            = 0u;
        void*           pPipelineBinary               = nullptr;

        // Get the number of files in dir and the size of the buffer to hold their names
        result = Util::ListDir(
            settings.devModeElfReplacementDirectory,
            &fileCount,
            nullptr,
            &fileNameBufferSize,
            nullptr);

        if (fileCount == 0u)
        {
            return result;
        }

        if (result == Util::Result::Success)
        {
            // Allocate space for ppFileNames and pFileNameBuffer
            ppFileNames = (const char**)AllocMem(sizeof(const char*) * fileCount);
            pFileNameBuffer = AllocMem(fileNameBufferSize);

            // Populate ppFileNames and pFileNameBuffer
            result = Util::ListDir(
                settings.devModeElfReplacementDirectory,
                &fileCount,
                ppFileNames,
                &fileNameBufferSize,
                pFileNameBuffer);

            if (result != Util::Result::Success)
            {
                FreeMem(pFileNameBuffer);
                FreeMem(ppFileNames);
            }
        }

        if (result == Util::Result::Success)
        {
            // Store each file into cache
            Util::Strncpy(filePath, settings.devModeElfReplacementDirectory, sizeof(filePath));
            Util::Strncat(filePath, sizeof(filePath), "\\");
            for (uint32_t fileIndex = 0; fileIndex < fileCount; fileIndex++)
            {
                filePath[dirLength] = '\0';
                Util::Strncat(filePath, sizeof(filePath), ppFileNames[fileIndex]);

                ppFileNames[fileIndex] = strstr(ppFileNames[fileIndex], "_0x");

                if ((ppFileNames[fileIndex] != nullptr) &&
                    (strlen(ppFileNames[fileIndex]) >= 32))
                {
                    ppFileNames[fileIndex] += 3u;
                    pipelineHash = ParseHash128(ppFileNames[fileIndex]);

                    if (Util::File::Exists(filePath))
                    {
                        pipelineBinarySize = Util::File::GetFileSize(filePath);
                        pPipelineBinary = AllocMem(pipelineBinarySize);

                        if (pPipelineBinary != nullptr)
                        {
                            if (file.Open(
                                filePath,
                                Util::FileAccessRead | Util::FileAccessBinary) == Util::Result::Success)
                            {
                                if (file.Read(pPipelineBinary, pipelineBinarySize, nullptr) == Util::Result::Success)
                                {
                                    StoreReinjectionBinary(&pipelineHash, pipelineBinarySize, pPipelineBinary);
                                }
                                else
                                {
                                    VK_NEVER_CALLED();
                                }

                                file.Close();
                            }
                            else
                            {
                                VK_NEVER_CALLED();
                            }
                        }

                        FreeMem(pPipelineBinary);
                    }
                    else
                    {
                        VK_NEVER_CALLED();
                    }
                }
            }

            FreeMem(pFileNameBuffer);
            FreeMem(ppFileNames);
        }
    }

    return result;
}
#endif

// =====================================================================================================================
// Initialize memory layer
VkResult PipelineBinaryCache::InitMemoryCacheLayer(
    const RuntimeSettings& settings)
{
    VK_ASSERT(m_pMemoryLayer == nullptr);

    Util::AllocCallbacks allocCallbacks = {};
    allocCallbacks.pClientData = m_pAllocationCallbacks;
    allocCallbacks.pfnAlloc    = allocator::PalAllocFuncDelegator;
    allocCallbacks.pfnFree     = allocator::PalFreeFuncDelegator;

    Util::MemoryCacheCreateInfo createInfo = {};
    createInfo.baseInfo.pCallbacks = &allocCallbacks;
    createInfo.maxObjectCount      = SIZE_MAX;

    // Reason: CTS generates a large number of cache applications and cause insufficient memory in 32-bit system.
    // Purpose: To limit the maximun value of MemorySize in 32-bit system.
#ifdef ICD_X86_BUILD
    createInfo.maxMemorySize       = 192 * 1024 * 1024;
#else
    createInfo.maxMemorySize       = SIZE_MAX;
#endif

    createInfo.expectedEntries     = m_expectedEntries;
    createInfo.evictOnFull         = true;
    createInfo.evictDuplicates     = true;

    size_t layerSize = Util::GetMemoryCacheLayerSize(&createInfo);
    void* pMem = AllocMem(layerSize);

    VkResult result = VK_SUCCESS;

    if (pMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        result = PalToVkResult(CreateMemoryCacheLayer(&createInfo, pMem, &m_pMemoryLayer));
        VK_ASSERT(result == VK_SUCCESS);

        if (result != VK_SUCCESS)
        {
            FreeMem(pMem);
        }
    }

    return result;
}

// =====================================================================================================================
// Initialize compression layer
VkResult PipelineBinaryCache::InitCompressingLayer(
    const RuntimeSettings& settings)
{
    VK_ASSERT(m_pCompressingLayer == nullptr);

    VkResult result = VK_SUCCESS;

    const size_t layerSize = Util::GetCompressingCacheLayerSize();
    void* pMem = AllocMem(layerSize);

    if (pMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        Util::CompressingCacheLayerCreateInfo createInfo = {};
        createInfo.useHighCompression = settings.pipelineCacheUseHighCompression;

        result = PalToVkResult(Util::CreateCompressingCacheLayer(&createInfo, pMem, &m_pCompressingLayer));
        VK_ASSERT(result == VK_SUCCESS);

        if (result != VK_SUCCESS)
        {
            FreeMem(pMem);
        }
    }

    return result;
}

// =====================================================================================================================
// Open an archive file from disk for read
Util::IArchiveFile* PipelineBinaryCache::OpenReadOnlyArchive(
    const char* pFilePath,
    const char* pFileName,
    size_t      bufferSize)
{
    VK_ASSERT(pFilePath != nullptr);
    VK_ASSERT(pFileName != nullptr);

    Util::ArchiveFileOpenInfo info  = {};
    Util::IArchiveFile*       pFile = nullptr;

    Util::AllocCallbacks allocCbs = {
        m_pAllocationCallbacks,
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    info.pMemoryCallbacks        = &allocCbs;
    info.pFilePath               = pFilePath;
    info.pFileName               = pFileName;
    info.pPlatformKey            = m_pPlatformKey;
    info.archiveType             = ArchiveType;
    info.useStrictVersionControl = true;
    info.allowWriteAccess        = false;
    info.allowCreateFile         = false;
    info.allowAsyncFileIo        = true;
    info.useBufferedReadMemory   = (bufferSize > 0);
    info.maxReadBufferMem        = bufferSize;

    size_t memSize = Util::GetArchiveFileObjectSize(&info);
    void*  pMem    = AllocMem(memSize);

    if (pMem != nullptr)
    {
        Util::Result openResult = Util::OpenArchiveFile(&info, pMem, &pFile);

        if (openResult == Util::Result::Success)
        {
            if (info.useBufferedReadMemory)
            {
                pFile->Preload(0, info.maxReadBufferMem);
            }
        }
        else
        {
            FreeMem(pMem);
            pFile = nullptr;
        }
    }

    return pFile;
}

// =====================================================================================================================
// Open an archive file from disk for read + write
Util::IArchiveFile* PipelineBinaryCache::OpenWritableArchive(
    const char* pFilePath,
    const char* pFileName,
    size_t      bufferSize)
{
    VK_ASSERT(pFilePath != nullptr);
    VK_ASSERT(pFileName != nullptr);

    Util::ArchiveFileOpenInfo info  = {};
    Util::IArchiveFile*       pFile = nullptr;

    Util::AllocCallbacks allocCbs = {
        m_pAllocationCallbacks,
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    info.pMemoryCallbacks        = &allocCbs;
    info.pFilePath               = pFilePath;
    info.pFileName               = pFileName;
    info.pPlatformKey            = m_pPlatformKey;
    info.archiveType             = ArchiveType;
    info.useStrictVersionControl = true;
    info.allowWriteAccess        = true;
    info.allowCreateFile         = true;
    info.allowAsyncFileIo        = true;
    info.useBufferedReadMemory   = (bufferSize > 0);
    info.maxReadBufferMem        = bufferSize;

    size_t memSize = Util::GetArchiveFileObjectSize(&info);
    void*  pMem    = AllocMem(memSize);

    if (pMem != nullptr)
    {
        Util::Result openResult = Util::OpenArchiveFile(&info, pMem, &pFile);

        if (openResult == Util::Result::ErrorIncompatibleLibrary)
        {
            if (Util::DeleteArchiveFile(&info)  == Util::Result::Success)
            {
                openResult = Util::OpenArchiveFile(&info, pMem, &pFile);
            }
        }

        if (openResult == Util::Result::Success)
        {
            if (info.useBufferedReadMemory)
            {
                pFile->Preload(0, info.maxReadBufferMem);
            }
        }
        else
        {
            FreeMem(pMem);
            pFile = nullptr;
        }
    }

    return pFile;
}

// =====================================================================================================================
// Create a cache layer from an open file
Util::ICacheLayer* PipelineBinaryCache::CreateFileLayer(
    Util::IArchiveFile* pFile)
{
    VK_ASSERT(pFile != nullptr);
    Util::ArchiveFileCacheCreateInfo info   = {};
    Util::ICacheLayer*               pLayer = nullptr;

    Util::AllocCallbacks allocCbs = {
        m_pAllocationCallbacks,
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    info.baseInfo.pCallbacks = &allocCbs;
    info.pFile               = pFile;
    info.pPlatformKey        = m_pPlatformKey;
    info.dataTypeId          = ElfType;

    size_t memSize = Util::GetArchiveFileCacheLayerSize(&info);
    void*  pMem    = AllocMem(memSize);

    if (pMem != nullptr)
    {
        if (Util::CreateArchiveFileCacheLayer(&info, pMem, &pLayer) != Util::Result::Success)
        {
            FreeMem(pMem);
            pLayer = nullptr;
        }
    }

    return pLayer;
}

// =====================================================================================================================
// Open the archive file and initialize its cache layer
VkResult PipelineBinaryCache::InitArchiveLayers(
    const char*            pDefaultCacheFilePath,
    const RuntimeSettings& settings)
{
    VkResult result = VK_SUCCESS;

    // Buffer to hold constructed path
    char pathBuffer[Util::PathBufferLen] = {};
    // If the environment variable AMD_VK_PIPELINE_CACHE_PATH is set, obey it first
    const char* pCachePath = getenv(EnvVarPath);

    // otherwise fetch the cache location from PAL
    if (pCachePath == nullptr)
    {
        // Default to a fail state here in case we cannot build the default path
        result = VK_ERROR_INITIALIZATION_FAILED;

        if (settings.usePipelineCachingDefaultLocation)
        {
            const char* pCacheSubPath = settings.pipelineCachingDefaultLocation;
            const char* pUserDataPath = pDefaultCacheFilePath;

            if ((pCacheSubPath != nullptr) &&
                (pUserDataPath != nullptr))
            {
                // Construct the path in the local buffer. Consider it valid if not empty
                if (Util::Snprintf(pathBuffer, sizeof(pathBuffer), "%s%s", pUserDataPath, pCacheSubPath) > 0)
                {
                    pCachePath = pathBuffer;

                    if (settings.allowCleanUpCacheDirectory)
                    {
                        uint64 totalSize = 0, oldestTime = 0;
                        if (Util::GetStatusOfDir(pCachePath, &totalSize, &oldestTime) == Util::Result::Success)
                        {
                            if (totalSize >= settings.pipelineCacheDefaultLocationLimitation)
                            {
                                Util::RemoveFilesOfDirOlderThan(pCachePath,
                                                                oldestTime + settings.thresholdOfCleanUpCache);
                            }
                        }
                    }
                    result = VK_SUCCESS;
                }
            }
        }
    }

    // Load the primary archive file
    if (result == VK_SUCCESS)
    {
        // Assume that the first layer we open should be the "primary" source and optimize its memory access
        constexpr size_t PrimaryLayerBufferSize   = 64 * 1024 * 1024;
        constexpr size_t SecondaryLayerBufferSize = 8 * 1024 * 1024;

        // Open the optional read only cache file. This may fail gracefully
        const char* const  pThirdPartyFileName = getenv(EnvVarReadOnlyFileName);
        Util::ICacheLayer* pThirdPartyLayer    = nullptr;

        if (pThirdPartyFileName != nullptr)
        {
            Util::IArchiveFile* pFile = OpenReadOnlyArchive(pCachePath, pThirdPartyFileName, PrimaryLayerBufferSize);

            if (pFile != nullptr)
            {
                Util::ICacheLayer* pLayer = CreateFileLayer(pFile);

                if (pLayer != nullptr)
                {
                    m_openFiles.PushBack(pFile);
                    m_archiveLayers.PushBack(pLayer);

                    pThirdPartyLayer = pLayer;

                    // If third party layer is given to us, have it be the primary layer
                    m_pArchiveLayer = pLayer;
                }
                else
                {
                    pFile->Destroy();
                    FreeMem(pFile);
                }
            }
        }

        // Buffer to hold constructed filename
        char nameBuffer[Util::FilenameBufferLen] = {};

        const char* const pCacheFileName = getenv(EnvVarFileName);

        if (pCacheFileName == nullptr)
        {
            // If no naming scheme is given, compute the name by AppHash + PlatformKey
            Util::Hash128 appHash        = {};
            char*         pExecutablePtr = nullptr;

            Util::Result palResult = Util::GetExecutableName(nameBuffer, &pExecutablePtr, sizeof(nameBuffer));
            VK_ASSERT(IsErrorResult(palResult) == false);
            Util::MetroHash128::Hash(reinterpret_cast<const uint8_t*>(nameBuffer), sizeof(nameBuffer), appHash.bytes);

            Util::Snprintf(
                nameBuffer,
                sizeof(nameBuffer),
                "%llX%llX",
                Util::MetroHash::Compact64(&appHash),
                m_pPlatformKey->GetKey64());
        }
        else
        {
            Util::Strncpy(nameBuffer, pCacheFileName, sizeof(nameBuffer));
        }

        Util::ICacheLayer* pWriteLayer    = nullptr;
        Util::ICacheLayer* pLastReadLayer = pThirdPartyLayer;

        char* const  nameEnd        = &nameBuffer[strnlen(nameBuffer, sizeof(nameBuffer))];
        const size_t charsRemaining = sizeof(nameBuffer) - (nameEnd - nameBuffer);

        constexpr int MaxAttempts = 10;
        for (int attemptCt = 0; attemptCt < MaxAttempts; ++attemptCt)
        {
            size_t bufferSize = (m_pArchiveLayer == nullptr) ? PrimaryLayerBufferSize : SecondaryLayerBufferSize;

            // Create the final name based off the attempt
            *nameEnd = '\0';
            if (attemptCt == 0)
            {
                Util::Strncat(nameBuffer, sizeof(nameBuffer), ".parc");
            }
            else
            {
                Util::Snprintf(nameEnd, charsRemaining, "_%d.parc", attemptCt);
            }

            Util::IArchiveFile* pFile = nullptr;
            bool                readOnly = false;

            if (pWriteLayer == nullptr)
            {
                pFile = OpenWritableArchive(pCachePath, nameBuffer, bufferSize);
            }

            // Attempt to open the file as a read only instead if we failed
            if (pFile == nullptr)
            {
                pFile    = OpenReadOnlyArchive(pCachePath, nameBuffer, bufferSize);
                readOnly = true;
            }

            // Only create the layer if one of the two above calls successfully openned the file
            if (pFile != nullptr)
            {
                Util::ICacheLayer* pLayer = CreateFileLayer(pFile);

                if (pLayer != nullptr)
                {
                    if (m_openFiles.PushBack(pFile) != Pal::Result::_Success)
                    {
                        pFile->Destroy();
                        FreeMem(pFile);
                        result = VK_ERROR_INITIALIZATION_FAILED;
                    }

                    if (m_archiveLayers.PushBack(pLayer) != Pal::Result::_Success)
                    {
                        pLayer->Destroy();
                        FreeMem(pLayer);
                        result = VK_ERROR_INITIALIZATION_FAILED;
                    }

                    if (result == VK_SUCCESS)
                    {
                        if (pLastReadLayer != nullptr)
                        {
                            if (pLastReadLayer != pWriteLayer)
                            {
                                // Connect to previous read layer as read-through / write-through + skip
                                pLastReadLayer->SetLoadPolicy(Util::ICacheLayer::LinkPolicy::PassCalls);
                                pLastReadLayer->SetStorePolicy(Util::ICacheLayer::LinkPolicy::Skip |
                                                               Util::ICacheLayer::LinkPolicy::PassData);
                            }

                            pLastReadLayer->Link(pLayer);
                        }

                        // Ensure the first read or write layer is set to "top" of the chain.
                        if (m_pArchiveLayer == nullptr)
                        {
                            m_pArchiveLayer = pLayer;
                        }

                        if (readOnly)
                        {
                            pLastReadLayer = pLayer;
                        }
                        else
                        {
                            pWriteLayer = pLayer;
                            pLastReadLayer = pLayer;
                        }
                    }
                }
                else
                {
                    pFile->Destroy();
                    FreeMem(pFile);
                }
            }
        }

        if (m_pArchiveLayer == nullptr)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }

        PAL_ALERT_MSG(pWriteLayer == nullptr, "No valid write layer for cache. No data will be written out.");
    }

    return result;
}

// =====================================================================================================================
// Initialize layers (a single layer that supports storage for binaries needs to succeed)
VkResult PipelineBinaryCache::InitLayers(
    const char*            pDefaultCacheFilePath,
    bool                   createArchiveLayers,
    const RuntimeSettings& settings)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    bool injectionLayerOnline = (InitReinjectionLayer(settings) >= VK_SUCCESS);
#else
    bool injectionLayerOnline = false;
#endif

    bool memoryLayerOnline = (InitMemoryCacheLayer(settings) >= VK_SUCCESS);

    bool archiveLayerOnline = createArchiveLayers && (InitArchiveLayers(pDefaultCacheFilePath, settings) >= VK_SUCCESS);

    if (((settings.pipelineCacheCompression == PipelineCacheCompressInMemory) && memoryLayerOnline) ||
        ((settings.pipelineCacheCompression != PipelineCacheCompressDisabled) && archiveLayerOnline))
    {
        InitCompressingLayer(settings);
    }

    return (injectionLayerOnline || memoryLayerOnline || archiveLayerOnline)
        ? VK_SUCCESS
        : VK_ERROR_INITIALIZATION_FAILED;
}

VkResult PipelineBinaryCache::AddLayerToChain(
    Util::ICacheLayer*  pLayer,
    Util::ICacheLayer** pBottomLayer)
{
    VkResult result = VK_SUCCESS;

    if (pLayer != nullptr)
    {
        if (m_pTopLayer == nullptr)
        {
            m_pTopLayer = pLayer;
            *pBottomLayer = pLayer;
        }
        else
        {
            if ((*pBottomLayer)->Link(pLayer) == Util::Result::Success)
            {
                *pBottomLayer = pLayer;
            }
            else
            {
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Order the layers for desired caching behaviour
VkResult PipelineBinaryCache::OrderLayers(
    const RuntimeSettings& settings)
{
    VkResult result                 = VK_SUCCESS;
    Util::ICacheLayer* pBottomLayer = nullptr;
    m_pTopLayer                     = nullptr;

    if ((m_pCompressingLayer != nullptr) && (result == VK_SUCCESS) &&
        (settings.pipelineCacheCompression == PipelineCacheCompressInMemory))
    {
        result = AddLayerToChain(m_pCompressingLayer, &pBottomLayer);
    }

    if (result == VK_SUCCESS)
    {
        result = AddLayerToChain(m_pMemoryLayer, &pBottomLayer);
    }

    if ((m_pCompressingLayer != nullptr) && (result == VK_SUCCESS) &&
        (settings.pipelineCacheCompression == PipelineCacheCompressOnDisk))
    {
        result = AddLayerToChain(m_pCompressingLayer, &pBottomLayer);
    }

    if (result == VK_SUCCESS)
    {
        result = AddLayerToChain(m_pArchiveLayer, &pBottomLayer);
    }

    if ((result == VK_SUCCESS) &&
        (m_pTopLayer == nullptr))
    {
        // The cache is not very useful if no layers are available.
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Copies the pipeline cache data to the memory blob provided by the calling function.
//
// NOTE: It is expected that the calling function has not used this pipeline cache since querying the size
VkResult PipelineBinaryCache::Serialize(
    void*   pBlob,    // [out] System memory pointer where the serialized data should be placed
    size_t* pSize)    // [in,out] Size of the memory pointed to by pBlob. If the value stored in pSize is zero then no
                      // data will be copied and instead the size required for serialization will be returned in pSize
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;

    if (m_pMemoryLayer != nullptr)
    {
        if (*pSize == 0)
        {
            size_t curCount, curDataSize;

            result = PalToVkResult(Util::GetMemoryCacheLayerCurSize(m_pMemoryLayer, &curCount, &curDataSize));
            if (result == VK_SUCCESS)
            {
                *pSize = PipelineBinaryCacheSerializer::CalculateAnticipatedCacheBlobSize(curCount, curDataSize);
            }
        }
        else
        {
            size_t curCount, curDataSize;

            result = PalToVkResult(Util::GetMemoryCacheLayerCurSize(m_pMemoryLayer, &curCount, &curDataSize));
            if (result == VK_SUCCESS)
            {
                PipelineBinaryCacheSerializer serializer;
                if (serializer.Initialize(*pSize, pBlob) == Util::Result::Success)
                {
                    Util::AutoBuffer<Util::Hash128, 8, PalAllocator> cacheIds(curCount, &m_palAllocator);
                    result = PalToVkResult(Util::GetMemoryCacheLayerHashIds(m_pMemoryLayer, curCount, &cacheIds[0]));
                    for (uint32_t i = 0; result == VK_SUCCESS && i < curCount; i++)
                    {
                        const void*      pBinaryCacheData = nullptr;
                        BinaryCacheEntry entry            = {cacheIds[i], 0};

                        result = PalToVkResult(LoadPipelineBinary(&entry.hashId, &entry.dataSize, &pBinaryCacheData));
                        if (result == VK_SUCCESS)
                        {
                            result = PalToVkResult(serializer.AddPipelineBinary(&entry, pBinaryCacheData));
                            FreeMem(const_cast<void*>(pBinaryCacheData));
                        }
                    }
                    result = PalToVkResult(serializer.Finalize(m_pAllocationCallbacks,
                                                               m_pPlatformKey,
                                                               nullptr,
                                                               nullptr));
                }
                else
                {
                    result = VK_ERROR_INITIALIZATION_FAILED;
                }
            }
        }
    }
    return result;
}

// =====================================================================================================================
// Merge the pipeline cache data into one
//
VkResult PipelineBinaryCache::Merge(
    uint32_t                    srcCacheCount,
    const PipelineBinaryCache** ppSrcCaches)
{
    Pal::Result result = Pal::Result::ErrorInitializationFailed;

    if (m_pMemoryLayer != nullptr)
    {
        for (uint32_t i = 0; i < srcCacheCount; i++)
        {
            Util::ICacheLayer* pMemoryLayer = ppSrcCaches[i]->GetMemoryLayer();
            size_t curCount, curDataSize;

            result = Util::GetMemoryCacheLayerCurSize(pMemoryLayer, &curCount, &curDataSize);

            if ((result == Pal::Result::Success) && (curCount > 0))
            {
                Util::AutoBuffer<Util::Hash128, 8, PalAllocator> cacheIds(curCount, &m_palAllocator);

                result = Util::GetMemoryCacheLayerHashIds(pMemoryLayer, curCount, &cacheIds[0]);
                if (result == Pal::Result::Success)
                {
                    for (uint32_t j = 0; j < curCount; j++)
                    {
                        size_t           dataSize;
                        const void*      pBinaryCacheData;

                        result = ppSrcCaches[i]->LoadPipelineBinary(&cacheIds[j], &dataSize, &pBinaryCacheData);
                        if (result == Pal::Result::Success)
                        {
                            result = StorePipelineBinary(&cacheIds[j], dataSize, pBinaryCacheData);
                            FreeMem(const_cast<void*>(pBinaryCacheData));

                            // Do not break for success cases or an already existing cache entry
                            if ((result != Pal::Result::Success) && (result != Pal::Result::AlreadyExists))
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    return (((result == Pal::Result::Success) ||
             (result == Pal::Result::AlreadyExists))?
            VK_SUCCESS : PalToVkResult(result));
}

} // namespace vk
