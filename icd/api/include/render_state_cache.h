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
 * @file  render_state_cache.h
 * @brief Manages previously seen static pipeline state values by mapping them to numbers or pointers for efficient
 *        redundancy checking during command buffer recording.
 ***********************************************************************************************************************
 */

#ifndef __RENDER_STATE_CACHE_H__
#define __RENDER_STATE_CACHE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"

#include "palHashMap.h"
#include "palColorBlendState.h"
#include "palDepthStencilState.h"
#include "palMsaaState.h"
#include "palCmdBuffer.h"

// Forward declare Vulkan classes used in this file
namespace vk
{
class Device;
};

namespace vk
{

// This is a magic number that is guaranteed to never be returned as an ID from the class below (see below class
// for details on what that means).  Command buffers can therefore use that to track on their own whether a particular
// piece of render state is static or not.
constexpr uint32_t DynamicRenderStateToken = 0;

// First valid parameter valid that can be assigned to static parameter state (i.e. those states mapped to a number
// as opposed to a pointer).
constexpr uint32_t FirstStaticRenderStateToken = DynamicRenderStateToken + 1;

// =====================================================================================================================
// The render state cache allows pipelines to register pieces of static pipeline state (or other such render state) and
// receive back a singular token (number or pointer, depending on state) that guarantees that, if those two tokens
// match, so do the state values.  It can be considered a perfect or pure hash of the particular state's values.
//
// These tokens can then be utilized during command buffer building to avoid reprogramming identical subsets of states
// during pipeline switches.
//
// Some of this state can be specified as dynamic state by certain pipelines, i.e. programmed via vkCmdSet* functions.
// Redundancy checking for such state is not tracked by this object -- command buffers are responsible for handling
// such conditions internally.
//
// This object is owned by the Vulkan Device.
class RenderStateCache
{
public:
    RenderStateCache(Device* pDevice);

    VkResult Init();

    Pal::Result CreateMsaaState(
        const Pal::MsaaStateCreateInfo& createInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSystemAllocationScope         parentScope,
        Pal::IMsaaState*                pMsaaStates[MaxPalDevices]);

    void DestroyMsaaState(
        Pal::IMsaaState**            ppMsaaStates,
        const VkAllocationCallbacks* pAllocator);

    Pal::Result CreateColorBlendState(
        const Pal::ColorBlendStateCreateInfo& createInfo,
        const VkAllocationCallbacks*          pAllocator,
        VkSystemAllocationScope               parentScope,
        Pal::IColorBlendState*                pStates[MaxPalDevices]);

    void DestroyColorBlendState(
        Pal::IColorBlendState**      ppStates,
        const VkAllocationCallbacks* pAllocator);

    Pal::Result CreateDepthStencilState(
        const Pal::DepthStencilStateCreateInfo& createInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkSystemAllocationScope                 parentScope,
        Pal::IDepthStencilState*                pStates[MaxPalDevices]);

    void DestroyDepthStencilState(
        Pal::IDepthStencilState**      ppStates,
        const VkAllocationCallbacks*   pAllocator);

    uint32_t CreateInputAssemblyState(const Pal::InputAssemblyStateParams& params);
    void DestroyInputAssemblyState(const Pal::InputAssemblyStateParams& params, uint32_t token);

    uint32_t CreateTriangleRasterState(const Pal::TriangleRasterStateParams& params);
    void DestroyTriangleRasterState(const Pal::TriangleRasterStateParams& params, uint32_t token);

    uint32_t CreatePointLineRasterState(const Pal::PointLineRasterStateParams& params);
    void DestroyPointLineRasterState(const Pal::PointLineRasterStateParams& params, uint32_t token);

    uint32_t CreateDepthBias(const Pal::DepthBiasParams& params);
    void DestroyDepthBias(const Pal::DepthBiasParams& params, uint32_t token);

    uint32_t CreateBlendConst(const Pal::BlendConstParams& params);
    void DestroyBlendConst(const Pal::BlendConstParams& params, uint32_t token);

    uint32_t CreateDepthBounds(const Pal::DepthBoundsParams& params);
    void DestroyDepthBounds(const Pal::DepthBoundsParams& params, uint32_t token);

    uint32_t CreateViewport(const Pal::ViewportParams& params);
    void DestroyViewport(const Pal::ViewportParams& params, uint32_t token);

    uint32_t CreateScissorRect(const Pal::ScissorRectParams& params);
    void DestroyScissorRect(const Pal::ScissorRectParams& params, uint32_t token);

