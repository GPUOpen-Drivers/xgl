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

#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline_cache.h"

#include "palShaderCache.h"
#include "palAutoBuffer.h"
#include "llpc.h"

namespace vk
{

// =====================================================================================================================
PipelineCache::PipelineCache(
    const Device*       pDevice,
    PipelineCacheType   cacheType,
    IShaderCachePtr*    pShaderCaches)
    :
    m_pDevice(pDevice),
    m_cacheType(cacheType)
{
    memcpy(m_pShaderCaches, pShaderCaches, sizeof(m_pShaderCaches[0]) * pDevice->NumPalDevices());
    memset(m_pShaderCaches + pDevice->NumPalDevices(),
           0,
           sizeof(m_pShaderCaches[0]) * (MaxPalDevices - pDevice->NumPalDevices()));
}

// =====================================================================================================================
PipelineCache::~PipelineCache()
{
    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
    {
        if (m_cacheType == PipelineCacheTypeLlpc)
        {
            m_pShaderCaches[i].pLlpcShaderCache->Destroy();
        }
        else
        {
            m_pShaderCaches[i].pPalShaderCache->Destroy();
        }
    }
}

// =====================================================================================================================
VkResult PipelineCache::Create(
    const Device*                    pDevice,
    const VkPipelineCacheCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*     pAllocator,
    VkPipelineCache*                 pPipelineCache)
{
    VkResult result = VK_SUCCESS;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    uint32_t numPalDevices = pDevice->NumPalDevices();

    size_t palSize = 0;
    size_t pipelineCacheSize[MaxPalDevices];
    PipelineCacheType cacheType = PipelineCacheTypePal;

    if ((settings.enableLlpc == LlpcModeEnable) || (settings.enableLlpc == LlpcModeAutoFallback))
    {
        cacheType = PipelineCacheTypeLlpc;
    }
    else
    {
        for (uint32_t i = 0; i < numPalDevices; i++)
        {
            const Pal::IDevice* pPalDevice = pDevice->PalDevice(i);
            pipelineCacheSize[i] = pPalDevice->GetShaderCacheSize();
            palSize += pipelineCacheSize[i];
        }
    }

    bool useInitialData = false;

    if (pCreateInfo->initialDataSize > 0)
    {
        const PipelineCacheHeaderData* pHeader = static_cast<const PipelineCacheHeaderData*>(pCreateInfo->pInitialData);

        if (pHeader->headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
        {
            const Pal::DeviceProperties& palProps = pDevice->VkPhysicalDevice()->PalProperties();

            if ((pHeader->vendorID == palProps.vendorId) &&
                (pHeader->deviceID == palProps.deviceId))
            {
                VkPhysicalDeviceProperties physicalDeviceProps;
                pDevice->VkPhysicalDevice()->GetDeviceProperties(&physicalDeviceProps);
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
    const size_t objSize = sizeof(PipelineCache) + palSize;

    void* pMemory = pDevice->AllocApiObject(objSize, pAllocator);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        const PipelineCachePrivateHeaderData* pPrivateDataHeader = nullptr;

        void* pBlobs[MaxPalDevices] = {};

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

        IShaderCachePtr shaderCaches[MaxPalDevices] = {};
        if (cacheType == PipelineCacheTypeLlpc)
        {
            Llpc::ShaderCacheCreateInfo createInfo = {};
            for (uint32_t i = 0; i < numPalDevices; i++)
            {
                auto pCompiler = pDevice->GetCompiler();

                if (useInitialData)
                {
                    createInfo.pInitialData    = pBlobs[i];
                    createInfo.initialDataSize = static_cast<size_t>(pPrivateDataHeader->blobSize[i]);
                }

                Llpc::Result llpcResult = pCompiler->CreateShaderCache(
                    &createInfo,
                    &shaderCaches[i].pLlpcShaderCache);

                if (llpcResult != Llpc::Result::Success)
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    break;
                }
            }

            // Something went wrong with creating the LLPC object. Free memory
            if (result != VK_SUCCESS)
            {
                for (uint32_t i = 0; i < numPalDevices; i++)
                {
                    if (shaderCaches[i].pLlpcShaderCache != nullptr)
                    {
                        shaderCaches[i].pLlpcShaderCache->Destroy();
                    }
                }
            }
        }
        else
        {
            Pal::ShaderCacheCreateInfo createInfo = {};
            createInfo.expectedEntries = const_cast<Device*>(pDevice)->GetPipelineCacheExpectedEntryCount();
            size_t palOffset = sizeof(PipelineCache);
            for (uint32_t i = 0; i < numPalDevices; i++)
            {
                const Pal::IDevice* pPalDevice = pDevice->PalDevice(i);

                if (useInitialData)
                {
                    createInfo.pInitialData    = pBlobs[i];
                    createInfo.initialDataSize = static_cast<size_t>(pPrivateDataHeader->blobSize[i]);
                }

                Pal::Result palResult = pPalDevice->CreateShaderCache(
                    createInfo,
                    Util::VoidPtrInc(pMemory, palOffset),
                    &shaderCaches[i].pPalShaderCache);

                if (palResult != Pal::Result::Success)
                {
                    result = PalToVkResult(palResult);
                    break;
                }

                // Move to next PAL object
                palOffset += pipelineCacheSize[i];
            }

            // Something went wrong with creating the PAL object. Free memory
            if (result != VK_SUCCESS)
            {
                for (uint32_t i = 0; i < numPalDevices; i++)
                {
                    if (shaderCaches[i].pPalShaderCache != nullptr)
                    {
                        shaderCaches[i].pPalShaderCache->Destroy();
                    }
                }
            }
        }

        if (result == VK_SUCCESS)
        {
            PipelineCache* pCache = VK_PLACEMENT_NEW(pMemory) PipelineCache(pDevice, cacheType, shaderCaches);
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
    this->~PipelineCache();

    pAllocator->pfnFree(pAllocator->pUserData, this);

    const_cast<Device*>(pDevice)->DecreasePipelineCacheCount();

    return VK_SUCCESS;
}

// =====================================================================================================================
// This function stores AMD specific pipeline cache data as follows:
// First, AMD private pipeline cache header data, then the contents of
// each PAL shader cache of each PAL device.
// ------------------------------------------------------------------------<-- offset 0 (after the header)
// | PipelineCachePrivateHeaderData                                       |
// ------------------------------------------------------------------------<-- offset H (PipelineCachePrivateHeaderData)
// | shader cache content of device 0 ...                                 |
// ------------------------------------------------------------------------<-- offset H + A
// | shader cache content of device 1 ...                                 |
// ------------------------------------------------------------------------<-- offset H + A + B
// | shader cache content of device 2 ...                                 |
// ------------------------------------------------------------------------<-- offset H + A + B + C
// | shader cache content of device 3 ...                                 |
// ------------------------------------------------------------------------
VkResult PipelineCache::GetData(
    void*   pData,
    size_t* pSize)
{
    VK_ASSERT(pSize != nullptr);

    VkResult        result = VK_SUCCESS;
    uint32_t numPalDevices = m_pDevice->NumPalDevices();

    // The starting is an array of blob sizes of each PAL shader cache.
    size_t allBlobSize = sizeof(PipelineCachePrivateHeaderData);
    PipelineCachePrivateHeaderData headerData = {};

    headerData.cacheType = m_cacheType;
    for (uint32_t i = 0; i < numPalDevices; i++)
    {
        size_t blobSize = 0;
        if (m_cacheType == PipelineCacheTypeLlpc)
        {
            auto llpcResult = m_pShaderCaches[i].pLlpcShaderCache->Serialize(nullptr, &blobSize);
            VK_ASSERT(llpcResult == Llpc::Result::Success);
        }
        else
        {
            auto palResult = m_pShaderCaches[i].pPalShaderCache->Serialize(nullptr, &blobSize);
            VK_ASSERT(palResult == Pal::Result::Success);
        }
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

            if (m_cacheType == PipelineCacheTypeLlpc)
            {
                auto llpcResult = m_pShaderCaches[i].pLlpcShaderCache->Serialize(pBlob, &blobSize);
                if (llpcResult != Llpc::Result::Success)
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                    break;
                }
            }
            else
            {
                auto palResult = m_pShaderCaches[i].pPalShaderCache->Serialize(pBlob, &blobSize);
                if (palResult != Pal::Result::Success)
                {
                    result = PalToVkResult(palResult);
                    break;
                }
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
    Util::AutoBuffer<IShaderCachePtr, 16, PalAllocator> shaderCaches(
        srcCacheCount * m_pDevice->NumPalDevices(),
        m_pDevice->VkInstance()->Allocator());

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        for (uint32_t cacheIdx = 0; cacheIdx < srcCacheCount; cacheIdx++)
        {
            VK_ASSERT(ppSrcCaches[cacheIdx]->GetPipelineCacheType() == GetPipelineCacheType());
            // Store all PAL caches like this d0c0,d0c1,d0c2...,d1c0,d1c2,d1c3...
            shaderCaches[deviceIdx * srcCacheCount + cacheIdx] = ppSrcCaches[cacheIdx]->GetShaderCache(deviceIdx);
        }
    }

    VkResult result = VK_SUCCESS;
    for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
    {
        if (GetPipelineCacheType() == PipelineCacheTypePal)
        {
            auto palResult = m_pShaderCaches[i].pPalShaderCache->Merge(
                srcCacheCount,
                const_cast<const Pal::IShaderCache **>(&shaderCaches[i * srcCacheCount].pPalShaderCache));

            if (palResult != Pal::Result::Success)
            {
                result = PalToVkResult(palResult);
                break;
            }
        }
        else
        {
            auto llpcResult = m_pShaderCaches[i].pLlpcShaderCache->Merge(
                srcCacheCount,
                const_cast<const Llpc::IShaderCache **>(&shaderCaches[i * srcCacheCount].pLlpcShaderCache));

            if (llpcResult != Llpc::Result::Success)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
                break;
            }
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

    const Pal::DeviceProperties& palProps = pDevice->VkPhysicalDevice()->PalProperties();

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
        ApiDevice::ObjectFromHandle(device)->VkPhysicalDevice()->GetDeviceProperties(&physicalDeviceProps);

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
