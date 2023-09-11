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

#ifndef __VK_CONV_H__
#define __VK_CONV_H__

#pragma once

#include "include/vk_utils.h"
#include "include/vk_formats.h"
#include "include/vk_shader_code.h"
#include "include/vk_instance.h"
#include "include/khronos/vk_icd.h"
#include "include/vk_descriptor_set_layout.h"
#include "settings/g_settings.h"

#include "pal.h"
#include "palColorBlendState.h"
#include "palCmdBuffer.h"
#include "palDepthStencilState.h"
#include "palDevice.h"
#include "palEventDefs.h"
#include "palMath.h"
#include "palImage.h"
#include "palPipeline.h"
#include "palQueryPool.h"
#include "palScreen.h"
#include "palSwapChain.h"

#if VKI_RAY_TRACING
#include "gpurt/gpurt.h"
#endif

#include "vkgcDefs.h"

namespace vk
{

namespace convert
{
extern Pal::SwizzledFormat VkToPalSwizzledFormatLookupTableStorage[VK_FORMAT_END_RANGE + 1];
};

constexpr uint32_t MaxPalAspectsPerMask         = 3;    // Images can have up to 3 planes (YUV image).
constexpr uint32_t MaxPalColorAspectsPerMask    = 3;    // YUV images can have up to 3 planes.
constexpr uint32_t MaxPalDepthAspectsPerMask    = 2;    // Depth/stencil images can have up to 2 planes.
constexpr uint32_t MaxRangePerAttachment        = 2;    // Depth/stencil images can have up to 2 planes.

constexpr uint32_t PfpPipelineStages = (Pal::PipelineStageFetchIndices |
                                        Pal::PipelineStageFetchIndirectArgs);

static_assert((MaxRangePerAttachment == MaxPalDepthAspectsPerMask),
              "API's max depth/stencil ranges per attachment and PAL max depth aspects must match");

inline Pal::SwizzledFormat VkToPalFormat(VkFormat format, const RuntimeSettings& settings);

#define VK_TO_PAL_TABLE_COMPLEX(srcType, srcTypeName, dstType, convertFunc, mapping) \
    VK_TO_PAL_TABLE_COMPLEX_WITH_SUFFIX(srcType, srcTypeName, dstType, convertFunc, mapping, )

#define VK_TO_PAL_TABLE_COMPLEX_AMD(srcType, srcTypeName, dstType, convertFunc, mapping) \
    VK_TO_PAL_TABLE_COMPLEX_WITH_SUFFIX(srcType, srcTypeName, dstType, convertFunc, mapping, _AMD)

// =====================================================================================================================
// Macro to construct helper function for converting non-trivial non-identity mapping enums
// The generated helper function's name takes the form "dstType convert::convertFunc(srcType)"
// Checks performed:
// - Checks at load-time whether all enum values in the source enum are handled
// - Checks at load-time whether the enum actually needs a non-identity mapping
// - Ensures that the lookup table initialization isn't done more than once
// - Checks at run-time whether an intentionally unhandled enum value is used
#define VK_TO_PAL_TABLE_COMPLEX_WITH_SUFFIX(srcType, srcTypeName, dstType, convertFunc, mapping, suffix) \
    namespace convert \
    { \
        extern dstType VkToPal##convertFunc##LookupTableStorage[VK_##srcType##_END_RANGE##suffix + 1]; \
        extern const dstType* VkToPal##convertFunc##LookupTable; \
        VK_DBG_DECL(extern bool VkToPal##convertFunc##Valid[VK_##srcType##_END_RANGE##suffix + 1]); \
        static const dstType* InitVkToPal##convertFunc##LookupTable() \
        { \
            dstType* lookupTable = VkToPal##convertFunc##LookupTableStorage; \
            VK_DBG_DECL(static size_t numMatching = 0); \
            VK_DBG_DECL(static bool initialized = false); \
            VK_DBG_CHECK(!initialized, "Lookup table of Vk" #srcTypeName " should not be initialized more than once"); \
            VK_DBG_EXPR(initialized = true); \
            for (size_t i = VK_##srcType##_BEGIN_RANGE##suffix; i <= VK_##srcType##_END_RANGE##suffix; ++i) \
            { \
                VK_DBG_DECL(bool& valid = VkToPal##convertFunc##Valid[i]); \
                VK_DBG_EXPR(valid = true); \
                switch (i) \
                { \
                mapping \
                default: \
                    VK_DBG_CHECK(false, "Unhandled Vk" #srcTypeName " enum value"); \
                } \
            } \
            VK_DBG_CHECK(numMatching != VK_##srcType##_RANGE_SIZE##suffix, "Enum Vk" #srcTypeName " should use " \
                "identity mapping"); \
            return lookupTable; \
        } \
        inline dstType convertFunc(Vk##srcTypeName value) \
        { \
            VK_DBG_CHECK(VkToPal##convertFunc##Valid[value], "Enum value intentionally unhandled"); \
            return VkToPal##convertFunc##LookupTable[value]; \
        } \
    }

// =====================================================================================================================
// Macro to construct helper function for converting trivial Vulkan-PAL non-identity mapping enums
// The generated helper function's name takes the form "Pal::dstType convert::dstType(srcType)"

#define VK_TO_PAL_TABLE_X(srcType, srcTypeName, dstType, mapping) \
    VK_TO_PAL_TABLE_COMPLEX(srcType, srcTypeName, Pal::dstType, dstType, mapping)

#define VK_TO_PAL_TABLE_X_AMD(srcType, srcTypeName, dstType, mapping) \
    VK_TO_PAL_TABLE_COMPLEX_AMD(srcType, srcTypeName, Pal::dstType, dstType, mapping)

// =====================================================================================================================
// Macro to construct a single enum value's mapping for converting non-identity mapping enums
#define VK_TO_PAL_ENTRY_X(srcValue, dstValue) \
    case VK_##srcValue: \
        VK_DBG_EXPR(if ((int32_t)i == (int32_t)Pal::dstValue) numMatching++); \
        lookupTable[i] = Pal::dstValue; \
        break;

// =====================================================================================================================
// Macro to construct a single enum value's mapping for converting enum to struct
#define VK_TO_PAL_STRUC_X(srcValue, dstValue) \
    case VK_##srcValue: \
        lookupTable[i] = dstValue; \
        break;

// =====================================================================================================================
// Macro to make an enum value invalid in case of non-identity mapping enums
#define VK_TO_PAL_ERROR_X(srcValue) \
    case VK_##srcValue: \
        VK_DBG_EXPR(numMatching++); \
        VK_DBG_EXPR(valid = false); \
        break;

// =====================================================================================================================
// Macro to construct helper function for converting identity mapping enums
// The generated helper function's name takes the form convert::dstType(srcType)
// Checks performed:
// - Checks at compile-time whether the enums actually have identity mapping
// - Checks at run-time whether all enum values in the source enum are handled
// - Checks at run-time whether an intentionally unhandled enum value is used
#define VK_TO_PAL_TABLE_I(srcType, srcTypeName, dstType, mapping) \
    VK_TO_PAL_TABLE_I_WITH_SUFFIX(srcType, srcTypeName, dstType, mapping, )

#define VK_TO_PAL_TABLE_I_AMD(srcType, srcTypeName, dstType, mapping) \
    VK_TO_PAL_TABLE_I_WITH_SUFFIX(srcType, srcTypeName, dstType, mapping, _AMD)

#define VK_TO_PAL_TABLE_I_EXT(srcType, srcTypeName, dstType, mapping) \
    VK_TO_PAL_TABLE_I_WITH_SUFFIX(srcType, srcTypeName, dstType, mapping, _EXT)

#define VK_TO_PAL_TABLE_I_WITH_SUFFIX(srcType, srcTypeName, dstType, mapping, suffix) \
    namespace convert \
    { \
        inline Pal::dstType dstType(Vk##srcTypeName value) \
        { \
            VK_DBG_DECL(size_t numHandled = 0); \
            mapping \
            VK_DBG_CHECK(numHandled == VK_##srcType##_RANGE_SIZE##suffix, "Not all Vk" #srcTypeName \
                                                                          " enum values are handled"); \
            return static_cast<Pal::dstType>(value); \
        } \
        VK_DBG_DECL(static Pal::dstType forceLoadTimeCallCheck##srcTypeTo##dstType##suffix = \
                                                   dstType(static_cast<Vk##srcTypeName>(0))); \
    }

// =====================================================================================================================
// Macro to construct a single enum value's mapping for converting identity mapping enums
#define VK_TO_PAL_ENTRY_I(srcValue, dstValue) \
    VK_DBG_EXPR(numHandled++); \
    static_assert((int32_t)VK_##srcValue == (int32_t)Pal::dstValue, "Mismatch between the value of VK_" #srcValue \
        " and Pal::" #dstValue);

// =====================================================================================================================
// Macro to make an enum value invalid in case of identity mapping enums
#define VK_TO_PAL_ERROR_I(srcValue) \
    VK_DBG_EXPR(numHandled++); \
    VK_ASSERT((value != VK_##srcValue) && ("Enum value intentionally unhandled"));

// =====================================================================================================================
// Macro to construct helper function for converting non-identity mapping enums from PAL to Vulkan.
#define PAL_TO_VK_TABLE_X(srcType, srcTypeName, dstType, mapping, returnValue) \
    namespace convert \
    { \
        inline Vk##dstType PalToVK##dstType(Pal::srcType value) \
        { \
            switch (value) \
            { \
            mapping \
            default: \
                VK_ASSERT(!"Unknown PAL Type!"); \
                returnValue \
            } \
        } \
   } \

// =====================================================================================================================
// Macro to construct a single enum value's mapping for converting non-identity mapping enums.
#define PAL_TO_VK_ENTRY_X(srcValue, dstValue) \
    case Pal::srcValue: \
        return VK_##dstValue; \

// =====================================================================================================================
// Macro to construct return value of error for converting non-identity mapping enums.
#define PAL_TO_VK_RETURN_X(dstValue) \
        return VK_##dstValue; \

VK_TO_PAL_TABLE_COMPLEX(PRIMITIVE_TOPOLOGY, PrimitiveTopology, Pal::PrimitiveType, PrimitiveType,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_POINT_LIST,                    PrimitiveType::Point    )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_LINE_LIST,                     PrimitiveType::Line     )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_LINE_STRIP,                    PrimitiveType::Line     )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                 PrimitiveType::Triangle )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                PrimitiveType::Triangle )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,                  PrimitiveType::Triangle )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,      PrimitiveType::Line     )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,     PrimitiveType::Line     )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,  PrimitiveType::Triangle )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, PrimitiveType::Triangle )
VK_TO_PAL_ENTRY_X(PRIMITIVE_TOPOLOGY_PATCH_LIST,                    PrimitiveType::Patch    )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan primitive topology to PAL primitive type
inline Pal::PrimitiveType VkToPalPrimitiveType(
    VkPrimitiveTopology     topology)
{
   return  convert::PrimitiveType(topology);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_COMPLEX(  PRIMITIVE_TOPOLOGY, PrimitiveTopology,  Pal::PrimitiveTopology, PrimitiveTopology,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_POINT_LIST,                      PrimitiveTopology::PointList                   )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_LIST,                       PrimitiveTopology::LineList                    )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_STRIP,                      PrimitiveTopology::LineStrip                   )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                   PrimitiveTopology::TriangleList                )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                  PrimitiveTopology::TriangleStrip               )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,                    PrimitiveTopology::TriangleFan                 )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,        PrimitiveTopology::LineListAdj                 )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,       PrimitiveTopology::LineStripAdj                )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,    PrimitiveTopology::TriangleListAdj             )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,   PrimitiveTopology::TriangleStripAdj            )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_PATCH_LIST,                      PrimitiveTopology::Patch                       )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan primitive topology to PAL equivalent
inline Pal::PrimitiveTopology VkToPalPrimitiveTopology(VkPrimitiveTopology topology)
{
    return convert::PrimitiveTopology(topology);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_X(  SAMPLER_ADDRESS_MODE, SamplerAddressMode,    TexAddressMode,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  SAMPLER_ADDRESS_MODE_REPEAT,                TexAddressMode::Wrap )
VK_TO_PAL_ENTRY_X(  SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,       TexAddressMode::Mirror)
VK_TO_PAL_ENTRY_X(  SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,         TexAddressMode::Clamp)
VK_TO_PAL_ENTRY_X(  SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,       TexAddressMode::ClampBorder)
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan texture addressing mode to PAL equivalent
inline Pal::TexAddressMode VkToPalTexAddressMode(
                              VkSamplerAddressMode texAddress)
{
    if (texAddress == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE)
    {
        // We expose extension VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE
        // so we can freely accept the 'hidden' VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE setting
        return Pal::TexAddressMode::MirrorOnce;
    }

    return convert::TexAddressMode(texAddress);
}

// =====================================================================================================================
// Converts Vulkan border color type to PAL equivalent
inline Pal::BorderColorType VkToPalBorderColorType(VkBorderColor borderColor)
{
    switch (borderColor)
    {
        case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
        case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
            return Pal::BorderColorType::TransparentBlack;
        case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
        case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
            return Pal::BorderColorType::OpaqueBlack;
        case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
        case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
            return Pal::BorderColorType::White;
        case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:
        case VK_BORDER_COLOR_INT_CUSTOM_EXT:
            return Pal::BorderColorType::PaletteIndex;
        default:
        {
            VK_ASSERT(!"Unknown VkBorderColor!");
            return Pal::BorderColorType::TransparentBlack;
        }
    }
}

VK_TO_PAL_TABLE_X(  POLYGON_MODE, PolygonMode,                FillMode,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_POINT,                       FillMode::Points                                         )
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_LINE,                        FillMode::Wireframe                                      )
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_FILL,                        FillMode::Solid                                          )
// =====================================================================================================================
)

inline Pal::FillMode VkToPalFillMode(VkPolygonMode fillMode)
{
    return convert::FillMode(fillMode);
}

// =====================================================================================================================
// Converts Vulkan cull mode to PAL equivalent
inline Pal::CullMode VkToPalCullMode(
    VkCullModeFlags cullMode)
{
    switch (cullMode)
    {
        case VK_CULL_MODE_NONE:
            return Pal::CullMode::None;
        case VK_CULL_MODE_FRONT_BIT:
            return Pal::CullMode::Front;
        case VK_CULL_MODE_BACK_BIT:
            return Pal::CullMode::Back;
        case VK_CULL_MODE_FRONT_AND_BACK:
            return Pal::CullMode::FrontAndBack;
        default:
        {
            VK_ASSERT(!"Unknown Cull Mode!");
            return Pal::CullMode::None;
        }
    }
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  FRONT_FACE, FrontFace,                         FaceOrientation,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  FRONT_FACE_COUNTER_CLOCKWISE,                  FaceOrientation::Ccw                                )
VK_TO_PAL_ENTRY_I(  FRONT_FACE_CLOCKWISE,                          FaceOrientation::Cw                                 )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan face orientation to PAL equivalent
inline Pal::FaceOrientation VkToPalFaceOrientation(VkFrontFace frontFace)
{
    return convert::FaceOrientation(frontFace);
}

VK_TO_PAL_TABLE_X(  LOGIC_OP, LogicOp,                      LogicOp,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  LOGIC_OP_CLEAR,                         LogicOp::Clear                                             )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_AND,                           LogicOp::And                                               )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_AND_REVERSE,                   LogicOp::AndReverse                                        )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_COPY,                          LogicOp::Copy                                              )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_AND_INVERTED,                  LogicOp::AndInverted                                       )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_NO_OP,                         LogicOp::Noop                                              )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_XOR,                           LogicOp::Xor                                               )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_OR,                            LogicOp::Or                                                )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_NOR,                           LogicOp::Nor                                               )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_EQUIVALENT,                    LogicOp::Equiv                                             )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_INVERT,                        LogicOp::Invert                                            )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_OR_REVERSE,                    LogicOp::OrReverse                                         )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_COPY_INVERTED,                 LogicOp::CopyInverted                                      )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_OR_INVERTED,                   LogicOp::OrInverted                                        )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_NAND,                          LogicOp::Nand                                              )
VK_TO_PAL_ENTRY_X(  LOGIC_OP_SET,                           LogicOp::Set                                               )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan logic operation to PAL equivalent
inline Pal::LogicOp VkToPalLogicOp(VkLogicOp logicOp)
{
    return convert::LogicOp(logicOp);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  BLEND_FACTOR, BlendFactor,                            Blend,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ZERO,                             Blend::Zero                                         )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE,                              Blend::One                                          )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_COLOR,                        Blend::SrcColor                                     )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC_COLOR,              Blend::OneMinusSrcColor                             )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_DST_COLOR,                        Blend::DstColor                                     )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_DST_COLOR,              Blend::OneMinusDstColor                             )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_ALPHA,                        Blend::SrcAlpha                                     )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,              Blend::OneMinusSrcAlpha                             )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_DST_ALPHA,                        Blend::DstAlpha                                     )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_DST_ALPHA,              Blend::OneMinusDstAlpha                             )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_CONSTANT_COLOR,                   Blend::ConstantColor                                )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,         Blend::OneMinusConstantColor                        )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_CONSTANT_ALPHA,                   Blend::ConstantAlpha                                )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,         Blend::OneMinusConstantAlpha                        )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_ALPHA_SATURATE,               Blend::SrcAlphaSaturate                             )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC1_COLOR,                       Blend::Src1Color                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,             Blend::OneMinusSrc1Color                            )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC1_ALPHA,                       Blend::Src1Alpha                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,             Blend::OneMinusSrc1Alpha                            )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan blend factor to PAL equivalent

