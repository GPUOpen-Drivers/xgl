/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/khronos/vk_icd.h"
#include "settings/g_settings.h"

#include "pal.h"
#include "palColorBlendState.h"
#include "palCmdBuffer.h"
#include "palDepthStencilState.h"
#include "palDevice.h"
#include "palMath.h"
#include "palImage.h"
#include "palPipeline.h"
#include "palQueryPool.h"
#include "palScreen.h"
#include "palSwapChain.h"

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

static_assert((MaxRangePerAttachment == MaxPalDepthAspectsPerMask),
              "API's max depth/stencil ranges per attachment and PAL max depth aspects must match");

VK_INLINE Pal::SwizzledFormat VkToPalFormat(VkFormat format);

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
        VK_INLINE dstType convertFunc(Vk##srcTypeName value) \
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
        VK_INLINE Pal::dstType dstType(Vk##srcTypeName value) \
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
        VK_INLINE Vk##dstType PalToVK##dstType(Pal::srcType value) \
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

// Helper structure for mapping Vulkan primitive topology to PAL primitive type + adjacency
struct PalPrimTypeAdjacency
{
    PalPrimTypeAdjacency()
        { }

    PalPrimTypeAdjacency(
        Pal::PrimitiveType primType,
        bool               adjacency) :
        primType(primType),
        adjacency(adjacency)
        { }

    Pal::PrimitiveType primType;
    bool               adjacency;
};

VK_TO_PAL_TABLE_COMPLEX(PRIMITIVE_TOPOLOGY, PrimitiveTopology, PalPrimTypeAdjacency, PrimTypeAdjacency,
// =====================================================================================================================
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_POINT_LIST,                    PalPrimTypeAdjacency(Pal::PrimitiveType::Point,    false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_LINE_LIST,                     PalPrimTypeAdjacency(Pal::PrimitiveType::Line,     false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_LINE_STRIP,                    PalPrimTypeAdjacency(Pal::PrimitiveType::Line,     false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                 PalPrimTypeAdjacency(Pal::PrimitiveType::Triangle, false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                PalPrimTypeAdjacency(Pal::PrimitiveType::Triangle, false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,                  PalPrimTypeAdjacency(Pal::PrimitiveType::Triangle, false              ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,      PalPrimTypeAdjacency(Pal::PrimitiveType::Line,     true               ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,     PalPrimTypeAdjacency(Pal::PrimitiveType::Line,     true               ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,  PalPrimTypeAdjacency(Pal::PrimitiveType::Triangle, true               ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, PalPrimTypeAdjacency(Pal::PrimitiveType::Triangle, true               ))
VK_TO_PAL_STRUC_X(PRIMITIVE_TOPOLOGY_PATCH_LIST,                    PalPrimTypeAdjacency(Pal::PrimitiveType::Patch,    false              ))
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan primitive topology to PAL primitive type + adjacency
VK_INLINE void VkToPalPrimitiveTypeAdjacency(
    VkPrimitiveTopology     topology,
    Pal::PrimitiveType*     pPrimType,
    bool*                   pAdjacency)
{
    const PalPrimTypeAdjacency& pa = convert::PrimTypeAdjacency(topology);

    *pPrimType  = pa.primType;
    *pAdjacency = pa.adjacency;
}

// =====================================================================================================================
VK_TO_PAL_TABLE_COMPLEX(  PRIMITIVE_TOPOLOGY, PrimitiveTopology,  Pal::PrimitiveTopology, PrimitiveTopology,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_POINT_LIST,                      PrimitiveTopology::PointList                              )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_LIST,                       PrimitiveTopology::LineList                               )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_STRIP,                      PrimitiveTopology::LineStrip                              )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                   PrimitiveTopology::TriangleList                           )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                  PrimitiveTopology::TriangleStrip                          )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,                    PrimitiveTopology::TriangleFan                            )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,        PrimitiveTopology::LineListAdj                            )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,       PrimitiveTopology::LineStripAdj                           )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,    PrimitiveTopology::TriangleListAdj                        )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,   PrimitiveTopology::TriangleStripAdj                       )
VK_TO_PAL_ENTRY_X(  PRIMITIVE_TOPOLOGY_PATCH_LIST,                      PrimitiveTopology::Patch                                  )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan primitive topology to PAL equivalent
VK_INLINE Pal::PrimitiveTopology VkToPalPrimitiveTopology(VkPrimitiveTopology topology)
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
VK_INLINE Pal::TexAddressMode VkToPalTexAddressMode(
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
VK_TO_PAL_TABLE_X(  BORDER_COLOR, BorderColor,              BorderColorType,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,   BorderColorType::TransparentBlack                          )
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_INT_TRANSPARENT_BLACK,     BorderColorType::TransparentBlack                          )
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_FLOAT_OPAQUE_BLACK,        BorderColorType::OpaqueBlack                               )
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_INT_OPAQUE_BLACK,          BorderColorType::OpaqueBlack                               )
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_FLOAT_OPAQUE_WHITE,        BorderColorType::White                                     )
VK_TO_PAL_ENTRY_X(  BORDER_COLOR_INT_OPAQUE_WHITE,          BorderColorType::White                                     )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan border color type to PAL equivalent
VK_INLINE Pal::BorderColorType VkToPalBorderColorType(VkBorderColor borderColor)
{
    return convert::BorderColorType(borderColor);
}

VK_TO_PAL_TABLE_X(  POLYGON_MODE, PolygonMode,                FillMode,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_POINT,                       FillMode::Points                                           )
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_LINE,                        FillMode::Wireframe                                        )
VK_TO_PAL_ENTRY_X(  POLYGON_MODE_FILL,                        FillMode::Solid                                            )
// =====================================================================================================================
)

VK_INLINE Pal::FillMode VkToPalFillMode(VkPolygonMode fillMode)
{
    return convert::FillMode(fillMode);
}

// No range size and begin range in VkCullModeFlagBits, so no direct macro mapping here
namespace convert
{
    VK_INLINE Pal::CullMode CullMode(VkCullModeFlags cullMode)
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
}

// =====================================================================================================================
// Converts Vulkan cull mode to PAL equivalent
VK_INLINE Pal::CullMode VkToPalCullMode(VkCullModeFlags cullMode)
{
    return convert::CullMode(cullMode);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  FRONT_FACE, FrontFace,                         FaceOrientation,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  FRONT_FACE_COUNTER_CLOCKWISE,                  FaceOrientation::Ccw                                       )
