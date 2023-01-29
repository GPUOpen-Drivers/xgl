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
* @file  sqtt_object_mgr.h
* @brief This object tracks various object-specific metadata about Vulkan objects, e.g. debug object names/tags
***********************************************************************************************************************
*/

#ifndef __SQTT_SQTT_OBJECT_MGR_H__
#define __SQTT_SQTT_OBJECT_MGR_H__

#pragma once

#include "khronos/vulkan.h"
#include "vk_alloccb.h"
#include "vk_utils.h"
#include "vk_device.h"

#include "palHashMap.h"
#include "palMutex.h"
#include "palUtil.h"

namespace vk
{
class Device;
};

namespace vk
{

struct SqttMetaState
{
    SqttMetaState();
    ~SqttMetaState();

    void Destroy(Device* pDevice);

    size_t debugNameCapacity;
    char*  pDebugName; // Debug object name string

    union
    {
        struct
        {
            VkShaderModule shaderModules[Pal::NumShaderTypes];
        } pipeline;
    };

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SqttMetaState);
};

// =====================================================================================================================
class SqttObjectMgr
{
public:
    SqttObjectMgr();
    ~SqttObjectMgr();

    void Init(Device* pDevice);

    template<typename ObjectType>
    bool IsEnabled(
        ObjectType objectType) const;

    template<typename HandleType,
             typename ObjectType> SqttMetaState* GetMetaState(
        ObjectType objectType,
        HandleType handle);

    template<typename HandleType,
             typename ObjectType> const char* GetDebugName(
        ObjectType objectType,
        HandleType handle);

    template<typename HandleType,
             typename ObjectType> SqttMetaState* ObjectCreated(
        Device*    pDevice,
        ObjectType objectType,
        HandleType handle);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SqttObjectMgr);

    template<typename ObjectType>
    void SetMetaState(
        ObjectType     objectType,
        uint64_t       handle,
        SqttMetaState* pState);

    typedef Util::HashMap<uint64_t, SqttMetaState*, PalAllocator> MetaDataMap;

    struct ObjectTypeState
    {
        ObjectTypeState(Device* pDevice);

        void Init(Device* pDevice);
        void Destroy(Device* pDevice);

        bool        enabled;
        Util::Mutex dataMutex;
        MetaDataMap dataMap;
    };

    Device*          m_pDevice;
    ObjectTypeState* m_pObjects;

    // Constants
    const uint32_t   ObjectTypeBeginRange;
    const uint32_t   ObjectTypeEndRange;
    const uint32_t   ObjectTypeRangeSize;
};

// =====================================================================================================================
template<typename ObjectType>
void SqttObjectMgr::SetMetaState(
    ObjectType     objectType,
    uint64_t       handle,
    SqttMetaState* pState)
{
    if (m_pObjects != nullptr)
    {
        const uint32_t idx = (objectType - ObjectTypeBeginRange);

        VK_ASSERT(idx < ObjectTypeRangeSize);

        if (m_pObjects[idx].enabled)
        {
            Util::MutexAuto lock(&m_pObjects[idx].dataMutex);

            VK_ASSERT(m_pObjects[idx].dataMap.FindKey(handle) == nullptr);

            m_pObjects[idx].dataMap.Insert(handle, pState);
        }
    }
}

// =====================================================================================================================
template<typename HandleType,
         typename ObjectType>
SqttMetaState* SqttObjectMgr::GetMetaState(
    ObjectType objectType,
    HandleType handle)
{
    return nullptr;
}

// =====================================================================================================================
template<typename ObjectType>
bool SqttObjectMgr::IsEnabled(
    ObjectType objectType
    ) const
{
    return false;
}

// =====================================================================================================================
template<typename HandleType,
         typename ObjectType>
const char* SqttObjectMgr::GetDebugName(
    ObjectType objectType,
    HandleType handle)
{
    return "";
}

// =====================================================================================================================
template<typename HandleType,
         typename ObjectType>
SqttMetaState* SqttObjectMgr::ObjectCreated(
    Device*    pDevice,
    ObjectType objectType,
    HandleType handle)
{
    SqttMetaState* pState = nullptr;

    if (IsEnabled(objectType))
    {
        pState = PAL_NEW(SqttMetaState, pDevice->VkInstance()->Allocator(), Util::AllocInternal);

        if (pState != nullptr)
        {
            SetMetaState(objectType, uint64_t(handle), pState);
        }
    }

    return pState;
}

}; // namespace vk

#endif /* __SQTT_SQTT_OBJECT_MGR_H__ */
