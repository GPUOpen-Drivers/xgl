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
* @file  pipeline_binary_cache.h
* @brief Declaration of Vulkan interface for a PAL layered cache specializing in pipeline binaries
***********************************************************************************************************************
*/
#pragma once
#include "pipeline_compiler.h"

#include "palHashMap.h"
#include "palMetroHash.h"
#include "palVector.h"
#include "palCacheLayer.h"
#include "cache_adapter.h"

namespace Util
{
class IPlatformKey;

#if ICD_GPUOPEN_DEVMODE_BUILD
class DevModeMgr;
#endif
} // namespace Util

namespace vk
{

class CacheAdapter;

// Unified pipeline cache interface
class PipelineBinaryCache
{
public:
    using CacheId                    = Util::MetroHash::Hash;

    static PipelineBinaryCache* Create(
        VkAllocationCallbacks*     pAllocationCallbacks,
        Util::IPlatformKey*        pKey,
        const Vkgc::GfxIpVersion&  gfxIp,
        const vk::RuntimeSettings& settings,
        const char*                pDefaultCacheFilePath,
#if ICD_GPUOPEN_DEVMODE_BUILD
        vk::DevModeMgr*            pDevModeMgr,
#endif
        uint32_t                   expectedEntries,
        size_t                     initDataSize,
        const void*                pInitData,
        bool                       createArchiveLayers);

    static bool IsValidBlob(
        VkAllocationCallbacks* pAllocationCallbacks,
        Util::IPlatformKey*    pKey,
        size_t                 dataSize,
        const void*            pData);

    ~PipelineBinaryCache();

    VkResult Initialize(
        const RuntimeSettings&    settings,
        bool                      createArchiveLayers,
        const char*               pDefaultCacheFilePath,
        const Util::IPlatformKey* pKey);

    Util::Result QueryPipelineBinary(
        const CacheId*     pCacheId,
        uint32_t           flags,
        Util::QueryResult* pQuery);

    Util::Result WaitPipelineBinary(
        const CacheId* pCacheId);

    Util::Result LoadPipelineBinary(
        const CacheId*  pCacheId,
        size_t*         pPipelineBinarySize,
        const void**    ppPipelineBinary) const;

    Util::Result StorePipelineBinary(
        const CacheId*  pCacheId,
        size_t          pipelineBinarySize,
        const void*     pPipelineBinary);

    Util::Result GetPipelineBinary(
        const Util::QueryResult* pQeuryId,
        void*                    pPipelineBinary) const;

    Util::Result ReleaseCacheRef(
        const Util::QueryResult* pQuery) const;

    Util::Result GetCacheDataPtr(
        const Util::QueryResult* pQuery,
        const void**             ppData) const;

    Util::Result EvictEntry(
        const Util::QueryResult* pQuery) const;

    Util::Result MarkEntryBad(
        const Util::QueryResult* pQuery) const;

    VkResult Serialize(
        void*   pBlob,
        size_t* pSize);

    VkResult Merge(
        uint32_t                    srcCacheCount,
        const PipelineBinaryCache** ppSrcCaches);

#if ICD_GPUOPEN_DEVMODE_BUILD
    Util::Result LoadReinjectionBinary(
        const CacheId*           pInternalPipelineHash,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary);

    Util::Result StoreReinjectionBinary(
        const CacheId*           pInternalPipelineHash,
        size_t                   pipelineBinarySize,
        const void*              pPipelineBinary);

    using HashMapping = Util::HashMap<Pal::PipelineHash, CacheId, PalAllocator>;

    void RegisterHashMapping(
        const Pal::PipelineHash* pInternalPipelineHash,
        const CacheId*           pCacheId);

    CacheId* GetCacheIdForPipeline(
        const Pal::PipelineHash* pInternalPipelineHash);

    HashMapping::Iterator GetHashMappingIterator()
        { return m_hashMapping.Begin(); }

    Util::RWLock* GetHashMappingLock()
        { return &m_hashMappingLock; }
#endif

    void FreePipelineBinary(const void* pPipelineBinary);

    void* AllocMem(
        size_t memSize) const;

    void FreeMem(
        void* pMem) const;

    void Destroy();

