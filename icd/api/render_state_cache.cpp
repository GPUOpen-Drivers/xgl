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
 * @file  render_state_cache.cpp
 * @brief Contains the implementation of the static render state cache.
 ***********************************************************************************************************************
 */

#include "include/khronos/vulkan.h"

#include "include/vk_device.h"
#include "include/render_state_cache.h"

#include "palHashMapImpl.h"

#include <climits>

namespace vk
{

// =====================================================================================================================
RenderStateCache::RenderStateCache(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_inputAssemblyState(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_inputAssemblyStateNextId(FirstStaticRenderStateToken),
    m_triangleRasterState(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_triangleRasterStateNextId(FirstStaticRenderStateToken),
    m_pointLineRasterState(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_pointLineRasterStateNextId(FirstStaticRenderStateToken),
    m_lineStippleState(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_lineStippleStateNextId(FirstStaticRenderStateToken),
    m_depthBias(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_depthBiasNextId(FirstStaticRenderStateToken),
    m_blendConst(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_blendConstNextId(FirstStaticRenderStateToken),
    m_depthBounds(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_depthBoundsNextId(FirstStaticRenderStateToken),
    m_viewport(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_viewportNextId(FirstStaticRenderStateToken),
    m_scissorRect(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_scissorRectNextId(FirstStaticRenderStateToken),
    m_msaaStates(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_msaaRefs(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_colorBlendStates(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_colorBlendRefs(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_depthStencilStates(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_depthStencilRefs(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_fragmentShadingRate(NumStateBuckets, pDevice->VkInstance()->Allocator()),
    m_fragmentShadingRateNextId(FirstStaticRenderStateToken)
{

}

// =====================================================================================================================
// Initializes the render state cache.  Should be called during device create.
VkResult RenderStateCache::Init()
{
    Pal::Result result = m_inputAssemblyState.Init();

    if (result == Pal::Result::Success)
    {
        result = m_triangleRasterState.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_pointLineRasterState.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_lineStippleState.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_depthBias.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_blendConst.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_depthBounds.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_viewport.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_scissorRect.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_msaaStates.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_msaaRefs.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_colorBlendStates.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_colorBlendRefs.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_depthStencilStates.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_depthStencilRefs.Init();
    }

    if (result == Pal::Result::Success)
    {
        result = m_fragmentShadingRate.Init();
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Erases the given state object from the two hash maps that track a particular mapping.
template<typename StateObject, typename InfoMap, typename RefMap> void
RenderStateCache::EraseFromMaps(
    StateObject* pState,
    InfoMap*     pInfoMap,
    RefMap*      pRefMap)
{
    if (pState->pObjects[0] != nullptr)
    {
        pRefMap->Erase(pState->pObjects[0]);
    }

    pInfoMap->Erase(pState->info);
}

// =====================================================================================================================
// Destroys the render state cache.  Should be called during device destroy.
// Not necessary to take the mutex in this function because, an application should ensure that no work is active on
// the device, and an application is responsible for destroying / freeing any Vulkan objects that were created using
// that device.
void RenderStateCache::Destroy()
{
    for (auto it = m_msaaRefs.Begin(); it.Get() != nullptr; it.Next())
    {
        DestroyPalObjects(it.Get()->value->pObjects, nullptr);
    }

    for (auto it = m_colorBlendRefs.Begin(); it.Get() != nullptr; it.Next())
    {
        DestroyPalObjects(it.Get()->value->pObjects, nullptr);
    }

    for (auto it = m_depthStencilRefs.Begin(); it.Get() != nullptr; it.Next())
    {
        DestroyPalObjects(it.Get()->value->pObjects, nullptr);
    }
}

// =====================================================================================================================
// Generic internal memory allocator.  Optionally uses the provided allocator callback (this is necessary for state
// that is not, for whatever reason e.g. panel setting, not being cached at the device-level).
Pal::Result RenderStateCache::AllocMem(
    size_t                        size,
    const VkAllocationCallbacks*  pAllocator,
    VkSystemAllocationScope       scope,
    void**                        ppResult)
{
    Pal::Result result = Pal::Result::Success;

    if (pAllocator != nullptr)
    {
        *ppResult = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            size,
            VK_DEFAULT_MEM_ALIGN,
            scope);
    }
    else
    {
        *ppResult = m_pDevice->VkInstance()->AllocMem(size, scope);
    }

    if (*ppResult == nullptr && size > 0)
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Frees memory, optionally via the provided allocator.
void RenderStateCache::FreeMem(
    void*                        pMem,
    const VkAllocationCallbacks* pAllocator)
{
    if (pAllocator != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pMem);
    }
    else
    {
        m_pDevice->VkInstance()->FreeMem(pMem);
    }
}

// =====================================================================================================================
// An overloaded function to create PAL MSAA state objects for each active device in the group.
Pal::Result RenderStateCache::CreatePalObjects(
    const Pal::MsaaStateCreateInfo& createInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkSystemAllocationScope         parentScope,
    Pal::IMsaaState**               pStates)
{
    Pal::Result result = Pal::Result::Success;

    size_t stateSizes[MaxPalDevices] = {};
    size_t totalSize = 0;
    Pal::IMsaaState* states[MaxPalDevices] = {};

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
         ++deviceIdx)
    {
        stateSizes[deviceIdx] = m_pDevice->PalDevice(deviceIdx)->GetMsaaStateSize(createInfo, &result);
        totalSize += stateSizes[deviceIdx];
    }

    void* pStorage = nullptr;

    if (result == Pal::Result::Success)
    {
        result = AllocMem(totalSize, pAllocator, parentScope, &pStorage);
    }

    void* pMemory = pStorage;

    for (uint32_t di = 0; (di < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success); ++di)
    {
        result = m_pDevice->PalDevice(di)->CreateMsaaState(createInfo, pMemory, &states[di]);

        pMemory = Util::VoidPtrInc(pMemory, stateSizes[di]);
    }

    VK_ASSERT((result != Pal::Result::Success) || (states[0] == pStorage));

    if (result == Pal::Result::Success)
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            pStates[di] = states[di];
        }
    }
    else
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            if (states[di] != nullptr)
            {
                states[di]->Destroy();
            }
        }

        if (pStorage != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(pStorage);
        }
    }

    return result;
}

// =====================================================================================================================
// An overloaded function to destroy PAL MSAA state objects for each active device in the group.
void RenderStateCache::DestroyPalObjects(
    Pal::IMsaaState**            ppStates,
    const VkAllocationCallbacks* pAllocator)
{
    if (ppStates[0] != nullptr)
    {
        void* pStorage = ppStates[0];

        for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); ++i)
        {
            ppStates[i]->Destroy();
        }

        FreeMem(pStorage, pAllocator);
    }
}

// =====================================================================================================================
// An overloaded function to create PAL color blend state objects for each active device in the group.
Pal::Result RenderStateCache::CreatePalObjects(
    const Pal::ColorBlendStateCreateInfo& createInfo,
    const VkAllocationCallbacks*          pAllocator,
    VkSystemAllocationScope               parentScope,
    Pal::IColorBlendState**               pStates)
{
    Pal::Result result = Pal::Result::Success;

    size_t stateSizes[MaxPalDevices] = {};
    size_t totalSize = 0;
    Pal::IColorBlendState* states[MaxPalDevices] = {};

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
        ++deviceIdx)
    {
        stateSizes[deviceIdx] = m_pDevice->PalDevice(deviceIdx)->GetColorBlendStateSize(createInfo, &result);
        totalSize += stateSizes[deviceIdx];
    }

    void* pStorage = nullptr;

    if (result == Pal::Result::Success)
    {
        result = AllocMem(totalSize, pAllocator, parentScope, &pStorage);
    }

    void* pMemory = pStorage;

    for (uint32_t di = 0; (di < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success); ++di)
    {
        result = m_pDevice->PalDevice(di)->CreateColorBlendState(createInfo, pMemory, &states[di]);

        pMemory = Util::VoidPtrInc(pMemory, stateSizes[di]);
    }

    VK_ASSERT((result != Pal::Result::Success) || (states[0] == pStorage));

    if (result == Pal::Result::Success)
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            pStates[di] = states[di];
        }
    }
    else
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            if (states[di] != nullptr)
            {
                states[di]->Destroy();
            }
        }

        if (pStorage != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(pStorage);
        }
    }

    return result;
}

// =====================================================================================================================
// An overloaded function to destroy PAL color blend state objects for each active device in the group.
void RenderStateCache::DestroyPalObjects(
    Pal::IColorBlendState**      ppStates,
    const VkAllocationCallbacks* pAllocator)
{
    if (ppStates[0] != nullptr)
    {
        void* pStorage = ppStates[0];

        for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); ++i)
        {
            ppStates[i]->Destroy();
        }

        FreeMem(pStorage, pAllocator);
    }
}

// =====================================================================================================================
// An overloaded function to create PAL depth stencil state objects for each active device in the group.
Pal::Result RenderStateCache::CreatePalObjects(
    const Pal::DepthStencilStateCreateInfo& createInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkSystemAllocationScope                 parentScope,
    Pal::IDepthStencilState**               pStates)
{
    Pal::Result result = Pal::Result::Success;

    size_t stateSizes[MaxPalDevices] = {};
    size_t totalSize = 0;
    Pal::IDepthStencilState* states[MaxPalDevices] = {};

    for (uint32_t deviceIdx = 0;
        (deviceIdx < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success);
        ++deviceIdx)
    {
        stateSizes[deviceIdx] = m_pDevice->PalDevice(deviceIdx)->GetDepthStencilStateSize(createInfo, &result);
        totalSize += stateSizes[deviceIdx];
    }

    void* pStorage = nullptr;

    if (result == Pal::Result::Success)
    {
        result = AllocMem(totalSize, pAllocator, parentScope, &pStorage);
    }

    void* pMemory = pStorage;

    for (uint32_t di = 0; (di < m_pDevice->NumPalDevices()) && (result == Pal::Result::Success); ++di)
    {
        result = m_pDevice->PalDevice(di)->CreateDepthStencilState(createInfo, pMemory, &states[di]);

        pMemory = Util::VoidPtrInc(pMemory, stateSizes[di]);
    }

    VK_ASSERT((result != Pal::Result::Success) || (states[0] == pStorage));

    if (result == Pal::Result::Success)
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            pStates[di] = states[di];
        }
    }
    else
    {
        for (uint32_t di = 0; di < m_pDevice->NumPalDevices(); ++di)
        {
            if (states[di] != nullptr)
            {
                states[di]->Destroy();
            }
        }

        if (pStorage != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(pStorage);
        }
    }

    return result;
}

// =====================================================================================================================
// An overloaded function to destroy PAL depth stencil state objects for each active device in the group.
void RenderStateCache::DestroyPalObjects(
    Pal::IDepthStencilState**    ppStates,
    const VkAllocationCallbacks* pAllocator)
{
    if (ppStates[0] != nullptr)
    {
        void* pStorage = ppStates[0];

        for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); ++i)
        {
            ppStates[i]->Destroy();
        }

        FreeMem(pStorage, pAllocator);
    }
}

