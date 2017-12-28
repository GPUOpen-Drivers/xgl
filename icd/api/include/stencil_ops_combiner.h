/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 **************************************************************************************************
 * @file  stencil ops combiner.h
 * @brief helper class to handle stencil ops, required due to the shared HW registers
 **************************************************************************************************
 */

#ifndef __STENCIL_OPS_COMBINER_H__
#define __STENCIL_OPS_COMBINER_H__

#pragma once

#include "palCmdBuffer.h"

namespace vk
{
class CmdBuffer;

// =====================================================================================================================
struct StencilRefMaskParams
{
    enum Fields
    {
        FrontRef       = 0,
        FrontReadMask,
        FrontWriteMask,
        FrontOpValue,
        BackRef,
        BackReadMask,
        BackWriteMask,
        BackOpValue,
        Num
    };

    StencilRefMaskParams();

    uint8_t* GetArray()
    {
        return static_cast<uint8_t*>(&m_palState.frontRef);
    }
    uint64_t& Get64bitRef()
    {
        return *reinterpret_cast<uint64_t*>(&m_palState.frontRef);
    }

    Pal::StencilRefMaskParams   m_palState;
};

class StencilOpsCombiner
{
public:
    StencilOpsCombiner()
    {
        Reset();
    }

    void Reset()
    {
        // Just invalidate m_previous_state only otherwise we would need to
        // setup the default front\back OpValues == 1

        m_previous_state.Get64bitRef() = 0;
    }

    void Set(StencilRefMaskParams::Fields field, uint8_t value)
    {
        m_state.GetArray()[field] = value;
    }

    void PalCmdSetStencilState(CmdBuffer* pCmdBuffer);

private:
    StencilRefMaskParams   m_state;
    StencilRefMaskParams   m_previous_state;
    uint32_t               m_palDeviceMask;
};

} // namespace vk

#endif /* __STENCIL_OPS_COMBINER_H__ */