    CacheAdapter* GetCacheAdapter() { return m_pCacheAdapter; }

private:
    PAL_DISALLOW_DEFAULT_CTOR(PipelineBinaryCache);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineBinaryCache);

    explicit PipelineBinaryCache(
        VkAllocationCallbacks*    pAllocationCallbacks,
        const Vkgc::GfxIpVersion& gfxIp,
        uint32_t                  expectedEntries);

    VkResult InitializePlatformKey(
        const PhysicalDevice*  pPhysicalDevice,
        const RuntimeSettings& settings);

    VkResult OrderLayers(
        const RuntimeSettings& settings);

    VkResult AddLayerToChain(
        Util::ICacheLayer*  pLayer,
        Util::ICacheLayer** pBottomLayer);

    VkResult InitLayers(
        const char*            pDefaultCacheFilePath,
        bool                   createArchiveLayers,
        const RuntimeSettings& settings);

#if ICD_GPUOPEN_DEVMODE_BUILD
    VkResult InitReinjectionLayer(
        const RuntimeSettings& settings);

    Util::Result InjectBinariesFromDirectory(
        const RuntimeSettings& settings);
#endif

    VkResult InitMemoryCacheLayer(
        const RuntimeSettings& settings);

    VkResult InitCompressingLayer(
        const RuntimeSettings& settings);

    VkResult InitArchiveLayers(
        const char*            pDefaultCacheFilePath,
        const RuntimeSettings& settings);

    Util::ICacheLayer*  GetMemoryLayer() const { return m_pMemoryLayer; }
    Util::IArchiveFile* OpenReadOnlyArchive(const char* path, const char* fileName, size_t bufferSize);
    Util::IArchiveFile* OpenWritableArchive(const char* path, const char* fileName, size_t bufferSize);
    Util::ICacheLayer*  CreateFileLayer(Util::IArchiveFile* pFile);

    // Override the driver's default location
    static constexpr char     EnvVarPath[] = "AMD_VK_PIPELINE_CACHE_PATH";

    // Override the driver's default name (Hash of application name)
    static constexpr char     EnvVarFileName[] = "AMD_VK_PIPELINE_CACHE_FILENAME";

    // Filename of an additional, read-only archive
    static constexpr char     EnvVarReadOnlyFileName[] = "AMD_VK_PIPELINE_CACHE_READ_ONLY_FILENAME";

    static const uint32_t     ArchiveType;                // TypeId created by hashed string VK_SHADER_PIPELINE_CACHE
    static const uint32_t     ElfType;                    // TypeId created by hashed string VK_PIPELINE_ELF

    Vkgc::GfxIpVersion        m_gfxIp;                    // Compared against e_flags of reinjected elf files

    VkAllocationCallbacks*    m_pAllocationCallbacks;     // Allocator for use when interacting with the cache

    vk::PalAllocator          m_palAllocator;             // PalAllocator for helper objects, e.g., FileVector

    const Util::IPlatformKey* m_pPlatformKey;             // Platform identifying key

    Util::ICacheLayer*        m_pTopLayer;                // Top layer of the cache chain where queries are submitted

#if ICD_GPUOPEN_DEVMODE_BUILD
    vk::DevModeMgr*           m_pDevModeMgr;
    Util::ICacheLayer*        m_pReinjectionLayer;        // Reinjection interface layer

    HashMapping               m_hashMapping;              // Maps the internalPipelineHash to the appropriate CacheId
    Util::RWLock              m_hashMappingLock;          // Prevents collisions during writes to the map
#endif

    Util::ICacheLayer*        m_pMemoryLayer;

    Util::ICacheLayer*        m_pCompressingLayer;

    uint32_t                  m_expectedEntries;

    // Archive based cache layers
    using FileVector  = Util::Vector<Util::IArchiveFile*, 8, PalAllocator>;
    using LayerVector = Util::Vector<Util::ICacheLayer*, 8, PalAllocator>;
    Util::ICacheLayer*  m_pArchiveLayer;  // Top of a chain of loaded archives.
    FileVector          m_openFiles;
    LayerVector         m_archiveLayers;

    CacheAdapter*       m_pCacheAdapter;

    Util::Mutex         m_entriesMutex;      // Mutex that will be used to get cache state by Query
};

} // namespace vk