// =====================================================================================================================
// A template function to create a mapping from some PAL create info (StateObject::CreateInfo) to a bindable PAL
// render state object (StateObject::PalObject).
//
// Objects will be cached by this function if it is enabled by the setting.  Otherwise, the objects are created without
// caching.
template<class StateObject, typename InfoMap, typename RefMap>
Pal::Result RenderStateCache::CreateStaticPalObjectState(
    uint32_t                                settingMask,
    const typename StateObject::CreateInfo& createInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkSystemAllocationScope                 parentScope,
    InfoMap*                                pStateMap,
    RefMap*                                 pRefMap,
    typename StateObject::PalObject*        pStates[MaxPalDevices])
{
    if (IsEnabled(settingMask) == false)
    {
        return CreatePalObjects(createInfo, pAllocator, parentScope, pStates);
    }

    // Try to find an existing static state object
    Pal::Result result = Pal::Result::Success;
    bool existed = false;
    StateObject** ppState = nullptr;

    Util::MutexAuto lock(&m_mutex);

    // Map the createinfo to a pre-existing state object.  Allocate a new (empty) entry if one does not exist.
    result = pStateMap->FindAllocate(createInfo, &existed, &ppState);

    if (result == Pal::Result::Success)
    {
        VK_ASSERT(ppState != nullptr);

        // If we allocated a new entry for this mapping, we need to initialize it by creating the mapped PAL
        // objects.
        if (existed == false)
        {
            // Allocate a new state object
            StateObject* pNewState = nullptr;

            result = AllocMem(
                sizeof(StateObject), nullptr, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, (void**)&pNewState);

            if (result == Pal::Result::Success)
            {
                // Initialize the state object
                memset(pNewState, 0, sizeof(*pNewState));

                pNewState->info = createInfo;

                // Create PAL objects for it
                result = CreatePalObjects(createInfo, nullptr, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, pNewState->pObjects);
            }

            // Insert it into the relevant maps
            if (result == Pal::Result::Success)
            {
                *ppState = pNewState;

                result = pRefMap->Insert(pNewState->pObjects[0], pNewState);
            }

            // On failure, remove any partial entries from all the maps
            if ((result != Pal::Result::Success) && (pNewState != nullptr))
            {
                EraseFromMaps(pNewState, pStateMap, pRefMap);
                DestroyPalObjects(pNewState->pObjects, nullptr);
                FreeMem(pNewState, nullptr);
            }
        }
        else
        {
            VK_ASSERT((*ppState)->refCount > 0);
        }

        // Increment reference count and output PAL object handles
        if (result == Pal::Result::Success)
        {
            auto* pState = *ppState;

            pState->refCount++;

            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
            {
                VK_ASSERT(pState->pObjects[deviceIdx] != nullptr);

                pStates[deviceIdx] = pState->pObjects[deviceIdx];
            }
        }
    }

    return result;
}