VK_TO_PAL_ENTRY_I(  FRONT_FACE_CLOCKWISE,                          FaceOrientation::Cw                                        )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan face orientation to PAL equivalent
VK_INLINE Pal::FaceOrientation VkToPalFaceOrientation(VkFrontFace frontFace)
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
VK_INLINE Pal::LogicOp VkToPalLogicOp(VkLogicOp logicOp)
{
    return convert::LogicOp(logicOp);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I(  BLEND_FACTOR, BlendFactor,                            Blend,
// =====================================================================================================================
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ZERO,                             Blend::Zero                                                )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE,                              Blend::One                                                 )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_COLOR,                        Blend::SrcColor                                            )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC_COLOR,              Blend::OneMinusSrcColor                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_DST_COLOR,                        Blend::DstColor                                            )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_DST_COLOR,              Blend::OneMinusDstColor                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_ALPHA,                        Blend::SrcAlpha                                            )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,              Blend::OneMinusSrcAlpha                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_DST_ALPHA,                        Blend::DstAlpha                                            )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_DST_ALPHA,              Blend::OneMinusDstAlpha                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_CONSTANT_COLOR,                   Blend::ConstantColor                                       )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,         Blend::OneMinusConstantColor                               )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_CONSTANT_ALPHA,                   Blend::ConstantAlpha                                       )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,         Blend::OneMinusConstantAlpha                               )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC_ALPHA_SATURATE,               Blend::SrcAlphaSaturate                                    )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC1_COLOR,                       Blend::Src1Color                                           )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,             Blend::OneMinusSrc1Color                                   )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_SRC1_ALPHA,                       Blend::Src1Alpha                                           )
VK_TO_PAL_ENTRY_I(  BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,             Blend::OneMinusSrc1Alpha                                   )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan blend factor to PAL equivalent

VK_INLINE Pal::Blend VkToPalBlend(VkBlendFactor blend)
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
VK_INLINE Pal::BlendFunc VkToPalBlendFunc(VkBlendOp blendFunc)
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
VK_INLINE Pal::StencilOp VkToPalStencilOp(VkStencilOp stencilOp)
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
VK_INLINE Pal::CompareFunc VkToPalCompareFunc(VkCompareOp compareOp)
{
    return convert::CompareFunc(compareOp);
}

VK_TO_PAL_TABLE_X(  INDEX_TYPE, IndexType,                  IndexType,
// =====================================================================================================================
VK_TO_PAL_ENTRY_X(  INDEX_TYPE_UINT16,                      IndexType::Idx16                                           )
VK_TO_PAL_ENTRY_X(  INDEX_TYPE_UINT32,                      IndexType::Idx32                                           )
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan index type to PAL equivalent.
VK_INLINE Pal::IndexType VkToPalIndexType(VkIndexType indexType)
{
    return convert::IndexType(indexType);
}

// =====================================================================================================================
// Converts Vulkan Filter parameters to the PAL equivalent.
VK_INLINE Pal::TexFilter VkToPalTexFilter(
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
    }

    return palTexFilter;
}

// =====================================================================================================================
// Converts a Vulkan texture filter quality parameter to the pal equivalent
VK_INLINE Pal::ImageTexOptLevel VkToPalTexFilterQuality(TextureFilterOptimizationSettings texFilterQuality)
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
// Selects a single PAL aspect that directly corresponds to the specified mask.
VK_INLINE Pal::ImageAspect VkToPalImageAspectSingle(VkImageAspectFlags aspectMask)
{
    switch (aspectMask)
    {
    case VK_IMAGE_ASPECT_COLOR_BIT:
        return Pal::ImageAspect::Color;
    case VK_IMAGE_ASPECT_DEPTH_BIT:
        return Pal::ImageAspect::Depth;
    case VK_IMAGE_ASPECT_STENCIL_BIT:
        return Pal::ImageAspect::Stencil;
    case VK_IMAGE_ASPECT_METADATA_BIT:
        return Pal::ImageAspect::Fmask;
    default:
        VK_ASSERT(!"Unsupported flag combination");
        return Pal::ImageAspect::Color;
    }
}