inline Pal::Blend VkToPalBlend(VkBlendFactor blend)
{
    return convert::Blend(blend);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  BLEND_OP, BlendOp,                      BlendFunc,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  BLEND_OP_ADD,                           BlendFunc::Add                                             )
VK_TO_PAL_ENTRY_I(  BLEND_OP_SUBTRACT,                      BlendFunc::Subtract                                        )
VK_TO_PAL_ENTRY_I(  BLEND_OP_REVERSE_SUBTRACT,              BlendFunc::ReverseSubtract                                 )
VK_TO_PAL_ENTRY_I(  BLEND_OP_MIN,                           BlendFunc::Min                                             )
VK_TO_PAL_ENTRY_I(  BLEND_OP_MAX,                           BlendFunc::Max                                             )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan blend func to PAL equivalent
inline Pal::BlendFunc VkToPalBlendFunc(VkBlendOp blendFunc)
{
    return convert::BlendFunc(blendFunc);
}

VK_TO_PAL_TABLE_I(  STENCIL_OP, StencilOp,                  StencilOp,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  STENCIL_OP_KEEP,                        StencilOp::Keep                                            )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_ZERO,                        StencilOp::Zero                                            )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_REPLACE,                     StencilOp::Replace                                         )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_INCREMENT_AND_CLAMP,         StencilOp::IncClamp                                        )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_DECREMENT_AND_CLAMP,         StencilOp::DecClamp                                        )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_INVERT,                      StencilOp::Invert                                          )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_INCREMENT_AND_WRAP,          StencilOp::IncWrap                                         )
VK_TO_PAL_ENTRY_I(  STENCIL_OP_DECREMENT_AND_WRAP,          StencilOp::DecWrap                                         )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan stencil op to PAL equivalent
inline Pal::StencilOp VkToPalStencilOp(VkStencilOp stencilOp)
{
    return convert::StencilOp(stencilOp);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  COMPARE_OP, CompareOp,                  CompareFunc,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  COMPARE_OP_NEVER,                       CompareFunc::Never                                         )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_LESS,                        CompareFunc::Less                                          )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_EQUAL,                       CompareFunc::Equal                                         )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_LESS_OR_EQUAL,               CompareFunc::LessEqual                                     )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_GREATER,                     CompareFunc::Greater                                       )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_NOT_EQUAL,                   CompareFunc::NotEqual                                      )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_GREATER_OR_EQUAL,            CompareFunc::GreaterEqual                                  )
VK_TO_PAL_ENTRY_I(  COMPARE_OP_ALWAYS,                      CompareFunc::Always                                        )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan stencil op to PAL equivalent
inline Pal::CompareFunc VkToPalCompareFunc(VkCompareOp compareOp)
{
    return convert::CompareFunc(compareOp);
}

// =====================================================================================================================
// Converts Vulkan index type to PAL equivalent.
inline Pal::IndexType VkToPalIndexType(
    VkIndexType indexType)
{
    switch (indexType)
    {
    case VK_INDEX_TYPE_UINT8_EXT:
        return Pal::IndexType::Idx8;
    case VK_INDEX_TYPE_UINT16:
        return Pal::IndexType::Idx16;
    case VK_INDEX_TYPE_UINT32:
        return Pal::IndexType::Idx32;
    default:
        VK_ASSERT(!"Unknown VkIndexType");
        return Pal::IndexType::Idx32;
    }
}

// =====================================================================================================================
// Converts Vulkan Filter parameters to the PAL equivalent.
inline Pal::TexFilter VkToPalTexFilter(
    VkBool32            anisotropicEnabled,
    VkFilter            magFilter,
    VkFilter            minFilter,
    VkSamplerMipmapMode mipMode)
{
    Pal::TexFilter palTexFilter = {};

    switch (mipMode)
    {
        case VK_SAMPLER_MIPMAP_MODE_NEAREST:
            palTexFilter.mipFilter     = Pal::MipFilterPoint;
            break;
        case VK_SAMPLER_MIPMAP_MODE_LINEAR:
            palTexFilter.mipFilter     = Pal::MipFilterLinear;
            break;
        default:
            VK_NOT_IMPLEMENTED;
            break;
    }

    const Pal::XyFilter pointFilter  = (anisotropicEnabled != VK_FALSE) ? Pal::XyFilterAnisotropicPoint :
                                                                          Pal::XyFilterPoint;
    const Pal::XyFilter linearFilter = (anisotropicEnabled != VK_FALSE) ? Pal::XyFilterAnisotropicLinear :
                                                                          Pal::XyFilterLinear;
    switch (magFilter)
    {
        case VK_FILTER_NEAREST:
            palTexFilter.magnification = pointFilter;
            break;
        case VK_FILTER_LINEAR:
            palTexFilter.magnification = linearFilter;
            break;
        default:
            VK_NOT_IMPLEMENTED;
            break;
    }

    switch (minFilter)
    {
        case VK_FILTER_NEAREST:
            palTexFilter.minification = pointFilter;
            break;
        case VK_FILTER_LINEAR:
            palTexFilter.minification = linearFilter;
            break;
        default:
            VK_NOT_IMPLEMENTED;
            break;
    }

    return palTexFilter;
}

// =====================================================================================================================
// Converts a Vulkan texture filter quality parameter to the pal equivalent
inline Pal::ImageTexOptLevel VkToPalTexFilterQuality(TextureFilterOptimizationSettings texFilterQuality)
{
    switch (texFilterQuality)
    {
    case TextureFilterOptimizationSettings::TextureFilterOptimizationsDisabled:
        return Pal::ImageTexOptLevel::Disabled;
    case TextureFilterOptimizationSettings::TextureFilterOptimizationsEnabled:
        return Pal::ImageTexOptLevel::Enabled;
    case TextureFilterOptimizationSettings::TextureFilterOptimizationsAggressive:
        return Pal::ImageTexOptLevel::Maximum;
    default:
        return Pal::ImageTexOptLevel::Default;
    }

    return Pal::ImageTexOptLevel::Default;
}

// =====================================================================================================================
// Selects the first PAL aspect from the Vulkan aspect mask and removes the corresponding bits from it.
inline uint32 VkToPalImagePlaneExtract(
    Pal::ChNumFormat    format,
    VkImageAspectFlags* pAspectMask)
{
    if (((*pAspectMask) & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
    {
        // No other aspect can be specified in this case.
        VK_ASSERT((*pAspectMask) == VK_IMAGE_ASPECT_COLOR_BIT);

        (*pAspectMask) = 0;

        return 0;
    }
    else if (((*pAspectMask) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0)
    {
        // Only the depth and/or stencil aspects can be specified in this case.
        VK_ASSERT(((*pAspectMask) & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0);

        if (((*pAspectMask) & VK_IMAGE_ASPECT_DEPTH_BIT) != 0)
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_DEPTH_BIT;

            return 0;
        }
        else
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_STENCIL_BIT;

            if (Pal::Formats::IsDepthStencilOnly(format))
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
    else if (((*pAspectMask) & (VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT)) != 0)
    {
        // Only the YUV specific aspects can be specified in this case.
        VK_ASSERT(((*pAspectMask) & ~(VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT)) == 0);

        // Must be a YUV image.
        VK_ASSERT(Pal::Formats::IsYuv(format));

        switch (format)
        {
            // YUV packed formats.
        case Pal::ChNumFormat::AYUV:
        case Pal::ChNumFormat::UYVY:
        case Pal::ChNumFormat::VYUY:
        case Pal::ChNumFormat::YUY2:
        case Pal::ChNumFormat::YVY2:
            // Application must specify all 3 YUV aspects, and only that.
            VK_ASSERT((*pAspectMask) == VK_IMAGE_ASPECT_PLANE_0_BIT);

            (*pAspectMask) = 0;
            return 0;

            // YUV planar formats with separate Y and UV planes.
        case Pal::ChNumFormat::NV11:
        case Pal::ChNumFormat::NV12:
        case Pal::ChNumFormat::NV21:
        case Pal::ChNumFormat::P016:
        case Pal::ChNumFormat::P010:
        case Pal::ChNumFormat::P208:
        case Pal::ChNumFormat::P210:
            if ((*pAspectMask) & VK_IMAGE_ASPECT_PLANE_0_BIT)
            {
                (*pAspectMask) ^= VK_IMAGE_ASPECT_PLANE_0_BIT;
                return 0;
            }
            else
            {
                // Only the CbCr aspect can be selected besides the Y one.
                VK_ASSERT((*pAspectMask) == VK_IMAGE_ASPECT_PLANE_1_BIT);

                (*pAspectMask) = 0;
                return 1;
            }

            // YUV planar formats with spearate Y, U, and V planes.
        case Pal::ChNumFormat::YV12:
            if ((*pAspectMask) & VK_IMAGE_ASPECT_PLANE_0_BIT)
            {
                (*pAspectMask) ^= VK_IMAGE_ASPECT_PLANE_0_BIT;
                return 0;
            }
            else if ((*pAspectMask) & VK_IMAGE_ASPECT_PLANE_1_BIT)
            {
                (*pAspectMask) ^= VK_IMAGE_ASPECT_PLANE_1_BIT;
                return 1;
            }
            else
            {
                // Only the Cr aspect can be selected besides the Y and Cb one.
                VK_ASSERT((*pAspectMask) == VK_IMAGE_ASPECT_PLANE_2_BIT);

                (*pAspectMask) = 0;
                return 2;
            }
            break;
        default:
            VK_ASSERT(!"Unexpected YUV image format");
        }
    }
#if defined(__unix__)
    else if (((*pAspectMask) & (VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT |
                                VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT |
                                VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT |
                                VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT)) != 0)
    {
        VK_ASSERT(((*pAspectMask) & ~(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT |
                                      VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT |
                                      VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT |
                                      VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT)) == 0);

        if (((*pAspectMask) & VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT) != 0)
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;

            return 0;
        }
        else if (((*pAspectMask) & VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT) != 0)
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;

            return 1;
        }
        else if (((*pAspectMask) & VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT) != 0)
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;

            return 2;
        }
        else if (((*pAspectMask) & VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT) != 0)
        {
            (*pAspectMask) ^= VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;

            return 3;
        }
    }
#endif
    VK_ASSERT(!"Unexpected aspect mask");
    return 0;
}

// =====================================================================================================================
// Selects a single PAL aspect that directly corresponds to the specified mask.
inline uint32 VkToPalImagePlaneSingle(
    VkFormat               format,
    VkImageAspectFlags     aspectMask,
    const RuntimeSettings& settings)
{
    if (Formats::IsYuvFormat(format))
    {
        switch (aspectMask)
        {
        // VK_FORMAT_G8B8G8R8_422_UNORM|VK_FORMAT_B8G8R8G8_422_UNORM
        case VK_IMAGE_ASPECT_COLOR_BIT:
            return 0;
        // Multi Plane Images
        case VK_IMAGE_ASPECT_PLANE_0_BIT:
        case VK_IMAGE_ASPECT_PLANE_1_BIT:
        case VK_IMAGE_ASPECT_PLANE_2_BIT:
            return VkToPalImagePlaneExtract(VkToPalFormat(format, settings).format, &aspectMask);
        default:
            VK_ASSERT(!"Unsupported flag combination");
            return 0;
        }
    }
    else
    {
        switch (aspectMask)
        {
        case VK_IMAGE_ASPECT_COLOR_BIT:
            return 0;
        case VK_IMAGE_ASPECT_DEPTH_BIT:
            return 0;
        case VK_IMAGE_ASPECT_STENCIL_BIT:
            return VkToPalImagePlaneExtract(VkToPalFormat(format, settings).format, &aspectMask);
        case VK_IMAGE_ASPECT_METADATA_BIT:
            return 0;
#if defined(__unix__)
        case VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT:
        case VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT:
        case VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT:
        case VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT:
            return VkToPalImagePlaneExtract(VkToPalFormat(format, settings).format, &aspectMask);
#endif
        default:
            VK_ASSERT(!"Unsupported flag combination");
            return 0;
        }
    }
}

// =====================================================================================================================
VK_TO_PAL_TABLE_X(  IMAGE_TILING, ImageTiling,              ImageTiling,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  IMAGE_TILING_LINEAR,                    ImageTiling::Linear                                        )
VK_TO_PAL_ENTRY_X(  IMAGE_TILING_OPTIMAL,                   ImageTiling::Optimal                                       )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan image tiling to PAL equivalent
inline Pal::ImageTiling VkToPalImageTiling(VkImageTiling tiling)
{
#if defined(__unix__)
    // ImageTiling will be reset later according to its modifier.
    if (tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
    {
        tiling = VK_IMAGE_TILING_OPTIMAL;
    }
#endif
    return convert::ImageTiling(tiling);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  IMAGE_TYPE, ImageType,                  ImageType,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  IMAGE_TYPE_1D,                          ImageType::Tex1d                                           )
VK_TO_PAL_ENTRY_I(  IMAGE_TYPE_2D,                          ImageType::Tex2d                                           )
VK_TO_PAL_ENTRY_I(  IMAGE_TYPE_3D,                          ImageType::Tex3d                                           )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan image type to PAL equivalent.
inline Pal::ImageType VkToPalImageType(VkImageType imgType)
{
    return convert::ImageType(imgType);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_X(  IMAGE_VIEW_TYPE, ImageViewType,         ImageViewType,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_1D,                     ImageViewType::Tex1d                                       )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_2D,                     ImageViewType::Tex2d                                       )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_3D,                     ImageViewType::Tex3d                                       )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_CUBE,                   ImageViewType::TexCube                                     )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_1D_ARRAY,               ImageViewType::Tex1d                                       )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_2D_ARRAY,               ImageViewType::Tex2d                                       )
VK_TO_PAL_ENTRY_X(  IMAGE_VIEW_TYPE_CUBE_ARRAY,             ImageViewType::TexCube                                     )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan image view type to PAL equivalent.
inline Pal::ImageViewType VkToPalImageViewType(VkImageViewType imgViewType)
{
    return convert::ImageViewType(imgViewType);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  SAMPLER_REDUCTION_MODE, SamplerReductionMode,   TexFilterMode,
// =====================================================================================================================
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,    TexFilterMode::Blend)
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_MIN,                 TexFilterMode::Min)
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_MAX,                 TexFilterMode::Max)
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan filter mode to PAL equivalent.
inline Pal::TexFilterMode VkToPalTexFilterMode(VkSamplerReductionMode filterMode)
{
    return convert::TexFilterMode(filterMode);
}

// =====================================================================================================================
// Converts Vulkan video profile level to PAL equivalent.
inline uint32_t VkToPalVideoProfileLevel(uint32_t level)
{
    // Vulkan level value is created using VK_MAKE_API_VERSION
    uint32_t major = level >> 22;
    uint32_t minor = (level >> 12) & 0x3FF;

    // PAL level is represented as version multiplied by 10
    return major * 10 + minor;
}

// =====================================================================================================================
// Converts PAL video profile level to Vulkan equivalent.
inline uint32_t PalToVkVideoProfileLevel(uint32_t level)
{
    // PAL level is represented as version multiplied by 10
    uint32_t major = level / 10;
    uint32_t minor = level % 10;

    // Vulkan level value is created using VK_MAKE_API_VERSION
    return VK_MAKE_API_VERSION(0, major, minor, 0);
}

// Helper structure for mapping Vulkan primitive topology to PAL primitive type + adjacency
struct PalQueryTypePool
{
    PalQueryTypePool()
    { }

    PalQueryTypePool(
        Pal::QueryType      type,
        Pal::QueryPoolType  poolType) :
        m_type(type),
        m_poolType(poolType)
    { }

    Pal::QueryType     m_type;
    Pal::QueryPoolType m_poolType;
};

