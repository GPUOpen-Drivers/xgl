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
* @file  vfx.h
* @brief Header file of vfxParser interface declaration
***********************************************************************************************************************
*/

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <assert.h>

#ifndef VFX_DISABLE_PIPLINE_DOC
    #include "llpc.h"
    using namespace Llpc;
#endif

extern int Snprintf(char* pOutput, size_t bufSize, const char* pFormat, ...);

namespace Vfx
{

// =====================================================================================================================
// Common definition of VfxParser
static const uint32_t MaxSectionCount = 16;
static const uint32_t MaxBindingCount = 16;
static const uint32_t MaxResultCount = 16;
static const uint32_t MaxPushConstRangCount = 16;
static const uint32_t MaxVertexBufferBindingCount = 16;
static const uint32_t MaxVertexAttributeCount = 32;
static const uint32_t MaxSpecConstantCount = 32;
static const uint32_t VfxSizeOfVec4        = 16;
static const uint32_t VfxInvalidValue      = 0xFFFFFFFF;
static const uint32_t VfxVertexBufferSetId = 0xFFFFFFFE;
static const uint32_t VfxIndexBufferSetId  = 0xFFFFFFFD;
static const uint32_t VfxDynamicArrayId    = 0xFFFFFFFC;
static const size_t MaxKeyBufSize   = 256;  // Buffer size to parse a key-value pair key in VFX file.
static const size_t MaxLineBufSize  = 512;  // Buffer size to parse a line in VFX file.

#define VFX_ASSERT(...) assert(__VA_ARGS__);
#define VFX_NEW new
#define VFX_DELETE delete
#define VFX_DELETE_ARRAY delete[]

#define VFX_NEVER_CALLED()  { VFX_ASSERT(0); }
#define VFX_NOT_IMPLEMENTED()  { VFX_ASSERT(0); }

#define _STRING(x) #x
#define STRING(x) _STRING(x)

#define SIZE_OF_ARRAY(ary) (sizeof(ary)/sizeof(ary[0]))

#define PARSE_ERROR(errorMsg, lineNum, ...) { \
    char errorBuf[4096]; \
    int pos = Snprintf(errorBuf, 4096, "Parse error at line %u: ", lineNum); \
    pos += Snprintf(errorBuf + pos, 4096 - pos, __VA_ARGS__); \
    pos += Snprintf(errorBuf + pos, 4096 - pos, "\n"); \
    VFX_ASSERT(pos < 4096); \
    errorMsg += errorBuf; \
}

namespace Math
{
    inline uint32_t Absu(
        int32_t number)
    {
        return static_cast<uint32_t>(abs(number));
    }
}

// =====================================================================================================================
// Represents binary form of IEEE 32-bit floating point type.
union Float32Bits
{
    struct
    {
#ifdef qLittleEndian
        uint32_t mantissa : 23;
        uint32_t exp      : 8;
        uint32_t sign     : 1;
#else
        uint32_t sign     : 1;
        uint32_t exp      : 8;
        uint32_t mantissa : 23;
#endif
    };                  // Bit fields

    uint32_t u32All;    // 32-bit binary value
};

// =====================================================================================================================
// Represents binary form of IEEE 16-bit floating point type.
union Float16Bits
{
    struct
    {
#ifdef qLittleEndian
        uint16_t mantissa : 10;
        uint16_t exp      : 5;
        uint16_t sign     : 1;
#else
        uint16_t sign     : 1;
        uint16_t exp      : 5;
        uint16_t mantissa : 10;
#endif
    };                  // Bit fields

    uint16_t u16All;    // 16-bit binary value
};

// =====================================================================================================================
// Represents IEEE 32-bit floating point type.
class Float32
{
public:
    // Default constructor
    Float32() { m_bits.u32All = 0; }

    // Constructor, initializes our VfxFloat32 with numeric float value
    Float32(float value) { m_bits.u32All = *reinterpret_cast<uint32_t*>(&value); }

    // Constructor, initializes our VfxFloat32 with another VfxFloat32
    Float32(const Float32& other)
        :
        m_bits(other.m_bits)
    {
    }

    // Destructor
    ~Float32() {}

    // Gets the numeric value
    float GetValue() const { return *reinterpret_cast<const float*>(&m_bits.u32All); }

    // Flush denormalized value to zero
    void FlushDenormToZero()
    {
        if ((m_bits.exp == 0) && (m_bits.mantissa != 0))
        {
            m_bits.mantissa = 0;
        }
    }

    // Whether the value is NaN
    bool IsNaN() const { return ((m_bits.exp == 0xFF) && (m_bits.mantissa != 0)); }

    // Whether the value is infinity
    bool IsInf() const { return ((m_bits.exp == 0xFF) && (m_bits.mantissa == 0)); }

