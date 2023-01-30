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
* @file  sqtt_object_mgr.cpp
* @brief Implementation of SqttObjectMgr
***********************************************************************************************************************
*/

#include "sqtt_object_mgr.h"
#include "vk_device.h"

#include "palHashMapImpl.h"
#include "palSysMemory.h"

namespace vk
{

// =====================================================================================================================
SqttObjectMgr::SqttObjectMgr()
    :
    m_pDevice(nullptr),
    m_pObjects(nullptr),
    ObjectTypeBeginRange(Util::Min(
        static_cast<uint32_t>(VK_DEBUG_REPORT_OBJECT_TYPE_BEGIN_RANGE_EXT),
        static_cast<uint32_t>(VK_OBJECT_TYPE_BEGIN_RANGE))),
    ObjectTypeEndRange(Util::Max(
        static_cast<uint32_t>(VK_DEBUG_REPORT_OBJECT_TYPE_END_RANGE_EXT),
        static_cast<uint32_t>(VK_OBJECT_TYPE_END_RANGE))),
    ObjectTypeRangeSize(ObjectTypeEndRange - ObjectTypeBeginRange + 1)
{

}

// =====================================================================================================================
SqttObjectMgr::~SqttObjectMgr()
{
    if (m_pObjects != nullptr)
    {
        for (uint32_t idx = 0; idx < ObjectTypeRangeSize; ++idx)
        {
            if (m_pObjects[idx].enabled)
            {
                m_pObjects[idx].Destroy(m_pDevice);
            }
        }

        m_pDevice->VkInstance()->FreeMem(m_pObjects);
    }
}

// =====================================================================================================================
void SqttObjectMgr::Init(
    Device* pDevice)
{
    m_pDevice = pDevice;

}

// =====================================================================================================================
SqttMetaState::SqttMetaState()
    :
    debugNameCapacity(0),
    pDebugName(nullptr)
{

}

// =====================================================================================================================
void SqttMetaState::Destroy(
    Device* pDevice)
{
    if (pDebugName != nullptr)
    {
        pDevice->VkInstance()->FreeMem(pDebugName);
    }
}

// =====================================================================================================================
SqttMetaState::~SqttMetaState()
{

}

// =====================================================================================================================
SqttObjectMgr::ObjectTypeState::ObjectTypeState(
    Device* pDevice)
    :
    enabled(false),
    dataMap(32, pDevice->VkInstance()->Allocator())
{
}

// =====================================================================================================================
void SqttObjectMgr::ObjectTypeState::Init(
    Device* pDevice)
{
    if (dataMap.Init() == Pal::Result::Success)
    {
        enabled = true;
    }
}

// =====================================================================================================================
void SqttObjectMgr::ObjectTypeState::Destroy(
    Device* pDevice)
{
    if (enabled)
    {
        auto* pInstance = pDevice->VkInstance();

        Util::MutexAuto lock(&dataMutex);

        auto it = dataMap.Begin();

        while (it.Get() != nullptr)
        {
            if (it.Get()->value != nullptr)
            {
                PAL_DELETE(it.Get()->value, pInstance->Allocator());
            }

            it.Next();
        }
    }
}

}; // namespace vk