    uint32_t CreateFragmentShadingRate(const Pal::VrsRateParams& params);
    void DestroyFragmentShadingRate(const Pal::VrsRateParams& params, uint32_t token);

    uint32_t CreateLineStipple(const Pal::LineStippleStateParams& params);
    void DestroyLineStipple(const Pal::LineStippleStateParams& params, uint32_t token);

    void Destroy();

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(RenderStateCache);

    static const uint32_t NumStateBuckets = 32;

    // State mapping for Pal::*Params -> uint32_t token mapping (for redundancy checking CmdSet* functions)
    struct StaticParamState
    {
        uint32_t paramToken;    // Token value the state maps to
        uint32_t refCount;      // Reference count of active pipelines holding to this state
    };

    // State mapping for a Pal::*CreateInfo -> Pal::I* bindable object (for redundancy checking CmdBind* functions)
    template<typename PalCreateInfo, typename PalStateObject>
    struct StaticStateObject
    {
        typedef PalCreateInfo  CreateInfo;   // PAL create info
        typedef PalStateObject PalObject;    // PAL bindable object type (mapping value)

        CreateInfo info;                     // Original create info (copy of the key)
        PalObject* pObjects[MaxPalDevices];  // Per-device object pointers (mapping value)
        uint32_t   refCount;                 // Reference count of pipelines holding on to this state
    };

    // Specializations for the three kinds of PAL objects we currently cache
    typedef StaticStateObject<Pal::MsaaStateCreateInfo, Pal::IMsaaState> StaticMsaaState;
    typedef StaticStateObject<Pal::ColorBlendStateCreateInfo, Pal::IColorBlendState> StaticColorBlendState;
    typedef StaticStateObject<Pal::DepthStencilStateCreateInfo, Pal::IDepthStencilState> StaticDepthStencilState;

    template<class StateObject, typename InfoMap, typename RefMap>
    Pal::Result CreateStaticPalObjectState(
        uint32_t                                 settingMask,
        const typename StateObject::CreateInfo&  createInfo,
        const VkAllocationCallbacks*             pAllocator,
        VkSystemAllocationScope                  parentScope,
        InfoMap*                                 pStateMap,
        RefMap*                                  pRefMap,
        typename StateObject::PalObject*         pStates[MaxPalDevices]);

    template<class StateObject, typename InfoMap, typename RefMap>
    void DestroyStaticPalObjectState(
        uint32_t                           settingsMask,
        typename StateObject::PalObject**  ppStates,
        const VkAllocationCallbacks*       pAllocator,
        InfoMap*                           pInfoMap,
        RefMap*                            pRefMap);

    template<typename StateObject, typename InfoMap, typename RefMap>
    void EraseFromMaps(
        StateObject* pState,
        InfoMap*     pInfoMap,
        RefMap*      pRefMap);

    template<typename ParamInfo, typename ParamHashMap>
    uint32_t CreateStaticParamsState(
        uint32_t         enabledType,
        const ParamInfo& params,
        ParamHashMap*    pMap,
        uint32_t*        pNextId);

    template<typename ParamInfo, typename ParamHashMap>
    void DestroyStaticParamsState(
        uint32_t         enabledType,
        const ParamInfo& params,
        uint32_t         token,
        ParamHashMap*    pMap);

    bool IsEnabled(uint32_t staticStateFlag) const;

    Pal::Result AllocMem(
        size_t                       size,
        const VkAllocationCallbacks* pAllocator,
        VkSystemAllocationScope      scope,
        void**                       ppResult);

    void FreeMem(
        void*                        pMem,
        const VkAllocationCallbacks* pAllocator);

    Pal::Result CreatePalObjects(
        const Pal::MsaaStateCreateInfo& createInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkSystemAllocationScope         parentScope,
        Pal::IMsaaState**               pStates);

    Pal::Result CreatePalObjects(
        const Pal::ColorBlendStateCreateInfo& createInfo,
        const VkAllocationCallbacks*          pAllocator,
        VkSystemAllocationScope               parentScope,
        Pal::IColorBlendState**               pStates);

    Pal::Result CreatePalObjects(
        const Pal::DepthStencilStateCreateInfo& createInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkSystemAllocationScope                 parentScope,
        Pal::IDepthStencilState**               pStates);

    void DestroyPalObjects(
        Pal::IMsaaState**            ppStates,
        const VkAllocationCallbacks* pAllocator);