// =====================================================================================================================
// Selects the first PAL aspect from the Vulkan aspect mask and removes the corresponding bits from it.
VK_INLINE Pal::ImageAspect VkToPalImageAspectExtract(
    Pal::ChNumFormat    format,
    VkImageAspectFlags& aspectMask)
{
    if ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
    {
        // No other aspect can be specified in this case.
        VK_ASSERT(aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);

        aspectMask = 0;

        return Pal::ImageAspect::Color;
    }
    else if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0)
    {
        // Only the depth and/or stencil aspects can be specified in this case.
        VK_ASSERT((aspectMask & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0);

        if ((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0)
        {
            aspectMask ^= VK_IMAGE_ASPECT_DEPTH_BIT;

            return Pal::ImageAspect::Depth;
        }
        else
        {
            aspectMask ^= VK_IMAGE_ASPECT_STENCIL_BIT;

            return Pal::ImageAspect::Stencil;
        }
    }

    VK_ASSERT(!"Unexpected aspect mask");
    return Pal::ImageAspect::Color;
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
VK_INLINE Pal::ImageTiling VkToPalImageTiling(VkImageTiling tiling)
{
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
VK_INLINE Pal::ImageType VkToPalImageType(VkImageType imgType)
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
VK_INLINE Pal::ImageViewType VkToPalImageViewType(VkImageViewType imgViewType)
{
    return convert::ImageViewType(imgViewType);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I_EXT(  SAMPLER_REDUCTION_MODE, SamplerReductionModeEXT,    TexFilterMode,
// =====================================================================================================================
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT,        TexFilterMode::Blend)
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_MIN_EXT,                     TexFilterMode::Min)
    VK_TO_PAL_ENTRY_I(  SAMPLER_REDUCTION_MODE_MAX_EXT,                     TexFilterMode::Max)
// =====================================================================================================================
)

// =====================================================================================================================
// Converts Vulkan filter mode to PAL equivalent.
VK_INLINE Pal::TexFilterMode VkToPalTexFilterMode(VkSamplerReductionModeEXT filterMode)
{
    return convert::TexFilterMode(filterMode);
}

// =====================================================================================================================
// Converts Vulkan video profile level to PAL equivalent.
VK_INLINE uint32_t VkToPalVideoProfileLevel(uint32_t level)
{
    // Vulkan level value is created using VK_MAKE_VERSION
    uint32_t major = level >> 22;
    uint32_t minor = (level >> 12) & 0x3FF;

    // PAL level is represented as version multiplied by 10
    return major * 10 + minor;
}

// =====================================================================================================================
// Converts PAL video profile level to Vulkan equivalent.
VK_INLINE uint32_t PalToVkVideoProfileLevel(uint32_t level)
{
    // PAL level is represented as version multiplied by 10
    uint32_t major = level / 10;
    uint32_t minor = level % 10;

    // Vulkan level value is created using VK_MAKE_VERSION
    return VK_MAKE_VERSION(major, minor, 0);
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
VK_INLINE Pal::QueryType VkToPalQueryType(VkQueryType queryType)
{
    return convert::QueryTypePool(queryType).m_type;
}

// =====================================================================================================================
// Converts Vulkan query type to PAL equivalent
VK_INLINE Pal::QueryPoolType VkToPalQueryPoolType(VkQueryType queryType)
{
    return convert::QueryTypePool(queryType).m_poolType;
}

// =====================================================================================================================
// Converts Vulkan query control flags to PAL equivalent
VK_INLINE Pal::QueryControlFlags VkToPalQueryControlFlags(VkQueryControlFlags flags)
{
    Pal::QueryControlFlags palFlags;
    palFlags.u32All = 0;
    if ((flags & VK_QUERY_CONTROL_PRECISE_BIT) == 0)
    {
        palFlags.impreciseData = 1;
    }

    return palFlags;
}

// =====================================================================================================================
// Converts Vulkan query result flags to PAL equivalent
VK_INLINE Pal::QueryResultFlags VkToPalQueryResultFlags(VkQueryResultFlags flags)
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
VK_INLINE Pal::QueryPipelineStatsFlags VkToPalQueryPipelineStatsFlags(VkQueryPipelineStatisticFlags flags)
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
        static_cast<uint32_t>(Pal::QueryPipelineStatsCsInvocations)),
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
VK_INLINE Pal::SwizzledFormat RemapFormatComponents(
    Pal::SwizzledFormat       format,
    const VkComponentMapping& mapping)
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

    if (format.format != Pal::ChNumFormat::Undefined)
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
// Converts Vulkan image subresource range to PAL equivalent.
// It may generate two PAL subresource range entries in case both depth and stencil aspect is selected in the mask.
VK_INLINE void VkToPalSubresRange(
    Pal::ChNumFormat                format,
    const VkImageSubresourceRange&  range,
    uint32_t                        mipLevels,
    uint32_t                        arraySize,
    Pal::SubresRange*               pPalSubresRanges,
    uint32_t*                       pPalSubresRangeIndex)
{
    constexpr uint32_t WHOLE_SIZE_UINT32 = (uint32_t)VK_WHOLE_SIZE;

    Pal::SubresRange palSubresRange;

    palSubresRange.startSubres.arraySlice   = range.baseArrayLayer;
    palSubresRange.startSubres.mipLevel     = range.baseMipLevel;
    palSubresRange.numMips                  = (range.levelCount == WHOLE_SIZE_UINT32) ? (mipLevels - range.baseMipLevel)   : range.levelCount;
    palSubresRange.numSlices                = (range.layerCount == WHOLE_SIZE_UINT32) ? (arraySize - range.baseArrayLayer) : range.layerCount;

    VkImageAspectFlags aspectMask = range.aspectMask;

    do
    {
        palSubresRange.startSubres.aspect = VkToPalImageAspectExtract(format, aspectMask);
        pPalSubresRanges[(*pPalSubresRangeIndex)++] = palSubresRange;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan scissor params to a PAL scissor rect params
VK_INLINE Pal::ScissorRectParams VkToPalScissorParams(const VkPipelineViewportStateCreateInfo& scissors)
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
VK_INLINE Pal::Offset2d VkToPalOffset2d(const VkOffset2D& offset)
{
    Pal::Offset2d result;
    result.x = offset.x;
    result.y = offset.y;
    return result;
}

// =====================================================================================================================
// Converts a Vulkan offset 3D to a PAL offset 3D
VK_INLINE Pal::Offset3d VkToPalOffset3d(const VkOffset3D& offset)
{
    Pal::Offset3d result;
    result.x = offset.x;
    result.y = offset.y;
    result.z = offset.z;
    return result;
}

// =====================================================================================================================
// Converts a Vulkan extent 2D to a PAL extent 2D
VK_INLINE Pal::Extent2d VkToPalExtent2d(const VkExtent2D& extent)
{
    Pal::Extent2d result;
    result.width  = extent.width;
    result.height = extent.height;
    return result;
}

// =====================================================================================================================
// Converts a PAL extent 2D to a Vulkan extent 2D
VK_INLINE VkExtent2D PalToVkExtent2d(const Pal::Extent2d& extent)
{
    VkExtent2D result;
    result.width  = extent.width;
    result.height = extent.height;
    return result;
}

// =====================================================================================================================
// Converts PAL GpuType to Vulkan VkPhysicalDeviceType
VK_INLINE VkPhysicalDeviceType PalToVkGpuType(const Pal::GpuType gpuType)
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
VK_INLINE Pal::Extent3d VkToPalExtent3d(const VkExtent3D& extent)
{
    Pal::Extent3d result;
    result.width  = extent.width;
    result.height = extent.height;
    result.depth  = extent.depth;
    return result;
}

// =====================================================================================================================
// Converts a PAL extent 3D to a Vulkan extent 3D
VK_INLINE VkExtent3D PalToVkExtent3d(const Pal::Extent3d& extent)
{
    VkExtent3D result;
    result.width  = extent.width;
    result.height = extent.height;
    result.depth  = extent.depth;
    return result;
}

// =====================================================================================================================
// Converts two Vulkan 3D offsets to a PAL signed extent 3D
VK_INLINE Pal::SignedExtent3d VkToPalSignedExtent3d(const VkOffset3D offsets[2])
{
    Pal::SignedExtent3d result;
    result.width  = offsets[1].x  - offsets[0].x;
    result.height = offsets[1].y  - offsets[0].y;
    result.depth  = offsets[1].z  - offsets[0].z;
    return result;
}

// =====================================================================================================================
// Converts value in texels to value in blocks, specifying block dimension for the given coordinate.
VK_INLINE uint32_t TexelsToBlocks(uint32_t texels, uint32_t blockSize)
{
    return Util::RoundUpToMultiple(texels, blockSize) / blockSize;
}

// =====================================================================================================================
// Converts signed value in texels to signed value in blocks, specifying block dimension for the given coordinate.
VK_INLINE int32_t TexelsToBlocks(int32_t texels, uint32_t blockSize)
{
    uint32_t value = Util::Math::Absu(texels);
    value = Util::RoundUpToMultiple(value, blockSize) / blockSize;

    int32_t retValue = (int32_t)value;
    return texels > 0 ? retValue : -retValue;
}

// =====================================================================================================================
// Converts pitch value in texels to pitch value in blocks, specifying block dimension for the given coordinate.
VK_INLINE Pal::gpusize PitchTexelsToBlocks(Pal::gpusize texels, uint32_t blockSize)
{
    return Util::RoundUpToMultiple(texels, static_cast<Pal::gpusize>(blockSize)) / blockSize;
}

// =====================================================================================================================
// Converts extent in texels to extent in blocks, specifying block dimensions.
VK_INLINE Pal::Extent3d TexelsToBlocks(Pal::Extent3d texels, Pal::Extent3d blockSize)
{
    Pal::Extent3d blocks;

    blocks.width    = TexelsToBlocks(texels.width,  blockSize.width);
    blocks.height   = TexelsToBlocks(texels.height, blockSize.height);
    blocks.depth    = TexelsToBlocks(texels.depth,  blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Converts signed extent in texels to signed extent in blocks, specifying block dimensions.
VK_INLINE Pal::SignedExtent3d TexelsToBlocks(Pal::SignedExtent3d texels, Pal::Extent3d blockSize)
{
    Pal::SignedExtent3d blocks;

    blocks.width  = TexelsToBlocks(texels.width,  blockSize.width);
    blocks.height = TexelsToBlocks(texels.height, blockSize.height);
    blocks.depth  = TexelsToBlocks(texels.depth,  blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Converts offset in texels to offset in blocks, specifying block dimensions.
VK_INLINE Pal::Offset3d TexelsToBlocks(Pal::Offset3d texels, Pal::Extent3d blockSize)
{
    Pal::Offset3d blocks;

    blocks.x = TexelsToBlocks(texels.x, blockSize.width);
    blocks.y = TexelsToBlocks(texels.y, blockSize.height);
    blocks.z = TexelsToBlocks(texels.z, blockSize.depth);

    return blocks;
}

// =====================================================================================================================
// Converts a Vulkan image-copy structure to one or more PAL image-copy-region structures.
VK_INLINE void VkToPalImageCopyRegion(
    const VkImageCopy&      imageCopy,
    Pal::ChNumFormat        srcFormat,
    Pal::ChNumFormat        dstFormat,
    Pal::ImageCopyRegion*   pPalRegions,
    uint32_t&               palRegionIndex)
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
    // Layer count may be different if copying between 2D and 3D images
    region.numSlices = Util::Max<uint32_t>(imageCopy.srcSubresource.layerCount, imageCopy.dstSubresource.layerCount);

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

    // Source and destination aspect masks must match
    VK_ASSERT(imageCopy.srcSubresource.aspectMask == imageCopy.dstSubresource.aspectMask);

    // As we don't allow copying between different types of aspects we don't need to worry about dealing with both
    // aspect masks separately.
    VkImageAspectFlags aspectMask = imageCopy.srcSubresource.aspectMask;

    do
    {
        region.srcSubres.aspect = region.dstSubres.aspect = VkToPalImageAspectExtract(srcFormat, aspectMask);
        pPalRegions[palRegionIndex++] = region;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan image-blit structure to one or more PAL image-scaled-copy-region structures.
VK_INLINE void VkToPalImageScaledCopyRegion(
    const VkImageBlit&          imageBlit,
    Pal::ChNumFormat            srcFormat,
    Pal::ChNumFormat            dstFormat,
    Pal::ImageScaledCopyRegion* pPalRegions,
    uint32_t&                   palRegionIndex)
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

    VK_ASSERT(imageBlit.srcSubresource.layerCount == imageBlit.dstSubresource.layerCount);
    VK_ASSERT(region.srcExtent.depth == region.srcExtent.depth);

    region.numSlices = Util::Max<uint32_t>(region.srcExtent.depth, imageBlit.srcSubresource.layerCount);

    // PAL expects all dimensions to be in blocks for compressed formats so let's handle that here
    if (Pal::Formats::IsBlockCompressed(srcFormat))
    {
        Pal::Extent3d blockDim  = Pal::Formats::CompressedBlockDim(srcFormat);

        region.srcOffset        = TexelsToBlocks(region.srcOffset, blockDim);
        region.srcExtent        = TexelsToBlocks(region.srcExtent, blockDim);
    }

    if (Pal::Formats::IsBlockCompressed(dstFormat))
    {
        Pal::Extent3d blockDim  = Pal::Formats::CompressedBlockDim(dstFormat);

        region.dstOffset        = TexelsToBlocks(region.dstOffset, blockDim);
        region.dstExtent        = TexelsToBlocks(region.dstExtent, blockDim);
    }

    // Source and destination aspect masks must match
    VK_ASSERT(imageBlit.srcSubresource.aspectMask == imageBlit.dstSubresource.aspectMask);

    // As we don't allow copying between different types of aspects we don't need to worry about dealing with both
    // aspect masks separately.
    VkImageAspectFlags aspectMask = imageBlit.srcSubresource.aspectMask;

    do
    {
        region.srcSubres.aspect = region.dstSubres.aspect = VkToPalImageAspectExtract(srcFormat, aspectMask);
        pPalRegions[palRegionIndex++] = region;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan image-blit structure to one or more PAL color-space-conversion-region structures.
VK_INLINE Pal::ColorSpaceConversionRegion VkToPalImageColorSpaceConversionRegion(
    const VkImageBlit&  imageBlit,
    Pal::SwizzledFormat srcFormat,
    Pal::SwizzledFormat dstFormat)
{

    Pal::ColorSpaceConversionRegion region = {};

    // Color conversion blits can only happen between a YUV and an RGB image.
    VK_ASSERT((Pal::Formats::IsYuv(srcFormat.format) && (Pal::Formats::IsYuv(dstFormat.format) == false)) ||
              ((Pal::Formats::IsYuv(srcFormat.format) == false) && (Pal::Formats::IsYuv(dstFormat.format))));

    const VkImageSubresourceLayers& rgbSubresource =
        Pal::Formats::IsYuv(srcFormat.format) ? imageBlit.dstSubresource : imageBlit.srcSubresource;

    const VkImageSubresourceLayers& yuvSubresource =
        Pal::Formats::IsYuv(srcFormat.format) ? imageBlit.srcSubresource : imageBlit.dstSubresource;

    // Convert values to temporary 3D variables as the PAL interface currently only accepts 2D
    Pal::Offset3d       srcOffset = VkToPalOffset3d(imageBlit.srcOffsets[0]);
    Pal::SignedExtent3d srcExtent = VkToPalSignedExtent3d(imageBlit.srcOffsets);
    Pal::Offset3d       dstOffset = VkToPalOffset3d(imageBlit.dstOffsets[0]);
    Pal::SignedExtent3d dstExtent = VkToPalSignedExtent3d(imageBlit.dstOffsets);

    region.rgbSubres.aspect     = Pal::ImageAspect::Color;
    region.rgbSubres.mipLevel   = rgbSubresource.mipLevel;
    region.rgbSubres.arraySlice = rgbSubresource.baseArrayLayer;

    VK_ASSERT(yuvSubresource.mipLevel == 0);

    region.yuvStartSlice        = yuvSubresource.baseArrayLayer;

    VK_ASSERT(imageBlit.srcSubresource.layerCount == imageBlit.dstSubresource.layerCount);
    VK_ASSERT(srcExtent.depth == srcExtent.depth);

    region.sliceCount = Util::Max<uint32_t>(srcExtent.depth, imageBlit.srcSubresource.layerCount);

    // PAL expects all dimensions to be in blocks for compressed formats so let's handle that here
    if (Pal::Formats::IsBlockCompressed(srcFormat.format))
    {
        Pal::Extent3d blockDim = Pal::Formats::CompressedBlockDim(srcFormat.format);

        srcOffset = TexelsToBlocks(srcOffset, blockDim);
        srcExtent = TexelsToBlocks(srcExtent, blockDim);
    }

    if (Pal::Formats::IsBlockCompressed(dstFormat.format))
    {
        Pal::Extent3d blockDim = Pal::Formats::CompressedBlockDim(dstFormat.format);

        dstOffset = TexelsToBlocks(dstOffset, blockDim);
        dstExtent = TexelsToBlocks(dstExtent, blockDim);
    }

    // Write the 2D coordinates and ignore the 3rd dimension for now
    region.srcOffset.x = srcOffset.x;
    region.srcOffset.y = srcOffset.y;

    VK_ASSERT(srcOffset.z == 0);

    region.srcExtent.width  = srcExtent.width;
    region.srcExtent.height = srcExtent.height;

    VK_ASSERT(srcExtent.depth == 1);

    region.dstOffset.x = dstOffset.x;
    region.dstOffset.y = dstOffset.y;

    VK_ASSERT(dstOffset.z == 0);

    region.dstExtent.width  = dstExtent.width;
    region.dstExtent.height = dstExtent.height;

    VK_ASSERT(dstExtent.depth == 1);

    return region;
}

// =====================================================================================================================
// Converts a Vulkan image-resolve structure to one or more PAL image-resolve-region structures.
VK_INLINE void VkToPalImageResolveRegion(
    const VkImageResolve&       imageResolve,
    Pal::ChNumFormat            srcFormat,
    Pal::ChNumFormat            dstFormat,
    Pal::ImageResolveRegion*    pPalRegions,
    uint32_t&                   palRegionIndex)
{
    Pal::ImageResolveRegion region = {};

    // We don't need to reinterpret the format during the resolve
    region.swizzledFormat = Pal::UndefinedSwizzledFormat;

    region.srcSlice     = imageResolve.srcSubresource.baseArrayLayer;

    region.dstSlice     = imageResolve.dstSubresource.baseArrayLayer;
    region.dstMipLevel  = imageResolve.dstSubresource.mipLevel;

    region.extent    = VkToPalExtent3d(imageResolve.extent);
    region.srcOffset = VkToPalOffset3d(imageResolve.srcOffset);
    region.dstOffset = VkToPalOffset3d(imageResolve.dstOffset);

    VK_ASSERT(imageResolve.srcSubresource.layerCount == imageResolve.dstSubresource.layerCount);

    region.numSlices = imageResolve.srcSubresource.layerCount;

    // Source and destination aspect masks must match
    VK_ASSERT(imageResolve.srcSubresource.aspectMask == imageResolve.dstSubresource.aspectMask);

    // As we don't allow copying between different types of aspects we don't need to worry about dealing with both
    // aspect masks separately.
    VkImageAspectFlags aspectMask = imageResolve.srcSubresource.aspectMask;

    do
    {
        region.srcAspect = region.dstAspect = VkToPalImageAspectExtract(srcFormat, aspectMask);
        pPalRegions[palRegionIndex++] = region;
    }
    while (aspectMask != 0);
}

// =====================================================================================================================
// Converts a Vulkan buffer-image-copy structure to a PAL memory-image-copy-region structure.
VK_INLINE Pal::MemoryImageCopyRegion VkToPalMemoryImageCopyRegion(
    const VkBufferImageCopy&    bufferImageCopy,
    Pal::ChNumFormat            format,
    Pal::gpusize                baseMemOffset)
{
    Pal::MemoryImageCopyRegion region = {};

    region.imageSubres.aspect       = VkToPalImageAspectSingle(bufferImageCopy.imageSubresource.aspectMask);

    region.imageSubres.arraySlice   = bufferImageCopy.imageSubresource.baseArrayLayer;
    region.imageSubres.mipLevel     = bufferImageCopy.imageSubresource.mipLevel;

    region.imageOffset          = VkToPalOffset3d(bufferImageCopy.imageOffset);
    region.imageExtent          = VkToPalExtent3d(bufferImageCopy.imageExtent);

    region.numSlices            = bufferImageCopy.imageSubresource.layerCount;

    region.gpuMemoryOffset      = baseMemOffset + bufferImageCopy.bufferOffset;
    region.gpuMemoryRowPitch    = (bufferImageCopy.bufferRowLength != 0)
                                    ? bufferImageCopy.bufferRowLength
                                    : bufferImageCopy.imageExtent.width;
    region.gpuMemoryDepthPitch  = (bufferImageCopy.bufferImageHeight != 0)
                                    ? bufferImageCopy.bufferImageHeight
                                    : bufferImageCopy.imageExtent.height;

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
    region.gpuMemoryRowPitch   *= Pal::Formats::BytesPerPixel(format);
    region.gpuMemoryDepthPitch *= region.gpuMemoryRowPitch;

    return region;
}

namespace convert
{
extern Pal::SwizzledFormat VkToPalSwizzledFormatLookupTableStorage[VK_FORMAT_END_RANGE + 1];
};

// =====================================================================================================================
constexpr VK_INLINE Pal::SwizzledFormat PalFmt(
    Pal::ChNumFormat    chNumFormat,
    Pal::ChannelSwizzle r,
    Pal::ChannelSwizzle g,
    Pal::ChannelSwizzle b,
    Pal::ChannelSwizzle a)
{
    return{ chNumFormat,{ r, g, b, a } };
}

// =====================================================================================================================
// Converts Vulkan format to PAL equivalent.
VK_INLINE Pal::SwizzledFormat VkToPalFormat(VkFormat format)
{
    if (VK_ENUM_IN_RANGE(format, VK_FORMAT))
    {
        return convert::VkToPalSwizzledFormatLookupTableStorage[format];
    }
    else
    {
        return Pal::UndefinedSwizzledFormat;
    }
}

// =====================================================================================================================
// TODO: VK_EXT_swapchain_colorspace combines the concept of a transfer function and a color space, which is
// insufficient. For now,  map the capabilities of Pal using either the transfer function OR color space
// settings to support the current revision of VK_EXT_swapchain_colorspace.
// To expose the complete capability, we should propose VK_EXT_swapchain_transfer_function (or a similar named)
// extension and propose revisions to VK_EXT_swapchain_colorspace.
namespace convert
{
    VK_INLINE Pal::ScreenColorSpace ScreenColorSpace(VkSurfaceFormatKHR surfaceFormat)
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

#if VK_USE_PLATFORM_WIN32_KHR
        case VK_COLOR_SPACE_FREESYNC_2_AMD:
        {
            if (surfaceFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT)
            {
                palColorSpaceBits = Pal::ScreenColorSpace::TfLinear0_125;
                palColorSpaceBits |= Pal::ScreenColorSpace::CsScrgb;
            }
            else
            {
                palColorSpaceBits = Pal::ScreenColorSpace::TfGamma22;
                palColorSpaceBits |= Pal::ScreenColorSpace::CsNative;
            }
            break;
        }
#endif

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
VK_INLINE Pal::ScreenColorSpace VkToPalScreenSpace(VkSurfaceFormatKHR colorFormat)
{
    return convert::ScreenColorSpace(colorFormat);
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL HW pipe point.
// Selects a source pipe point that matches all stage flags to use for setting/resetting events.
VK_INLINE Pal::HwPipePoint VkToPalSrcPipePoint(VkPipelineStageFlags flags)
{
    // Flags that only require signaling at top-of-pipe.
    static const VkPipelineStageFlags srcTopOfPipeFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // Flags that only require signaling post-index-fetch.
    static const VkPipelineStageFlags srcPostIndexFetchFlags =
        srcTopOfPipeFlags |
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

    // Flags that only require signaling pre-rasterization.
    static const VkPipelineStageFlags srcPreRasterizationFlags =
        srcPostIndexFetchFlags                                  |
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                      |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT       |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT    |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;

    // Flags that only require signaling post-PS.
    static const VkPipelineStageFlags srcPostPsFlags =
        srcPreRasterizationFlags                        |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT      |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    // Flags that only require signaling post-CS.
    static const VkPipelineStageFlags srcPostCsFlags =
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    // Flags that only require signaling post-Blt
    static const VkPipelineStageFlags srcPostBltFlags =
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
        srcPipePoint = Pal::HwPipePostIndexFetch;
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
VK_INLINE Pal::HwPipePoint VkToPalSrcPipePointForTimestampWrite(VkPipelineStageFlags flags)
{
    // Flags that require signaling at top-of-pipe.
    static const VkPipelineStageFlags srcTopOfPipeFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    Pal::HwPipePoint srcPipePoint;

    if ((flags & ~srcTopOfPipeFlags) == 0)
    {
        srcPipePoint = Pal::HwPipeTop;
    }
    else
    {
        srcPipePoint = Pal::HwPipeBottom;
    }

    return srcPipePoint;
}

// =====================================================================================================================
// Converts Vulkan source pipeline stage flags to PAL buffer marker writes (top/bottom only)
VK_INLINE Pal::HwPipePoint VkToPalSrcPipePointForMarkers(
    VkPipelineStageFlags flags,
    Pal::EngineType      engineType)
{
    // This function is written against the following three engine types.  If you hit this assert then check if this
    // new engine supports top of pipe writes at all (e.g. SDMA doesn't).
    VK_ASSERT(engineType == Pal::EngineTypeDma ||
              engineType == Pal::EngineTypeUniversal ||
              engineType == Pal::EngineTypeCompute);

    // Flags that allow signaling at top-of-pipe (anything else maps to bottom)
    constexpr VkPipelineStageFlags SrcTopOfPipeFlags =
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
    VkPipelineStageFlags    stateFlags;
};

static const HwPipePointMappingEntry hwPipePointMappingTable[] =
{
    // Flags that require flushing index-fetch workload.
    {
        Pal::HwPipePostIndexFetch,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
    },
    // Flags that require flushing pre-rasterization workload.
    {
        Pal::HwPipePreRasterization,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                      |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT       |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT    |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
    },
    // Flags that require flushing PS workload.
    {
        Pal::HwPipePostPs,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT      |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
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
    },

    // flags that require flush Post-Blt workload.
    {
        Pal::HwPipePostBlt,
        VK_PIPELINE_STAGE_TRANSFER_BIT
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
VK_INLINE uint32_t VkToPalSrcPipePoints(VkPipelineStageFlags flags, Pal::HwPipePoint* pPalPipePoints)
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
VK_INLINE Pal::HwPipePoint VkToPalWaitPipePoint(VkPipelineStageFlags flags)
{
    static_assert((Pal::HwPipePostIndexFetch == Pal::HwPipePreCs) && (Pal::HwPipePostIndexFetch == Pal::HwPipePreBlt),
        "The code here assumes pre-CS and pre-blit match post-index-fetch.");

    // Flags that only require waiting pre-rasterization.
    static const VkPipelineStageFlags dstPreRasterizationFlags =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT              |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                   |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT               |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // Flags that only require waiting post-index-fetch.
    static const VkPipelineStageFlags dstPostIndexFetchFlags =
        dstPreRasterizationFlags                                |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT       |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT    |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                   |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                    |
        VK_PIPELINE_STAGE_TRANSFER_BIT;

    Pal::HwPipePoint dstPipePoint;

    // Check if pre-rasterization waiting is enough.
    if ((flags & ~dstPreRasterizationFlags) == 0)
    {
        dstPipePoint = Pal::HwPipePreRasterization;
    }
    // Otherwise see if post-index-fetch waiting is enough.
    else if ((flags & ~dstPostIndexFetchFlags) == 0)
    {
        dstPipePoint = Pal::HwPipePostIndexFetch;
    }
    // Otherwise we have to resort to top-of-pipe waiting.
    else
    {
        dstPipePoint = Pal::HwPipeTop;
    }

    return dstPipePoint;
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

VK_INLINE VkImageTiling PalToVkImageTiling(Pal::ImageTiling tiling)
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
VK_INLINE VkSurfaceTransformFlagBitsKHR PalToVkSurfaceTransform(Pal::SurfaceTransformFlags transformFlag)
{
    if (transformFlag)
    {
        return convert::PalToVKSurfaceTransformFlagBitsKHR(transformFlag);
    }
    return static_cast<VkSurfaceTransformFlagBitsKHR>(0);
}

// =====================================================================================================================
// Converts Vulkan WSI Platform Type to PAL equivalent.
VK_INLINE Pal::WsiPlatform VkToPalWsiPlatform(VkIcdWsiPlatform Platform)
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 415
    case VK_ICD_WSI_PLATFORM_DISPLAY:
        palPlatform = Pal::WsiPlatform::DirectDisplay;
        break;
#endif
    case VK_ICD_WSI_PLATFORM_WIN32:
    default:
        palPlatform = Pal::WsiPlatform::Win32;
        break;
    }
    return palPlatform;
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
VK_INLINE Pal::SwapChainMode VkToPalSwapChainMode(VkPresentModeKHR presentMode)
{
    return convert::SwapChainMode(presentMode);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 445
namespace convert
{
    VK_INLINE Pal::CompositeAlphaMode CompositeAlpha(VkCompositeAlphaFlagBitsKHR compositeAlpha)
    {
        switch (compositeAlpha)
        {
            case VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:
                return Pal::CompositeAlphaMode::Opaque;

            case VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR:
                return Pal::CompositeAlphaMode::PreMultiplied;

            case VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR:
                return Pal::CompositeAlphaMode::PostMultiplied;

            case VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR:
                return Pal::CompositeAlphaMode::Inherit;

            default:
                VK_ASSERT(!"Unknown CompositeAlphaFlag!");
                return Pal::CompositeAlphaMode::Opaque;
        }
    }
}

// =====================================================================================================================
// Converts Vulkan composite alpha flag to PAL equivalent.
VK_INLINE Pal::CompositeAlphaMode VkToPalCompositeAlphaMode(VkCompositeAlphaFlagBitsKHR compositeAlpha)
{
    return convert::CompositeAlpha(compositeAlpha);
}
#endif

// =====================================================================================================================
// Converts Vulkan image creation flags to PAL image creation flags (unfortunately, PAL doesn't define a dedicated type
// for the image creation flags so we have to return the constructed flag set as a uint32_t)
VK_INLINE uint32_t VkToPalImageCreateFlags(VkImageCreateFlags imageCreateFlags,
                                           VkFormat           format)
{
    Pal::ImageCreateInfo palImageCreateInfo;
    palImageCreateInfo.flags.u32All         = 0;

    palImageCreateInfo.flags.cubemap            = (imageCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)   ? 1 : 0;
    palImageCreateInfo.flags.prt                = (imageCreateFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)  ? 1 : 0;
    palImageCreateInfo.flags.invariant          = (imageCreateFlags & VK_IMAGE_CREATE_ALIAS_BIT)             ? 1 : 0;

    // We must not use any metadata if sparse aliasing is enabled
    palImageCreateInfo.flags.noMetadata         = (imageCreateFlags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)    ? 1 : 0;

    // Always provide pQuadSamplePattern to PalCmdResolveImage for depth formats to allow optimizations
    palImageCreateInfo.flags.sampleLocsAlwaysKnown = Formats::HasDepth(format) ? 1 : 0;

    // Flag VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT is supported by default for all 3D images
    VK_IGNORE(VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT);

    return palImageCreateInfo.flags.u32All;
}

// =====================================================================================================================
// Converts Vulkan image usage flags to PAL image usage flags
VK_INLINE Pal::ImageUsageFlags VkToPalImageUsageFlags(VkImageUsageFlags imageUsageFlags,
                                                      VkFormat          format,
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

    //    //  Vulkan client driver can set resolveSrc usage flag bit  when msaa image setting Transfer_Src bit. Pal will use
    //    // resolveSrc and shaderRead flag as well as other conditions to decide whether msaa surface and fmask is tc-compatible.
    //    palImageUsageFlags.resolveSrc   = ((samples > 1) && (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT));

    // For some reasons, see CL#1414376, we cannot set resolveSrc flag for all images temporary. However, a resolve
    // dst flag is essential for Pal to create htile lookup table for depth stencil image on Gfx9. So we set
    // resolve dst floag for msaa depth-stencil image with VK_IMAGE_USAGE_TRANSFER_SRC_BIT bit set.
    palImageUsageFlags.resolveSrc = ((samples > 1) &&
                                    (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
                                    Formats::IsDepthStencilFormat(format));

    palImageUsageFlags.resolveDst   = ((samples == 1) && (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    palImageUsageFlags.colorTarget  = (imageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)         ? 1 : 0;
    palImageUsageFlags.depthStencil = (imageUsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? 1 : 0;

    return palImageUsageFlags;
}

// =====================================================================================================================
// Converts PAL image usage flag to Vulkan.
VK_INLINE VkImageUsageFlags PalToVkImageUsageFlags(Pal::ImageUsageFlags imageUsageFlags)
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
VK_INLINE VkResult PalToVkResult(
    Pal::Result result)
{
    if (result == Pal::Result::Success)
    {
        return VK_SUCCESS;
    }
    else
    {
        return PalToVkError(result);
    }
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
VK_INLINE Pal::PipelineBindPoint VkToPalPipelineBindPoint(VkPipelineBindPoint pipelineBind)
{
    return convert::PipelineBindPoint(pipelineBind);
}

// =====================================================================================================================
VK_INLINE Pal::ShaderType VkToPalShaderType(
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
VK_INLINE float VkToPalClearDepth(float depth)
{
    if (Util::Math::IsNaN(depth))
    {
        depth = 1.0f;
    }

    return depth;
}

// =====================================================================================================================
// Converts Vulkan clear color value to PAL equivalent
VK_INLINE Pal::ClearColor VkToPalClearColor(
    const VkClearColorValue*   pClearColor,
    const Pal::SwizzledFormat& swizzledFormat)
{
    Pal::ClearColor clearColor;

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
        clearColor.type = Pal::ClearColorType::Uint;
        Pal::Formats::ConvertColor(swizzledFormat, &pClearColor->float32[0], &clearColor.u32Color[0]);
        break;
    case Pal::Formats::NumericSupportFlags::Sint:
        clearColor.type        = Pal::ClearColorType::Sint;
        clearColor.u32Color[0] = pClearColor->uint32[0];
        clearColor.u32Color[1] = pClearColor->uint32[1];
        clearColor.u32Color[2] = pClearColor->uint32[2];
        clearColor.u32Color[3] = pClearColor->uint32[3];
        break;
    default:
        clearColor.type        = Pal::ClearColorType::Uint;
        clearColor.u32Color[0] = pClearColor->uint32[0];
        clearColor.u32Color[1] = pClearColor->uint32[1];
        clearColor.u32Color[2] = pClearColor->uint32[2];
        clearColor.u32Color[3] = pClearColor->uint32[3];
        break;
    }

    return clearColor;
}

// =====================================================================================================================
// Converts integer nanoseconds to single precision seconds
VK_INLINE float NanosecToSec(uint64_t nanosecs)
{
    return static_cast<float>(static_cast<double>(nanosecs) / 1000000000.0);
}

// =====================================================================================================================
// Converts maximum sample count to VkSampleCountFlags
VK_INLINE VkSampleCountFlags MaxSampleCountToSampleCountFlags(uint32_t maxSampleCount)
{
    return (maxSampleCount << 1) - 1;
}

// Constant for the number of memory types supported by our Vulkan implementation (matches the number of PAL GPU heaps)
constexpr uint32_t VK_MEMORY_TYPE_NUM = Pal::GpuHeapCount;

// =====================================================================================================================
// Converts PAL GPU heap to Vulkan memory heap flags
VK_INLINE VkMemoryHeapFlags PalGpuHeapToVkMemoryHeapFlags(Pal::GpuHeap heap)
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
VK_INLINE VkFormatFeatureFlags PalToVkFormatFeatureFlags(Pal::FormatFeatureFlags flags)
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
VK_INLINE bool VkToPalRasterizationOrder(VkRasterizationOrderAMD order)
{
    VK_ASSERT(VK_ENUM_IN_RANGE_AMD(order, VK_RASTERIZATION_ORDER));

    return (order == VK_RASTERIZATION_ORDER_RELAXED_AMD);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I_AMD(GPA_PERF_BLOCK, GpaPerfBlockAMD, GpuBlock,
    // =====================================================================================================================
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_CPF_AMD,          GpuBlock::Cpf)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_IA_AMD,           GpuBlock::Ia)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_VGT_AMD,          GpuBlock::Vgt)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_PA_AMD,           GpuBlock::Pa)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_SC_AMD,           GpuBlock::Sc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_SPI_AMD,          GpuBlock::Spi)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_SQ_AMD,           GpuBlock::Sq)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_SX_AMD,           GpuBlock::Sx)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TA_AMD,           GpuBlock::Ta)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TD_AMD,           GpuBlock::Td)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TCP_AMD,          GpuBlock::Tcp)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TCC_AMD,          GpuBlock::Tcc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TCA_AMD,          GpuBlock::Tca)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_DB_AMD,           GpuBlock::Db)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_CB_AMD,           GpuBlock::Cb)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_GDS_AMD,          GpuBlock::Gds)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_SRBM_AMD,         GpuBlock::Srbm)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_GRBM_AMD,         GpuBlock::Grbm)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_GRBM_SE_AMD,      GpuBlock::GrbmSe)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_RLC_AMD,          GpuBlock::Rlc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_DMA_AMD,          GpuBlock::Dma)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_MC_AMD,           GpuBlock::Mc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_CPG_AMD,          GpuBlock::Cpg)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_CPC_AMD,          GpuBlock::Cpc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_WD_AMD,           GpuBlock::Wd)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_TCS_AMD,          GpuBlock::Tcs)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_ATC_AMD,          GpuBlock::Atc)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_ATC_L2_AMD,       GpuBlock::AtcL2)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_MC_VM_L2_AMD,     GpuBlock::McVmL2)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_EA_AMD,           GpuBlock::Ea)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_RPB_AMD,          GpuBlock::Rpb)
    VK_TO_PAL_ENTRY_I(GPA_PERF_BLOCK_RMI_AMD,          GpuBlock::Rmi)
