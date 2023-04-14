/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_deferred_operation.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"

#include "palFormatInfo.h"

#include <climits>

namespace vk
{

// =====================================================================================================================
// Create a new deferred host operation object
VkResult DeferredHostOperation::Create(
    Device*                            pDevice,
    const VkAllocationCallbacks*       pAllocator,
    VkDeferredOperationKHR*            pDeferredOperation)
{
    // Allocate memory for the host operation handle
    const size_t apiSize = sizeof(DeferredHostOperation);

    void* pMemory = pDevice->AllocApiObject(pAllocator, apiSize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VK_PLACEMENT_NEW(pMemory) DeferredHostOperation(pDevice->VkInstance());

    *pDeferredOperation = DeferredHostOperation::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// =====================================================================================================================
DeferredHostOperation::DeferredHostOperation(
    Instance* pInstance)
    :
    m_pfnCallback(&UnusedCallback),
#if VKI_RAY_TRACING
    m_rtPipelineCreate{},
#endif
    m_pInstance(pInstance),
    m_workloadCount(0),
    m_pWorkloads(nullptr)
{

}

// =====================================================================================================================
void DeferredHostOperation::DestroyWorkloads()
{
    if (m_pWorkloads != nullptr)
    {
        // Destroy workload events
        for (uint32_t i = 0; i < m_workloadCount; ++i)
        {
            Util::Destructor(&m_pWorkloads[i].event);
        }

        // Free corresponding memory
        m_workloadCount = 0;
        m_pInstance->FreeMem(m_pWorkloads);
        m_pWorkloads = nullptr;
    }
}

// =====================================================================================================================
VkResult DeferredHostOperation::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    DestroyWorkloads();

    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

// =====================================================================================================================
int32_t DeferredHostOperation::UnusedCallback(
    Device*                pDevice,
    DeferredHostOperation* pHost,
    DeferredCallbackType   type)
{
    uint32_t result;

    switch (type)
    {
    case DeferredCallbackType::Join:
    {
        result = VK_SUCCESS;
        break;
    }
    case DeferredCallbackType::GetMaxConcurrency:
    {
        result = 1;
        break;
    }
    case DeferredCallbackType::GetResult:
    {
        result = VK_SUCCESS;
        break;
    }
    default:
        VK_NEVER_CALLED();
        result = 0;
        break;
    }

    return result;
}

// =====================================================================================================================
void DeferredHostOperation::SetOperation(
    DeferredHostCallback pfnCallback)
{
    m_pfnCallback = pfnCallback;
}

// =====================================================================================================================
VkResult DeferredHostOperation::Join(
    Device* pDevice)
{
    return static_cast<VkResult>(m_pfnCallback(pDevice, this, DeferredCallbackType::Join));
}

// =====================================================================================================================
VkResult DeferredHostOperation::GetOperationResult(
    Device* pDevice)
{
    return static_cast<VkResult>(m_pfnCallback(pDevice, this, DeferredCallbackType::GetResult));
}

// =====================================================================================================================
uint32_t DeferredHostOperation::GetMaxConcurrency(
    Device* pDevice)
{
    return static_cast<uint32_t>(m_pfnCallback(pDevice, this, DeferredCallbackType::GetMaxConcurrency));
}

// =====================================================================================================================
VkResult DeferredHostOperation::GenerateWorkloads(uint32_t count)
{
    VkResult result = VK_SUCCESS;

    DestroyWorkloads();

    size_t memSize = sizeof(DeferredWorkload) * count;
    void* pMem = m_pInstance->AllocMem(memSize,VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pMem != nullptr)
    {
        memset(pMem, 0, memSize);
        m_pWorkloads = static_cast<DeferredWorkload*>(pMem);
        m_workloadCount = count;

        Util::EventCreateFlags flags = {};
        flags.manualReset       = false;
        flags.initiallySignaled = false;

        for (uint32_t i = 0; i < m_workloadCount; ++i)
        {
            PAL_PLACEMENT_NEW(&(m_pWorkloads[i].event)) Util::Event();
            m_pWorkloads[i].event.Init(flags);
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
void DeferredHostOperation::ExecuteWorkload(DeferredWorkload* pWorkload)
{
    uint32_t totalInstances = pWorkload->totalInstances;

    if ((totalInstances != UINT_MAX) && (pWorkload->nextInstance < totalInstances))
    {
        pWorkload->Execute(pWorkload->pPayloads);
    }
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyDeferredOperationKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation,
    const VkAllocationCallbacks*                pAllocator)
{
    Device*                      pDevice    = ApiDevice::ObjectFromHandle(device);
    DeferredHostOperation*       pOperation = DeferredHostOperation::ObjectFromHandle(operation);
    const VkAllocationCallbacks* pAllocCB   = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    pOperation->Destroy(pDevice, pAllocCB);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetDeferredOperationResultKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation)
{
    DeferredHostOperation* pOperation = DeferredHostOperation::ObjectFromHandle(operation);

    return pOperation->GetOperationResult(ApiDevice::ObjectFromHandle(device));
}

// =====================================================================================================================
VKAPI_ATTR uint32_t VKAPI_CALL vkGetDeferredOperationMaxConcurrencyKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation)
{
    DeferredHostOperation* pOperation = DeferredHostOperation::ObjectFromHandle(operation);

    return pOperation->GetMaxConcurrency(ApiDevice::ObjectFromHandle(device));
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDeferredOperationJoinKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      operation)
{
    DeferredHostOperation* pOperation = DeferredHostOperation::ObjectFromHandle(operation);

    return pOperation->Join(ApiDevice::ObjectFromHandle(device));
}

} // namespace entry
} // namespace vk
