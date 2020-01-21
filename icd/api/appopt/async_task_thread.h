/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  async_task_thread.h
* @brief Implementation of template class async::TaskThread
***********************************************************************************************************************
*/
#ifndef __ASYNC_TASK_THREAD_H__
#define __ASYNC_TASK_THREAD_H__

#pragma once

#include "include/vk_alloccb.h"
#include "palThread.h"
#include "palMutex.h"
#include "palList.h"
#include "palEvent.h"

namespace vk
{

class AsyncLayer;
struct PalAllocator;

namespace async
{

// =====================================================================================================================
// Represents the general thread for async shader/pipeline compiler.
template<class Task>
class TaskThread : public Util::Thread
{
public:
    TaskThread(AsyncLayer* pAsyncLayer, PalAllocator* pAllocator)
        :
        m_pAsyncLayer(pAsyncLayer),
        m_taskList(pAllocator),
        m_stop(false)
    {
        m_lock.Init();
        Util::EventCreateFlags flags = {};
        flags.manualReset = false;
        flags.initiallySignaled = false;
        m_event.Init(flags);
    }

    // Starts a new thread which starts by running function TaskThreadFunc.
    VK_INLINE void Begin()
    {
        Util::Thread::Begin(ThreadFunc, this);
    }

    // Adds task to list.
    void AddTask(Task* pTask)
    {
        Util::MutexAuto mutexAuto(&m_lock);
        m_taskList.PushBack(*pTask);
        m_event.Set();
    }

    // Set flag stop and trig event.
    VK_INLINE void SetStop()
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
        auto pThis = reinterpret_cast<TaskThread<Task>*>(pParam);
        pThis->TaskThreadFunc();
    }

    // The implementation of async thread function
    void TaskThreadFunc()
    {
        while (m_stop == false)
        {
            // Waits for new signal.
            m_event.Wait(1.0f);

            Task task;
            while (FetchTask(&task))
            {
                task.pObj->Execute(m_pAsyncLayer, &task);
            }
        }
    }

    // Fetches task in list, return false if task list is empty.
    bool FetchTask(Task* pTask)
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

    AsyncLayer* m_pAsyncLayer;                      // Async compiler layer object
    Util::List<Task, vk::PalAllocator> m_taskList;  // Async compile task list
    volatile bool                  m_stop;          // Flag to stop the thread
    Util::Mutex                    m_lock;          // Lock for accessing task list
    Util::Event                    m_event;         // Event to notify async thread
};

} // namespace async

} // namespace vk

#endif // __ASYNC_TASK_THREAD_H__
