/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 39
#define Vkgc Llpc
#endif

#include "include/pipeline_binary_cache.h"
#include "include/vk_physical_device.h"

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
#include <limits.h>
#include <string.h>

namespace vk
{
#if defined(__unix__)
#define _MAX_FNAME NAME_MAX
#endif

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

static Util::Result CalculateHashId(
    Instance*                   pInstance,
    const Util::IPlatformKey*   pPlatformKey,
    const void*                 pData,
    size_t                      dataSize,
    uint8_t*                    pHashId)
{
    Util::Result        result          = Util::Result::Success;
    Util::IHashContext* pContext        = nullptr;
    size_t              contextSize     = pPlatformKey->GetKeyContext()->GetDuplicateObjectSize();
    void*               pContextMem     = pInstance->AllocMem(
                                            contextSize,
                                            VK_DEFAULT_MEM_ALIGN,
                                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pContextMem != nullptr)
    {
        result = pPlatformKey->GetKeyContext()->Duplicate(pContextMem, &pContext);
    }
    if (result == Util::Result::Success)
    {
        result = pContext->AddData(pData, dataSize);
    }
    if (result == Util::Result::Success)
    {
        result = pContext->Finish(pHashId);
    }
    if (pContext != nullptr)
    {
        pContext->Destroy();
    }
    if (pContextMem != nullptr)
    {
        pInstance->FreeMem(pContextMem);
    }

    return result;
}

bool PipelineBinaryCache::IsValidBlob(
    const PhysicalDevice* pPhysicalDevice,
    size_t dataSize,
    const void* pData)
{
    bool     isValid            = false;
    size_t   blobSize           = dataSize;
    auto pBinaryPrivateHeader   = static_cast<const PipelineBinaryCachePrivateHeader*>(pData);
    uint8_t  hashId[SHA_DIGEST_LENGTH];

    pData         = Util::VoidPtrInc(pData, sizeof(PipelineBinaryCachePrivateHeader));
    blobSize     -= sizeof(PipelineBinaryCachePrivateHeader);

    if (pPhysicalDevice->GetPlatformKey() != nullptr)
    {
        Util::Result        result          = CalculateHashId(
                                                pPhysicalDevice->Manager()->VkInstance(),
                                                pPhysicalDevice->GetPlatformKey(),
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
    Instance*                 pInstance,
    size_t                    initDataSize,
    const void*               pInitData,
    bool                      internal,
    const Vkgc::GfxIpVersion& gfxIp,
    const PhysicalDevice*     pPhysicalDevice)
{
    PipelineBinaryCache* pObj = nullptr;
    void*                pMem = pInstance->AllocMem(sizeof(PipelineBinaryCache), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (pMem != nullptr)
    {
        pObj = VK_PLACEMENT_NEW(pMem) PipelineBinaryCache(pInstance, gfxIp, internal);

        if (pObj->Initialize(pPhysicalDevice) != VK_SUCCESS)
        {
            pObj->Destroy();
            pInstance->FreeMem(pMem);
            pObj = nullptr;
        }
        else if ((pInitData != nullptr) &&
                 (initDataSize > (sizeof(BinaryCacheEntry) + sizeof(PipelineBinaryCachePrivateHeader))))
        {
            const void* pBlob           = pInitData;
            size_t   blobSize           = initDataSize;
            constexpr size_t EntrySize  = sizeof(BinaryCacheEntry);

            pBlob         = Util::VoidPtrInc(pBlob, sizeof(PipelineBinaryCachePrivateHeader));
            blobSize     -= sizeof(PipelineBinaryCachePrivateHeader);
            while (blobSize > EntrySize)
            {
                const BinaryCacheEntry* pEntry  = static_cast<const BinaryCacheEntry*>(pBlob);
                const void*             pData   = Util::VoidPtrInc(pBlob, sizeof(BinaryCacheEntry));
                const size_t entryAndDataSize   = pEntry->dataSize + sizeof(BinaryCacheEntry);

                if (blobSize >= entryAndDataSize)
                {
                    //add to cache
                    Util::Result result = pObj->StorePipelineBinary(&pEntry->hashId, pEntry->dataSize, pData);
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
    Instance*                 pInstance,
    const Vkgc::GfxIpVersion& gfxIp,
    bool                      internal)
    :
    m_pInstance        { pInstance },
    m_pPlatformKey     { nullptr },
    m_pTopLayer        { nullptr },
#if ICD_GPUOPEN_DEVMODE_BUILD
    m_pReinjectionLayer{ nullptr },
    m_hashMapping      { 32, pInstance->Allocator() },
#endif
    m_pMemoryLayer     { nullptr },
    m_pArchiveLayer    { nullptr },
    m_openFiles        { pInstance->Allocator() },
    m_archiveLayers    { pInstance->Allocator() },
    m_isInternalCache  { internal }
{
    // Without copy constructor, a class type variable can't be initialized in initialization list with gcc 4.8.5.
    // Initialize m_gfxIp here instead to make gcc 4.8.5 work.
    m_gfxIp = gfxIp;

}

// =====================================================================================================================
PipelineBinaryCache::~PipelineBinaryCache()
{
    for (FileVector::Iter i = m_openFiles.Begin(); i.IsValid(); i.Next())
    {
        i.Get()->Destroy();
        m_pInstance->FreeMem(i.Get());
    }

    m_openFiles.Clear();

    for (LayerVector::Iter i = m_archiveLayers.Begin(); i.IsValid(); i.Next())
    {
        i.Get()->Destroy();
        m_pInstance->FreeMem(i.Get());
    }

    m_archiveLayers.Clear();

    if (m_pMemoryLayer != nullptr)
    {
        m_pMemoryLayer->Destroy();
        m_pInstance->FreeMem(m_pMemoryLayer);
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    if (m_pReinjectionLayer != nullptr)
    {
        m_pReinjectionLayer->Destroy();
    }
#endif
}

// =====================================================================================================================
// Query if a pipeline binary exists in cache
Util::Result PipelineBinaryCache::QueryPipelineBinary(
    const CacheId*     pCacheId,
    Util::QueryResult* pQuery)
{
    VK_ASSERT(m_pTopLayer != nullptr);

    return m_pTopLayer->Query(pCacheId, pQuery);
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
    Util::Result      result = m_pTopLayer->Query(pCacheId, &query);

    if (result == Util::Result::Success)
    {
        void* pOutputMem = m_pInstance->AllocMem(
            query.dataSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
                m_pInstance->FreeMem(pOutputMem);
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

    return m_pTopLayer->Store(pCacheId, pPipelineBinary, pipelineBinarySize);
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
        result = m_pReinjectionLayer->Query(pInternalPipelineHash, &query);

        if (result == Util::Result::Success)
        {
            void* pOutputMem = m_pInstance->AllocMem(
                query.dataSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
                    m_pInstance->FreeMem(pOutputMem);
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

        Util::Abi::PipelineAbiReader reader(m_pInstance->Allocator(), pPipelineBinary);
        reader.GetGfxIpVersion(&gfxIpMajor, &gfxIpMinor, &gfxIpStepping);

        if (gfxIpMajor == m_gfxIp.major &&
            gfxIpMinor == m_gfxIp.minor &&
            gfxIpStepping == m_gfxIp.stepping)
        {
            result = m_pReinjectionLayer->Store(pInternalPipelineHash, pPipelineBinary, pipelineBinarySize);
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
    if (pPipelineBinary != nullptr)
    {
        m_pInstance->FreeMem(const_cast<void*>(pPipelineBinary));
    }
}

// =====================================================================================================================
// Build the cache layer chain
VkResult PipelineBinaryCache::Initialize(
    const PhysicalDevice* pPhysicalDevice)
{
    VkResult result = VK_SUCCESS;

    const RuntimeSettings& settings = pPhysicalDevice->GetRuntimeSettings();

    if (result == VK_SUCCESS)
    {
        m_pPlatformKey = pPhysicalDevice->GetPlatformKey();
    }

    if (m_pPlatformKey == nullptr)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    if (result == VK_SUCCESS)
    {
        result = InitLayers(pPhysicalDevice, m_isInternalCache, settings);
    }

    if (result == VK_SUCCESS)
    {
        result = OrderLayers(settings);
    }

#if ICD_GPUOPEN_DEVMODE_BUILD
    if ((result == VK_SUCCESS) &&
        (m_pReinjectionLayer != nullptr))
    {
        Util::Result palResult = m_pInstance->GetDevModeMgr()->RegisterPipelineCache(
            this,
            settings.devModePipelineUriServicePostSizeLimit);

        if (palResult == Util::Result::Success)
        {
            palResult = m_hashMapping.Init();
        }

        if (palResult == Util::Result::Success)
        {
            palResult = m_hashMappingLock.Init();
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

    return result;
}

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// Initialize reinjection cache layer
VkResult PipelineBinaryCache::InitReinjectionLayer(
    const RuntimeSettings& settings)
{
    VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;

    if (m_pInstance->GetDevModeMgr() != nullptr)
    {
        Util::MemoryCacheCreateInfo info = {};
        Util::AllocCallbacks        allocCbs = {
            m_pInstance->GetAllocCallbacks(),
            allocator::PalAllocFuncDelegator,
            allocator::PalFreeFuncDelegator
        };

        info.baseInfo.pCallbacks = &allocCbs;
        info.maxObjectCount = SIZE_MAX;
        info.maxMemorySize = SIZE_MAX;
        info.evictOnFull = false;
        info.evictDuplicates = true;

        size_t memSize = Util::GetMemoryCacheLayerSize(&info);
        void*  pMem    = m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
                m_pInstance->FreeMem(pMem);
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
        char            filePath[260]; // Windows MAX_PATH = 260
        uint32_t        fileCount           = 0u;
        const char**    ppFileNames         = nullptr;
        size_t          fileNameBufferSize  = 0u;
        void*           pFileNameBuffer     = nullptr;
        size_t          dirLength           = strlen(settings.devModeElfReplacementDirectory) + 1u;

        Util::Hash128   pipelineHash        = {};
        size_t          pipelineBinarySize  = 0u;
        void*           pPipelineBinary     = nullptr;

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
            ppFileNames = (const char**)m_pInstance->AllocMem(
                (sizeof(const char*) * fileCount),
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
            pFileNameBuffer = m_pInstance->AllocMem(
                fileNameBufferSize,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

            // Populate ppFileNames and pFileNameBuffer
            result = Util::ListDir(
                settings.devModeElfReplacementDirectory,
                &fileCount,
                ppFileNames,
                &fileNameBufferSize,
                pFileNameBuffer);

            if (result != Util::Result::Success)
            {
                m_pInstance->FreeMem(pFileNameBuffer);
                m_pInstance->FreeMem(ppFileNames);
            }
        }

        if (result == Util::Result::Success)
        {
            // Store each file into cache
            strcpy(filePath, settings.devModeElfReplacementDirectory);
            strcat(filePath, "\\");
            for (uint32_t fileIndex = 0; fileIndex < fileCount; fileIndex++)
            {
                filePath[dirLength] = '\0';
                strcat(filePath, ppFileNames[fileIndex]);

                ppFileNames[fileIndex] = strstr(ppFileNames[fileIndex], "_0x");

                if ((ppFileNames[fileIndex] != nullptr) &&
                    (strlen(ppFileNames[fileIndex]) >= 32))
                {
                    ppFileNames[fileIndex] += 3u;
                    pipelineHash = ParseHash128(ppFileNames[fileIndex]);

                    if (Util::File::Exists(filePath))
                    {
                        pipelineBinarySize = Util::File::GetFileSize(filePath);
                        pPipelineBinary = m_pInstance->AllocMem(pipelineBinarySize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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

                        m_pInstance->FreeMem(pPipelineBinary);
                    }
                    else
                    {
                        VK_NEVER_CALLED();
                    }
                }
            }

            m_pInstance->FreeMem(pFileNameBuffer);
            m_pInstance->FreeMem(ppFileNames);
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
    allocCallbacks.pClientData = m_pInstance->GetAllocCallbacks();
    allocCallbacks.pfnAlloc    = allocator::PalAllocFuncDelegator;
    allocCallbacks.pfnFree     = allocator::PalFreeFuncDelegator;

    Util::MemoryCacheCreateInfo createInfo = {};
    createInfo.baseInfo.pCallbacks = &allocCallbacks;
    createInfo.maxObjectCount      = SIZE_MAX;
    createInfo.maxMemorySize       = SIZE_MAX;
    createInfo.evictOnFull         = true;
    createInfo.evictDuplicates     = true;

    size_t layerSize = Util::GetMemoryCacheLayerSize(&createInfo);
    void*  pMem      = m_pInstance->AllocMem(layerSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
            m_pInstance->FreeMem(pMem);
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
        m_pInstance->GetAllocCallbacks(),
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    Util::Strncpy(info.filePath, pFilePath, sizeof(info.filePath));
    Util::Strncpy(info.fileName, pFileName, sizeof(info.fileName));

    info.pMemoryCallbacks        = &allocCbs;
    info.pPlatformKey            = m_pPlatformKey;
    info.archiveType             = ArchiveType;
    info.useStrictVersionControl = true;
    info.allowWriteAccess        = false;
    info.allowCreateFile         = false;
    info.allowAsyncFileIo        = true;
    info.useBufferedReadMemory   = (bufferSize > 0);
    info.maxReadBufferMem        = bufferSize;

    size_t memSize = Util::GetArchiveFileObjectSize(&info);
    void*  pMem    = m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
            m_pInstance->FreeMem(pMem);
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
        m_pInstance->GetAllocCallbacks(),
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    Util::Strncpy(info.filePath, pFilePath, sizeof(info.filePath));
    Util::Strncpy(info.fileName, pFileName, sizeof(info.fileName));

    info.pMemoryCallbacks        = &allocCbs;
    info.pPlatformKey            = m_pPlatformKey;
    info.archiveType             = ArchiveType;
    info.useStrictVersionControl = true;
    info.allowWriteAccess        = true;
    info.allowCreateFile         = true;
    info.allowAsyncFileIo        = true;
    info.useBufferedReadMemory   = (bufferSize > 0);
    info.maxReadBufferMem        = bufferSize;

    size_t memSize = Util::GetArchiveFileObjectSize(&info);
    void*  pMem    = m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

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
            m_pInstance->FreeMem(pMem);
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
        m_pInstance->GetAllocCallbacks(),
        allocator::PalAllocFuncDelegator,
        allocator::PalFreeFuncDelegator
    };

    info.baseInfo.pCallbacks = &allocCbs;
    info.pFile               = pFile;
    info.pPlatformKey        = m_pPlatformKey;
    info.dataTypeId          = ElfType;

    size_t memSize = Util::GetArchiveFileCacheLayerSize(&info);
    void*  pMem    = m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMem != nullptr)
    {
        if (Util::CreateArchiveFileCacheLayer(&info, pMem, &pLayer) != Util::Result::Success)
        {
            m_pInstance->FreeMem(pMem);
            pLayer = nullptr;
        }
    }

    return pLayer;
}

// =====================================================================================================================
// Open the archive file and initialize its cache layer
VkResult PipelineBinaryCache::InitArchiveLayers(
    const PhysicalDevice*  pPhysicalDevice,
    const RuntimeSettings& settings)
{
    VkResult result = VK_SUCCESS;

    // Buffer to hold constructed path
    char pathBuffer[_MAX_FNAME] = {};
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
            const char* pUserDataPath = pPhysicalDevice->PalDevice()->GetCacheFilePath();

            if ((pCacheSubPath != nullptr) &&
                (pUserDataPath != nullptr))
            {
                // Construct the path in the local buffer. Consider it valid if not empty
                if (Util::Snprintf(pathBuffer, _MAX_FNAME, "%s%s", pUserDataPath, pCacheSubPath) > 0)
                {
                    pCachePath = pathBuffer;
#if VK_IS_PAL_VERSION_AT_LEAST(582, 2)
                    if (settings.allowCleanUpCacheDirectory)
                    {
                        uint64 totalSize = 0, oldestTime = 0;
                        if (Util::GetStatusOfDir(pCachePath, &totalSize, &oldestTime) == Util::Result::Success)
                        {
                            if (totalSize >= settings.pipelineCacheDefaultLocationLimitation)
                            {
                                Util::RemoveFilesOfDir(pCachePath, oldestTime + settings.thresholdOfCleanUpCache);
                            }
                        }
                    }
#endif
                    result = VK_SUCCESS;
                }
            }
        }
    }

    // Load the primary archive file
    if (result == VK_SUCCESS)
    {
        // Assume that the first layer we open should be the "primary" source and optimize its memory access
        constexpr size_t PrimayrLayerBufferSize   = 64 * 1024 * 1024;
        constexpr size_t SecondaryLayerBufferSize = 8 * 1024 * 1024;

        // Open the optional read only cache file. This may fail gracefully
        const char* const  pThirdPartyFileName = getenv(EnvVarReadOnlyFileName);
        Util::ICacheLayer* pThirdPartyLayer    = nullptr;

        if (pThirdPartyFileName != nullptr)
        {
            Util::IArchiveFile* pFile = OpenReadOnlyArchive(pCachePath, pThirdPartyFileName, PrimayrLayerBufferSize);

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
                    m_pInstance->FreeMem(pFile);
                }
            }
        }

        // Buffer to hold constructed filename
        char nameBuffer[_MAX_FNAME] = {};

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
            size_t bufferSize = (m_pArchiveLayer == nullptr) ? PrimayrLayerBufferSize : SecondaryLayerBufferSize;

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

            Util::IArchiveFile* pFile    = OpenWritableArchive(pCachePath, nameBuffer, bufferSize);
            bool                readOnly = false;

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
                    m_openFiles.PushBack(pFile);
                    m_archiveLayers.PushBack(pLayer);

                    if (pLastReadLayer != nullptr)
                    {
                        pLastReadLayer->SetLoadPolicy(Util::ICacheLayer::LinkPolicy::PassCalls);
                        pLastReadLayer->SetStorePolicy(Util::ICacheLayer::LinkPolicy::Skip | Util::ICacheLayer::LinkPolicy::PassData);
                        // Connect to previous read layer as read-through / write-through + skip
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
                        break;
                    }
                }
                else
                {
                    pFile->Destroy();
                    m_pInstance->FreeMem(pFile);
                }
            }
        }

        if (m_pArchiveLayer == nullptr)
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }

        VK_ASSERT(pWriteLayer != nullptr);
    }

    return result;
}

// =====================================================================================================================
// Initialize layers (a single layer that supports storage for binaries needs to succeed)
VkResult PipelineBinaryCache::InitLayers(
    const PhysicalDevice*  pPhysicalDevice,
    bool                   internal,
    const RuntimeSettings& settings)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;

#if ICD_GPUOPEN_DEVMODE_BUILD
    if ((InitReinjectionLayer(settings) == VK_SUCCESS))
    {
        result = VK_SUCCESS;
    }
#endif

    if (InitMemoryCacheLayer(settings) == VK_SUCCESS)
    {
        result = VK_SUCCESS;
    }

    // If cache handle is vkPipelineCache, we shouldn't store it to disk.
    if (internal)
    {
        if (InitArchiveLayers(pPhysicalDevice, settings) == VK_SUCCESS)
        {
            result = VK_SUCCESS;
        }
    }

    return result;
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

    if (result == VK_SUCCESS)
    {
        result = AddLayerToChain(m_pMemoryLayer, &pBottomLayer);
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 534
    if (m_pMemoryLayer != nullptr)
    {
        if (*pSize == 0)
        {
            size_t curCount, curDataSize;

            result = PalToVkResult(Util::GetMemoryCacheLayerCurSize(m_pMemoryLayer, &curCount, &curDataSize));
            if (result == VK_SUCCESS)
            {
                *pSize = curCount * sizeof(BinaryCacheEntry) + curDataSize + sizeof(PipelineBinaryCachePrivateHeader);
            }
        }
        else
        {
            size_t curCount, curDataSize;

            result = PalToVkResult(Util::GetMemoryCacheLayerCurSize(m_pMemoryLayer, &curCount, &curDataSize));
            if (result == VK_SUCCESS)
            {
                if (*pSize > (sizeof(BinaryCacheEntry) + sizeof(PipelineBinaryCachePrivateHeader)))
                {
                    Util::AutoBuffer<Util::Hash128, 8, PalAllocator> cacheIds(curCount, m_pInstance->Allocator());
                    size_t remainingSpace    = *pSize - sizeof(PipelineBinaryCachePrivateHeader);

                    result = PalToVkResult(Util::GetMemoryCacheLayerHashIds(m_pMemoryLayer, curCount, &cacheIds[0]));
                    if (result == VK_SUCCESS)
                    {
                        void* pDataDst = pBlob;

                        // reserved for privateHeader
                        pDataDst = Util::VoidPtrInc(pDataDst, sizeof(PipelineBinaryCachePrivateHeader));

                        for (uint32_t i = 0; i < curCount && remainingSpace > sizeof(BinaryCacheEntry); i++)
                        {
                            size_t           dataSize;
                            const void*      pBinaryCacheData;

                            result = PalToVkResult(LoadPipelineBinary(&cacheIds[i], &dataSize, &pBinaryCacheData));
                            if (result == VK_SUCCESS)
                            {
                                if (remainingSpace >= (sizeof(BinaryCacheEntry) + dataSize))
                                {
                                    BinaryCacheEntry* pEntry =  static_cast<BinaryCacheEntry*>(pDataDst);

                                    pEntry->hashId    = cacheIds[i];
                                    pEntry->dataSize  = dataSize;

                                    pDataDst = Util::VoidPtrInc(pDataDst, sizeof(BinaryCacheEntry));
                                    memcpy(pDataDst, pBinaryCacheData, dataSize);
                                    pDataDst = Util::VoidPtrInc(pDataDst, dataSize);
                                    remainingSpace -= (sizeof(BinaryCacheEntry) + dataSize);
                                }
                                m_pInstance->FreeMem(const_cast<void*>(pBinaryCacheData));
                            }
                        }
                    }
                    if (*pSize < (sizeof(BinaryCacheEntry) * curCount + curDataSize + sizeof(PipelineBinaryCachePrivateHeader)))
                    {
                        result = VK_INCOMPLETE;
                    }
                    *pSize -= remainingSpace;

                    auto pBinaryPrivateHeader = static_cast<PipelineBinaryCachePrivateHeader*>(pBlob);
                    void* pData               = Util::VoidPtrInc(pBlob, sizeof(PipelineBinaryCachePrivateHeader));

                    result = PalToVkResult(CalculateHashId(
                                                m_pInstance,
                                                m_pPlatformKey,
                                                pData,
                                                *pSize - sizeof(PipelineBinaryCachePrivateHeader),
                                                pBinaryPrivateHeader->hashId));
                }
                else
                {
                    result = VK_ERROR_INITIALIZATION_FAILED;
                }
            }
        }
    }
#endif
    return result;
}

// =====================================================================================================================
// Merge the pipeline cache data into one
//
VkResult PipelineBinaryCache::Merge(
    uint32_t                    srcCacheCount,
    const PipelineBinaryCache** ppSrcCaches)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 534
    if (m_pMemoryLayer != nullptr)
    {
        for (uint32_t i = 0; i < srcCacheCount; i++)
        {
            Util::ICacheLayer* pMemoryLayer = ppSrcCaches[i]->GetMemoryLayer();
            size_t curCount, curDataSize;

            result = PalToVkResult(Util::GetMemoryCacheLayerCurSize(pMemoryLayer, &curCount, &curDataSize));
            if ((result == VK_SUCCESS) && (curCount > 0))
            {
                Util::AutoBuffer<Util::Hash128, 8, PalAllocator> cacheIds(curCount, m_pInstance->Allocator());

                result = PalToVkResult(Util::GetMemoryCacheLayerHashIds(pMemoryLayer, curCount, &cacheIds[0]));
                if (result == VK_SUCCESS)
                {
                    for (uint32_t j = 0; j < curCount; j++)
                    {
                        size_t           dataSize;
                        const void*      pBinaryCacheData;

                        result = PalToVkResult(ppSrcCaches[i]->LoadPipelineBinary(&cacheIds[j], &dataSize, &pBinaryCacheData));
                        if (result == VK_SUCCESS)
                        {
                            result = PalToVkResult(StorePipelineBinary(&cacheIds[j], dataSize, pBinaryCacheData));
                            m_pInstance->FreeMem(const_cast<void*>(pBinaryCacheData));
                            if (result != VK_SUCCESS)
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    return result;
}

} // namespace vk
