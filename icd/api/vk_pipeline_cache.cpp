/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_physical_device.h"
#include "include/vk_pipeline_cache.h"
#include "palAutoBuffer.h"

#include "include/binary_cache_serialization.h"

namespace vk
{

// =====================================================================================================================
PipelineCache::PipelineCache(
    const Device*           pDevice,
    PipelineBinaryCache*    pBinaryCache
    )
    :
    m_pDevice(pDevice),
    m_pBinaryCache(pBinaryCache)
{
}

// =====================================================================================================================
PipelineCache::~PipelineCache()
{
}

// =====================================================================================================================
VkResult PipelineCache::Create(
    Device*                          pDevice,
    const VkPipelineCacheCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*     pAllocator,
    VkPipelineCache*                 pPipelineCache)
{
    VkResult                result           = VK_SUCCESS;
    const RuntimeSettings&  settings         = pDevice->GetRuntimeSettings();
    uint32_t                numPalDevices    = pDevice->NumPalDevices();

    bool                    usePipelineCacheInitialData   = false;

    if ((pCreateInfo->initialDataSize > 0) && settings.usePipelineCacheInitialData)
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
                    const void* pData   = Util::VoidPtrInc(pCreateInfo->pInitialData, sizeof(PipelineCacheHeaderData));
                    size_t dataSize     = pCreateInfo->initialDataSize - sizeof(PipelineCacheHeaderData);
                    vk::PhysicalDevice* pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);

                    if (PipelineBinaryCache::IsValidBlob(pPhysicalDevice->VkInstance()->GetAllocCallbacks(),
                                                         pPhysicalDevice->GetPlatformKey(),
                                                         dataSize,
                                                         pData))
                    {
                        usePipelineCacheInitialData = true;
                    }
                }
            }
        }
    }

    // Allocate system memory for all objects
    const size_t objSize = sizeof(PipelineCache);
    void* pMemory = pDevice->AllocApiObject(pAllocator, objSize);

    if (pMemory == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    else
    {
        uint32_t expectedEntries = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPipelineCacheExpectedEntryCount();

        PipelineBinaryCache* pBinaryCache = nullptr;
        if (settings.allowExternalPipelineCacheObject)
        {
            const void* pInitialData = nullptr;
            size_t initialDataSize = 0;

            if (usePipelineCacheInitialData)
            {
                pInitialData    = Util::VoidPtrInc(pCreateInfo->pInitialData, sizeof(PipelineCacheHeaderData));
                initialDataSize = pCreateInfo->initialDataSize - sizeof(PipelineCacheHeaderData);
            }

            vk::PhysicalDevice* pDefaultPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
            pBinaryCache = PipelineBinaryCache::Create(
                pDefaultPhysicalDevice->VkInstance()->GetAllocCallbacks(),
                pDefaultPhysicalDevice->GetPlatformKey(),
                pDevice->GetCompiler(DefaultDeviceIndex)->GetGfxIp(),
                pDefaultPhysicalDevice->GetRuntimeSettings(),
                pDefaultPhysicalDevice->PalDevice()->GetCacheFilePath(),
#if ICD_GPUOPEN_DEVMODE_BUILD
                pDefaultPhysicalDevice->VkInstance()->GetDevModeMgr(),
#endif
                expectedEntries,
                initialDataSize,
                pInitialData,
                false);

            // This isn't a terminal failure, the device can continue without the pipeline cache if need be.
            VK_ALERT(pBinaryCache == nullptr);
        }
        PipelineCache* pCache = VK_PLACEMENT_NEW(pMemory) PipelineCache(pDevice, pBinaryCache);
        *pPipelineCache = PipelineCache::HandleFromVoidPointer(pMemory);
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineCache::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    pDevice->VkPhysicalDevice(DefaultDeviceIndex)->DecreasePipelineCacheCount();
    if (m_pBinaryCache != nullptr)
    {
        m_pBinaryCache->Destroy();
        m_pBinaryCache = nullptr;
    }

    this->~PipelineCache();

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
// Stores AMD specific pipeline cache data to binary cache.
VkResult PipelineCache::GetData(
    void*   pData,
    size_t* pSize)
{
    VK_ASSERT(pSize != nullptr);

    VkResult        result = VK_SUCCESS;

    if (m_pBinaryCache != nullptr)
    {
        result = m_pBinaryCache->Serialize(pData, pSize);
    }
    else
    {
        *pSize = 0;
    }

    return result;
}

// =====================================================================================================================
VkResult PipelineCache::Merge(
    uint32_t              srcCacheCount,
    const PipelineCache** ppSrcCaches)
{
    VkResult result = VK_SUCCESS;

    if (m_pBinaryCache != nullptr)
    {
        Util::AutoBuffer<const PipelineBinaryCache *, 16, PalAllocator> binaryCaches(
            srcCacheCount,
            m_pDevice->VkInstance()->Allocator());

        for (uint32_t cacheIdx = 0; cacheIdx < srcCacheCount; cacheIdx++)
        {
            binaryCaches[cacheIdx] = ppSrcCaches[cacheIdx]->GetPipelineCache();
        }

        result = m_pBinaryCache->Merge(srcCacheCount, &binaryCaches[0]);
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
        Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
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

    const size_t       fullDataSize = VkPipelineCacheHeaderDataSize + privateDataSize;

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

        size_t       headerBytesWritten = 0;
        Util::Result headerWriteRes     =  WriteVkPipelineCacheHeaderData(
                                            pData,
                                            fullDataSize,
                                            palProps.vendorId,
                                            palProps.deviceId,
                                            physicalDeviceProps.pipelineCacheUUID,
                                            sizeof(physicalDeviceProps.pipelineCacheUUID),
                                            &headerBytesWritten);

        if (headerWriteRes != Util::Result::Success)
        {
            *pDataSize = 0;
            result = VK_INCOMPLETE;
        }
        else
        {
            VK_ASSERT(headerBytesWritten == VkPipelineCacheHeaderDataSize);
            if (privateDataSize > 0)
            {
                void* pPrivateData = Util::VoidPtrInc(pData, headerBytesWritten);
                result = pCache->GetData(pPrivateData, &privateDataSize);
            }
            // set pDataSize, privateDataSize can be 0.
            *pDataSize = privateDataSize + headerBytesWritten;
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