// =====================================================================================================================
// A template function to "destroy" potentially cached render state objects.  This will decrement the cached object's
// reference count and destroy them when it reaches zero.
//
// If caching is disabled for the given object, the object is destroyed immediately.
template<class StateObject, typename InfoMap, typename RefMap>
void RenderStateCache::DestroyStaticPalObjectState(
    uint32_t                          settingsMask,
    typename StateObject::PalObject** ppStates,
    const VkAllocationCallbacks*      pAllocator,
    InfoMap*                          pInfoMap,
    RefMap*                           pRefMap)
{
    if ((ppStates == nullptr) || (ppStates[0] == nullptr))
    {
        return;
    }

    if (IsEnabled(settingsMask) == false)
    {
        DestroyPalObjects(ppStates, pAllocator);
    }
    else
    {
        Util::MutexAuto lock(&m_mutex);

        // Find the state object containing the given PAL object.  This should always exist.
        auto** pValue = pRefMap->FindKey(ppStates[0]);

        if (pValue != nullptr)
        {
            StateObject* pState = *pValue;

            VK_ASSERT(pState->refCount > 0);

            // Decrement the reference count and destroy if it hits zero.
            pState->refCount--;

            if (pState->refCount == 0)
            {
                EraseFromMaps(pState, pInfoMap, pRefMap);
                DestroyPalObjects(pState->pObjects, nullptr);
                FreeMem(pState, nullptr);
            }
        }
        else
        {
            VK_NEVER_CALLED();
        }
    }
}

