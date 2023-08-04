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

#ifndef __VK_SHADER_H__
#define __VK_SHADER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/pipeline_compiler.h"

namespace Pal { enum class ResourceMappingNodeType : Pal::uint32; }

#if VKI_RAY_TRACING
#define VK_INTERNAL_SHADER_FLAGS_RAY_TRACING_INTERNAL_SHADER_BIT 0x80000000u
#endif

namespace vk
{

class Device;
class ApiShader;
class Instance;

typedef void* (VKAPI_CALL *BilShaderAllocFun)(Instance* pInstance, void* pUserData, size_t size);

extern void* VKAPI_CALL AllocateShaderOutput(void* pInstance, void* pUserData, size_t size);

// =====================================================================================================================
// Implementation of a Vulkan shader module
class ShaderModule final : public NonDispatchable<VkShaderModule, ShaderModule>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkShaderModule*                 pShaderModule);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    size_t GetCodeSize() const { return m_codeSize; }
    const void* GetCode() const { return m_pCode; }
    const ShaderModuleHandle* GetShaderModuleHandle() const { return &m_handle; }

    Pal::ShaderHash GetCodeHash(const char* pEntryPoint) const
        { return GetCodeHash(m_codeHash, pEntryPoint); }

    const Pal::ShaderHash& GetCodeHash() const
        { return m_codeHash; }

    void* GetShaderData(PipelineCompilerType compilerType) const
        { return GetShaderData(compilerType, &m_handle); }

    void* GetFirstValidShaderData() const
        { return GetFirstValidShaderData(&m_handle); }

    static Pal::ShaderHash BuildCodeHash(
        const void*                  pCode,
        const size_t                 codeSize);

    static Pal::ShaderHash GetCodeHash(Pal::ShaderHash codeHash, const char* pEntryPoint);

    static void* GetShaderData(PipelineCompilerType compilerType, const ShaderModuleHandle* pHandle);

    static void* GetFirstValidShaderData(const ShaderModuleHandle* pHandle);

protected:
    ShaderModule(size_t codeSize, const void* pCode);
    VkResult Init(Device* pDevice, VkShaderModuleCreateFlags flags);

    size_t                     m_codeSize;
    const void*                m_pCode;
    ShaderModuleHandle         m_handle;
    Pal::ShaderHash            m_codeHash;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderModule);
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(
    VkDevice                                    device,
    VkShaderModule                              shaderModule,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkGetShaderModuleIdentifierEXT(
    VkDevice                                    device,
    VkShaderModule                              shaderModule,
    VkShaderModuleIdentifierEXT*                pIdentifier);

VKAPI_ATTR void VKAPI_CALL vkGetShaderModuleCreateInfoIdentifierEXT(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    VkShaderModuleIdentifierEXT*                pIdentifier);

} // namespace entry

} // namespace vk

#endif /* __VK_SHADER_H__ */