// =====================================================================================================================
VK_TO_PAL_TABLE_COMPLEX( QUERY_TYPE, QueryType,  PalQueryTypePool, QueryTypePool,
// =====================================================================================================================
VK_TO_PAL_STRUC_X( QUERY_TYPE_OCCLUSION,
                                   PalQueryTypePool( Pal::QueryType::Occlusion, Pal::QueryPoolType::Occlusion ))
VK_TO_PAL_STRUC_X( QUERY_TYPE_PIPELINE_STATISTICS,
                                   PalQueryTypePool( Pal::QueryType::PipelineStats, Pal::QueryPoolType::PipelineStats ))
VK_TO_PAL_STRUC_X( QUERY_TYPE_TIMESTAMP,
                                   PalQueryTypePool( Pal::QueryType::Occlusion, Pal::QueryPoolType::Occlusion ))
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan query type to PAL equivalent
inline Pal::QueryType VkToPalQueryType(VkQueryType queryType)
{
    return convert::QueryTypePool(queryType).m_type;
}

// =====================================================================================================================
// Converts Vulkan query type to PAL equivalent
inline Pal::QueryPoolType VkToPalQueryPoolType(VkQueryType queryType)
{
    return convert::QueryTypePool(queryType).m_poolType;
}

// =====================================================================================================================
// Converts Vulkan query control flags to PAL equivalent
inline Pal::QueryControlFlags VkToPalQueryControlFlags(
    VkQueryType         queryType,
    VkQueryControlFlags flags)
{
    Pal::QueryControlFlags palFlags;
    palFlags.u32All = 0;
    if (((flags & VK_QUERY_CONTROL_PRECISE_BIT) == 0) && (queryType == VK_QUERY_TYPE_OCCLUSION))
    {
        palFlags.impreciseData = 1;
    }

    return palFlags;
}

// =====================================================================================================================
// Converts Vulkan query result flags to PAL equivalent
inline Pal::QueryResultFlags VkToPalQueryResultFlags(VkQueryResultFlags flags)
{
    uint32_t palFlags = Pal::QueryResultDefault;

    if ((flags & VK_QUERY_RESULT_64_BIT) != 0)
    {
        palFlags |= Pal::QueryResult64Bit;
    }

    if ((flags & VK_QUERY_RESULT_WAIT_BIT) != 0)
    {
        palFlags |= Pal::QueryResultWait;
    }

    if ((flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0)
    {
        palFlags |= Pal::QueryResultAvailability;
    }

    if ((flags & VK_QUERY_RESULT_PARTIAL_BIT) != 0)
    {
        palFlags |= Pal::QueryResultPartial;
    }

    return static_cast<Pal::QueryResultFlags>(palFlags);
}

// =====================================================================================================================
// Converts Vulkan pipeline statistics query flags to PAL equivalent
inline Pal::QueryPipelineStatsFlags VkToPalQueryPipelineStatsFlags(VkQueryPipelineStatisticFlags flags)
{
    static_assert(
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsIaVertices)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsIaPrimitives)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsVsInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsGsInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsGsPrimitives)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsCInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsCPrimitives)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsPsInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsHsInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsDsInvocations)) &&
    (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsCsInvocations))
    && (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsTsInvocations))
    && (static_cast<uint32_t>(VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT) ==
        static_cast<uint32_t>(Pal::QueryPipelineStatsMsInvocations))
    ,
    "Need to update this function");

    return static_cast<Pal::QueryPipelineStatsFlags>(flags);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_X(  COMPONENT_SWIZZLE, ComponentSwizzle,      ChannelSwizzle,
// =====================================================================================================================
// NOTE: VK_COMPONENT_SWIZZLE_IDENTITY is handled in the actual conversion function.  Don't call this conversion
// function directly as how a format is remapped is more complicated.  Call RemapFormatComponents() instead.
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_IDENTITY,               ChannelSwizzle::One                                      )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_ZERO,                   ChannelSwizzle::Zero                                     )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_ONE,                    ChannelSwizzle::One                                      )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_R,                      ChannelSwizzle::X                                        )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_G,                      ChannelSwizzle::Y                                        )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_B,                      ChannelSwizzle::Z                                        )
VK_TO_PAL_ENTRY_X(  COMPONENT_SWIZZLE_A,                      ChannelSwizzle::W                                        )

// =====================================================================================================================
)

// ====================================================================================================================
// Reswizzles a format given a component mapping.  The input image format should be a previously unswizzled format,
// such as one returned by VkToPalFormat() function.
inline Pal::SwizzledFormat RemapFormatComponents(
    Pal::SwizzledFormat       format,
    Pal::SubresRange          subresRange,
    const VkComponentMapping& mapping,
    const Pal::IDevice*       pPalDevice,
    Pal::ImageTiling          imageTiling)
{
    using Pal::ChannelSwizzle;

    Pal::ChannelMapping swizzle;

    // First map to PAL enums.  At the same time, convert the VK_COMPONENT_SWIZZLE_IDENTITY identity mapping
    // which maps to {RGBA}
    swizzle.r = (mapping.r != VK_COMPONENT_SWIZZLE_IDENTITY) ? convert::ChannelSwizzle(mapping.r) : ChannelSwizzle::X;
    swizzle.g = (mapping.g != VK_COMPONENT_SWIZZLE_IDENTITY) ? convert::ChannelSwizzle(mapping.g) : ChannelSwizzle::Y;
    swizzle.b = (mapping.b != VK_COMPONENT_SWIZZLE_IDENTITY) ? convert::ChannelSwizzle(mapping.b) : ChannelSwizzle::Z;
    swizzle.a = (mapping.a != VK_COMPONENT_SWIZZLE_IDENTITY) ? convert::ChannelSwizzle(mapping.a) : ChannelSwizzle::W;

    // Copy the unswizzled format
    Pal::SwizzledFormat newFormat = format;

    // See if we can use MM formats for YUV images
    Pal::MergedFormatPropertiesTable formatProperties = {};
    pPalDevice->GetFormatProperties(&formatProperties);

    uint32_t tilingIdx         = (imageTiling == Pal::ImageTiling::Linear) ? 0 : 1;
    uint32_t x8MmformatIdx     = static_cast<uint32_t>(Pal::ChNumFormat::X8_MM_Unorm);
    uint32_t x8Y8MmformatIdx   = static_cast<uint32_t>(Pal::ChNumFormat::X8Y8_MM_Unorm);
    uint32_t x16MmformatIdx    = static_cast<uint32_t>(Pal::ChNumFormat::X16_MM10_Unorm);
    uint32_t x16Y16MmformatIdx = static_cast<uint32_t>(Pal::ChNumFormat::X16Y16_MM10_Unorm);

    // As spec says, the remapping must be identity for any VkImageView used with a combined image sampler that
    // enables sampler YCbCr conversion, thus we could totally ignore the setting in VkComponentMapping.
    // For YCbCr conversions, the remapping is settled in VkSamplerYcbcrConversionCreateInfo, and happens when
    // the conversion finishes.
    // Note: AYUV && NV11 are not available in VK_KHR_sampler_ycbcr_conversion extension.
    if ((format.format >= Pal::ChNumFormat::AYUV) &&
        (format.format <= Pal::ChNumFormat::P208))
    {
        switch (format.format)
        {
        case Pal::ChNumFormat::UYVY:
            newFormat.format = Pal::ChNumFormat::X8Y8_Z8Y8_Unorm;
            newFormat.swizzle.r = ChannelSwizzle::Z;
            newFormat.swizzle.g = ChannelSwizzle::Y;
            newFormat.swizzle.b = ChannelSwizzle::X;
            newFormat.swizzle.a = ChannelSwizzle::One;
            break;
        case Pal::ChNumFormat::VYUY:
            newFormat.format = Pal::ChNumFormat::X8Y8_Z8Y8_Unorm;
            newFormat.swizzle.r = ChannelSwizzle::X;
            newFormat.swizzle.g = ChannelSwizzle::Y;
            newFormat.swizzle.b = ChannelSwizzle::Z;
            newFormat.swizzle.a = ChannelSwizzle::One;
            break;
        case Pal::ChNumFormat::YUY2:
            newFormat.format = Pal::ChNumFormat::Y8X8_Y8Z8_Unorm;
            newFormat.swizzle.r = ChannelSwizzle::Z;
            newFormat.swizzle.g = ChannelSwizzle::Y;
            newFormat.swizzle.b = ChannelSwizzle::X;
            newFormat.swizzle.a = ChannelSwizzle::One;
            break;
        case Pal::ChNumFormat::YVY2:
            newFormat.format = Pal::ChNumFormat::Y8X8_Y8Z8_Unorm;
            newFormat.swizzle.r = ChannelSwizzle::X;
            newFormat.swizzle.g = ChannelSwizzle::Y;
            newFormat.swizzle.b = ChannelSwizzle::Z;
            newFormat.swizzle.a = ChannelSwizzle::One;
            break;
        case Pal::ChNumFormat::YV12:
            newFormat.format = (formatProperties.features[x8MmformatIdx][tilingIdx] != 0) ?
                               Pal::ChNumFormat::X8_MM_Unorm : Pal::ChNumFormat::X8_Unorm;
            if (subresRange.startSubres.plane == 0)
            {
                newFormat.swizzle.r = ChannelSwizzle::Zero;
                newFormat.swizzle.g = ChannelSwizzle::X;
                newFormat.swizzle.b = ChannelSwizzle::Zero;
            }
            else if (subresRange.startSubres.plane == 1)
            {
                newFormat.swizzle.r = ChannelSwizzle::Zero;
                newFormat.swizzle.g = ChannelSwizzle::Zero;
                newFormat.swizzle.b = ChannelSwizzle::X;
            }
            else if (subresRange.startSubres.plane == 2)
            {
                newFormat.swizzle.r = ChannelSwizzle::X;
                newFormat.swizzle.g = ChannelSwizzle::Zero;
                newFormat.swizzle.b = ChannelSwizzle::Zero;
            }
            newFormat.swizzle.a = ChannelSwizzle::One;
            break;
        case Pal::ChNumFormat::NV12:
        case Pal::ChNumFormat::NV21:
        case Pal::ChNumFormat::P208:
            if (subresRange.startSubres.plane == 0)
            {
                newFormat.format = (formatProperties.features[x8MmformatIdx][tilingIdx] != 0) ?
                                   Pal::ChNumFormat::X8_MM_Unorm : Pal::ChNumFormat::X8_Unorm;
                newFormat.swizzle.r = ChannelSwizzle::Zero;
                newFormat.swizzle.g = ChannelSwizzle::X;
                newFormat.swizzle.b = ChannelSwizzle::Zero;
                newFormat.swizzle.a = ChannelSwizzle::One;
            }
            else if (subresRange.startSubres.plane == 1)
            {
                newFormat.format = (formatProperties.features[x8Y8MmformatIdx][tilingIdx] != 0) ?
                                   Pal::ChNumFormat::X8Y8_MM_Unorm : Pal::ChNumFormat::X8Y8_Unorm;
                if ((format.format == Pal::ChNumFormat::NV12) ||
                    (format.format == Pal::ChNumFormat::P208))
                {
                    newFormat.swizzle.r = ChannelSwizzle::Y;
                    newFormat.swizzle.b = ChannelSwizzle::X;
                }
                else
                {
                    newFormat.swizzle.r = ChannelSwizzle::X;
                    newFormat.swizzle.b = ChannelSwizzle::Y;
                }
                newFormat.swizzle.g = ChannelSwizzle::Zero;
                newFormat.swizzle.a = ChannelSwizzle::Zero;
            }
            break;
        case Pal::ChNumFormat::P016:
            if (subresRange.startSubres.plane == 0)
            {
                newFormat.format = Pal::ChNumFormat::X16_Unorm;
                newFormat.swizzle.r = ChannelSwizzle::Zero;
                newFormat.swizzle.g = ChannelSwizzle::X;
                newFormat.swizzle.b = ChannelSwizzle::Zero;
                newFormat.swizzle.a = ChannelSwizzle::One;
            }
            else if (subresRange.startSubres.plane == 1)
            {
                newFormat.format = Pal::ChNumFormat::X16Y16_Unorm;
                newFormat.swizzle.r = ChannelSwizzle::Y;
                newFormat.swizzle.g = ChannelSwizzle::Zero;
                newFormat.swizzle.b = ChannelSwizzle::X;
                newFormat.swizzle.a = ChannelSwizzle::One;
            }
            break;
        case Pal::ChNumFormat::P010:
        case Pal::ChNumFormat::P210:
            if (subresRange.startSubres.plane == 0)
            {
                newFormat.format = (formatProperties.features[x16MmformatIdx][tilingIdx] != 0) ?
                                   Pal::ChNumFormat::X16_MM10_Unorm : Pal::ChNumFormat::X16_Unorm;
                newFormat.swizzle.r = ChannelSwizzle::Zero;
                newFormat.swizzle.g = ChannelSwizzle::X;
                newFormat.swizzle.b = ChannelSwizzle::Zero;
                newFormat.swizzle.a = ChannelSwizzle::One;
            }
            else if (subresRange.startSubres.plane == 1)
            {
                newFormat.format = (formatProperties.features[x16Y16MmformatIdx][tilingIdx] != 0) ?
                                   Pal::ChNumFormat::X16Y16_MM10_Unorm : Pal::ChNumFormat::X16Y16_Unorm;
                newFormat.swizzle.r = ChannelSwizzle::Y;
                newFormat.swizzle.g = ChannelSwizzle::Zero;
                newFormat.swizzle.b = ChannelSwizzle::X;
                newFormat.swizzle.a = ChannelSwizzle::One;
            }
            break;
        default:
            break;
        }
    }
    else if (format.format != Pal::ChNumFormat::Undefined)
    {
        // PAL expects a single swizzle which combines the user-defined VkComponentMapping and the format-defined
        // swizzle together.  In Vulkan these are separate, so we must combine them by building the lookup table below
        // that stores the HW swizzle (X/Y/Z/W, which corresponds to HW data format components where X = LSB) for each
        // logical image component (R/G/B/A) if any.  We build this table from the original input format which also
        // contains the swizzle which means it's important that this function is not called with an already-remapped
        // format as input.
        const ChannelSwizzle rgbaToFinalSwizzle[] =
        {
            ChannelSwizzle::Zero, // Zero
            ChannelSwizzle::One,  // One
            format.swizzle.r,     // R (location of R on the data format)
            format.swizzle.g,     // G (location of G on the data format)
            format.swizzle.b,     // B (location of B on the data format)
            format.swizzle.a,     // A (location of A on the data format)
        };

        // Remap the components.
        newFormat.swizzle.r = rgbaToFinalSwizzle[static_cast<size_t>(swizzle.r)];
        newFormat.swizzle.g = rgbaToFinalSwizzle[static_cast<size_t>(swizzle.g)];
        newFormat.swizzle.b = rgbaToFinalSwizzle[static_cast<size_t>(swizzle.b)];
        newFormat.swizzle.a = rgbaToFinalSwizzle[static_cast<size_t>(swizzle.a)];
    }
    else
    {
        newFormat.swizzle = Pal::UndefinedSwizzledFormat.swizzle;
    }

    return newFormat;
}

// =====================================================================================================================
// Returns the Vulkan image aspect flag bits corresponding to the given PAL YUV format.
inline VkImageAspectFlags PalYuvFormatToVkImageAspectPlane(
    const Pal::ChNumFormat format)
{
    switch (format)
    {
        // YUV packed formats.
    case Pal::ChNumFormat::AYUV:
    case Pal::ChNumFormat::UYVY:
    case Pal::ChNumFormat::VYUY:
    case Pal::ChNumFormat::YUY2:
    case Pal::ChNumFormat::YVY2:
        return VK_IMAGE_ASPECT_PLANE_0_BIT;
        // YUV planar formats with separate Y and UV planes.
    case Pal::ChNumFormat::NV11:
    case Pal::ChNumFormat::NV12:
    case Pal::ChNumFormat::NV21:
    case Pal::ChNumFormat::P016:
    case Pal::ChNumFormat::P010:
    case Pal::ChNumFormat::P208:
    case Pal::ChNumFormat::P210:
        return VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
        // YUV planar formats with spearate Y, U, and V planes.
    case Pal::ChNumFormat::YV12:
        return VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT;
    default:
        VK_ASSERT(!"Unexpected YUV image format");
    }

    return VkImageAspectFlags();
}