// =====================================================================================================================
// Create a cached version of a PAL MSAA state object.  The function will return an array of state objects, one
// per active device.
Pal::Result RenderStateCache::CreateMsaaState(
    const Pal::MsaaStateCreateInfo& createInfo,
    const VkAllocationCallbacks*    pAllocator,
    VkSystemAllocationScope         parentScope,
    Pal::IMsaaState*                pStates[MaxPalDevices])
{
    return CreateStaticPalObjectState<StaticMsaaState>(
        OptRenderStateCacheMsaaState,
        createInfo,
        pAllocator,
        parentScope,
        &m_msaaStates,
        &m_msaaRefs,
        pStates);
}

// =====================================================================================================================
// Destroy a cached version of a PAL MSAA state object.  The input to this function should be the original array of
// returned state objects.  Reference counting for the objects is handled internally.
void RenderStateCache::DestroyMsaaState(
    Pal::IMsaaState**            ppStates,
    const VkAllocationCallbacks* pAllocator)
{
    return DestroyStaticPalObjectState<StaticMsaaState>(
        OptRenderStateCacheMsaaState,
        ppStates,
        pAllocator,
        &m_msaaStates,
        &m_msaaRefs);
}

// =====================================================================================================================
// Create a cached version of a PAL color blend state object.  The function will return an array of state objects, one
// per active device.
Pal::Result RenderStateCache::CreateColorBlendState(
    const Pal::ColorBlendStateCreateInfo& createInfo,
    const VkAllocationCallbacks*          pAllocator,
    VkSystemAllocationScope               parentScope,
    Pal::IColorBlendState*                pStates[MaxPalDevices])
{
    return CreateStaticPalObjectState<StaticColorBlendState>(
        OptRenderStateCacheColorBlendState,
        createInfo,
        pAllocator,
        parentScope,
        &m_colorBlendStates,
        &m_colorBlendRefs,
        pStates);
}

