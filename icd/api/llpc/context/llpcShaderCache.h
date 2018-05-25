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
 @file llpcShaderCache.h
 @brief LLPC header file: contains declaration of class Llpc::ShaderCache.
 ***********************************************************************************************************************
 */
#pragma once

#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include "llvm/Support/Mutex.h"

#include "llpc.h"
#include "llpcDebug.h"
#include "llpcFile.h"
#include "llpcInternal.h"
#include "llpcMetroHash.h"

namespace Llpc
{

// Header data that is stored with each shader in the cache.
struct ShaderHeader
{
    ShaderHash  key;    // Compacted hash key used to identify shaders
    uint64_t    crc;    // CRC of the shader cache entry, used to detect data corruption.
    size_t      size;   // Total size of the shader data in the storage file
};

// Enum defining the states a shader cache entry can be in
enum class ShaderEntryState : uint32_t
{
    New         = 0,    // Initial state
    Compiling   = 1,    // An entry was created and must be compiled/populated by the caller
    Ready       = 2,    // A matching shader was found and is ready for use
    Unavailable = 3,    // Entry doesn't exist in cache
};

// Enumerates modes used in shader cache.
enum ShaderCacheMode
{
    ShaderCacheDisable       = 0,             // Disabled
    ShaderCacheEnableRuntime = 1,             // Enabled for runtime use only
    ShaderCacheEnableOnDisk  = 2,             // Enabled with on-disk file
    ShaderCacheForceInternalCacheOnDisk = 3,  // Force to use internal cache on disk
    ShaderCacheEnableOnDiskReadOnly = 4,      // Only read on-disk file with write-protection
};

// Stores data in the hash map of cached shaders and helps correlated a shader in the hash to a location in the
// cache's linear allocators where the shader is actually stored.
struct ShaderIndex
{
    ShaderHeader                header;      // Shader header data (key, crc, size)
    volatile ShaderEntryState   state;       // Shader entry state
    void*                       pDataBlob;   // Serialized data blob representing a cached RelocatableShader object.
};

typedef std::unordered_map<ShaderHash, ShaderIndex*> ShaderIndexMap;

// Specifies auxiliary info necessary to create a shader cache object.
struct ShaderCacheAuxCreateInfo
{
    ShaderCacheMode        shaderCacheMode;    // Mode of shader cache
    GfxIpVersion           gfxIp;              // Graphics IP version info
    MetroHash::Hash        hash;               // Hash code of compilation options
    const char*            pCacheFilePath;     // root directory of cache file
    const char*            pExecutableName;    // Name of executable file
};

// Length of date field used in BuildUniqueId
static constexpr uint8_t DateLength = 11;

// Length of time field used in BuildUniqueId
static constexpr uint8_t TimeLength = 8;

// Opaque data type representing an ID that uniquely identifies a particular build of LLPC. Such an ID will be stored
// with all serialized pipelines and in the shader cache, and used during load of that data to ensure the version of
// PAL that loads the data is exactly the same as the version that stored it. Currently, this ID is just the date
// and time when LLPC was built.
struct BuildUniqueId
{
    uint8_t buildDate[DateLength];     // Build date
    uint8_t buildTime[TimeLength];     // Build time
    GfxIpVersion gfxIp;                // Graphics IP version info
    MetroHash::Hash hash;              // Hash code of compilation options
};

// This the header for the shader cache data when the cache is serialized/written to disk
struct ShaderCacheSerializedHeader
{
    size_t              headerSize;    // Size of the header structure. This member must always be first
                                       // since it is used to validate the serialized data.
    BuildUniqueId       buildId;       // Build time/date of the PAL version that created the cache file
    size_t              shaderCount;   // Number of shaders in the shaderIndex array
    size_t              shaderDataEnd; // Offset to the end of shader data
};

constexpr uint32_t MaxFilePathLen = 256;

typedef void* CacheEntryHandle;

// =====================================================================================================================
// This class implements a cache for compiled shaders. The shader cache persists in memory at runtime and can be
// serialized to disk by the client/application for persistence between runs.
class ShaderCache : public IShaderCache
{
public:
    ShaderCache();
    virtual ~ShaderCache();

