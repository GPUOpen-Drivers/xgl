/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcGfx9ConfigBuilder.h
 * @brief LLPC header file: contains declaration of class Llpc::Gfx9::ConfigBuilder.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcGfx9Chip.h"

namespace Llpc
{

class Context;
struct ElfDataEntry;

namespace Gfx9
{

// =====================================================================================================================
// Represents the builder to generate register configurations for GFX6-generation chips.
class ConfigBuilder
{
public:
    static Result BuildPipelineVsFsRegConfig(Context*            pContext,
                                             uint8_t**           ppConfig,
                                             size_t*             pConfigSize);

    static Result BuildPipelineVsTsFsRegConfig(Context*            pContext,
                                               uint8_t**           ppConfig,
                                               size_t*             pConfigSize);

    static Result BuildPipelineVsGsFsRegConfig(Context*            pContext,
                                               uint8_t**           ppConfig,
                                               size_t*             pConfigSize);

    static Result BuildPipelineVsTsGsFsRegConfig(Context*            pContext,
                                                 uint8_t**           ppConfig,
                                                 size_t*             pConfigSize);

    static Result BuildPipelineCsRegConfig(Context*            pContext,
                                           uint8_t**           ppConfig,
                                           size_t*             pConfigSize);

private:
    LLPC_DISALLOW_DEFAULT_CTOR(ConfigBuilder);
    LLPC_DISALLOW_COPY_AND_ASSIGN(ConfigBuilder);

    template <typename T>
    static Result BuildVsRegConfig(Context*            pContext,
                                   ShaderStage         shaderStage,
                                   T*                  pConfig);

    template <typename T>
    static Result BuildLsHsRegConfig(Context*            pContext,
                                     ShaderStage         shaderStage1,
                                     ShaderStage         shaderStage2,
                                     T*                  pConfig);

    template <typename T>
    static Result BuildEsGsRegConfig(Context*            pContext,
                                     ShaderStage         shaderStage1,
                                     ShaderStage         shaderStage2,
                                     T*                  pConfig);

    template <typename T>
    static Result BuildPsRegConfig(Context*            pContext,
                                   ShaderStage         shaderStage,
                                   T*                  pConfig);

    static Result BuildCsRegConfig(Context*             pContext,
                                   ShaderStage          shaderStage,
                                   PipelineCsRegConfig* pConfig);

    template <typename T>
    static Result BuildUserDataConfig(Context*    pContext,
                                      ShaderStage shaderStage1,
                                      ShaderStage shaderStage2,
                                      uint32_t    startUserData,
                                      T*          pConfig);

    static void SetupVgtTfParam(Context* pContext, LsHsRegConfig* pConfig);

    static void BuildApiHwShaderMapping(uint32_t           vsHwShader,
                                        uint32_t           tcsHwShader,
                                        uint32_t           tesHwShader,
                                        uint32_t           gsHwShader,
                                        uint32_t           fsHwShader,
                                        uint32_t           csHwShader,
                                        PipelineRegConfig* pConfig);
};

} // Gfx6

} // Llpc