    void DestroyPalObjects(
        Pal::IColorBlendState**      ppStates,
        const VkAllocationCallbacks* pAllocator);

    void DestroyPalObjects(
        Pal::IDepthStencilState**    ppStates,
        const VkAllocationCallbacks* pAllocator);

    Device* const                                 m_pDevice;
    Util::Mutex                                   m_mutex;

    // These hash tables map static graphics pipeline state to a unique token i.e. a perfect hash.
    Util::HashMap<Pal::InputAssemblyStateParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc>          m_inputAssemblyState;
    uint32_t                                      m_inputAssemblyStateNextId;

    Util::HashMap<Pal::TriangleRasterStateParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc>          m_triangleRasterState;
    uint32_t                                      m_triangleRasterStateNextId;

    Util::HashMap<Pal::PointLineRasterStateParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc>          m_pointLineRasterState;
    uint32_t                                      m_pointLineRasterStateNextId;

    Util::HashMap<Pal::LineStippleStateParams,
                  StaticParamState,
                  PalAllocator>                   m_lineStippleState;
    uint32_t                                      m_lineStippleStateNextId;

    Util::HashMap<Pal::DepthBiasParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc>          m_depthBias;
    uint32_t                                      m_depthBiasNextId;

    Util::HashMap<Pal::BlendConstParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc>          m_blendConst;
    uint32_t                                      m_blendConstNextId;

    Util::HashMap<Pal::DepthBoundsParams,
                  StaticParamState,
                  PalAllocator>                   m_depthBounds;
    uint32_t                                      m_depthBoundsNextId;

    static const size_t ViewportHashGroupSize = (sizeof(Pal::ViewportParams) + sizeof(StaticParamState)) * 8;

    Util::HashMap<Pal::ViewportParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc,
                  Util::DefaultEqualFunc,
                  Util::HashAllocator<PalAllocator>,
                  ViewportHashGroupSize>          m_viewport;
    uint32_t                                      m_viewportNextId;

    static const size_t ScissorRectHashGroupSize = (sizeof(Pal::ScissorRectParams) + sizeof(StaticParamState)) * 8;

    Util::HashMap<Pal::ScissorRectParams,
                  StaticParamState,
                  PalAllocator,
                  Util::JenkinsHashFunc,
                  Util::DefaultEqualFunc,
                  Util::HashAllocator<PalAllocator>,
                  ScissorRectHashGroupSize>       m_scissorRect;
    uint32_t                                      m_scissorRectNextId;

    // These hash tables do the same for certain PAL state objects that are owned by graphics pipelines.  Because
    // they are objects, the pointer address acts as an implicit unique ID.
    Util::HashMap<Pal::MsaaStateCreateInfo,
                  StaticMsaaState*,
                  PalAllocator,
                  Util::JenkinsHashFunc,
                  Util::DefaultEqualFunc,
                  Util::HashAllocator<PalAllocator>,
                  1024>                              m_msaaStates;

    Util::HashMap<Pal::IMsaaState*,
                  StaticMsaaState*,
                  PalAllocator>                      m_msaaRefs;

    Util::HashMap<Pal::ColorBlendStateCreateInfo,
        StaticColorBlendState*,
        PalAllocator,
        Util::JenkinsHashFunc,
        Util::DefaultEqualFunc,
        Util::HashAllocator<PalAllocator>,
        1024>                                         m_colorBlendStates;

    Util::HashMap<Pal::IColorBlendState*,
        StaticColorBlendState*,
        PalAllocator>                                 m_colorBlendRefs;

    Util::HashMap<Pal::DepthStencilStateCreateInfo,
        StaticDepthStencilState*,
        PalAllocator,
        Util::JenkinsHashFunc,
        Util::DefaultEqualFunc,
        Util::HashAllocator<PalAllocator>,
        1024>                                         m_depthStencilStates;

    Util::HashMap<Pal::IDepthStencilState*,
        StaticDepthStencilState*,
        PalAllocator>                                 m_depthStencilRefs;

    Util::HashMap<Pal::VrsRateParams,
        StaticParamState,
        PalAllocator,
        Util::JenkinsHashFunc,
        Util::DefaultEqualFunc,
        Util::HashAllocator<PalAllocator>,
        1024>                                         m_fragmentShadingRate;
    uint32_t                                          m_fragmentShadingRateNextId;
};

};

#endif /* __RENDER_STATE_CACHE_H__ */
