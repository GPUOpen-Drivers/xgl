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
* @file  wolfenstein2_layer.h
* @brief App optimization layer for Wolfenstein II: The New Colossus
***********************************************************************************************************************
*/

#ifndef __WOLFENSTEIN_2_LAYER_H__
#define __WOLFENSTEIN_2_LAYER_H__

#pragma once

#include "opt_layer.h"

namespace vk
{
// =====================================================================================================================
// Class for the Wolfenstein2 Layer to simplify calls to the overriden dispatch table from the layer's entrypoints
class Wolfenstein2Layer : public OptLayer
{
public:
    Wolfenstein2Layer();
    virtual ~Wolfenstein2Layer();

    virtual void OverrideDispatchTable(DispatchTable* pDispatchTable) override;
};

} // namespace vk

#endif /* __WOLFENSTEIN_2_LAYER_H__ */