    // Gets bits
    Float32Bits GetBits() const { return m_bits; }

private:
    Float32Bits  m_bits; // Value
};

// =====================================================================================================================
// Represents IEEE 16-bit floating point type.
class Float16
{
public:
    // Initializes VfxFloat16 from numeric float value
    void FromFloat32(float value)
    {
        const Float32 f32(value);
        const int32_t exp = f32.GetBits().exp - 127 + 1;

        m_bits.sign = f32.GetBits().sign;

        if (value == 0.0f)
        {
            // Zero
            m_bits.exp      = 0;
            m_bits.mantissa = 0;
        }
        else if (f32.IsNaN())
        {
            // NaN
            m_bits.exp      = 0x1F;
            m_bits.mantissa = 0x3FF;
        }
        else if (f32.IsInf())
        {
            // Infinity
            m_bits.exp      = 0x1F;
            m_bits.mantissa = 0;
        }
        else if (exp > 16)
        {
            // Value is too large, -> infinity
            m_bits.exp      = 0x1F;
            m_bits.mantissa = 0;
        }
        else
        {
            if (exp < -13)
            {
                // Denormalized (exponent = 0, mantissa = abs(int(value * 2^24))
                m_bits.exp      = 0;
                m_bits.mantissa = Math::Absu(static_cast<int32_t>(value * (1u << 24)));
            }
            else
            {
                // Normalized (exponent = exp + 14, mantissa = abs(int(value * 2^(11 - exp))))
                m_bits.exp = exp + 14;
                if (exp <= 11)
                {
                    m_bits.mantissa = Math::Absu(static_cast<int32_t>(value * (1u << (11 - exp))));
                }
                else
                {
                    m_bits.mantissa = Math::Absu(static_cast<int32_t>(value / (1u << (exp - 11))));
                }
            }
        }
    }

    // Gets the numeric value
    float GetValue() const
    {
        float value = 0.0f;

        if ((m_bits.exp == 0) && (m_bits.mantissa == 0))
        {
            // Zero
            value = 0.0f;
        }
        else if (IsNaN())
        {
            // NaN
            Float32Bits nan = {};
            nan.exp      = 0xFF;
            nan.mantissa = 0x3FF;
            value = *reinterpret_cast<float*>(&nan.u32All);
        }
        else if (IsInf())
        {
            // Infinity
            Float32Bits infinity = {};
            infinity.exp      = 0xFF;
            infinity.mantissa = 0;
            value = *reinterpret_cast<float*>(&infinity.u32All);
        }
        else
        {
            if (m_bits.exp != 0)
            {
                // Normalized (value = (mantissa | 0x400) * 2^(exponent - 25))
                if (m_bits.exp >= 25)
                {
                    value = (m_bits.mantissa | 0x400) * static_cast<float>(1u << (m_bits.exp - 25));
                }
                else
                {
                    value = (m_bits.mantissa | 0x400) / static_cast<float>(1u << (25 - m_bits.exp));
                }
            }
            else
            {
                // Denormalized (value = mantissa * 2^-24)
                value = m_bits.mantissa / static_cast<float>(1u << 24);
            }
        }

        return (m_bits.sign) ? -value : value;
    }

    // Flush denormalized value to zero
    void FlushDenormToZero()
    {
        if ((m_bits.exp == 0) && (m_bits.mantissa != 0))
        {
            m_bits.mantissa = 0;
        }
    }

    // Whether the value is NaN
    bool IsNaN() const { return ((m_bits.exp == 0x1F) && (m_bits.mantissa != 0)); }

    // Whether the value is infinity
    bool IsInf() const { return ((m_bits.exp == 0x1F) && (m_bits.mantissa == 0)); }

    // Gets bits
    Float16Bits GetBits() const { return m_bits; }

private:
    Float16Bits  m_bits; // Bits
};

// Represents the combination union of vec4 values.
typedef struct IUFValue_
{
    union
    {
    int32_t     iVec4[4];
    uint32_t    uVec4[4];
    int64_t     i64Vec2[2];
    float       fVec4[4];
    Float16     f16Vec4[4];
    double      dVec2[2];
    };
    struct
    {
        uint32_t length     : 16;
        bool     isInt64    : 1;
        bool     isFloat    : 1;
        bool     isFloat16  : 1;
        bool     isDouble   : 1;
        bool     isHex      : 1;
    } props;
} IUFValue;

// Represents the shader binary data
struct ShaderSource
{
    uint32_t              dataSize;   // Size of the shader binary data
    uint8_t*              pData;      // Shader binary data
};

// =====================================================================================================================
// Definitions for RenderDocument

// Enumerates the type of ResultItem's resultSource
enum ResultSource : uint32_t
{
    ResultSourceColor             = 0,
    ResultSourceDepthStencil      = 1,
    ResultSourceBuffer            = 2,
    ResultSourceMaxEnum           = VfxInvalidValue,
};