// =====================================================================================================================
// Destroy a cached version of a PAL color blend state object.  The input to this function should be the original array
// of returned state objects.  Reference counting for the objects is handled internally.
void RenderStateCache::DestroyColorBlendState(
    Pal::IColorBlendState**      ppStates,
    const VkAllocationCallbacks* pAllocator)
{
    return DestroyStaticPalObjectState<StaticColorBlendState>(
        OptRenderStateCacheColorBlendState,
        ppStates,
        pAllocator,
        &m_colorBlendStates,
        &m_colorBlendRefs);
}

// =====================================================================================================================
// Create a cached version of a PAL depth stencil state object.  The function will return an array of state objects, one
// per active device.
Pal::Result RenderStateCache::CreateDepthStencilState(
    const Pal::DepthStencilStateCreateInfo& createInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkSystemAllocationScope                 parentScope,
    Pal::IDepthStencilState*                pStates[MaxPalDevices])
{
    return CreateStaticPalObjectState<StaticDepthStencilState>(
        OptRenderStateCacheDepthStencilState,
        createInfo,
        pAllocator,
        parentScope,
        &m_depthStencilStates,
        &m_depthStencilRefs,
        pStates);
}

// =====================================================================================================================
// Destroy a cached version of a PAL depth stencil state object.  The input to this function should be the original
// array of returned state objects.  Reference counting for the objects is handled internally.
void RenderStateCache::DestroyDepthStencilState(
    Pal::IDepthStencilState**      ppStates,
    const VkAllocationCallbacks*   pAllocator)
{
    return DestroyStaticPalObjectState<StaticDepthStencilState>(
        OptRenderStateCacheDepthStencilState,
        ppStates,
        pAllocator,
        &m_depthStencilStates,
        &m_depthStencilRefs);
}

// =====================================================================================================================
// Returns true if the given
bool RenderStateCache::IsEnabled(
    uint32_t staticStateFlag
    ) const
{
    return (staticStateFlag & m_pDevice->GetRuntimeSettings().optRenderStateCacheEnable) != 0;
}

// =====================================================================================================================
// Template function for creating a cached mapping of a struct of parameters (for PAL CmdSet*) to a uint32_t token.
template<typename ParamInfo, typename ParamHashMap>
uint32_t RenderStateCache::CreateStaticParamsState(
    uint32_t         enabledType,
    const ParamInfo& params,
    ParamHashMap*    pMap,
    uint32_t*        pNextId)
{
    uint32_t token = DynamicRenderStateToken;

    if (IsEnabled(enabledType))
    {
        Util::MutexAuto lock(&m_mutex);

        bool existed = false;
        StaticParamState* pState = nullptr;
        Pal::Result result = pMap->FindAllocate(params, &existed, &pState);

        if (result == Pal::Result::Success)
        {
            if (existed == false)
            {
                pState->refCount   = 0;
                pState->paramToken = DynamicRenderStateToken;

                if (*pNextId < UINT_MAX)
                {
                    pState->paramToken = *pNextId;

                    *pNextId = pState->paramToken + 1;
                }
                else
                {
                    result = Pal::Result::ErrorOutOfMemory;
                }
            }
            else if (pState->refCount == UINT_MAX)
            {
                result = Pal::Result::ErrorOutOfMemory;
            }
        }

        if (result == Pal::Result::Success)
        {
            pState->refCount++;
            token = pState->paramToken;
        }
    }

    return token;
}

