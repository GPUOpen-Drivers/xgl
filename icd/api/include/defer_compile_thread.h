/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  defer_compile_thread.h
* @brief Implementation of class DeferCompileThread & DeferCompileManager
***********************************************************************************************************************
*/
#ifndef __DEFER_COMPILE_THREAD_H__
#define __DEFER_COMPILE_THREAD_H__

#pragma once

#include "include/vk_alloccb.h"
#include "palThread.h"
#include "palMutex.h"
#include "palList.h"
#include "palEvent.h"

namespace vk
{

class DeferCompileManager;
class PalAllocator;

struct DeferredCompileWorkload
{
    void*       pPayloads;
    void        (*Execute)(void*); // Function pointer to the call used to execute the workload
    Util::Event* pEvent;
};

// =====================================================================================================================
// Represents the general thread for async shader/pipeline compiler.
class DeferCompileThread final : public Util::Thread
{
public:
    DeferCompileThread(PalAllocator* pAllocator)
        :
        m_taskList(pAllocator),
        m_stop(false)
    {
        Util::EventCreateFlags flags = {};
        flags.manualReset = true;
        flags.initiallySignaled = false;
        m_event.Init(flags);
    }

    // Starts a new thread which starts by running function TaskThreadFunc.
    void Begin()
    {
        Util::Thread::Begin(ThreadFunc, this);
    }

    // Adds task to list.
    void AddTask(DeferredCompileWorkload* pTask)
    {
        Util::MutexAuto mutexAuto(&m_lock);
        m_taskList.PushBack(*pTask);
        m_event.Set();
    }

    // Set flag stop and trig event.
    void SetStop()
    {
        m_event.Set();
        m_stop = true;
    }

    // Returns until all tasks are executed.
    void SyncAll()
    {
        m_event.Set();
        while (m_taskList.Begin() != m_taskList.End())
        {
            Util::YieldThread();
        }
    }

protected:
    // Async thread function
    static void ThreadFunc(
        void* pParam)
    {
        auto pThis = reinterpret_cast<DeferCompileThread*>(pParam);
        pThis->TaskThreadFunc();
    }

    // The implementation of async thread function
    void TaskThreadFunc()
    {
        while (m_stop == false)
        {
            // Waits for new signal.
            m_event.Wait(1.0f);
            m_event.Reset();

            DeferredCompileWorkload task;
            while (FetchTask(&task))
            {
                task.Execute(task.pPayloads);
                if (task.pEvent != nullptr)
                {
                    task.pEvent->Set();
                }
            }
        }
    }

    // Fetches task in list, return false if task list is empty.
    bool FetchTask(DeferredCompileWorkload* pTask)
    {
        Util::MutexAuto mutexAuto(&m_lock);
        auto beginIt = m_taskList.Begin();
        if (beginIt != m_taskList.End())
        {
            *pTask = *(beginIt.Get());
            m_taskList.Erase(&beginIt);
            return true;
        }
        return false;
    }

    Util::List<DeferredCompileWorkload, vk::PalAllocator> m_taskList;      // Deferred compile task list
    volatile bool                                         m_stop;          // Flag to stop the thread
    Util::Mutex                                           m_lock;          // Lock for accessing task list
    Util::Event                                           m_event;         // Event to notify async thread
};

// =====================================================================================================================
// Class that manage DeferCompileThread instance.
class DeferCompileManager
{
public:
    DeferCompileManager()
        :
        m_pCompileThreads{},
        m_taskId(0),
        m_activeThreadCount(0)
    {
    }

    void Init(uint32_t threadCount, PalAllocator* pAllocator)
    {
        if (threadCount == 0)
        {
            m_activeThreadCount = 0;
        }
        else if (threadCount == UINT32_MAX)
        {
            Util::SystemInfo sysInfo = {};
            Util::QuerySystemInfo(&sysInfo);
            m_activeThreadCount = Util::Min(MaxThreads, sysInfo.cpuLogicalCoreCount / 2);
        }
        else
        {
            m_activeThreadCount = Util::Min(MaxThreads, threadCount);
        }

        for (uint32_t i = 0; i < m_activeThreadCount; ++i)
        {
            m_pCompileThreads[i] = VK_PLACEMENT_NEW(m_compileThreadBuffer[i])
                DeferCompileThread(pAllocator);
            m_pCompileThreads[i]->Begin();
        }
    }

    ~DeferCompileManager()
    {
        for (uint32_t i = 0; i < m_activeThreadCount; ++i)
        {
            m_pCompileThreads[i]->SetStop();
            m_pCompileThreads[i]->Join();
            Util::Destructor(m_pCompileThreads[i]);
            m_pCompileThreads[i] = nullptr;
        }
        m_activeThreadCount = 0;
    }

    void SyncAll()
    {
        for (uint32_t i = 0; i < m_activeThreadCount; ++i)
        {
            m_pCompileThreads[i]->SyncAll();
        }
    }

    DeferCompileThread* GetCompileThread()
    {
        return (m_activeThreadCount > 0) ?
            m_pCompileThreads[(m_taskId++) % m_activeThreadCount] :
            nullptr;
    }

protected:
    static constexpr uint32_t        MaxThreads = 8;  // Max thread count for shader module compile
    DeferCompileThread*              m_pCompileThreads[MaxThreads]; // Async compiler threads
    uint32_t                         m_taskId;                      // Hint to select compile thread
    uint32_t                         m_activeThreadCount;           // Active thread count

    // Internal buffer for m_pCompileThreads
    uint8_t                          m_compileThreadBuffer[MaxThreads][sizeof(DeferCompileThread)];
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DeferCompileManager);
};

} // namespace vk

#endif