    Result Init(const ShaderCacheCreateInfo* pCreateInfo, const ShaderCacheAuxCreateInfo* pAuxCreateInfo);
    virtual void Destroy();

    virtual Result Serialize(void* pBlob, size_t* pSize);

    virtual Result Merge(uint32_t srcCacheCount, const IShaderCache** ppSrcCaches);

    ShaderEntryState FindShader(MetroHash::Hash   hash,
                                bool              allocateOnMiss,
                                CacheEntryHandle* phEntry);

    void InsertShader(CacheEntryHandle         hEntry,
                      const void*              pBlob,
                      size_t                   size);

    void ResetShader(CacheEntryHandle         hEntry);

    Result RetrieveShader(CacheEntryHandle   hEntry,
                          const void**       ppBlob,
                          size_t*            pSize);

    bool IsCompatible(const ShaderCacheCreateInfo* pCreateInfo, const ShaderCacheAuxCreateInfo* pAuxCreateInfo);

private:
    LLPC_DISALLOW_COPY_AND_ASSIGN(ShaderCache);

    Result BuildFileName(const char*  pExecutableName,
                         const char*  pCacheFilePath,
                         GfxIpVersion gfxIp,
                         bool*        pCacheFileExists);
    Result ValidateAndLoadHeader(const ShaderCacheSerializedHeader* pHeader, size_t dataSourceSize);
    Result LoadCacheFromBlob(const void* pInitialData, size_t initialDataSize);
    Result PopulateIndexMap(void* pDataStart, size_t dataSize);
    uint64_t CalculateCrc(const uint8_t* pData, size_t numBytes);

    Result LoadCacheFromFile();
    void ResetCacheFile();
    void AddShaderToFile(const ShaderIndex* pIndex);

    void* GetCacheSpace(size_t numBytes);

    // Lock cache map
    void LockCacheMap(bool readOnly) { m_lock.lock(); }

    // Unlock cache map
    void UnlockCacheMap(bool readOnly) { m_lock.unlock(); }

    bool UseExternalCache()
        { return ((m_pfnGetValueFunc != nullptr) && (m_pfnStoreValueFunc != nullptr)); }

    void ResetRuntimeCache();
    void GetBuildTime(BuildUniqueId *pBuildId);

    // -----------------------------------------------------------------------------------------------------------------

    llvm::sys::Mutex  m_lock;       // Read/Write lock for access to the shader cache hash map
    File              m_onDiskFile; // File for on-disk storage of the cache
    bool              m_disableCache; // Whether disable cache completely

    // Map of shader index data which detail the hash, crc, size and CPU memory location for each shader
    // in the cache.
    ShaderIndexMap  m_shaderIndexMap;

    // In memory copy of the shaderDataEnd and totalShaders stored in the on-disk file. We keep a copy to avoid having
    //  to do a read/modify/write of the value when adding a new shader.
    size_t          m_shaderDataEnd;
    size_t          m_totalShaders;

    char            m_fileFullPath[MaxFilePathLen]; // Full path/filename of the shader cache on-disk file

    std::list<std::pair<uint8_t*, size_t> > m_allocationList;  // Memory allcoated by GetCacheSpace
    uint32_t                 m_serializedSize;      // Serialized byte size of whole shader cache
    std::mutex               m_conditionMutex;      // Mutex that will be used with the condition variable
    std::condition_variable  m_conditionVariable;   // Condition variable that will be used to wait compile finish
    const void*              m_pClientData;         // Client data that will be used by function GetValue and StoreValue
    ShaderCacheGetValue      m_pfnGetValueFunc;     // GetValue function used to query an external cache for shader data
    ShaderCacheStoreValue    m_pfnStoreValueFunc;   // StoreValue function used to store shader data in an external cache
    GfxIpVersion             m_gfxIp;               // Graphics IP version info
    MetroHash::Hash          m_hash;                // Hash code of compilation options
};

} // Llpc