// =====================================================================================================================
// Converts Vulkan image subresource range to PAL equivalent.
// It may generate two PAL subresource range entries in case both depth and stencil aspect is selected in the mask.
inline void VkToPalSubresRange(
    VkFormat                        format,
    const VkImageSubresourceRange&  range,
    uint32_t                        mipLevels,
    uint32_t                        arraySize,
    Pal::SubresRange*               pPalSubresRanges,
    uint32_t*                       pPalSubresRangeIndex,
    const RuntimeSettings&          settings)
{
    // The minimums below are used for VkImageSubresourceRange VK_WHOLE_SIZE handling.

    Pal::SubresRange palSubresRange;

    palSubresRange.startSubres.arraySlice   = range.baseArrayLayer;
    palSubresRange.startSubres.mipLevel     = range.baseMipLevel;
    palSubresRange.numPlanes                = 1;
    palSubresRange.numMips                  = Util::Min(range.levelCount, (mipLevels - range.baseMipLevel));
    palSubresRange.numSlices                = Util::Min(range.layerCount, (arraySize - range.baseArrayLayer));

    VkImageAspectFlags aspectMask = range.aspectMask;
    Pal::ChNumFormat palFormat = VkToPalFormat(format, settings).format;

    if ((Pal::Formats::IsYuv(palFormat)) &&
        (aspectMask == VK_IMAGE_ASPECT_COLOR_BIT))
    {
        aspectMask = PalYuvFormatToVkImageAspectPlane(palFormat);
    }

    do
    {
        palSubresRange.startSubres.plane = VkToPalImagePlaneExtract(palFormat, &aspectMask);
        pPalSubresRanges[(*pPalSubresRangeIndex)++] = palSubresRange;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan scissor params to a PAL scissor rect params
inline Pal::ScissorRectParams VkToPalScissorParams(const VkPipelineViewportStateCreateInfo& scissors)
{
    Pal::ScissorRectParams palScissors;

    palScissors.count = scissors.scissorCount;
    for (uint32_t i = 0; i < scissors.scissorCount; ++i)
    {
        palScissors.scissors[i].offset.x      = scissors.pScissors[i].offset.x;
        palScissors.scissors[i].offset.y      = scissors.pScissors[i].offset.y;
        palScissors.scissors[i].extent.width  = scissors.pScissors[i].extent.width;
        palScissors.scissors[i].extent.height = scissors.pScissors[i].extent.height;
    }

    return palScissors;
}

// =====================================================================================================================
// Converts a Vulkan offset 2D to a PAL offset 2D
inline Pal::Offset2d VkToPalOffset2d(const VkOffset2D& offset)
{
    Pal::Offset2d result;
    result.x = offset.x;
    result.y = offset.y;
    return result;
}

// =====================================================================================================================
// Converts a Vulkan offset 3D to a PAL offset 3D
inline Pal::Offset3d VkToPalOffset3d(const VkOffset3D& offset)
{
    Pal::Offset3d result;
    result.x = offset.x;
    result.y = offset.y;
    result.z = offset.z;
    return result;
}

// =====================================================================================================================
// Converts a Vulkan extent 2D to a PAL extent 2D
inline Pal::Extent2d VkToPalExtent2d(const VkExtent2D& extent)
{
    Pal::Extent2d result;
    result.width  = extent.width;
    result.height = extent.height;
    return result;
}

// =====================================================================================================================
// Converts a PAL extent 2D to a Vulkan extent 2D
inline VkExtent2D PalToVkExtent2d(const Pal::Extent2d& extent)
{
    VkExtent2D result;
    result.width  = extent.width;
    result.height = extent.height;
    return result;
}

// =====================================================================================================================
// Converts PAL GpuType to Vulkan VkPhysicalDeviceType
inline VkPhysicalDeviceType PalToVkGpuType(const Pal::GpuType gpuType)
{
    const VkPhysicalDeviceType gpuTypeTbl[] =
    {
        VK_PHYSICAL_DEVICE_TYPE_OTHER,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
    };

    const int32_t index = (int32_t)gpuType;
    VK_ASSERT(index >= VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE && index < VK_PHYSICAL_DEVICE_TYPE_END_RANGE);
    return gpuTypeTbl[index];
}

// =====================================================================================================================
// Converts a Vulkan extent 3D to a PAL extent 3D
inline Pal::Extent3d VkToPalExtent3d(const VkExtent3D& extent)
{
    Pal::Extent3d result;
    result.width  = extent.width;
    result.height = extent.height;
    result.depth  = extent.depth;
    return result;
}

// =====================================================================================================================
// Converts a PAL extent 3D to a Vulkan extent 3D
inline VkExtent3D PalToVkExtent3d(const Pal::Extent3d& extent)
{
    VkExtent3D result;
    result.width  = extent.width;
    result.height = extent.height;
    result.depth  = extent.depth;
    return result;
}

// =====================================================================================================================
// Converts two Vulkan 3D offsets to a PAL signed extent 3D
inline Pal::SignedExtent3d VkToPalSignedExtent3d(const VkOffset3D offsets[2])
{
    Pal::SignedExtent3d result;
    result.width  = offsets[1].x  - offsets[0].x;
    result.height = offsets[1].y  - offsets[0].y;
    result.depth  = offsets[1].z  - offsets[0].z;
    return result;
}

// =====================================================================================================================
// Converts value in texels to value in blocks, specifying block dimension for the given coordinate.
inline uint32_t TexelsToBlocks(uint32_t texels, uint32_t blockSize)
{
    return Util::RoundUpToMultiple(texels, blockSize) / blockSize;
}

// =====================================================================================================================
// Converts signed value in texels to signed value in blocks, specifying block dimension for the given coordinate.
inline int32_t TexelsToBlocks(int32_t texels, uint32_t blockSize)
{
    uint32_t value = Util::Math::Absu(texels);
    value = Util::RoundUpToMultiple(value, blockSize) / blockSize;

    int32_t retValue = (int32_t)value;
    return texels > 0 ? retValue : -retValue;
}

// =====================================================================================================================
// Converts pitch value in texels to pitch value in blocks, specifying block dimension for the given coordinate.
inline Pal::gpusize PitchTexelsToBlocks(Pal::gpusize texels, uint32_t blockSize)
{
    return Util::RoundUpToMultiple(texels, static_cast<Pal::gpusize>(blockSize)) / blockSize;
}

// =====================================================================================================================
// Converts extent in texels to extent in blocks, specifying block dimensions.
inline Pal::Extent3d TexelsToBlocks(Pal::Extent3d texels, Pal::Extent3d blockSize)
{
    Pal::Extent3d blocks;

    blocks.width    = TexelsToBlocks(texels.width,  blockSize.width);
    blocks.height   = TexelsToBlocks(texels.height, blockSize.height);
    blocks.depth    = TexelsToBlocks(texels.depth,  blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Converts signed extent in texels to signed extent in blocks, specifying block dimensions.
inline Pal::SignedExtent3d TexelsToBlocks(Pal::SignedExtent3d texels, Pal::Extent3d blockSize)
{
    Pal::SignedExtent3d blocks;

    blocks.width  = TexelsToBlocks(texels.width,  blockSize.width);
    blocks.height = TexelsToBlocks(texels.height, blockSize.height);
    blocks.depth  = TexelsToBlocks(texels.depth,  blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Converts offset in texels to offset in blocks, specifying block dimensions.
inline Pal::Offset3d TexelsToBlocks(Pal::Offset3d texels, Pal::Extent3d blockSize)
{
    Pal::Offset3d blocks;

    blocks.x = TexelsToBlocks(texels.x, blockSize.width);
    blocks.y = TexelsToBlocks(texels.y, blockSize.height);
    blocks.z = TexelsToBlocks(texels.z, blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Queries the number of bytes in a pixel or element for the given format.
inline Pal::uint32 BytesPerPixel(Pal::ChNumFormat format, uint32 plane)
{
    if (Pal::Formats::IsYuvPlanar(format))
    {
        Pal::uint32 bytesPerPixel = 0;

        switch (format)
        {
        case Pal::ChNumFormat::YV12:
            VK_ASSERT((0 == plane) || (1 == plane) || (2 == plane));
            bytesPerPixel = 1;
            break;
        case Pal::ChNumFormat::NV11:
        case Pal::ChNumFormat::NV12:
        case Pal::ChNumFormat::NV21:
        case Pal::ChNumFormat::P208:
            VK_ASSERT((0 == plane) || (1 == plane));
            bytesPerPixel = (0 == plane) ? 1 : 2;
            break;
        case Pal::ChNumFormat::P016:
        case Pal::ChNumFormat::P010:
        case Pal::ChNumFormat::P210:
            VK_ASSERT((0 == plane) || (1 == plane));
            bytesPerPixel = (0 == plane) ? 2 : 4;
            break;
        default:
            VK_NEVER_CALLED();
            break;
        }

        return bytesPerPixel;
    }
    else
    {
        return Pal::Formats::BytesPerPixel(format);
    }
}

// =====================================================================================================================
// Converts a Vulkan image-copy structure to one or more PAL image-copy-region structures.
template<typename ImageCopyType>
void VkToPalImageCopyRegion(
    const ImageCopyType&    imageCopy,
    Pal::ChNumFormat        srcFormat,
    uint32_t                srcArraySize,
    Pal::ChNumFormat        dstFormat,
    uint32_t                dstArraySize,
    Pal::ImageCopyRegion*   pPalRegions,
    uint32_t*               pPalRegionIndex)
{
    Pal::ImageCopyRegion region = {};

    region.srcSubres.arraySlice = imageCopy.srcSubresource.baseArrayLayer;
    region.srcSubres.mipLevel   = imageCopy.srcSubresource.mipLevel;

    region.dstSubres.arraySlice = imageCopy.dstSubresource.baseArrayLayer;
    region.dstSubres.mipLevel   = imageCopy.dstSubresource.mipLevel;

    region.extent    = VkToPalExtent3d(imageCopy.extent);
    region.srcOffset = VkToPalOffset3d(imageCopy.srcOffset);
    region.dstOffset = VkToPalOffset3d(imageCopy.dstOffset);

    VK_ASSERT(imageCopy.srcSubresource.layerCount != 0u);
    VK_ASSERT(imageCopy.dstSubresource.layerCount != 0u);
    VK_ASSERT(imageCopy.extent.width  != 0u);
    VK_ASSERT(imageCopy.extent.height != 0u);
    VK_ASSERT(imageCopy.extent.depth  != 0u);

    uint32_t srcLayerCount = (imageCopy.srcSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
        (srcArraySize - imageCopy.srcSubresource.baseArrayLayer) : imageCopy.srcSubresource.layerCount;
    uint32_t dstLayerCount = (imageCopy.dstSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
        (dstArraySize - imageCopy.dstSubresource.baseArrayLayer) : imageCopy.dstSubresource.layerCount;

    // Layer count may be different if copying between 2D and 3D images
    region.numSlices = Util::Max<uint32_t>(srcLayerCount, dstLayerCount);

    // PAL expects all dimensions to be in blocks for compressed formats so let's handle that here
    if (Pal::Formats::IsBlockCompressed(srcFormat))
    {
        Pal::Extent3d blockDim  = Pal::Formats::CompressedBlockDim(srcFormat);

        region.extent           = TexelsToBlocks(region.extent, blockDim);
        region.srcOffset        = TexelsToBlocks(region.srcOffset, blockDim);
    }
    if (Pal::Formats::IsBlockCompressed(dstFormat))
    {
        Pal::Extent3d blockDim  = Pal::Formats::CompressedBlockDim(dstFormat);

        region.dstOffset        = TexelsToBlocks(region.dstOffset, blockDim);
    }

    VkImageAspectFlags srcAspectMask = imageCopy.srcSubresource.aspectMask;
    VkImageAspectFlags dstAspectMask = imageCopy.dstSubresource.aspectMask;

    do
    {
        VK_ASSERT(pPalRegionIndex != nullptr);

        region.srcSubres.plane = VkToPalImagePlaneExtract(srcFormat, &srcAspectMask);
        region.dstSubres.plane = VkToPalImagePlaneExtract(dstFormat, &dstAspectMask);
        pPalRegions[(*pPalRegionIndex)++] = region;
    }
    while (srcAspectMask != 0 || dstAspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan image-blit structure to one or more PAL image-scaled-copy-region structures.
template<typename ImageBlitType>
void VkToPalImageScaledCopyRegion(
    const ImageBlitType&        imageBlit,
    Pal::ChNumFormat            srcFormat,
    uint32_t                    srcArraySize,
    Pal::ChNumFormat            dstFormat,
    Pal::ImageScaledCopyRegion* pPalRegions,
    uint32_t*                   pPalRegionIndex)
{
    Pal::ImageScaledCopyRegion region = {};

    region.srcSubres.arraySlice = imageBlit.srcSubresource.baseArrayLayer;
    region.srcSubres.mipLevel   = imageBlit.srcSubresource.mipLevel;

    region.dstSubres.arraySlice = imageBlit.dstSubresource.baseArrayLayer;
    region.dstSubres.mipLevel   = imageBlit.dstSubresource.mipLevel;

    region.srcOffset = VkToPalOffset3d(imageBlit.srcOffsets[0]);
    region.srcExtent = VkToPalSignedExtent3d(imageBlit.srcOffsets);

    region.dstOffset = VkToPalOffset3d(imageBlit.dstOffsets[0]);
    region.dstExtent = VkToPalSignedExtent3d(imageBlit.dstOffsets);

    // Source and destination aspect masks and layer counts must match
    VK_ASSERT(imageBlit.srcSubresource.aspectMask == imageBlit.dstSubresource.aspectMask);
    VK_ASSERT(imageBlit.srcSubresource.layerCount == imageBlit.dstSubresource.layerCount);

    region.numSlices = (imageBlit.srcSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
        (srcArraySize - imageBlit.srcSubresource.baseArrayLayer) : imageBlit.srcSubresource.layerCount;

    // As we don't allow copying between different types of aspects we don't need to worry about dealing with both
    // aspect masks separately.
    VkImageAspectFlags aspectMask = imageBlit.srcSubresource.aspectMask;

    do
    {
        VK_ASSERT(pPalRegionIndex != nullptr);

        region.srcSubres.plane = region.dstSubres.plane = VkToPalImagePlaneExtract(srcFormat, &aspectMask);
        pPalRegions[(*pPalRegionIndex)++] = region;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan image-resolve structure to one or more PAL image-resolve-region structures.
template<typename ImageResolveType>
void VkToPalImageResolveRegion(
    const ImageResolveType&     imageResolve,
    Pal::SwizzledFormat         srcFormat,
    uint32_t                    srcArraySize,
    bool                        reinterpretToSrcFormat,
    Pal::ImageResolveRegion*    pPalRegions,
    uint32_t*                   pPalRegionIndex)
{
    Pal::ImageResolveRegion region = {};

    region.swizzledFormat = reinterpretToSrcFormat ? srcFormat : Pal::UndefinedSwizzledFormat;

    region.srcSlice     = imageResolve.srcSubresource.baseArrayLayer;

    region.dstSlice     = imageResolve.dstSubresource.baseArrayLayer;
    region.dstMipLevel  = imageResolve.dstSubresource.mipLevel;

    region.extent    = VkToPalExtent3d(imageResolve.extent);
    region.srcOffset = VkToPalOffset3d(imageResolve.srcOffset);
    region.dstOffset = VkToPalOffset3d(imageResolve.dstOffset);

    VK_ASSERT(imageResolve.srcSubresource.layerCount == imageResolve.dstSubresource.layerCount);

    region.numSlices = (imageResolve.srcSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
        (srcArraySize - imageResolve.srcSubresource.baseArrayLayer) : imageResolve.srcSubresource.layerCount;

    // Source and destination aspect masks must match
    VK_ASSERT(imageResolve.srcSubresource.aspectMask == imageResolve.dstSubresource.aspectMask);

    // As we don't allow copying between different types of aspects we don't need to worry about dealing with both
    // aspect masks separately.
    VkImageAspectFlags aspectMask = imageResolve.srcSubresource.aspectMask;

    do
    {
        VK_ASSERT(pPalRegionIndex != nullptr);

        region.srcPlane = region.dstPlane = VkToPalImagePlaneExtract(srcFormat.format, &aspectMask);
        pPalRegions[(*pPalRegionIndex)++] = region;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan buffer-image-copy structure to a PAL memory-image-copy-region structure.
template<typename BufferImageCopyType>
Pal::MemoryImageCopyRegion VkToPalMemoryImageCopyRegion(
    const BufferImageCopyType&  bufferImageCopy,
    Pal::ChNumFormat            format,
    uint32                      plane,
    uint32_t                    arraySize,
    Pal::gpusize                baseMemOffset)
{
    Pal::MemoryImageCopyRegion region = {};

    region.imageSubres.plane        = plane;
    region.imageSubres.arraySlice   = bufferImageCopy.imageSubresource.baseArrayLayer;
    region.imageSubres.mipLevel     = bufferImageCopy.imageSubresource.mipLevel;

    region.imageOffset          = VkToPalOffset3d(bufferImageCopy.imageOffset);
    region.imageExtent          = VkToPalExtent3d(bufferImageCopy.imageExtent);

    region.numSlices            = (bufferImageCopy.imageSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
        (arraySize - bufferImageCopy.imageSubresource.baseArrayLayer) : bufferImageCopy.imageSubresource.layerCount;

    region.gpuMemoryOffset      = baseMemOffset + bufferImageCopy.bufferOffset;
    region.gpuMemoryRowPitch    = (bufferImageCopy.bufferRowLength != 0)
                                    ? bufferImageCopy.bufferRowLength
                                    : bufferImageCopy.imageExtent.width;
    region.gpuMemoryDepthPitch  = (bufferImageCopy.bufferImageHeight != 0)
                                    ? bufferImageCopy.bufferImageHeight
                                    : bufferImageCopy.imageExtent.height;

    // For best performance, let PAL choose the copy format
    region.swizzledFormat = Pal::UndefinedSwizzledFormat;

    // PAL expects all dimensions to be in blocks for compressed formats so let's handle that here
    if (Pal::Formats::IsBlockCompressed(format))
    {
        Pal::Extent3d blockDim      = Pal::Formats::CompressedBlockDim(format);

        region.imageExtent          = TexelsToBlocks(region.imageExtent, blockDim);
        region.imageOffset          = TexelsToBlocks(region.imageOffset, blockDim);
        region.gpuMemoryRowPitch    = PitchTexelsToBlocks(region.gpuMemoryRowPitch, blockDim.width);
        region.gpuMemoryDepthPitch  = PitchTexelsToBlocks(region.gpuMemoryDepthPitch, blockDim.height);
    }

    // Convert pitch to bytes per pixel and multiply depth pitch by row pitch after the texel-to-block conversion
    region.gpuMemoryRowPitch   *= BytesPerPixel(format, plane);
    region.gpuMemoryDepthPitch *= region.gpuMemoryRowPitch;

    return region;
}

namespace convert
{
extern Pal::SwizzledFormat VkToPalSwizzledFormatLookupTableStorage[VK_FORMAT_END_RANGE + 1];
};

// =====================================================================================================================
constexpr Pal::SwizzledFormat PalFmt(
    Pal::ChNumFormat    chNumFormat,
    Pal::ChannelSwizzle r,
    Pal::ChannelSwizzle g,
    Pal::ChannelSwizzle b,
    Pal::ChannelSwizzle a)
{
    return { chNumFormat, Pal::ChannelMapping{{{ r, g, b, a }}} };
}

#if (VKI_GPU_DECOMPRESS)
static VkFormat convertCompressedFormat(VkFormat format, bool useBC3)
{
    if (Formats::IsASTCFormat(format))
    {
        AstcMappedInfo mapInfo = {};
        Formats::GetAstcMappedInfo(format, &mapInfo);
        format = useBC3 ? VK_FORMAT_BC3_UNORM_BLOCK : mapInfo.format;
    }
    else if (Formats::IsEtc2Format(format))
    {
        if( (VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK == format)   ||
            (VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK == format) ||
            (VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK == format))
        {
            format = useBC3 ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_R8G8B8A8_SRGB;
        }
        else
        {
            format = useBC3 ? VK_FORMAT_BC3_UNORM_BLOCK : VK_FORMAT_R8G8B8A8_UNORM;
        }
    }
    return format;
}
#endif

namespace convert
{
#if VKI_RAY_TRACING
    // =================================================================================================================
    // Performs conversion of traceRayProfileRayFlags to RayTracingRayFlag
    static uint32_t  TraceRayProfileRayFlagsToRayTracingRayFlags(
        uint32_t                           traceRayProfileRayFlag) // [in] Input trace ray profile ray flag
    {
        uint32_t rayFlag = 0;

        switch (traceRayProfileRayFlag)
        {
        case vk::TraceRayProfileFlags::TraceRayProfileForceOpaque:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagForceOpaque;
            break;
        case vk::TraceRayProfileFlags::TraceRayProfileAcceptFirstHitAndEndSearch:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagAcceptFirstHitAndEndSearch;
            break;
        case vk::TraceRayProfileFlags::TraceRayProfileSkipClosestHitShader:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagSkipClosestHitShader;
            break;
        case vk::TraceRayProfileFlags::TraceRayProfileCullFrontFacingTriangles:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagCullBackFacingTriangles;
            break;
        case vk::TraceRayProfileFlags::TraceRayProfileCullBackFacingTriangles:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagCullFrontFacingTriangles;
            break;
        default:
            rayFlag = Vkgc::RayTracingRayFlag::RayTracingRayFlagNone;
        }

        return rayFlag;
    }
#endif
}

// =====================================================================================================================
// Converts Vulkan format to PAL equivalent.
inline Pal::SwizzledFormat VkToPalFormat(VkFormat format, const RuntimeSettings& settings)
{
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT))
    {

#if VKI_GPU_DECOMPRESS
        if (settings.enableShaderDecode)
        {
            format = convertCompressedFormat(format, settings.enableBC3Encoder);
        }
#endif
        return convert::VkToPalSwizzledFormatLookupTableStorage[format];
    }
    else
    {
        switch (static_cast<int32_t>(format))
        {
        case VK_FORMAT_G8B8G8R8_422_UNORM:             return PalFmt(Pal::ChNumFormat::YUY2,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_B8G8R8G8_422_UNORM:             return PalFmt(Pal::ChNumFormat::UYVY,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:      return PalFmt(Pal::ChNumFormat::YV12,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:       return PalFmt(Pal::ChNumFormat::NV12,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:       return PalFmt(Pal::ChNumFormat::P208,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:    return PalFmt(Pal::ChNumFormat::P010,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:    return PalFmt(Pal::ChNumFormat::P016,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:    return PalFmt(Pal::ChNumFormat::P210,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::One);
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:   return PalFmt(Pal::ChNumFormat::X4Y4Z4W4_Unorm,
            Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::W);
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT:   return PalFmt(Pal::ChNumFormat::X4Y4Z4W4_Unorm,
            Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y, Pal::ChannelSwizzle::Z, Pal::ChannelSwizzle::W);
        }
        return Pal::UndefinedSwizzledFormat;
    }
}

// =====================================================================================================================
// Pal to Vulkan swapchain format, for use with formats returned from IDevice::GetSwapChainProperties only.
inline VkFormat PalToVkSwapChainFormat(Pal::SwizzledFormat palFormat)
{
    VkFormat format = VK_FORMAT_UNDEFINED;

    switch (palFormat.format)
    {
    case Pal::ChNumFormat::X8Y8Z8W8_Unorm:
    {
        if ((palFormat.swizzle.r == Pal::ChannelSwizzle::X) &&
            (palFormat.swizzle.g == Pal::ChannelSwizzle::Y) &&
            (palFormat.swizzle.b == Pal::ChannelSwizzle::Z) &&
            (palFormat.swizzle.a == Pal::ChannelSwizzle::W))
        {
            format = VK_FORMAT_R8G8B8A8_UNORM;
        }
        else if ((palFormat.swizzle.r == Pal::ChannelSwizzle::Z) &&
                 (palFormat.swizzle.g == Pal::ChannelSwizzle::Y) &&
                 (palFormat.swizzle.b == Pal::ChannelSwizzle::X) &&
                 (palFormat.swizzle.a == Pal::ChannelSwizzle::W))
        {
            format = VK_FORMAT_B8G8R8A8_UNORM;
        }
        break;
    }
    case Pal::ChNumFormat::X16Y16Z16W16_Float:
    {
        if ((palFormat.swizzle.r == Pal::ChannelSwizzle::X) &&
            (palFormat.swizzle.g == Pal::ChannelSwizzle::Y) &&
            (palFormat.swizzle.b == Pal::ChannelSwizzle::Z) &&
            (palFormat.swizzle.a == Pal::ChannelSwizzle::W))
        {
            format = VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        break;
    }
    case Pal::ChNumFormat::X10Y10Z10W2_Unorm:
    {
        if ((palFormat.swizzle.r == Pal::ChannelSwizzle::X) &&
            (palFormat.swizzle.g == Pal::ChannelSwizzle::Y) &&
            (palFormat.swizzle.b == Pal::ChannelSwizzle::Z) &&
            (palFormat.swizzle.a == Pal::ChannelSwizzle::W))
        {
            format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        }
        break;
    }
    default:
    {
        break;
    }
    }

    VK_ASSERT_MSG(format != VK_FORMAT_UNDEFINED, "Unknown swapchain format, consider adding it here");

    return format;
}

// =====================================================================================================================
// TODO: VK_EXT_swapchain_colorspace combines the concept of a transfer function and a color space, which is
// insufficient. For now,  map the capabilities of Pal using either the transfer function OR color space
// settings to support the current revision of VK_EXT_swapchain_colorspace.
// To expose the complete capability, we should propose VK_EXT_swapchain_transfer_function (or a similar named)
// extension and propose revisions to VK_EXT_swapchain_colorspace.
namespace convert
{
    inline Pal::ScreenColorSpace ScreenColorSpace(VkSurfaceFormatKHR surfaceFormat)
    {
        union
        {
            Pal::ScreenColorSpace palColorSpace;
            uint32_t              palColorSpaceBits;
        };

        switch (static_cast<uint32_t>(surfaceFormat.colorSpace))
        {
        // sRGB
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
            palColorSpaceBits  = Pal::ScreenColorSpace::TfSrgb;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsSrgb;
            break;

        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
        case VK_COLOR_SPACE_DCI_P3_LINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfSrgb;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsDciP3;
            break;

        case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfSrgb;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsScrgb;
            break;

        // Adobe
        case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfSrgb;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsAdobe;
            break;

        // BT 709
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfBt709;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsBt709;
            break;

        // HDR 10
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfPq2084;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsBt2020;
            break;

        case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfLinear0_125;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsBt2020;
            break;

        case VK_COLOR_SPACE_HDR10_HLG_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfHlg;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsBt709;
            break;

        // Dolby
        case VK_COLOR_SPACE_DOLBYVISION_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfDolbyVision;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsDolbyVision;
            break;

        // MS
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfLinear0_125;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsScrgb;
            break;

        // User defined
        case VK_COLOR_SPACE_PASS_THROUGH_EXT:
            palColorSpaceBits = Pal::ScreenColorSpace::TfSrgb;
            palColorSpaceBits |= Pal::ScreenColorSpace::CsUserDefined;
            break;

        // Unknown
        default:
            palColorSpace = Pal::ScreenColorSpace::TfUndefined;
            VK_ASSERT(!"Unknown Colorspace!");
            break;
        }

        return palColorSpace;
    }
}

// =====================================================================================================================
inline Pal::ScreenColorSpace VkToPalScreenSpace(VkSurfaceFormatKHR colorFormat)
{
    return convert::ScreenColorSpace(colorFormat);
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL HW pipe point.
// Selects a source pipe point that matches all stage flags to use for setting/resetting events.
inline Pal::HwPipePoint VkToPalSrcPipePoint(
    PipelineStageFlags flags)
{
    // Flags that only require signaling at top-of-pipe.
    static const PipelineStageFlags srcTopOfPipeFlags =
        VK_PIPELINE_STAGE_HOST_BIT                           |
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // Flags that only require signaling post-index-fetch.
    static const PipelineStageFlags srcPostIndexFetchFlags =
        srcTopOfPipeFlags                                    |
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT                  |
        VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR              |
        VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT;

    // Flags that only require signaling pre-rasterization.
    static const PipelineStageFlags srcPreRasterizationFlags =
        srcPostIndexFetchFlags                               |
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                   |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                  |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT    |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR   |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                |
        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT         |
        VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR;

    // Flags that only require signaling post-PS.
    static const PipelineStageFlags srcPostPsFlags =
        srcPreRasterizationFlags                             |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT           |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                |
        VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    // Flags that only require signaling post-CS.
    static const PipelineStageFlags srcPostCsFlags =
#if VKI_RAY_TRACING
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR           |
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
#endif
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    // Flags that only require signaling post-BLT operations.
    static const PipelineStageFlags srcPostBltFlags =
        VK_PIPELINE_STAGE_2_COPY_BIT_KHR                     |
        VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR                  |
        VK_PIPELINE_STAGE_2_BLIT_BIT_KHR                     |
        VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR                    |
        VK_PIPELINE_STAGE_TRANSFER_BIT;

    Pal::HwPipePoint srcPipePoint;

    // Check if top-of-pipe signaling is enough.
    if ((flags & ~srcTopOfPipeFlags) == 0)
    {
        srcPipePoint = Pal::HwPipeTop;
    }
    // Otherwise see if post-index-fetch signaling is enough.
    else if ((flags & ~srcPostIndexFetchFlags) == 0)
    {
        srcPipePoint = Pal::HwPipePostPrefetch;
    }
    // Otherwise see if pre-rasterization signaling is enough.
    else if ((flags & ~srcPreRasterizationFlags) == 0)
    {
        srcPipePoint = Pal::HwPipePreRasterization;
    }
    // Otherwise see if post-PS signaling is enough.
    else if ((flags & ~srcPostPsFlags) == 0)
    {
        srcPipePoint = Pal::HwPipePostPs;
    }
    // Otherwise see if post-CS signaling is enough.
    else if ((flags & ~srcPostCsFlags) == 0)
    {
        srcPipePoint = Pal::HwPipePostCs;
    }
    // Otherwise we have to resort to post Blt signaling.
    else if ((flags & ~srcPostBltFlags) == 0)
    {
        srcPipePoint = Pal::HwPipePostBlt;
    }
    // Otherwise we have to resort to bottom-of-pipe signaling.
    else
    {
        srcPipePoint = Pal::HwPipeBottom;
    }

    return srcPipePoint;
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL HW top or bottom pipe point.
inline Pal::HwPipePoint VkToPalSrcPipePointForTimestampWrite(
    PipelineStageFlags flags)
{
    // Flags that require signaling at top-of-pipe.
    static const PipelineStageFlags srcTopOfPipeFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    Pal::HwPipePoint srcPipePoint;

    if ((flags & srcTopOfPipeFlags) != 0)
    {
        srcPipePoint = Pal::HwPipePostPrefetch;
    }
    else
    {
        srcPipePoint = Pal::HwPipeBottom;
    }

    return srcPipePoint;
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL buffer marker writes (top/bottom only)
inline Pal::HwPipePoint VkToPalSrcPipePointForMarkers(
    PipelineStageFlags   flags,
    Pal::EngineType      engineType)
{
    // This function is written against the following three engine types.  If you hit this assert then check if this
    // new engine supports top of pipe writes at all (e.g. SDMA doesn't).
    VK_ASSERT(engineType == Pal::EngineTypeDma ||
              engineType == Pal::EngineTypeUniversal ||
              engineType == Pal::EngineTypeCompute);

    // Flags that allow signaling at top-of-pipe (anything else maps to bottom)
    constexpr PipelineStageFlags SrcTopOfPipeFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    Pal::HwPipePoint srcPipePoint;

    if (((flags & ~SrcTopOfPipeFlags) == 0) &&
        (engineType != Pal::EngineTypeDma)) // SDMA engines only support bottom of pipe writes
    {
        srcPipePoint = Pal::HwPipeTop;
    }
    else
    {
        srcPipePoint = Pal::HwPipeBottom;
    }

    return srcPipePoint;
}

// Helper structure for mapping stage flag sets to PAL pipe points
struct HwPipePointMappingEntry
{
    Pal::HwPipePoint        pipePoint;
    PipelineStageFlags      stateFlags;
};

static const HwPipePointMappingEntry hwPipePointMappingTable[] =
{
    // Flags that require flushing index-fetch workload.
    {
        Pal::HwPipePostPrefetch,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT                |
        VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR            |
        VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT
    },
    // Flags that require flushing pre-rasterization workload.
    {
        Pal::HwPipePreRasterization,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                      |
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR      |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT       |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT    |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                   |
        VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR   |
        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT
    },
    // Flags that require flushing PS workload.
    {
        Pal::HwPipePostPs,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT      |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT           |
        VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR
    },

    // Flags that require flushing all workload.
    {
        Pal::HwPipeBottom,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT               |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT           |
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                    |
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT                      |
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    },

    // Flags that require flushing CS workload.
    {
        Pal::HwPipePostCs,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
#if VKI_RAY_TRACING
        | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
#endif
    },

    // flags that require flush Post-Blt workload.
    {
        Pal::HwPipePostBlt,
        VK_PIPELINE_STAGE_TRANSFER_BIT      |
        VK_PIPELINE_STAGE_2_COPY_BIT_KHR    |
        VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR |
        VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR   |
        VK_PIPELINE_STAGE_2_BLIT_BIT_KHR
    }
};

// The maximum number of pipe points that may be returned by VkToPalSrcPipePoints.
static const size_t MaxHwPipePoints = sizeof(hwPipePointMappingTable) / sizeof(hwPipePointMappingTable[0]);

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to a set of PAL HW pipe points.
// Selects one or more source pipe points that matches all stage flags to use for pipeline barriers.
// By having the flexibility to specify multiple pipe points for barriers we can avoid going with the least common
// denominator like in case of event sets/resets.
// The function returns the number of pipe points set in the return value.
inline uint32_t VkToPalSrcPipePoints(
    PipelineStageFlags flags,
    Pal::HwPipePoint*  pPalPipePoints)
{
    uint32_t pipePointCount = 0;

    // Go through each mapping and add the corresponding pipe point to the array if needed
    for (uint32_t i = 0; i < MaxHwPipePoints; ++i)
    {
        if ((flags & hwPipePointMappingTable[i].stateFlags) != 0)
        {
            pPalPipePoints[pipePointCount++] = hwPipePointMappingTable[i].pipePoint;
        }
    }

    return pipePointCount;
}

// =====================================================================================================================
// Converts Vulkan destination pipeline stage flags to PAL HW pipe point.
// This way a target pipeline stage is selected where the wait for events happens
inline Pal::HwPipePoint VkToPalWaitPipePoint(PipelineStageFlags flags)
{
    static_assert((Pal::HwPipePostPrefetch == Pal::HwPipePreCs) && (Pal::HwPipePostPrefetch == Pal::HwPipePreBlt),
        "The code here assumes pre-CS and pre-blit match post-index-fetch.");

    // Flags that only require waiting bottom-of-pipe.
    static const PipelineStageFlags dstBottomOfPipeFlags =
        VK_PIPELINE_STAGE_HOST_BIT                              |
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    // Flags that only require waiting pre-rasterization.
    static const PipelineStageFlags dstPreRasterizationFlags =
        dstBottomOfPipeFlags                                    |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT              |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                   |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT               |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT           |
        VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    // Flags that only require waiting post-index-fetch.
    static const PipelineStageFlags dstPostIndexFetchFlags =
        dstPreRasterizationFlags                                |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT       |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT    |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                   |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                    |
        VK_PIPELINE_STAGE_2_COPY_BIT_KHR                        |
        VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR                     |
        VK_PIPELINE_STAGE_2_BLIT_BIT_KHR                        |
        VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR                       |
        VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR   |
#if VKI_RAY_TRACING
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR            |
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR  |
#endif
        VK_PIPELINE_STAGE_TRANSFER_BIT                          |
        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

    Pal::HwPipePoint dstPipePoint;

    // Check if bottom-of-pipe waiting is enough.
    if ((flags & ~dstBottomOfPipeFlags) == 0)
    {
        dstPipePoint = Pal::HwPipeBottom;
    }
    // Check if pre-rasterization waiting is enough.
    else if ((flags & ~dstPreRasterizationFlags) == 0)
    {
        dstPipePoint = Pal::HwPipePreRasterization;
    }
    // Otherwise see if post-index-fetch waiting is enough.
    else if ((flags & ~dstPostIndexFetchFlags) == 0)
    {
        dstPipePoint = Pal::HwPipePostPrefetch;
    }
    // Otherwise we have to resort to top-of-pipe waiting.
    else
    {
        dstPipePoint = Pal::HwPipeTop;
    }

    return dstPipePoint;
}

#if VKI_RAY_TRACING
inline uint32_t TraceRayProfileFlagsToRayFlag(
    const RuntimeSettings& settings)
{
    uint32_t profileRayFlags = 0;

    if (settings.rtTraceRayProfileFlags != TraceRayProfileDisable)
    {
        if (settings.rtTraceRayProfileFlags & TraceRayProfileForceOpaque)
        {
            profileRayFlags |= convert::TraceRayProfileRayFlagsToRayTracingRayFlags(TraceRayProfileForceOpaque);
        }
        if (settings.rtTraceRayProfileFlags & TraceRayProfileAcceptFirstHitAndEndSearch)
        {
            profileRayFlags |=
                convert::TraceRayProfileRayFlagsToRayTracingRayFlags(TraceRayProfileAcceptFirstHitAndEndSearch);
        }
        if (settings.rtTraceRayProfileFlags & TraceRayProfileSkipClosestHitShader)
        {
            profileRayFlags |=
                convert::TraceRayProfileRayFlagsToRayTracingRayFlags(TraceRayProfileSkipClosestHitShader);
        }
        if (settings.rtTraceRayProfileFlags & TraceRayProfileCullFrontFacingTriangles)
        {
            profileRayFlags |=
                convert::TraceRayProfileRayFlagsToRayTracingRayFlags(TraceRayProfileCullFrontFacingTriangles);
        }
        if (settings.rtTraceRayProfileFlags & TraceRayProfileCullBackFacingTriangles)
        {
            profileRayFlags |=
                convert::TraceRayProfileRayFlagsToRayTracingRayFlags(TraceRayProfileCullBackFacingTriangles);
        }
    }

    return profileRayFlags;
}

inline uint32_t TraceRayProfileMaxIterationsToMaxIterations(
    const RuntimeSettings& settings)
{
    uint32_t profileMaxIterations = 0xFFFFFFFF;

    if ((settings.rtTraceRayProfileFlags != TraceRayProfileDisable) &&
        (settings.rtTraceRayProfileFlags & TraceRayProfileForceMaxIteration))
    {
        profileMaxIterations = settings.rtProfileMaxIteration;
    }

    return profileMaxIterations;
}
#endif

// =====================================================================================================================
// Converts pipe points to src Pal::PipelineStage
inline uint32_t ConvertPipePointToPipeStage(
    Pal::HwPipePoint pipePoint)
{
    uint32_t stageMask = 0;

    switch (pipePoint)
    {
        case Pal::HwPipeTop:
            stageMask = Pal::PipelineStageTopOfPipe;
            break;
        // Same as Pal::HwPipePreCs and Pal::HwPipePreBlt
        case Pal::HwPipePostPrefetch:
            stageMask = Pal::PipelineStageFetchIndirectArgs;
            break;
        case Pal::HwPipePreRasterization:
            stageMask = Pal::PipelineStageFetchIndices | Pal::PipelineStageVs | Pal::PipelineStageHs |
                        Pal::PipelineStageDs | Pal::PipelineStageGs;
            break;
        case Pal::HwPipePostPs:
            stageMask = Pal::PipelineStagePs;
            break;
        case Pal::HwPipePreColorTarget:
            stageMask = Pal::PipelineStageLateDsTarget;
            break;
        case Pal::HwPipePostCs:
            stageMask = Pal::PipelineStageCs;
            break;
        case Pal::HwPipePostBlt:
            stageMask = Pal::PipelineStageBlt;
            break;
        case Pal::HwPipeBottom:
            stageMask = Pal::PipelineStageBottomOfPipe;
            break;
    }

    return stageMask;
}

// =====================================================================================================================
// Converts wait points to dst Pal::PipelineStage
inline uint32_t ConvertWaitPointToPipeStage(
    Pal::HwPipePoint pipePoint)
{
    uint32_t stageMask = 0;

    switch (pipePoint)
    {
        case Pal::HwPipeTop:
            stageMask = Pal::PipelineStageTopOfPipe;
            break;
        // Same as Pal::HwPipePreCs and Pal::HwPipePreBlt
        case Pal::HwPipePostPrefetch:
            stageMask = Pal::PipelineStageCs | Pal::PipelineStageVs | Pal::PipelineStageBlt;
            break;
        case Pal::HwPipePreRasterization:
            stageMask = Pal::PipelineStageEarlyDsTarget;
            break;
        case Pal::HwPipePostPs:
            stageMask = Pal::PipelineStageLateDsTarget;
            break;
        case Pal::HwPipePreColorTarget:
            stageMask = Pal::PipelineStageColorTarget;
            break;
        case Pal::HwPipePostCs:
        case Pal::HwPipePostBlt:
        case Pal::HwPipeBottom:
            stageMask = Pal::PipelineStageBottomOfPipe;
            break;
    }

    return stageMask;
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL pipeline stage mask.
inline uint32_t VkToPalPipelineStageFlags(
    PipelineStageFlags   stageMask,
    bool                 isSrcStage)
{
    uint32_t palPipelineStageMask = 0;

    if (stageMask & VK_PIPELINE_STAGE_2_HOST_BIT_KHR)
    {
        palPipelineStageMask |= (isSrcStage ? Pal::PipelineStageTopOfPipe : Pal::PipelineStageBottomOfPipe);
    }

    if (stageMask & VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageTopOfPipe;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageFetchIndirectArgs;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR |
                     VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT))
    {
        palPipelineStageMask |= Pal::PipelineStageFetchIndices;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageFetchIndices |
                                Pal::PipelineStageVs;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT)
    {
        palPipelineStageMask |= Pal::PipelineStageStreamOut;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR          |
                     VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR))
    {
        palPipelineStageMask |= Pal::PipelineStageVs;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageHs;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageDs;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT_KHR |
                     VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT))
    {
        palPipelineStageMask |= Pal::PipelineStageGs;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR |
                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR))
    {
        palPipelineStageMask |= Pal::PipelineStageVs |
                                Pal::PipelineStageHs |
                                Pal::PipelineStageDs |
                                Pal::PipelineStageGs;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStagePs;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageEarlyDsTarget;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageLateDsTarget;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageColorTarget;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageTopOfPipe         |
                                Pal::PipelineStageFetchIndirectArgs |
                                Pal::PipelineStageFetchIndices      |
                                Pal::PipelineStageVs                |
                                Pal::PipelineStageHs                |
                                Pal::PipelineStageDs                |
                                Pal::PipelineStageGs                |
                                Pal::PipelineStagePs                |
                                Pal::PipelineStageEarlyDsTarget     |
                                Pal::PipelineStageLateDsTarget      |
                                Pal::PipelineStageColorTarget;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR
                   | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
#if VKI_RAY_TRACING
                   | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR
                   | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
                   | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR
#endif
        ))
    {
        palPipelineStageMask |= Pal::PipelineStageCs;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR |
                     VK_PIPELINE_STAGE_2_COPY_BIT_KHR     |
                     VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR  |
                     VK_PIPELINE_STAGE_2_BLIT_BIT_KHR     |
                     VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR))
    {
        palPipelineStageMask |= Pal::PipelineStageBlt;
    }

    if (stageMask & VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR)
    {
        palPipelineStageMask |= Pal::PipelineStageAllStages;
    }

    if (stageMask & (VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR
                     ))
    {
        palPipelineStageMask |= Pal::PipelineStageBottomOfPipe;
    }

    return palPipelineStageMask;
}

// =====================================================================================================================
PAL_TO_VK_TABLE_X(  ImageTiling, ImageTiling, ImageTiling,
// =====================================================================================================================
PAL_TO_VK_ENTRY_X(  ImageTiling::Linear,                    IMAGE_TILING_LINEAR                                        )
PAL_TO_VK_ENTRY_X(  ImageTiling::Optimal,                   IMAGE_TILING_OPTIMAL                                       )
,
// =====================================================================================================================
PAL_TO_VK_RETURN_X(                                         IMAGE_TILING_LINEAR                                        )
)

inline VkImageTiling PalToVkImageTiling(Pal::ImageTiling tiling)
{
    return convert::PalToVKImageTiling(tiling);
}

// =====================================================================================================================
PAL_TO_VK_TABLE_X(  SurfaceTransformFlags, SurfaceTransform, SurfaceTransformFlagBitsKHR,
// =====================================================================================================================
PAL_TO_VK_ENTRY_X(  SurfaceTransformNone,                   SURFACE_TRANSFORM_IDENTITY_BIT_KHR                         )
PAL_TO_VK_ENTRY_X(  SurfaceTransformRot90,                  SURFACE_TRANSFORM_ROTATE_90_BIT_KHR                        )
PAL_TO_VK_ENTRY_X(  SurfaceTransformRot180,                 SURFACE_TRANSFORM_ROTATE_180_BIT_KHR                       )
PAL_TO_VK_ENTRY_X(  SurfaceTransformRot270,                 SURFACE_TRANSFORM_ROTATE_270_BIT_KHR                       )
PAL_TO_VK_ENTRY_X(  SurfaceTransformHMirror,                SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR                )
PAL_TO_VK_ENTRY_X(  SurfaceTransformHMirrorRot90,           SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR      )
PAL_TO_VK_ENTRY_X(  SurfaceTransformHMirrorRot180,          SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR     )
PAL_TO_VK_ENTRY_X(  SurfaceTransformHMirrorRot270,          SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR     )
PAL_TO_VK_ENTRY_X(  SurfaceTransformInherit,                SURFACE_TRANSFORM_INHERIT_BIT_KHR                          )
,
// =====================================================================================================================
PAL_TO_VK_RETURN_X(                                         SURFACE_TRANSFORM_IDENTITY_BIT_KHR                         )
)

// =====================================================================================================================
// Converts PAL surface transform to Vulkan.
inline VkSurfaceTransformFlagBitsKHR PalToVkSurfaceTransform(Pal::SurfaceTransformFlags transformFlag)
{
    if (transformFlag)
    {
        return convert::PalToVKSurfaceTransformFlagBitsKHR(transformFlag);
    }
    return static_cast<VkSurfaceTransformFlagBitsKHR>(0);
}

// =====================================================================================================================
// Converts Vulkan WSI Platform Type to PAL equivalent.
inline Pal::WsiPlatform VkToPalWsiPlatform(VkIcdWsiPlatform Platform)
{
    Pal::WsiPlatform palPlatform = Pal::WsiPlatform::Win32;

    switch (Platform)
    {
    case VK_ICD_WSI_PLATFORM_XCB:
        palPlatform = Pal::WsiPlatform::Xcb;
        break;
    case VK_ICD_WSI_PLATFORM_XLIB:
        palPlatform = Pal::WsiPlatform::Xlib;
        break;
    case VK_ICD_WSI_PLATFORM_WAYLAND:
        palPlatform = Pal::WsiPlatform::Wayland;
        break;
    case VK_ICD_WSI_PLATFORM_MIR:
        palPlatform = Pal::WsiPlatform::Mir;
        break;
    case VK_ICD_WSI_PLATFORM_DISPLAY:
        palPlatform = Pal::WsiPlatform::DirectDisplay;
        break;
    case VK_ICD_WSI_PLATFORM_WIN32:
    default:
        palPlatform = Pal::WsiPlatform::Win32;
        break;
    }
    return palPlatform;
}

// =====================================================================================================================
// Converts Vulkan Provoking Vertex Mode to PAL equivalent.
inline constexpr Pal::ProvokingVertex VkToPalProvokingVertex(VkProvokingVertexModeEXT provokingVertexMode)
{
    static_assert(static_cast<Pal::ProvokingVertex>(VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT) ==
                  Pal::ProvokingVertex::First,
                  "VK and PAL enums don't match");
    static_assert(static_cast<Pal::ProvokingVertex>(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT) ==
                  Pal::ProvokingVertex::Last,
                  "VK and PAL enums don't match");

    return static_cast<Pal::ProvokingVertex>(provokingVertexMode);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I_WITH_SUFFIX(  PRESENT_MODE, PresentModeKHR,                SwapChainMode,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  PRESENT_MODE_IMMEDIATE_KHR,                  SwapChainMode::Immediate                              )
VK_TO_PAL_ENTRY_I(  PRESENT_MODE_MAILBOX_KHR,                    SwapChainMode::Mailbox                                )
VK_TO_PAL_ENTRY_I(  PRESENT_MODE_FIFO_KHR,                       SwapChainMode::Fifo                                   )
VK_TO_PAL_ENTRY_I(  PRESENT_MODE_FIFO_RELAXED_KHR,               SwapChainMode::FifoRelaxed                            )
// =====================================================================================================================
, _KHR)

// =====================================================================================================================
// Converts Vulkan present mode to PAL equivalent.
inline Pal::SwapChainMode VkToPalSwapChainMode(VkPresentModeKHR presentMode)
{
    return convert::SwapChainMode(presentMode);
}

// =====================================================================================================================
// Converts Vulkan composite alpha flag to PAL equivalent.
inline Pal::CompositeAlphaMode VkToPalCompositeAlphaMode(VkCompositeAlphaFlagBitsKHR compositeAlpha)
{
    return static_cast<Pal::CompositeAlphaMode>(compositeAlpha);
}

// =====================================================================================================================
// Converts Vulkan composite alpha flag to PAL equivalent.
inline VkCompositeAlphaFlagsKHR PalToVkSupportedCompositeAlphaMode(uint32 compositeAlpha)
{
    static_assert((static_cast<uint32>(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ==
                   static_cast<uint32>(Pal::CompositeAlphaMode::Opaque)) &&
                  (static_cast<uint32>(VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ==
                   static_cast<uint32>(Pal::CompositeAlphaMode::PreMultiplied)) &&
                  (static_cast<uint32>(VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ==
                   static_cast<uint32>(Pal::CompositeAlphaMode::PostMultiplied)) &&
                  (static_cast<uint32>(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) ==
                   static_cast<uint32>(Pal::CompositeAlphaMode::Inherit)),
                  "Pal::CompositeAlphaMode is not matched with VkCompositeAlphaFlagBitsKHR.");

    return static_cast<VkCompositeAlphaFlagsKHR>(compositeAlpha);
}

// =====================================================================================================================
// Converts Vulkan image creation flags to PAL image creation flags (unfortunately, PAL doesn't define a dedicated type
// for the image creation flags so we have to return the constructed flag set as a uint32_t)
inline uint32_t VkToPalImageCreateFlags(VkImageCreateFlags imageCreateFlags,
                                        VkFormat           format,
                                        VkImageUsageFlags  imageUsage)
{
    Pal::ImageCreateFlags flags = {};

    flags.cubemap            = (imageCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)        ? 1 : 0;
    flags.prt                = (imageCreateFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)       ? 1 : 0;
    flags.invariant          = (imageCreateFlags & VK_IMAGE_CREATE_ALIAS_BIT)                  ? 1 : 0;
    flags.tmzProtected       = (imageCreateFlags & VK_IMAGE_CREATE_PROTECTED_BIT)              ? 1 : 0;
    flags.view3dAs2dArray    = (imageCreateFlags & VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT) ? 1 : 0;

    // Always provide pQuadSamplePattern to PalCmdResolveImage for depth formats to allow optimizations
    flags.sampleLocsAlwaysKnown = Formats::HasDepth(format) ? 1 : 0;

    // Ignore Flag VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT. It is supported by default for all 3D images

    return flags.u32All;
}

// =====================================================================================================================
// Converts  PAL image creation flags to Vulkan image creation flags.
inline VkImageCreateFlags PalToVkImageCreateFlags(Pal::ImageCreateFlags imageCreateFlags)
{
    VkImageUsageFlags vkImageCreateFlags = 0;

    if (imageCreateFlags.cubemap == 1)
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    if (imageCreateFlags.prt == 1)
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    }

    if (imageCreateFlags.invariant == 1)
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_ALIAS_BIT;
    }

    if (imageCreateFlags.tmzProtected  == 1)
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_PROTECTED_BIT;
    }

    if (imageCreateFlags.view3dAs2dArray == 1)
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT;
    }

    return vkImageCreateFlags;
}

// =====================================================================================================================
// Converts Vulkan image usage flags to PAL image usage flags
inline Pal::ImageUsageFlags VkToPalImageUsageFlags(VkImageUsageFlags imageUsageFlags,
                                                      uint32_t          samples,
                                                      VkImageUsageFlags maskSetShaderReadForTransferSrc,
                                                      VkImageUsageFlags maskSetShaderWriteForTransferDst)
{
    Pal::ImageUsageFlags palImageUsageFlags;

    palImageUsageFlags.u32All       = 0;

    palImageUsageFlags.shaderRead   = ((imageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) ||
                                       (imageUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) ||
                                       ((imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
                                        (maskSetShaderReadForTransferSrc & imageUsageFlags)) ||
                                       (imageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))       ? 1 : 0;
    palImageUsageFlags.shaderWrite  = ((imageUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) ||
                                       ((imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) &&
                                        (maskSetShaderWriteForTransferDst & imageUsageFlags)))        ? 1 : 0;

    // Vulkan client driver can set resolveSrc usage flag bit  when msaa image setting Transfer_Src bit. Pal will use
    // resolveSrc and shaderRead flag as well as other conditions to decide whether msaa surface and fmask is tc-compatible.
    palImageUsageFlags.resolveSrc = ((samples > 1) && (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT));

    palImageUsageFlags.resolveDst   = ((samples == 1) && (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    palImageUsageFlags.colorTarget  = (imageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)         ? 1 : 0;
    palImageUsageFlags.depthStencil = (imageUsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? 1 : 0;

    return palImageUsageFlags;
}

// =====================================================================================================================
// Converts PAL image usage flag to Vulkan.
inline VkImageUsageFlags PalToVkImageUsageFlags(Pal::ImageUsageFlags imageUsageFlags)
{
    VkImageUsageFlags vkImageUsageFlags = 0;

    if (imageUsageFlags.colorTarget == 1)
    {
        vkImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (imageUsageFlags.depthStencil == 1)
    {
        vkImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (imageUsageFlags.shaderWrite == 1)
    {
        vkImageUsageFlags |= (VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_STORAGE_BIT);
    }

    if (imageUsageFlags.shaderRead == 1)
    {
        vkImageUsageFlags |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    }

    return vkImageUsageFlags;
}

extern VkResult PalToVkError(Pal::Result result);

// =====================================================================================================================
// Converts a PAL result to an equivalent VK result.
inline VkResult PalToVkResult(
    Pal::Result result)
{
    VkResult vkResult = VK_SUCCESS;

    // This switch statement handles the non-error Vulkan return codes directly; the error Vulkan return codes are
    // handled separately by the call to PalToVkError in the default case.
    switch (result)
    {
    case Pal::Result::Success:
    // These PAL error codes currently aren't handled specially and they indicate success otherwise
    case Pal::Result::TooManyFlippableAllocations:
    case Pal::Result::PresentOccluded:
        vkResult = VK_SUCCESS;
        break;

    case Pal::Result::NotReady:
        vkResult = VK_NOT_READY;
        break;

    case Pal::Result::Timeout:
    case Pal::Result::ErrorFenceNeverSubmitted:
        vkResult = VK_TIMEOUT;
        break;

    case Pal::Result::EventSet:
        vkResult = VK_EVENT_SET;
        break;

    case Pal::Result::EventReset:
        vkResult = VK_EVENT_RESET;
        break;

    default:
        vkResult = PalToVkError(result);
        break;
    }

    return vkResult;
}

// =====================================================================================================================
VK_TO_PAL_TABLE_X(PIPELINE_BIND_POINT, PipelineBindPoint,   PipelineBindPoint,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(PIPELINE_BIND_POINT_COMPUTE,              PipelineBindPoint::Compute                                 )
VK_TO_PAL_ENTRY_X(PIPELINE_BIND_POINT_GRAPHICS,             PipelineBindPoint::Graphics                                )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan pipeline bind point to PAL equivalent
inline Pal::PipelineBindPoint VkToPalPipelineBindPoint(VkPipelineBindPoint pipelineBind)
{
    return convert::PipelineBindPoint(pipelineBind);
}

// =====================================================================================================================
inline Pal::ShaderType VkToPalShaderType(
    VkShaderStageFlagBits shaderStage)
{
    switch (shaderStage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return Pal::ShaderType::Vertex;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return Pal::ShaderType::Hull;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return Pal::ShaderType::Domain;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return Pal::ShaderType::Geometry;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return Pal::ShaderType::Pixel;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return Pal::ShaderType::Compute;
    default:
        VK_NEVER_CALLED();
        return Pal::ShaderType::Compute;
    }
}

// =====================================================================================================================
// Converts Vulkan clear depth to PAL equivalent valid range
inline float VkToPalClearDepth(float depth)
{
    if (Util::Math::IsNaN(depth))
    {
        depth = 1.0f;
    }

    return depth;
}

// =====================================================================================================================
// Converts Vulkan clear color value to PAL equivalent
inline Pal::ClearColor VkToPalClearColor(
    const VkClearColorValue&   clearColor,
    const Pal::SwizzledFormat& swizzledFormat)
{
    Pal::ClearColor palClearColor = { };

    const auto& formatInfo = Pal::Formats::FormatInfoTable[static_cast<size_t>(swizzledFormat.format)];

    switch (formatInfo.numericSupport)
    {
    case Pal::Formats::NumericSupportFlags::Float:
    case Pal::Formats::NumericSupportFlags::Unorm:
    case Pal::Formats::NumericSupportFlags::Snorm:
    case Pal::Formats::NumericSupportFlags::Uscaled:
    case Pal::Formats::NumericSupportFlags::Sscaled:
    case Pal::Formats::NumericSupportFlags::Srgb:
        // Perform the conversion to UINT ourselves because PAL always implicitly performs float conversions to UINT
        // based on the image format. For mutable images, this may not match the view format used here.
        palClearColor.type = Pal::ClearColorType::Uint;
        Pal::Formats::ConvertColor(swizzledFormat, &clearColor.float32[0], &palClearColor.u32Color[0]);
        break;
    case Pal::Formats::NumericSupportFlags::Sint:
        palClearColor.type        = Pal::ClearColorType::Sint;
        palClearColor.u32Color[0] = clearColor.uint32[0];
        palClearColor.u32Color[1] = clearColor.uint32[1];
        palClearColor.u32Color[2] = clearColor.uint32[2];
        palClearColor.u32Color[3] = clearColor.uint32[3];
        break;
    default:
        palClearColor.type        = Pal::ClearColorType::Uint;
        palClearColor.u32Color[0] = clearColor.uint32[0];
        palClearColor.u32Color[1] = clearColor.uint32[1];
        palClearColor.u32Color[2] = clearColor.uint32[2];
        palClearColor.u32Color[3] = clearColor.uint32[3];
        break;
    }

    return palClearColor;
}

// =====================================================================================================================
// Converts integer nanoseconds to single precision seconds
inline float NanosecToSec(uint64_t nanosecs)
{
    return static_cast<float>(static_cast<double>(nanosecs) / 1000000000.0);
}

// =====================================================================================================================
// Converts maximum sample count to VkSampleCountFlags
inline VkSampleCountFlags MaxSampleCountToSampleCountFlags(uint32_t maxSampleCount)
{
    return (maxSampleCount << 1) - 1;
}

// Constant for the number of memory heap supported by our Vulkan implementation, two pal Gart memory heap is mapped one
// vulkan mmeory heap.
constexpr uint32_t VkMemoryHeapNum = Pal::GpuHeapCount - 1;

// =====================================================================================================================
// Converts PAL GPU heap to Vulkan memory heap flags
inline VkMemoryHeapFlags PalGpuHeapToVkMemoryHeapFlags(Pal::GpuHeap heap)
{
    switch (heap)
    {
    case Pal::GpuHeapLocal:
    case Pal::GpuHeapInvisible:
        return (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT | VK_MEMORY_HEAP_MULTI_INSTANCE_BIT);
    case Pal::GpuHeapGartUswc:
    case Pal::GpuHeapGartCacheable:
        return 0;
    default:
        VK_ASSERT(!"Unexpected PAL GPU heap");
        return 0;
    }
}

// =====================================================================================================================
// Returns the Vulkan format feature flags corresponding to the given PAL format feature flags.
inline VkFormatFeatureFlags PalToVkFormatFeatureFlags(Pal::FormatFeatureFlags flags)
{
    VkFormatFeatureFlags retFlags = 0;

    if (flags & Pal::FormatFeatureFormatConversion)
    {
        retFlags |= VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    }

    if (flags & Pal::FormatFeatureFormatConversionSrc)
    {
        retFlags |= VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    }

    if (flags & Pal::FormatFeatureFormatConversionDst)
    {
        retFlags |= VK_FORMAT_FEATURE_BLIT_DST_BIT;
    }

    if (flags & Pal::FormatFeatureCopy)
    {
        retFlags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    }

    if (flags & Pal::FormatFeatureImageShaderRead)
    {
        retFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

        if (flags & Pal::FormatFeatureImageFilterMinMax)
        {
            retFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT;
        }
    }

    if (flags & Pal::FormatFeatureImageShaderWrite)
    {
        retFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

        if (flags & Pal::FormatFeatureImageShaderAtomics)
        {
            retFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;
        }
    }

    if (flags & Pal::FormatFeatureMemoryShaderRead)
    {
        retFlags |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
    }

    if (flags & Pal::FormatFeatureMemoryShaderWrite)
    {
        retFlags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
    }

    if (flags & Pal::FormatFeatureMemoryShaderAtomics)
    {
        retFlags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;
    }

    if (flags & Pal::FormatFeatureColorTargetWrite)
    {
        retFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    }

    if (flags & Pal::FormatFeatureColorTargetBlend)
    {
        retFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (flags & Pal::FormatFeatureDepthTarget)
    {
        retFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (flags & Pal::FormatFeatureStencilTarget)
    {
        retFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (flags & Pal::FormatFeatureImageFilterLinear)
    {
        retFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    }

    return retFlags;
}

// =====================================================================================================================
// Converts Vulkan rasterization order to PAL equivalent (out-of-order primitive enable)
inline bool VkToPalRasterizationOrder(VkRasterizationOrderAMD order)
{
    VK_ASSERT(VK_ENUM_IN_RANGE_AMD(order, VK_RASTERIZATION_ORDER));

    return (order == VK_RASTERIZATION_ORDER_RELAXED_AMD);
}

// =====================================================================================================================
inline Pal::GpuBlock VkToPalGpuBlock(
    VkGpaPerfBlockAMD perfBlock)
{
    static_assert(
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CPF_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Cpf)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_IA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ia)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_VGT_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Vgt)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_PA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Pa)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Sc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SPI_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Spi)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SQ_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Sq)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SX_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Sx)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ta)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TD_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Td)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TCP_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Tcp)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TCC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Tcc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TCA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Tca)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_DB_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Db)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CB_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Cb)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GDS_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gds)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SRBM_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Srbm)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GRBM_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Grbm)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GRBM_SE_AMD) == static_cast<uint32_t>(Pal::GpuBlock::GrbmSe)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_RLC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Rlc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_DMA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Dma)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_MC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Mc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CPG_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Cpg)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CPC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Cpc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_WD_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Wd)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_TCS_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Tcs)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_ATC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Atc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_ATC_L2_AMD) == static_cast<uint32_t>(Pal:: GpuBlock::AtcL2)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_MC_VM_L2_AMD) == static_cast<uint32_t>(Pal::GpuBlock::McVmL2)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_EA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ea)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_RPB_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Rpb)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_RMI_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Rmi)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_UMCCH_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Umcch)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GE_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ge)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GL1A_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gl1a)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GL1C_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gl1c)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GL1CG_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gl1cg)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GL2A_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gl2a)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GL2C_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gl2c)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CHA_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Cha)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CHC_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Chc)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_CHCG_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Chcg)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GUS_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gus)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GCR_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Gcr)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_PH_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ph)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_UTCL1_AMD) == static_cast<uint32_t>(Pal::GpuBlock::UtcL1)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GE1_AMD) == static_cast<uint32_t>(Pal::GpuBlock::Ge1)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GE_DIST_AMD) == static_cast<uint32_t>(Pal::GpuBlock::GeDist)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_GE_SE_AMD) == static_cast<uint32_t>(Pal::GpuBlock::GeSe)) &&
    (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_DF_MALL_AMD) == static_cast<uint32_t>(Pal::GpuBlock::DfMall))
