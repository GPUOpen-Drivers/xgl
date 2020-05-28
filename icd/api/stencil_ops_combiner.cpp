/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/stencil_ops_combiner.h"
#include "include/vk_cmdbuffer.h"
#include "palCmdBuffer.h"

namespace vk
{

// =====================================================================================================================
StencilRefMaskParams::StencilRefMaskParams()
{
    constexpr uint8_t allBits = 0xff;   // Inform Pal we are setting all bits of the stencil. This enables the
                                        // more efficient PM4 packets. ie Not use Read-Modify-Write on the HW registers
    m_palState.flags.u8All = allBits;

    // Validate our data layout assumptions
    VK_ASSERT((&m_palState.frontRef      - &m_palState.frontRef) == static_cast<ptrdiff_t>(FrontRef      ) &&
             (&m_palState.frontReadMask  - &m_palState.frontRef) == static_cast<ptrdiff_t>(FrontReadMask ) &&
             (&m_palState.frontWriteMask - &m_palState.frontRef) == static_cast<ptrdiff_t>(FrontWriteMask) &&
             (&m_palState.frontOpValue   - &m_palState.frontRef) == static_cast<ptrdiff_t>(FrontOpValue  ) &&
             (&m_palState.backRef        - &m_palState.frontRef) == static_cast<ptrdiff_t>(BackRef       ) &&
             (&m_palState.backReadMask   - &m_palState.frontRef) == static_cast<ptrdiff_t>(BackReadMask  ) &&
             (&m_palState.backWriteMask  - &m_palState.frontRef) == static_cast<ptrdiff_t>(BackWriteMask ) &&
             (&m_palState.backOpValue    - &m_palState.frontRef) == static_cast<ptrdiff_t>(BackOpValue   ) );
}

// =====================================================================================================================
void StencilOpsCombiner::PalCmdSetStencilState(CmdBuffer* pCmdBuffer)
{
    const uint32_t palDeviceMask = pCmdBuffer->GetDeviceMask();

    if ((m_previous_state.m_palState64bit != m_state.m_palState64bit) ||
        (m_palDeviceMask                  != palDeviceMask))
    {
        utils::IterateMask deviceGroup(palDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdSetStencilRefMasks(m_state.m_palState);
        }
        while (deviceGroup.IterateNext());

        m_previous_state.m_palState64bit = m_state.m_palState64bit;
        m_palDeviceMask                  = palDeviceMask;
    }
}

} // namespace vk
