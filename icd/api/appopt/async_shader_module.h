/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  async_shader_module.h
* @brief Header file of class async::ShaderModule
***********************************************************************************************************************
*/

#ifndef __ASYNC_SHADER_MODULE_H__
#define __ASYNC_SHADER_MODULE_H__

#pragma once

#include "include/vk_dispatch.h"

namespace vk
{

namespace async
{

// =====================================================================================================================
// Implementation of a async shader module
class ShaderModule final : public vk::NonDispatchable<VkShaderModule, ShaderModule>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkShaderModule*                 pShaderModule);

    VkResult Destroy(
        Device*                      pDevice,
        const VkAllocationCallbacks* pAllocator);

    VK_INLINE VkShaderModule GetNextLayerModule()
    {
        return (m_asyncModule == VK_NULL_HANDLE) ? m_immedModule : m_asyncModule;
    }

    void Execute(AsyncLayer* pAsyncLayer, ShaderModuleTask* pTask);

    void AsyncBuildShaderModule(AsyncLayer* pAsyncLayer);

protected:
    ShaderModule(VkShaderModule immedModule);

    VkShaderModule m_immedModule;        // Shader module handle which is compiled with immedidate mode
    VkShaderModule m_asyncModule;        // Shader module handle which is compiled with async mode

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderModule);
};

} // namespace async

} // namespace vk

#endif
