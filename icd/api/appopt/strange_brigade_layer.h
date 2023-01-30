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
* @file  strange_brigade_layer.h
* @brief Contains shadowed entry points related to strange brigade.
***********************************************************************************************************************
*/

#ifndef __STRANGE_BRIGADE_LAYER_H__
#define __STRANGE_BRIGADE_LAYER_H__

#pragma once

#include "opt_layer.h"

namespace vk
{
// =====================================================================================================================
// Class for the Strange Brigade Layer to simplify calls to the overriden dispatch table from the layer's entrypoints
class StrangeBrigadeLayer final : public OptLayer
{
public:
    StrangeBrigadeLayer() {}
    virtual ~StrangeBrigadeLayer() {}

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(StrangeBrigadeLayer);
};

}; // namespace vk

#endif /* __STRANGE_BRIGADE_LAYER_H__ */
