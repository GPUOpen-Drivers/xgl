/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"

#include "palGpuMemoryBindable.h"
#include "palPipeline.h"
#include "palMetroHash.h"

#include "llpc.h"

#include <climits>

namespace vk
{

// =====================================================================================================================
// Allocate shader converter and patch's output, it is an callback function.
//
// NOTE: It must be called only once for each shader conversion or IL patching. The base address of allocated memory
// is stored in user data's ppSystemData.
void* VKAPI_CALL AllocateShaderOutput(
    void*     pInstance,
    void*     pUserData,
    size_t    size)
{
    void** ppSystemData = reinterpret_cast<void**>(pUserData);
    // Make sure this function is called only once
    VK_ASSERT(ppSystemData != nullptr);
    VK_ASSERT(*ppSystemData == nullptr);

    // Allocate system memory
    *ppSystemData = reinterpret_cast<Instance*>(pInstance)->AllocMem(
        size,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    return *ppSystemData;
}

// =====================================================================================================================
// Concatenates a MetroHash::Hash to two 64-bit uints.
VK_INLINE void MetroHashTo128Bit(
    const Util::MetroHash::Hash& hash,
    uint64_t*                    pLower,
    uint64_t*                    pUpper)
{
    *pLower = static_cast<uint64_t>(hash.dwords[0]) |
              static_cast<uint64_t>(hash.dwords[1]) << 32;
    *pUpper = static_cast<uint64_t>(hash.dwords[2]) |
              static_cast<uint64_t>(hash.dwords[3]) << 32;
}

// =====================================================================================================================
// Returns a 128-bit hash based on this module's SPIRV code plus an optional entry point combination.
Pal::ShaderHash ShaderModule::GetCodeHash(
    const char* pEntryPoint
    ) const
{
    Pal::ShaderHash hash = m_codeHash;

    if (pEntryPoint != nullptr)
    {
        size_t entryLength = strlen(pEntryPoint);

        if (entryLength > 0)
        {
            Util::MetroHash::Hash entryHash = {};
            Util::MetroHash128::Hash(reinterpret_cast<const uint8_t*>(pEntryPoint), entryLength, entryHash.bytes);

            uint64_t entryLower;
            uint64_t entryUpper;

            MetroHashTo128Bit(entryHash, &entryLower, &entryUpper);

            hash.lower ^= entryLower;
            hash.upper ^= entryUpper;
        }
    }

    return hash;
}

// =====================================================================================================================
// Gets shader data per compiler type.
void* ShaderModule::GetShaderData(
    PipelineCompilerType       compilerType,
    const ShaderModuleHandle*  pHandle)
{
    void* pShaderModule = nullptr;
    if (compilerType == PipelineCompilerTypeLlpc)
    {
        pShaderModule = pHandle->pLlpcShaderModule;
    }
    return pShaderModule;
}

// =====================================================================================================================
// Gets first valid shader data.
void* ShaderModule::GetFirstValidShaderData() const
{
    if (m_handle.pLlpcShaderModule)
    {
        return m_handle.pLlpcShaderModule;
    }

    return nullptr;
}

// =====================================================================================================================
ShaderModule::ShaderModule(
    size_t      codeSize,
    const void* pCode)
{
    m_codeSize = codeSize;
    m_pCode    = pCode;

    // Calculate a 128-bit hash from the SPIRV code.  This is used by profile-guided compilation
    // parameter tuning.
    Util::MetroHash::Hash codeHash = {};
    Util::MetroHash128::Hash(static_cast<const uint8_t*>(pCode), codeSize, codeHash.bytes);

    MetroHashTo128Bit(codeHash, &m_codeHash.lower, &m_codeHash.upper);
    memset(&m_handle, 0, sizeof(m_handle));
}

// =====================================================================================================================
VkResult ShaderModule::Create(
    const Device*                   pDevice,
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkShaderModule*                 pShaderModule)
{
    const size_t objSize = sizeof(ShaderModule) + pCreateInfo->codeSize;

    void* pMemory = pDevice->AllocApiObject(objSize, pAllocator);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    void* pCode = Util::VoidPtrInc(pMemory, sizeof(ShaderModule));

    memcpy(pCode, pCreateInfo->pCode, pCreateInfo->codeSize);

    VK_PLACEMENT_NEW(pMemory) ShaderModule(pCreateInfo->codeSize, pCode);

    ShaderModule* pShaderModuleObj = static_cast<ShaderModule*>(pMemory);
    VkResult vkResult = pShaderModuleObj->Init(pDevice, pCreateInfo->flags);
    VK_ASSERT(vkResult == VK_SUCCESS);

    *pShaderModule = ShaderModule::HandleFromVoidPointer(pMemory);

    return vkResult;
}

// =====================================================================================================================
// Initialize shader module object, performing SPIR-V to AMD IL shader binary conversion.
VkResult ShaderModule::Init(const Device* pDevice, VkShaderModuleCreateFlags flags)
{
    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);
    return pCompiler->BuildShaderModule(pDevice,
                                        flags,
                                        m_codeSize,
                                        m_pCode,
                                        &m_handle
                                        );
}

// =====================================================================================================================
VkResult ShaderModule::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    PipelineCompiler* pCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    pCompiler->FreeShaderModule(&m_handle);

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);

    return VK_SUCCESS;
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(
    VkDevice                                    device,
    VkShaderModule                              shaderModule,
    const VkAllocationCallbacks*                pAllocator)
{
    if (shaderModule != VK_NULL_HANDLE)
    {
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        ShaderModule::ObjectFromHandle(shaderModule)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
