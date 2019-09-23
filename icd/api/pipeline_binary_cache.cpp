/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_physical_device.h"

#include "palArchiveFile.h"
#include "palPlatformKey.h"
#include "palSysMemory.h"
#include "palVectorImpl.h"
#include "palHashBaseImpl.h"
#include "palFile.h"
#if ICD_GPUOPEN_DEVMODE_BUILD
#include "palPipelineAbiProcessorImpl.h"

#include "devmode/devmode_mgr.h"
#endif
#include <limits.h>
#include <string.h>

namespace vk
{
#define _MAX_FNAME NAME_MAX

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

// =====================================================================================================================
// Allocate and initialize a PipelineBinaryCache object
PipelineBinaryCache* PipelineBinaryCache::Create(
    Instance*                 pInstance,
    size_t                    initDataSize,
    const void*               pInitData,
    bool                      internal,
    const Llpc::GfxIpVersion& gfxIp,
    const PhysicalDevice*     pPhysicalDevice)
{
    PipelineBinaryCache* pObj = nullptr;
    void*                pMem = pInstance->AllocMem(sizeof(PipelineBinaryCache), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (pMem != nullptr)
    {
        pObj = VK_PLACEMENT_NEW(pMem) PipelineBinaryCache(pInstance, gfxIp, internal);

        if (pObj->Initialize(pPhysicalDevice, initDataSize, pInitData) != VK_SUCCESS)
        {
            pObj->Destroy();
            pInstance->FreeMem(pMem);
            pObj = nullptr;
        }
    }
    return pObj;
}

// =====================================================================================================================
PipelineBinaryCache::PipelineBinaryCache(
    Instance*                 pInstance,
    const Llpc::GfxIpVersion& gfxIp,
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
    m_isInternalCache    { internal }
{
    // Without copy constructor, a class type variable can't be initialized in initialization list with gcc 4.8.5.
    // Initialize m_gfxIp here instead to make gcc 4.8.5 work.
    m_gfxIp = gfxIp;
}

// =====================================================================================================================
PipelineBinaryCache::~PipelineBinaryCache()
{
    if (m_pPlatformKey != nullptr)
    {
        m_pPlatformKey->Destroy();
        m_pInstance->FreeMem(m_pPlatformKey);
    }

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
    const void**   ppPipelineBinary)
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

        Util::Abi::PipelineAbiProcessor<PalAllocator> processor(m_pInstance->Allocator());
        result = processor.LoadFromBuffer(pPipelineBinary, pipelineBinarySize);

        if (result == Util::Result::Success)
        {
            processor.GetGfxIpVersion(&gfxIpMajor, &gfxIpMinor, &gfxIpStepping);

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
    const PhysicalDevice* pPhysicalDevice,
    size_t                initDataSize,
    const void*           pInitData)
{
    VkResult result = VK_SUCCESS;

    const RuntimeSettings& settings = pPhysicalDevice->GetRuntimeSettings();

    if (result == VK_SUCCESS)
    {
        result = InitializePlatformKey(pPhysicalDevice, settings);
    }

    if (result == VK_SUCCESS)
    {
        result = InitLayers(pPhysicalDevice, initDataSize, pInitData, m_isInternalCache, settings);
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

// =====================================================================================================================
// Generate our platform key
VkResult PipelineBinaryCache::InitializePlatformKey(
    const PhysicalDevice*  pPhysicalDevice,
    const RuntimeSettings& settings)
{
    static constexpr Util::HashAlgorithm KeyAlgorithm = Util::HashAlgorithm::Sha1;

    struct
    {
        VkPhysicalDeviceProperties properties;
        char*                      timestamp[sizeof(__TIMESTAMP__)];
    } initialData;

    memset(&initialData, 0, sizeof(initialData));

    VkResult result = pPhysicalDevice->GetDeviceProperties(&initialData.properties);

    if (result == VK_SUCCESS)
    {
        size_t memSize = Util::GetPlatformKeySize(KeyAlgorithm);
        void*  pMem    = m_pInstance->AllocMem(memSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            if (settings.markPipelineCacheWithBuildTimestamp)
            {
                memcpy(initialData.timestamp, __TIMESTAMP__, sizeof(__TIMESTAMP__));
            }

            if (Util::CreatePlatformKey(KeyAlgorithm, &initialData, sizeof(initialData), pMem, &m_pPlatformKey) !=
                Util::Result::Success)
            {
                m_pInstance->FreeMem(pMem);
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
        }
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
    size_t                 initDataSize,
    const void*            pInitData,
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

} // namespace vk