// =====================================================================================================================
// Template function to destroy a mapping of a PAL CmdSet* struct of parameters -> uint32_t token.
template<typename ParamInfo, typename ParamHashMap>
void RenderStateCache::DestroyStaticParamsState(
    uint32_t         enabledType,
    const ParamInfo& params,
    uint32_t         token,
    ParamHashMap*    pMap)
{
    if (IsEnabled(enabledType) && (token != DynamicRenderStateToken))
    {
        Util::MutexAuto lock(&m_mutex);

        StaticParamState* pValue = pMap->FindKey(params);

        if (pValue != nullptr)
        {
            VK_ASSERT(pValue->refCount > 0);

            pValue->refCount--;

            if (pValue->refCount == 0)
            {
                pMap->Erase(params);
            }
        }
    }
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateInputAssemblyState(
    const Pal::InputAssemblyStateParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheInputAssemblyState,
        params,
        &m_inputAssemblyState,
        &m_inputAssemblyStateNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyInputAssemblyState(
    const Pal::InputAssemblyStateParams& params,
    uint32_t                             token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheInputAssemblyState,
        params,
        token,
        &m_inputAssemblyState);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateTriangleRasterState(
    const Pal::TriangleRasterStateParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheTriangleRasterState,
        params,
        &m_triangleRasterState,
        &m_triangleRasterStateNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyTriangleRasterState(
    const Pal::TriangleRasterStateParams& params,
    uint32_t                              token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheTriangleRasterState,
        params,
        token,
        &m_triangleRasterState);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreatePointLineRasterState(
    const Pal::PointLineRasterStateParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticPointLineRasterState,
        params,
        &m_pointLineRasterState,
        &m_pointLineRasterStateNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyPointLineRasterState(
    const Pal::PointLineRasterStateParams& params,
    uint32_t                               token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticPointLineRasterState,
        params,
        token,
        &m_pointLineRasterState);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateDepthBias(
    const Pal::DepthBiasParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticDepthBias,
        params,
        &m_depthBias,
        &m_depthBiasNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyDepthBias(
    const Pal::DepthBiasParams& params,
    uint32_t                    token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticDepthBias,
        params,
        token,
        &m_depthBias);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateBlendConst(
    const Pal::BlendConstParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticBlendConst,
        params,
        &m_blendConst,
        &m_blendConstNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyBlendConst(
    const Pal::BlendConstParams& params,
    uint32_t                     token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticBlendConst,
        params,
        token,
        &m_blendConst);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateDepthBounds(
    const Pal::DepthBoundsParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticDepthBounds,
        params,
        &m_depthBounds,
        &m_depthBoundsNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyDepthBounds(
    const Pal::DepthBoundsParams& params,
    uint32_t                      token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticDepthBounds,
        params,
        token,
        &m_depthBounds);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateViewport(
    const Pal::ViewportParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticViewport,
        params,
        &m_viewport,
        &m_viewportNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyViewport(
    const Pal::ViewportParams& params,
    uint32_t                   token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticViewport,
        params,
        token,
        &m_viewport);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateScissorRect(
    const Pal::ScissorRectParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticScissorRect,
        params,
        &m_scissorRect,
        &m_scissorRectNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyScissorRect(
    const Pal::ScissorRectParams& params,
    uint32_t                      token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticScissorRect,
        params,
        token,
        &m_scissorRect);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateLineStipple(
    const Pal::LineStippleStateParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateCacheStaticLineStipple,
        params,
        &m_lineStippleState,
        &m_lineStippleStateNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyLineStipple(
    const Pal::LineStippleStateParams& params,
    uint32_t                           token)
{
    return DestroyStaticParamsState(
        OptRenderStateCacheStaticLineStipple,
        params,
        token,
        &m_lineStippleState);
}

// =====================================================================================================================
uint32_t RenderStateCache::CreateFragmentShadingRate(
    const Pal::VrsRateParams& params)
{
    return CreateStaticParamsState(
        OptRenderStateFragmentShadingRate,
        params,
        &m_fragmentShadingRate,
        &m_fragmentShadingRateNextId);
}

// =====================================================================================================================
void RenderStateCache::DestroyFragmentShadingRate(
    const Pal::VrsRateParams&                    params,
    uint32_t                                     token)
{
    return DestroyStaticParamsState(
        OptRenderStateFragmentShadingRate,
        params,
        token,
        &m_fragmentShadingRate);
}

};