// Enumerates the type of ResultItem's compareMethod
enum ResultCompareMethod : uint32_t
{
    ResultCompareMethodEqual     = 0,
    ResultCompareMethodNotEqual  = 1,
    ResultCompareMethodMaxEnum   = VfxInvalidValue,
};

// Enumerates the type of Sampler's dataPattern
enum SamplerPattern : uint32_t
{
    SamplerNearest,
    SamplerLinear,
    SamplerNearestMipNearest,
    SamplerLinearMipLinear,
};

// Enumerates the type of ImageView's dataPattern
enum ImagePattern :uint32_t
{
    ImageCheckBoxUnorm,
    ImageCheckBoxFloat,
    ImageCheckBoxDepth,
    ImageLinearUnorm,
    ImageLinearFloat,
    ImageLinearDepth,
    ImageSolidUnorm,
    ImageSolidFloat,
    ImageSolidDepth,
};

// Represents a result item in Result section.
struct ResultItem
{
    ResultSource resultSource;          // Where to get the result value (Color, DepthStencil, Buffer)
    IUFValue     bufferBinding;         // Buffer binding if resultSource is buffer
    IUFValue     offset;                // Offset of result value
    union
    {
        IUFValue iVec4Value;            // Int      expected result value
        IUFValue i64Vec2Value;          // Int      expected result value
        IUFValue fVec4Value;            // Uint     expected result value
        IUFValue f16Vec4Value;          // Float16  expected result value
        IUFValue dVec2Value;            // Double   expected result value
    };
    ResultCompareMethod compareMethod;  // How to compare result to expected value
};

// Represents Result section.
struct TestResult
{
    uint32_t   numResult;               // Number of valid result items
    ResultItem result[MaxResultCount];  // Whole test results
};

// Represents one specialization constant
struct SpecConstItem
{
    union
    {
        IUFValue  i;                  // Int constant
        IUFValue  f;                  // Float constant
        IUFValue  d;                  // Double constant
    };
};

// Represents specializaton constants for one shader stage.
struct SpecConst
{
    uint32_t       numSpecConst;                     // Number of specialization constants
    SpecConstItem  specConst[MaxSpecConstantCount];  // All specialization constants
};

// Represents one vertex binding
//
// NOTE: deprecated!!
struct VertrexBufferBinding
{
    uint32_t          binding;      // Where to get the result value (Color, DepthStencil, Buffer)
    uint32_t          strideInBytes;// Buffer binding if resultSource is buffer
    VkVertexInputRate stepRate;     // Offset of result value
};

// Represents one vertex attribute
//
// NOTE: deprecated!!
struct VertexAttribute
{
    uint32_t binding;           // Attribute binding
    VkFormat format;            // Attribute format
    uint32_t location;          // Attribute location
    uint32_t offsetInBytes;     // Attribute offset
};

// Represents vertex input state
//
// NOTE: deprecated!!
struct VertexState
{
    uint32_t             numVbBinding;                            // Number of vertex input bindings
    VertrexBufferBinding vbBinding[MaxVertexBufferBindingCount];  // All vertex input bindings
    uint32_t             numAttribute;                            // Number of vertex input attributes
    VertexAttribute      attribute[MaxVertexAttributeCount];      // All vertex input attributes
};

// Represents one BufferView section.
struct BufferView
{
    IUFValue           binding;       // Binding of this view, consist of set, binding, arrayIndex
    VkDescriptorType   descriptorType;// Descriptor type of this view
    uint32_t           size;          // Size of this buffer view, assume same size for the buffer
    VkFormat           format;        // VkFormat of this view
    uint32_t           dataSize;      // Data size in bytes
    uint8_t*           pData;         // Buffer data
};

// Represents one ImageView section.
struct ImageView
{
    IUFValue           binding;       // Binding of this view, consist of set, binding, arrayIndex.
    VkDescriptorType   descriptorType;// Descriptor type of this view. enum type is VkDescriptorType
    IUFValue           size;          // Size of this image
    VkImageViewType    viewType;      // Image view type, enum type is VkImageViewType
    ImagePattern       dataPattern;   // Image data pattern
    uint32_t           samples;       // Number of image samples, only 1 is supportted now
    uint32_t           mipmap;        // Whether this image has mipmap
};

// Represents one Sampler section.
struct Sampler
{
    IUFValue           binding;        // Binding of this view, consist of set, binding, arrayIndex
    VkDescriptorType   descriptorType; // Descriptor type of this view
    SamplerPattern     dataPattern;    // Sampler pattern
};