#if VKI_BUILD_GFX11
    && (static_cast<uint32_t>(VK_GPA_PERF_BLOCK_SQ_WGP_AMD) == static_cast<uint32_t>(Pal::GpuBlock::SqWgp))
#endif
    ,
    "Need to update function convert::GpuBlock");

    return static_cast<Pal::GpuBlock>(perfBlock);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I_AMD(GPA_DEVICE_CLOCK_MODE, GpaDeviceClockModeAMD, DeviceClockMode,
    // =================================================================================================================
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_DEFAULT_AMD,    DeviceClockMode::Default)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_QUERY_AMD,      DeviceClockMode::Query)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_PROFILING_AMD,  DeviceClockMode::Profiling)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_MIN_MEMORY_AMD, DeviceClockMode::MinimumMemory)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_MIN_ENGINE_AMD, DeviceClockMode::MinimumEngine)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_PEAK_AMD,       DeviceClockMode::Peak)
    // =================================================================================================================
)

// =====================================================================================================================
inline Pal::DeviceClockMode VkToPalDeviceClockMode(
    VkGpaDeviceClockModeAMD clockMode)
{
    return convert::DeviceClockMode(clockMode);
}

// =====================================================================================================================
inline uint32_t VkToPalPerfExperimentShaderFlags(
    VkGpaSqShaderStageFlags stageMask)
{
    uint32_t perfFlags = 0;

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_PS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskPs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_VS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskVs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_GS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskGs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_ES_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskEs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_HS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskHs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_LS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskLs;
    }

    if ((stageMask & VK_GPA_SQ_SHADER_STAGE_CS_BIT_AMD) != 0)
    {
        perfFlags |= Pal::PerfShaderMaskCs;
    }

    return perfFlags;
}

