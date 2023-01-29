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
/**
 ***********************************************************************************************************************
 * @file  vk_gpa_session.h
 * @brief Functionality for the VkGpaSession object (part of VK_AMD_gpa_interface)
 ***********************************************************************************************************************
 */

#ifndef __VK_GPA_SESSION_H__
#define __VK_GPA_SESSION_H__
#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"

#include "palGpaSession.h"

namespace Pal
{
} // namespace Pal

namespace vk
{

class CmdBuffer;
class Device;

// =====================================================================================================================
// Implements the VkGpaSessionAMD object that is part of the VK_AMD_gpa_interface extension.  This is a thin wrapper
// around a GpuUtil::GpaSession object which is a utility class for performing performance counting operations
// through Vulkan.  The primary client of these kinds of objects is AMD's GPUPerfAPI.
class GpaSession final : public NonDispatchable<VkGpaSessionAMD, GpaSession>
{
public:
    static VkResult Create(
        Device*                          pDevice,
        const VkGpaSessionCreateInfoAMD* pCreateInfo,
        const VkAllocationCallbacks*     pAllocator,
        VkGpaSessionAMD*                 pGpaSession);

    VkResult Init();
    VkResult Reset();
    VkResult CmdBegin(CmdBuffer* pCmdBuf);
    VkResult CmdEnd(CmdBuffer* pCmdBuf);
    VkResult CmdBeginSample(CmdBuffer* pCmdbuf, const VkGpaSampleBeginInfoAMD* pGpaSampleBeginInfo, uint32_t* pSampleID);
    void CmdEndSample(CmdBuffer* pCmdbuf, uint32_t sampleID);
    VkResult GetResults(uint32_t sampleID, size_t* pSizeInBytes, void* pData);
    void CmdCopyResults(CmdBuffer* pCmdBuf);
    VkResult SetClockMode(VkGpaDeviceClockModeInfoAMD* pInfo);

    void Destroy(const VkAllocationCallbacks* pAllocator);

    VkResult GetStatus()
        { return m_session.IsReady() ? VK_SUCCESS : VK_NOT_READY; }

    GpuUtil::GpaSession* PalSession()
        { return &m_session; }

private:
    GpaSession(Device* pDevice);
    GpaSession(const GpaSession& session);

    Device* const       m_pDevice;
    GpuUtil::GpaSession m_session;

    GpaSession& operator=(const GpaSession&) = delete;
};

namespace entry
{

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGpaSessionAMD(
    VkDevice                                    device,
    const VkGpaSessionCreateInfoAMD*            pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkGpaSessionAMD*                            pGpaSession);

VKAPI_ATTR void VKAPI_CALL vkDestroyGpaSessionAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkCmdBeginGpaSessionAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession);

VKAPI_ATTR VkResult VKAPI_CALL vkCmdEndGpaSessionAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession);

VKAPI_ATTR VkResult VKAPI_CALL vkCmdBeginGpaSampleAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession,
    const VkGpaSampleBeginInfoAMD*              pGpaSampleBeginInfo,
    uint32_t*                                   pSampleID);

VKAPI_ATTR void VKAPI_CALL vkCmdEndGpaSampleAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession,
    uint32_t                                    sampleID);

VKAPI_ATTR VkResult VKAPI_CALL vkGetGpaSessionStatusAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession);

VKAPI_ATTR VkResult VKAPI_CALL vkGetGpaSessionResultsAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession,
    uint32_t                                    sampleID,
    size_t*                                     pSizeInBytes,
    void*                                       pData);

VKAPI_ATTR VkResult VKAPI_CALL vkResetGpaSessionAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyGpaSessionResultsAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession);

} // namespace entry

} // namespace vk

#endif /* __VK_GPA_SESSION_H__ */