// =====================================================================================================================
)

// =====================================================================================================================
VK_INLINE Pal::GpuBlock VkToPalGpuBlock(
    VkGpaPerfBlockAMD perfBlock)
{
    return convert::GpuBlock(perfBlock);
}

// =====================================================================================================================
VK_TO_PAL_TABLE_I_AMD(GPA_DEVICE_CLOCK_MODE, GpaDeviceClockModeAMD, DeviceClockMode,
    // =====================================================================================================================
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_DEFAULT_AMD,    DeviceClockMode::Default)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_QUERY_AMD,      DeviceClockMode::Query)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_PROFILING_AMD,  DeviceClockMode::Profiling)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_MIN_MEMORY_AMD, DeviceClockMode::MinimumMemory)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_MIN_ENGINE_AMD, DeviceClockMode::MinimumEngine)
    VK_TO_PAL_ENTRY_I(GPA_DEVICE_CLOCK_MODE_PEAK_AMD,       DeviceClockMode::Peak)
    // =====================================================================================================================
)

// =====================================================================================================================
VK_INLINE Pal::DeviceClockMode VkToPalDeviceClockMode(
    VkGpaDeviceClockModeAMD clockMode)
{
    return convert::DeviceClockMode(clockMode);
}

// =====================================================================================================================
VK_INLINE uint32_t VkToPalPerfExperimentShaderFlags(
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
PalClearRegion VkToPalClearRegion(const VkClearRect& clearRect);

// =====================================================================================================================
// Converts Vulkan clear rect to an equivalent PAL box
template <>
VK_INLINE Pal::Box VkToPalClearRegion<Pal::Box>(
    const VkClearRect& clearRect)
{
    Pal::Box box { };

    box.offset.x      = clearRect.rect.offset.x;
    box.offset.y      = clearRect.rect.offset.y;
    box.offset.z      = clearRect.baseArrayLayer;
    box.extent.width  = clearRect.rect.extent.width;
    box.extent.height = clearRect.rect.extent.height;
    box.extent.depth  = clearRect.layerCount;

    return box;
}

// =====================================================================================================================
// Converts Vulkan clear rect to an equivalent PAL clear bound target region
template <>
VK_INLINE Pal::ClearBoundTargetRegion VkToPalClearRegion<Pal::ClearBoundTargetRegion>(
    const VkClearRect& clearRect)
{
    Pal::ClearBoundTargetRegion clearRegion { };

    clearRegion.rect.offset.x      = clearRect.rect.offset.x;
    clearRegion.rect.offset.y      = clearRect.rect.offset.y;
    clearRegion.rect.extent.width  = clearRect.rect.extent.width;
    clearRegion.rect.extent.height = clearRect.rect.extent.height;
    clearRegion.startSlice         = clearRect.baseArrayLayer;
    clearRegion.numSlices          = clearRect.layerCount;

    return clearRegion;
}

// =====================================================================================================================
// Overrides range of layers in PAL clear region
VK_INLINE void OverrideLayerRanges(
    Pal::ClearBoundTargetRegion& clearRegion,
    const Pal::Range             layerRange)
{
    VK_ASSERT(clearRegion.startSlice == 0);
    VK_ASSERT(clearRegion.numSlices  == 1);

    clearRegion.startSlice = layerRange.offset;
    clearRegion.numSlices  = layerRange.extent;
}

// =====================================================================================================================
// Overrides range of layers in PAL box
VK_INLINE void OverrideLayerRanges(
    Pal::Box&        box,
    const Pal::Range layerRange)
{
    VK_ASSERT(box.offset.z     == 0);
    VK_ASSERT(box.extent.depth == 1);

    box.offset.z      = layerRange.offset;
    box.extent.depth  = layerRange.extent;
}

// =====================================================================================================================
// Converts Vulkan rect 2D to an equivalent PAL rect
VK_INLINE Pal::Rect VkToPalRect(
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
VK_INLINE void VkToPalViewport(
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
VK_INLINE VkImageUsageFlags VkFormatFeatureFlagsToImageUsageFlags(
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
VK_INLINE void VkToPalScissorRect(
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
VK_INLINE Pal::QueuePriority VkToPalGlobalPriority(
    VkQueueGlobalPriorityEXT vkPriority)
{
    Pal::QueuePriority palPriority = Pal::QueuePriority::Low;
    switch (static_cast<int32_t>(vkPriority))
    {
    case VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT:
        palPriority = Pal::QueuePriority::VeryLow;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT:
        palPriority = Pal::QueuePriority::Low;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT:
        palPriority = Pal::QueuePriority::Medium;
        break;
    case VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT:
        palPriority = Pal::QueuePriority::High;
        break;
    default:
        palPriority = Pal::QueuePriority::Low;
        break;
    }

    return palPriority;
}

} // namespace vk

#endif /* __VK_CONV_H__ */