// =====================================================================================================================
template <typename PalClearRegion>
PalClearRegion VkToPalClearRegion(const VkClearRect& clearRect, const uint32_t zOffset);

// =====================================================================================================================
// Converts Vulkan clear rect to an equivalent PAL box
template <>
inline Pal::Box VkToPalClearRegion<Pal::Box>(
    const VkClearRect& clearRect,
    const uint32_t     zOffset)
{
    Pal::Box box { };

    box.offset.x      = clearRect.rect.offset.x;
    box.offset.y      = clearRect.rect.offset.y;
    box.offset.z      = clearRect.baseArrayLayer + zOffset;
    box.extent.width  = clearRect.rect.extent.width;
    box.extent.height = clearRect.rect.extent.height;
    box.extent.depth  = clearRect.layerCount;

    return box;
}

// =====================================================================================================================
// Converts Vulkan clear rect to an equivalent PAL clear bound target region
template <>
inline Pal::ClearBoundTargetRegion VkToPalClearRegion<Pal::ClearBoundTargetRegion>(
    const VkClearRect& clearRect,
    const uint32_t     zOffset)
{
    Pal::ClearBoundTargetRegion clearRegion { };

    clearRegion.rect.offset.x      = clearRect.rect.offset.x;
    clearRegion.rect.offset.y      = clearRect.rect.offset.y;
    clearRegion.rect.extent.width  = clearRect.rect.extent.width;
    clearRegion.rect.extent.height = clearRect.rect.extent.height;
    clearRegion.startSlice         = clearRect.baseArrayLayer + zOffset;
    clearRegion.numSlices          = clearRect.layerCount;

    return clearRegion;
}

