/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline_cache.h"
#include "palAutoBuffer.h"

#include "include/pipeline_binary_cache.h"

namespace vk
{

// =====================================================================================================================
PipelineCache::PipelineCache(
    const Device*           pDevice,
    ShaderCache*            pShaderCaches,
    PipelineBinaryCache*    pBinaryCache
    )
    :
    m_pDevice(pDevice),
    m_pBinaryCache(pBinaryCache)
{
    memcpy(m_shaderCaches, pShaderCaches, sizeof(m_shaderCaches[0]) * pDevice->NumPalDevices());
    memset(m_shaderCaches + pDevice->NumPalDevices(),
           0,
           sizeof(m_shaderCaches[0]) * (MaxPalDevices - pDevice->NumPalDevices()));
}

// =====================================================================================================================
PipelineCache::~PipelineCache()
{
    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
    {
        m_shaderCaches[i].Destroy(m_pDevice->GetCompiler(i));
    }
}

// =====================================================================================================================
VkResult PipelineCache::Create(
    const Device*                    pDevice,
    const VkPipelineCacheCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*     pAllocator,
    VkPipelineCache*                 pPipelineCache)
{
    VkResult                result           = VK_SUCCESS;
    const RuntimeSettings&  settings         = pDevice->GetRuntimeSettings();
    uint32_t                numPalDevices    = pDevice->NumPalDevices();
    bool                    useInitialData   = false;
    size_t                  shaderCacheSize  = 0;
    size_t                  pipelineCacheSize[MaxPalDevices];

    PipelineCompilerType       cacheType = pDevice->GetCompiler(DefaultDeviceIndex)->GetShaderCacheType();

    for (uint32_t i = 0; i < numPalDevices; i++)
    {
        pipelineCacheSize[i] = pDevice->GetCompiler(DefaultDeviceIndex)->GetShaderCacheSize(cacheType);
        shaderCacheSize += pipelineCacheSize[i];
    }

    if (pCreateInfo->initialDataSize > 0)
    {
        const PipelineCacheHeaderData* pHeader = static_cast<const PipelineCacheHeaderData*>(pCreateInfo->pInitialData);

        if (pHeader->headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
        {
            const Pal::DeviceProperties& palProps = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

            if ((pHeader->vendorID == palProps.vendorId) &&
                (pHeader->deviceID == palProps.deviceId))
            {
                VkPhysicalDeviceProperties physicalDeviceProps;
                pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetDeviceProperties(&physicalDeviceProps);
                if (memcmp(pHeader->UUID, physicalDeviceProps.pipelineCacheUUID, sizeof(pHeader->UUID)) == 0)
                {
                    auto pPrivateDataHeader = reinterpret_cast<const PipelineCachePrivateHeaderData*>(
                            Util::VoidPtrInc(pCreateInfo->pInitialData, sizeof(PipelineCacheHeaderData)));

                    if (pPrivateDataHeader->cacheType == cacheType)
                    {
                        useInitialData = true;
                    }
                }
            }
        }
    }

    // Allocate system memory for all objects
    const size_t objSize = sizeof(PipelineCache) + shaderCacheSize;
    void* pMemory = pDevice->AllocApiObject(objSize, pAllocator);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        const PipelineCachePrivateHeaderData* pPrivateDataHeader = nullptr;
        const void* pBlobs[MaxPalDevices] = {};

        if (useInitialData)
        {
            pPrivateDataHeader = reinterpret_cast<const PipelineCachePrivateHeaderData*>(
                Util::VoidPtrInc(pCreateInfo->pInitialData, sizeof(PipelineCacheHeaderData)));

            pBlobs[0] = Util::VoidPtrInc(pPrivateDataHeader, sizeof(PipelineCachePrivateHeaderData));
            for (uint32_t i = 1; i < numPalDevices; i++)
            {
                pBlobs[i] = Util::VoidPtrInc(pBlobs[i - 1], static_cast<size_t>(pPrivateDataHeader->blobSize[i - 1]));
            }
        }

        ShaderCache shaderCaches[MaxPalDevices];
        size_t shaderCacheOffset = sizeof(PipelineCache);

        for (uint32_t i = 0; i < numPalDevices; i++)
        {
            const void* pInitialData = nullptr;
            size_t initialDataSize = 0;

            if (useInitialData)
            {
                pInitialData    = pBlobs[i];
                initialDataSize = static_cast<size_t>(pPrivateDataHeader->blobSize[i]);
            }

            if (result == VK_SUCCESS)
            {
                result = pDevice->GetCompiler(DefaultDeviceIndex)->CreateShaderCache(
                    pInitialData,
                    initialDataSize,
                    Util::VoidPtrInc(pMemory, shaderCacheOffset),
                    &shaderCaches[i]);
            }
            else
            {
                break;
            }

            // Move to next shader cache object
            shaderCacheOffset += pipelineCacheSize[i];
        }

        // Something went wrong with creating the PAL object. Free memory
        if (result != VK_SUCCESS)
        {
            for (uint32_t i = 0; i < numPalDevices; i++)
            {
                shaderCaches[i].Destroy(pDevice->GetCompiler(i));
            }
        }

        if (result == VK_SUCCESS)
        {
            PipelineBinaryCache* pBinaryCache = nullptr;
            if (((settings.usePalPipelineCaching) ||
                 (pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->GetDevModeMgr() != nullptr)) &&
                (settings.allowExternalPipelineCacheObject))
            {
                pBinaryCache = PipelineBinaryCache::Create(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance(),
                    pCreateInfo->initialDataSize, pCreateInfo->pInitialData, false,
                    pDevice->GetCompiler(DefaultDeviceIndex)->GetGfxIp(), pDevice->VkPhysicalDevice(DefaultDeviceIndex));

                // This isn't a terminal failure, the device can continue without the pipeline cache if need be.
                VK_ALERT(pBinaryCache == nullptr);
            }
            PipelineCache* pCache = VK_PLACEMENT_NEW(pMemory) PipelineCache(pDevice, shaderCaches, pBinaryCache);
            *pPipelineCache = PipelineCache::HandleFromVoidPointer(pMemory);
        }
        else
        {
            pAllocator->pfnFree(pAllocator->pUserData, pMemory);
        }
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineCache::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    if (m_pBinaryCache)
    {
        m_pBinaryCache->Destroy();
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->VkInstance()->FreeMem(m_pBinaryCache);
        m_pBinaryCache = nullptr;
    }

    this->~PipelineCache();

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
VkResult PipelineCache::GetData(
    void*   pData,
    size_t* pSize)
{
    VK_ASSERT(pSize != nullptr);

    VkResult        result = VK_SUCCESS;
    uint32_t numPalDevices = m_pDevice->NumPalDevices();

    size_t allBlobSize = sizeof(PipelineCachePrivateHeaderData);
    PipelineCachePrivateHeaderData headerData = {};

    headerData.cacheType = m_shaderCaches[0].GetCacheType();
    for (uint32_t i = 0; i < numPalDevices; i++)
    {
        size_t blobSize = 0;
        result = m_shaderCaches[i].Serialize(nullptr, &blobSize);
        VK_ASSERT(result == VK_SUCCESS);
        headerData.blobSize[i] = blobSize;
        allBlobSize += blobSize;
    }

    if (*pSize == 0)
    {
        *pSize = allBlobSize;
    }
    else
    {
        VK_ASSERT(*pSize >= allBlobSize);
        memcpy(pData, &headerData, sizeof(headerData));

        void* pBlob = Util::VoidPtrInc(pData, sizeof(headerData));

        for (uint32_t i = 0; i < numPalDevices; i++)
        {
            size_t blobSize = static_cast<size_t>(headerData.blobSize[i]);
            result = m_shaderCaches[i].Serialize(pBlob, &blobSize);
            if (result != VK_SUCCESS)
            {
                break;
            }
            pBlob = Util::VoidPtrInc(pBlob, blobSize);
        }
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineCache::Merge(
    uint32_t              srcCacheCount,
    const PipelineCache** ppSrcCaches)
{
    Util::AutoBuffer<ShaderCache::ShaderCachePtr, 16, PalAllocator> shaderCaches(
        srcCacheCount * m_pDevice->NumPalDevices(),
        m_pDevice->VkInstance()->Allocator());

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        for (uint32_t cacheIdx = 0; cacheIdx < srcCacheCount; cacheIdx++)
        {
            VK_ASSERT(ppSrcCaches[cacheIdx]->GetShaderCache(deviceIdx).GetCacheType() ==
                GetShaderCache(deviceIdx).GetCacheType());
            // Store all PAL caches like this d0c0,d0c1,d0c2...,d1c0,d1c2,d1c3...
            shaderCaches[deviceIdx * srcCacheCount + cacheIdx] =
                ppSrcCaches[cacheIdx]->GetShaderCache(deviceIdx).GetCachePtr();
        }
    }

    VkResult result = VK_SUCCESS;
    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
    {
        result = m_shaderCaches[i].Merge(srcCacheCount, &shaderCaches[i * srcCacheCount]);

        if (result != VK_SUCCESS)
        {
            break;
        }
    }

    return result;
}

namespace entry
{
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    const VkAllocationCallbacks*                pAllocator)
{
    if (pipelineCache != VK_NULL_HANDLE)
    {
        const Device*                pDevice = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        PipelineCache::ObjectFromHandle(pipelineCache)->Destroy(pDevice, pAllocCB);
    }
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineCacheData(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    size_t*                                     pDataSize,
    void*                                       pData)
{
    size_t                     nBytesRequested = *pDataSize;
    VkPhysicalDeviceProperties physicalDeviceProps;
    VkResult                   result = VK_SUCCESS;

    Device*        pDevice = ApiDevice::ObjectFromHandle(device);
    PipelineCache* pCache = PipelineCache::ObjectFromHandle(pipelineCache);

    const Pal::DeviceProperties& palProps = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    size_t privateDataSize = 0;

    result = pCache->GetData(nullptr, &privateDataSize);
    VK_ASSERT(result == VK_SUCCESS);

    PipelineCacheHeaderData pipelineCacheHeaderData;

    constexpr uint32_t HeaderSize = sizeof(pipelineCacheHeaderData);
    const size_t       fullDataSize = HeaderSize + privateDataSize;

    if (pData == nullptr)
    {
        *pDataSize = fullDataSize;

        return VK_SUCCESS;
    }

    if (nBytesRequested < fullDataSize)
    {
        // "If pDataSize is less than what is necessary to store this header, nothing will be written to pData and
        // zero will be written to pDataSize."
        *pDataSize = 0;

        result = VK_INCOMPLETE;
    }

    // The vk spec says the data should be written least significant byte first.
#ifdef BIGENDIAN_CPU
#error we need to byte-swap the data we are about to write to pData
#endif

    if (result == VK_SUCCESS)
    {
        ApiDevice::ObjectFromHandle(device)->VkPhysicalDevice(DefaultDeviceIndex)->GetDeviceProperties(&physicalDeviceProps);

        pipelineCacheHeaderData.headerLength = HeaderSize;
        pipelineCacheHeaderData.headerVersion = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
        pipelineCacheHeaderData.vendorID = palProps.vendorId;
        pipelineCacheHeaderData.deviceID = palProps.deviceId;

        memcpy(pipelineCacheHeaderData.UUID,
            physicalDeviceProps.pipelineCacheUUID,
            sizeof(physicalDeviceProps.pipelineCacheUUID));

        // Store the header first.
        memcpy(pData, &pipelineCacheHeaderData, HeaderSize);

        if (privateDataSize > 0)
        {
            void* pPrivateData = Util::VoidPtrInc(pData, HeaderSize);
            result = pCache->GetData(pPrivateData, &privateDataSize);
        }
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkMergePipelineCaches(
    VkDevice                                    device,
    VkPipelineCache                             dstCache,
    uint32_t                                    srcCacheCount,
    const VkPipelineCache*                      pSrcCaches)
{
    Device*        pDevice = ApiDevice::ObjectFromHandle(device);
    PipelineCache* pDstCache = PipelineCache::ObjectFromHandle(dstCache);

    Util::AutoBuffer<const PipelineCache*, 8, PalAllocator> srcCaches(srcCacheCount, pDevice->VkInstance()->Allocator());

    for (uint32_t i = 0; i < srcCacheCount; i++)
    {
        srcCaches[i] = PipelineCache::ObjectFromHandle(pSrcCaches[i]);
    }

    VkResult result = pDstCache->Merge(srcCacheCount, &srcCaches[0]);

    return result;
}

} // namespace entry

} // namespace vk