// Represents one push constant range
struct PushConstRange
{
    uint32_t                     start;         // Push constant range start
    uint32_t                     length;        // Push constant range length
    uint32_t                     dataSize;      // Data size in byte
    uint32_t*                    pData;         // Push constant data
};

// Represents DrawState section
struct DrawState
{
    uint32_t              instance;                                 // Instance count for draw array
    uint32_t              vertex;                                   // Vertex count for draw array
    uint32_t              firstInstance;                            // First instance in draw array
    uint32_t              firstVertex;                              // First vertex in draw array
    uint32_t              index;                                    // Index count for draw index
    uint32_t              firstIndex;                               // First index in draw index
    uint32_t              vertexOffset;                             // Vertex offset in draw index
    VkPrimitiveTopology   topology;                                 // Primitive topology
    uint32_t              patchControlPoints;                       // Patch control points
    IUFValue              dispatch;                                 // Dispatch dimension
    uint32_t              width;                                    // Window width
    uint32_t              height;                                   // Window height
    float                 lineWidth;                                // Line width
    IUFValue              viewport;                                 // Viewport dimension
    SpecConst             vs;                                       // Vertex shader's spec constant
    SpecConst             tcs;                                      // Tessellation control shader's spec constant
    SpecConst             tes;                                      // Tessellation evaluation shader's spec constant
    SpecConst             gs;                                       // Geometry shader's spec constant
    SpecConst             fs;                                       // Fragment shader's spec constant
    SpecConst             cs;                                       // Compute shader shader's spec constant
    uint32_t              numPushConstRange;                        // Number of push constant range
    PushConstRange        pushConstRange[MaxPushConstRangCount];    // Pipeline push constant ranges
};

#ifndef DISABLE_PIPLINE_DOC
// Represents the state of ColorBuffer.
struct ColorBuffer
{
    VkFormat format;                  // The format of color buffer
    uint32_t blendEnable;             // Whether the blend is enabled on this color buffer
    uint32_t blendSrcAlphaToColor;    // Whether source alpha is blended to color channels for this target at draw time
};

// Represents GraphicsPipelineState section.
struct GraphicsPipelineState
{
    VkPrimitiveTopology  topology;            // Primitive type
    uint32_t    patchControlPoints;           // Patch control points
    uint32_t    deviceIndex;                  // Device index for device group
    uint32_t    disableVertexReuse;           // Disable reusing vertex shader output for indexed draws
    uint32_t    depthClipEnable;              // Enable clipping based on Z coordinate
    uint32_t    rasterizerDiscardEnable;      // Kill all rasterized pixels
    uint32_t    perSampleShading;             // Enable per sample shading
    uint32_t    numSamples;                   // Number of coverage samples used when rendering with this pipeline
    uint32_t    samplePatternIdx;             // Index into the currently bound MSAA sample pattern table
    uint32_t    usrClipPlaneMask;             // Mask to indicate the enabled user defined clip planes
    uint32_t    alphaToCoverageEnable;        // Enable alpha to coverage
    uint32_t    dualSourceBlendEnable;        // Blend state bound at draw time will use a dual source blend mode
    uint32_t    switchWinding;                // reverse the TCS declared output primitive vertex order
    uint32_t    enableMultiView;              // Whether to enable multi-views mask
    ColorBuffer colorBuffer[MaxColorTargets]; // Color target state.
};

// Represents ComputePipelineState section.
struct ComputePipelineState
{
    uint32_t    deviceIndex;                  // Device index for device group
};
#endif

};

// Represents the content of RenderDocument.
struct VfxRenderState
{
    uint32_t          version;                                  // Render state version
    Vfx::TestResult   result;                                   // Section "Result"
    uint32_t          numBufferView;                            // Number of section "BufferView"
    Vfx::BufferView   bufferView[Vfx::MaxSectionCount];         // Section "BufferView"
    Vfx::VertexState  vertexState;                              // Section "VertexState"
    Vfx::DrawState    drawState;                                // Section "DrawState"
    uint32_t          numImageView;                             // Number of section "ImageView"
    Vfx::ImageView    imageView[Vfx::MaxSectionCount];          // Section "ImageView"
    uint32_t          numSampler;                               // Number of section "Sampler"
    Vfx::Sampler      sampler[Vfx::MaxSectionCount];            // Section "Sampler"
    Vfx::ShaderSource stages[EShLangCount];                     // Shader source sections
};

#ifndef DISABLE_PIPLINE_DOC
// Represents the content of PipelineDoucment.
struct VfxPipelineState
{
    uint32_t                  version;                          // Pipeline state version
    GraphicsPipelineBuildInfo gfxPipelineInfo;                  // LLPC graphics pipeline build info
    ComputePipelineBuildInfo  compPipelineInfo;                 // LLPC compute pipeline build info
    Vfx::ShaderSource         stages[ShaderStageCount];         // Shader source sections
};
#endif