// =====================================================================================================================
// Overrides range of layers in PAL clear region
inline void OverrideLayerRanges(
    Pal::ClearBoundTargetRegion& clearRegion,
    const Pal::Range             layerRange)
{
    VK_ASSERT(clearRegion.numSlices  == 1);

    clearRegion.startSlice += layerRange.offset;
    clearRegion.numSlices   = layerRange.extent;
}

// =====================================================================================================================
// Overrides range of layers in PAL box
inline void OverrideLayerRanges(
    Pal::Box&        box,
    const Pal::Range layerRange)
{
    VK_ASSERT(box.extent.depth == 1);

    box.offset.z     += layerRange.offset;
    box.extent.depth  = layerRange.extent;
}

// =====================================================================================================================
// Converts Vulkan rect 2D to an equivalent PAL rect
inline Pal::Rect VkToPalRect(
    const VkRect2D& rect2D)
{
    Pal::Rect rect { };

    rect.offset.x      = rect2D.offset.x;
    rect.offset.y      = rect2D.offset.y;
    rect.extent.width  = rect2D.extent.width;
    rect.extent.height = rect2D.extent.height;

    return rect;
}

// =====================================================================================================================
inline void VkToPalViewport(
    const VkViewport&    viewport,
    uint32_t             viewportIdx,
    bool                 khrMaintenance1,
    Pal::ViewportParams* pParams)
{
    auto* pViewport = &pParams->viewports[viewportIdx];

    pViewport->originX  = viewport.x;
    pViewport->originY  = viewport.y;
    pViewport->width    = viewport.width;
    pViewport->minDepth = viewport.minDepth;
    pViewport->maxDepth = viewport.maxDepth;

    if (viewport.height >= 0.0f)
    {
        pViewport->height = viewport.height;
        pViewport->origin = Pal::PointOrigin::UpperLeft;
    }
    else
    {
        if (khrMaintenance1)
        {
            pViewport->originY = viewport.y + viewport.height;
        }

        pViewport->height = -viewport.height;
        pViewport->origin = Pal::PointOrigin::LowerLeft;
    }
}

// =====================================================================================================================
inline VkImageUsageFlags VkFormatFeatureFlagsToImageUsageFlags(
        VkFormatFeatureFlags formatFeatures)
{
    VkImageUsageFlags imageUsage = 0;

    if (formatFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (formatFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        imageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    if (formatFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (formatFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (formatFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    return imageUsage;
}

// =====================================================================================================================
inline void VkToPalScissorRect(
    const VkRect2D&         scissorRect,
    uint32_t                scissorIdx,
    Pal::ScissorRectParams* pParams)
{
    auto* pRect = &pParams->scissors[scissorIdx];

    pRect->offset.x      = scissorRect.offset.x;
    pRect->offset.y      = scissorRect.offset.y;
    pRect->extent.width  = scissorRect.extent.width;
    pRect->extent.height = scissorRect.extent.height;
}

// =====================================================================================================================
template<class T>
inline Pal::QueuePriority VkToPalGlobalPriority(
    VkQueueGlobalPriorityKHR vkPriority,
    const T&                 engineCapabilities)
{
    const bool idleSupported     = ((engineCapabilities.queuePrioritySupport &
                                    Pal::QueuePrioritySupport::SupportQueuePriorityIdle) != 0);
    const bool normalSupported   = ((engineCapabilities.queuePrioritySupport &
                                    Pal::QueuePrioritySupport::SupportQueuePriorityNormal) != 0);
    const bool mediumSupported   = ((engineCapabilities.queuePrioritySupport &
                                    Pal::QueuePrioritySupport::SupportQueuePriorityMedium) != 0);
    const bool highSupported     = ((engineCapabilities.queuePrioritySupport &
                                    Pal::QueuePrioritySupport::SupportQueuePriorityHigh) != 0);
    const bool realtimeSupported = ((engineCapabilities.queuePrioritySupport &
                                    Pal::QueuePrioritySupport::SupportQueuePriorityRealtime) != 0);

    VK_ASSERT(idleSupported || normalSupported || mediumSupported || highSupported || realtimeSupported);

    Pal::QueuePriority palPriority = Pal::QueuePriority::Normal;
    switch (static_cast<int32_t>(vkPriority))
    {
    case VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR:
        if (idleSupported)
        {
            palPriority = Pal::QueuePriority::Idle;
        }
        else if (normalSupported)
        {
            palPriority = Pal::QueuePriority::Normal;
        }
        else if (mediumSupported)
        {
            palPriority = Pal::QueuePriority::Medium;
        }
        else if (highSupported)
        {
            palPriority = Pal::QueuePriority::High;
        }
        else if (realtimeSupported)
        {
            palPriority = Pal::QueuePriority::Realtime;
        }
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR:
        if (highSupported)
        {
            palPriority = Pal::QueuePriority::High;
        }
        else if (mediumSupported)
        {
            palPriority = Pal::QueuePriority::Medium;
        }
        else if (normalSupported)
        {
            palPriority = Pal::QueuePriority::Normal;
        }
        else if (idleSupported)
        {
            palPriority = Pal::QueuePriority::Idle;
        }
        else if (realtimeSupported)
        {
            palPriority = Pal::QueuePriority::Realtime;
        }
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR:
        if (realtimeSupported)
        {
            palPriority = Pal::QueuePriority::Realtime;
        }
        else if (highSupported)
        {
            palPriority = Pal::QueuePriority::High;
        }
        else if (mediumSupported)
        {
            palPriority = Pal::QueuePriority::Medium;
        }
        else if (normalSupported)
        {
            palPriority = Pal::QueuePriority::Normal;
        }
        else if (idleSupported)
        {
            palPriority = Pal::QueuePriority::Idle;
        }
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR:
    default:
        if (normalSupported)
        {
            palPriority = Pal::QueuePriority::Normal;
        }
        else if (mediumSupported)
        {
            palPriority = Pal::QueuePriority::Medium;
        }
        else if (highSupported)
        {
            palPriority = Pal::QueuePriority::High;
        }
        else if (idleSupported)
        {
            palPriority = Pal::QueuePriority::Idle;
        }
        else if (realtimeSupported)
        {
            palPriority = Pal::QueuePriority::Realtime;
        }
        break;
    }

    return palPriority;
}

// =====================================================================================================================
inline Pal::QueuePrioritySupport VkToPalGlobaPrioritySupport(
    VkQueueGlobalPriorityKHR vkPriority)
{
    Pal::QueuePrioritySupport palPrioritySupport = Pal::QueuePrioritySupport::SupportQueuePriorityNormal;
    switch (static_cast<int32_t>(vkPriority))
    {
    case VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR:
        palPrioritySupport = Pal::QueuePrioritySupport::SupportQueuePriorityIdle;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR:
        palPrioritySupport = Pal::QueuePrioritySupport::SupportQueuePriorityNormal;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR:
        palPrioritySupport = Pal::QueuePrioritySupport::SupportQueuePriorityHigh;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR:
        palPrioritySupport = Pal::QueuePrioritySupport::SupportQueuePriorityRealtime;
        break;
    default:
        break;
    }

    return palPrioritySupport;
}

// =====================================================================================================================
// Is the queue suitable for normal use (i.e. non-exclusive and no elevated priority).
template<class T>
static bool IsNormalQueue(const T& engineCapabilities)
{
    return ((engineCapabilities.flags.exclusive == 0) &&
        ((engineCapabilities.queuePrioritySupport & Pal::QueuePrioritySupport::SupportQueuePriorityNormal) != 0));
}

inline Pal::ResolveMode VkToPalResolveMode(
    VkResolveModeFlagBits vkResolveMode)
{
    switch (vkResolveMode)
    {
    case VK_RESOLVE_MODE_MIN_BIT:
        return Pal::ResolveMode::Minimum;
    case VK_RESOLVE_MODE_MAX_BIT:
        return Pal::ResolveMode::Maximum;
    case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT:
        return Pal::ResolveMode::Average;
    case VK_RESOLVE_MODE_AVERAGE_BIT:
        return Pal::ResolveMode::Average;
    case VK_RESOLVE_MODE_NONE:
    default:
        VK_NEVER_CALLED();
        return Pal::ResolveMode::Average;
    }
}

// =====================================================================================================================
// VFS extension states implementations have to accept anything in the range{ 1,2,4 } ^ 2.
// Clamp fragment to maxFragmentSize after each combiner operation.
inline VkExtent2D VkClampShadingRate(
    const VkExtent2D& fragmentSize,
    const VkExtent2D& maxSupportedFragmentSize)
{
    VkExtent2D extent2D = {
        Util::Min(fragmentSize.width,  maxSupportedFragmentSize.width),
        Util::Min(fragmentSize.height, maxSupportedFragmentSize.height)
    };

    return extent2D;
}

// =====================================================================================================================
inline Pal::VrsShadingRate VkToPalShadingSize(
    const VkExtent2D& fragmentSize)
{
    Pal::VrsShadingRate vrsShadingRate = Pal::VrsShadingRate::_1x1;

    if ((fragmentSize.width == 1) && (fragmentSize.height == 1))
    {
        vrsShadingRate = Pal::VrsShadingRate::_1x1;
    }
    else if ((fragmentSize.width == 2) && (fragmentSize.height == 1))
    {
        vrsShadingRate = Pal::VrsShadingRate::_2x1;
    }
    else if ((fragmentSize.width == 1) && (fragmentSize.height == 2))
    {
        vrsShadingRate = Pal::VrsShadingRate::_1x2;
    }
    else if ((fragmentSize.width == 2) && (fragmentSize.height == 2))
    {
        vrsShadingRate = Pal::VrsShadingRate::_2x2;
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return vrsShadingRate;
}

// =====================================================================================================================
inline VkExtent2D PalToVkShadingSize(
    const Pal::VrsShadingRate& vrsShadingRate)
{
    VkExtent2D fragmentSize = {0,0};

    switch (vrsShadingRate)
    {
    case Pal::VrsShadingRate::_1x1:
        fragmentSize = {1, 1};
        break;
    case Pal::VrsShadingRate::_1x2:
        fragmentSize = {1, 2};
        break;
    case Pal::VrsShadingRate::_2x1:
        fragmentSize = {2, 1};
        break;
    case Pal::VrsShadingRate::_2x2:
        fragmentSize = {2, 2};
        break;
    case Pal::VrsShadingRate::_16xSsaa:
    case Pal::VrsShadingRate::_8xSsaa:
    case Pal::VrsShadingRate::_4xSsaa:
    case Pal::VrsShadingRate::_2xSsaa:
        // Unsupported by VK_KHR_fragment_shading_rate extension
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return fragmentSize;
}

// =====================================================================================================================
inline Pal::VrsCombiner VkToPalShadingRateCombinerOp(
    VkFragmentShadingRateCombinerOpKHR fragmentShadingRateCombinerOp)
{
    Pal::VrsCombiner vrsCombiner = Pal::VrsCombiner::Passthrough;

    switch (fragmentShadingRateCombinerOp)
    {
    case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR:
        vrsCombiner = Pal::VrsCombiner::Passthrough;
        break;
    case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR:
        vrsCombiner = Pal::VrsCombiner::Override;
        break;
    case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR:
        vrsCombiner = Pal::VrsCombiner::Min;
        break;
    case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR:
        vrsCombiner = Pal::VrsCombiner::Max;
        break;
    case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR:
        vrsCombiner = Pal::VrsCombiner::Sum;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    return vrsCombiner;
}

// =====================================================================================================================
inline Pal::ResourceDescriptionDescriptorType VkToPalDescriptorType(
    VkDescriptorType vkType)
{
    Pal::ResourceDescriptionDescriptorType retType = Pal::ResourceDescriptionDescriptorType::Count;
    switch (vkType)
    {
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER:
            retType = Pal::ResourceDescriptionDescriptorType::Sampler;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            retType = Pal::ResourceDescriptionDescriptorType::CombinedImageSampler; break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            retType = Pal::ResourceDescriptionDescriptorType::SampledImage;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            retType = Pal::ResourceDescriptionDescriptorType::StorageImage;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            retType = Pal::ResourceDescriptionDescriptorType::UniformTexelBuffer;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            retType = Pal::ResourceDescriptionDescriptorType::StorageTexelBuffer;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            retType = Pal::ResourceDescriptionDescriptorType::UniformBuffer;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            retType = Pal::ResourceDescriptionDescriptorType::StorageBuffer;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            retType = Pal::ResourceDescriptionDescriptorType::UniformBufferDynamic;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            retType = Pal::ResourceDescriptionDescriptorType::StorageBufferDynamic;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            retType = Pal::ResourceDescriptionDescriptorType::InputAttachment;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            retType = Pal::ResourceDescriptionDescriptorType::InlineUniformBlock;
            break;
        case VkDescriptorType::VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            retType = Pal::ResourceDescriptionDescriptorType::AccelerationStructure;
            break;
        default:
            retType = Pal::ResourceDescriptionDescriptorType::Count;
            break;
    }
    return retType;
}

// =====================================================================================================================
inline DescriptorBindingFlags VkToInternalDescriptorBindingFlag(
        VkDescriptorBindingFlags vkFlagBits)
{
    DescriptorBindingFlags internalFlagBits = {};

    if (vkFlagBits & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
    {
        internalFlagBits.variableDescriptorCount = 1;
    }

    return internalFlagBits;
}

// =====================================================================================================================
inline uint32_t VkToVkgcShaderStageMask(VkShaderStageFlags vkShaderStageFlags)
{
    uint32_t vkgcShaderMask = 0;
    uint32_t expectedShaderStageCount = 6;

    if ((vkShaderStageFlags & VK_SHADER_STAGE_VERTEX_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageVertexBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageTessControlBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageTessEvalBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_GEOMETRY_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageGeometryBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageFragmentBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_COMPUTE_BIT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageComputeBit;
    }

    expectedShaderStageCount += 2;

    if ((vkShaderStageFlags & VK_SHADER_STAGE_TASK_BIT_EXT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageTaskBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_MESH_BIT_EXT) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageMeshBit;
    }

#if VKI_RAY_TRACING
    expectedShaderStageCount += 6;

    if ((vkShaderStageFlags & VK_SHADER_STAGE_RAYGEN_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingRayGenBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_ANY_HIT_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingAnyHitBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingClosestHitBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_MISS_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingMissBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_INTERSECTION_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingIntersectBit;
    }

    if ((vkShaderStageFlags & VK_SHADER_STAGE_CALLABLE_BIT_KHR) != 0)
    {
        vkgcShaderMask |= Vkgc::ShaderStageRayTracingCallableBit;
    }
#endif

    VK_ASSERT(expectedShaderStageCount == Vkgc::ShaderStageCount); // Need update this function if mismatch
    return vkgcShaderMask;
}

// =====================================================================================================================
inline VkShaderStageFlags VkgcToVkShaderStageMask(uint32_t vkgcShaderStageFlags)
{
    VkShaderStageFlags vkShaderMask   = 0;
    uint32_t expectedShaderStageCount = 6;

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageVertexBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_VERTEX_BIT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageTessControlBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageTessEvalBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageGeometryBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageFragmentBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageComputeBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    expectedShaderStageCount += 2;

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageTaskBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageMeshBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }

#if VKI_RAY_TRACING
    expectedShaderStageCount += 6;

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingRayGenBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingAnyHitBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingClosestHitBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingMissBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_MISS_BIT_KHR;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingIntersectBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    }

    if ((vkgcShaderStageFlags & Vkgc::ShaderStageRayTracingCallableBit) != 0)
    {
        vkShaderMask |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    }
#endif

    VK_ASSERT(expectedShaderStageCount == Vkgc::ShaderStageCount); // Need update this function if mismatch
    return vkShaderMask;
}

// =====================================================================================================================
struct UberFetchShaderFormatInfo
{
    Pal::SwizzledFormat swizzledFormat;
    Pal::SwizzledFormat unpackedFormat;
    uint32_t            bufferFormat;
    uint32_t            unpackedBufferFormat;
    union
    {
        struct
        {
            uint32_t            isPacked          : 1;
            uint32_t            isFixed           : 1;
            uint32_t            componentCount    : 4;
            uint32_t            componentSize     : 4;
            uint32_t            alignment         : 4;
            uint32_t            reserved          : 18;
        };
        uint32_t u32All;
    };
};

// =====================================================================================================================
class UberFetchShaderFormatInfoMap :
    public Util::HashMap<VkFormat, UberFetchShaderFormatInfo, PalAllocator, Util::JenkinsHashFunc,
        Util::DefaultEqualFunc, Util::HashAllocator<PalAllocator>, 1024>
{
public:
    explicit UberFetchShaderFormatInfoMap(uint32 numBuckets, PalAllocator* const pAllocator)
        :
        Util::HashMap<VkFormat, UberFetchShaderFormatInfo, PalAllocator, Util::JenkinsHashFunc, Util::DefaultEqualFunc,
            Util::HashAllocator<PalAllocator>, 1024>(numBuckets, pAllocator),
        m_bufferFormatMask(0)
    { }

    void SetBufferFormatMask(uint32_t mask) { m_bufferFormatMask = mask; }

    uint32_t GetBufferFormatMask() const { return m_bufferFormatMask; }

private:
    uint32_t m_bufferFormatMask;
};

class PhysicalDevice;

// =====================================================================================================================
VkResult InitializeUberFetchShaderFormatTable(
    const PhysicalDevice*         pPhysicalDevice,
    UberFetchShaderFormatInfoMap* pFormatInfoMap);

UberFetchShaderFormatInfo GetUberFetchShaderFormatInfo(
    const UberFetchShaderFormatInfoMap* pFormatInfoMap,
    const VkFormat                      vkFormat,
    const bool                          isZeroStride);

} // namespace vk

#endif /* __VK_CONV_H__ */
