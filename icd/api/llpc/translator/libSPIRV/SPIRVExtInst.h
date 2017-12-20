//===- SPIRVBuiltin.h - SPIR-V extended instruction --------------*- C++ -*-===//
//
//                     The LLVM/SPIRV Translator
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright (c) 2014 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimers.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimers in the documentation
// and/or other materials provided with the distribution.
// Neither the names of Advanced Micro Devices, Inc., nor the names of its
// contributors may be used to endorse or promote products derived from this
// Software without specific prior written permission.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
// THE SOFTWARE.
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines SPIR-V extended instructions.
///
//===----------------------------------------------------------------------===//

#ifndef SPIRVBUILTIN_HPP_
#define SPIRVBUILTIN_HPP_

#include "SPIRVUtil.h"
#include "OpenCL.std.h"
#include "GLSL.std.450.h"

#include <string>
#include <vector>

namespace SPIRV{


inline bool
isOpenCLBuiltinSet (SPIRVExtInstSetKind Set) {
  return Set == SPIRVEIS_OpenCL;
}

typedef OpenCLLIB::Entrypoints OCLExtOpKind;

template<> inline void
SPIRVMap<OCLExtOpKind, std::string>::init() {
  add(OpenCLLIB::Acos, "acos");
  add(OpenCLLIB::Acosh, "acosh");
  add(OpenCLLIB::Acospi, "acospi");
  add(OpenCLLIB::Asin, "asin");
  add(OpenCLLIB::Asinh, "asinh");
  add(OpenCLLIB::Asinpi, "asinpi");
  add(OpenCLLIB::Atan, "atan");
  add(OpenCLLIB::Atan2, "atan2");
  add(OpenCLLIB::Atanh, "atanh");
  add(OpenCLLIB::Atanpi, "atanpi");
  add(OpenCLLIB::Atan2pi, "atan2pi");
  add(OpenCLLIB::Cbrt, "cbrt");
  add(OpenCLLIB::Ceil, "ceil");
  add(OpenCLLIB::Copysign, "copysign");
  add(OpenCLLIB::Cos, "cos");
  add(OpenCLLIB::Cosh, "cosh");
  add(OpenCLLIB::Cospi, "cospi");
  add(OpenCLLIB::Erfc, "erfc");
  add(OpenCLLIB::Erf, "erf");
  add(OpenCLLIB::Exp, "exp");
  add(OpenCLLIB::Exp2, "exp2");
  add(OpenCLLIB::Exp10, "exp10");
  add(OpenCLLIB::Expm1, "expm1");
  add(OpenCLLIB::Fabs, "fabs");
  add(OpenCLLIB::Fdim, "fdim");
  add(OpenCLLIB::Floor, "floor");
  add(OpenCLLIB::Fma, "fma");
  add(OpenCLLIB::Fmax, "fmax");
  add(OpenCLLIB::Fmin, "fmin");
  add(OpenCLLIB::Fmod, "fmod");
  add(OpenCLLIB::Fract, "fract");
  add(OpenCLLIB::Frexp, "frexp");
  add(OpenCLLIB::Hypot, "hypot");
  add(OpenCLLIB::Ilogb, "ilogb");
  add(OpenCLLIB::Ldexp, "ldexp");
  add(OpenCLLIB::Lgamma, "lgamma");
  add(OpenCLLIB::Lgamma_r, "lgamma_r");
  add(OpenCLLIB::Log, "log");
  add(OpenCLLIB::Log2, "log2");
  add(OpenCLLIB::Log10, "log10");
  add(OpenCLLIB::Log1p, "log1p");
  add(OpenCLLIB::Logb, "logb");
  add(OpenCLLIB::Mad, "mad");
  add(OpenCLLIB::Maxmag, "maxmag");
  add(OpenCLLIB::Minmag, "minmag");
  add(OpenCLLIB::Modf, "modf");
  add(OpenCLLIB::Nan, "nan");
  add(OpenCLLIB::Nextafter, "nextafter");
  add(OpenCLLIB::Pow, "pow");
  add(OpenCLLIB::Pown, "pown");
  add(OpenCLLIB::Powr, "powr");
  add(OpenCLLIB::Remainder, "remainder");
  add(OpenCLLIB::Remquo, "remquo");
  add(OpenCLLIB::Rint, "rint");
  add(OpenCLLIB::Rootn, "rootn");
  add(OpenCLLIB::Round, "round");
  add(OpenCLLIB::Rsqrt, "rsqrt");
  add(OpenCLLIB::Sin, "sin");
  add(OpenCLLIB::Sincos, "sincos");
  add(OpenCLLIB::Sinh, "sinh");
  add(OpenCLLIB::Sinpi, "sinpi");
  add(OpenCLLIB::Sqrt, "sqrt");
  add(OpenCLLIB::Tan, "tan");
  add(OpenCLLIB::Tanh, "tanh");
  add(OpenCLLIB::Tanpi, "tanpi");
  add(OpenCLLIB::Tgamma, "tgamma");
  add(OpenCLLIB::Trunc, "trunc");
  add(OpenCLLIB::Half_cos, "half_cos");
  add(OpenCLLIB::Half_divide, "half_divide");
  add(OpenCLLIB::Half_exp, "half_exp");
  add(OpenCLLIB::Half_exp2, "half_exp2");
  add(OpenCLLIB::Half_exp10, "half_exp10");
  add(OpenCLLIB::Half_log, "half_log");
  add(OpenCLLIB::Half_log2, "half_log2");
  add(OpenCLLIB::Half_log10, "half_log10");
  add(OpenCLLIB::Half_powr, "half_powr");
  add(OpenCLLIB::Half_recip, "half_recip");
  add(OpenCLLIB::Half_rsqrt, "half_rsqrt");
  add(OpenCLLIB::Half_sin, "half_sin");
  add(OpenCLLIB::Half_sqrt, "half_sqrt");
  add(OpenCLLIB::Half_tan, "half_tan");
  add(OpenCLLIB::Native_cos, "native_cos");
  add(OpenCLLIB::Native_divide, "native_divide");
  add(OpenCLLIB::Native_exp, "native_exp");
  add(OpenCLLIB::Native_exp2, "native_exp2");
  add(OpenCLLIB::Native_exp10, "native_exp10");
  add(OpenCLLIB::Native_log, "native_log");
  add(OpenCLLIB::Native_log2, "native_log2");
  add(OpenCLLIB::Native_log10, "native_log10");
  add(OpenCLLIB::Native_powr, "native_powr");
  add(OpenCLLIB::Native_recip, "native_recip");
  add(OpenCLLIB::Native_rsqrt, "native_rsqrt");
  add(OpenCLLIB::Native_sin, "native_sin");
  add(OpenCLLIB::Native_sqrt, "native_sqrt");
  add(OpenCLLIB::Native_tan, "native_tan");
  add(OpenCLLIB::FClamp, "fclamp");
  add(OpenCLLIB::Degrees, "degrees");
  add(OpenCLLIB::Mix, "mix");
  add(OpenCLLIB::FMax_common, "fmax_common");
  add(OpenCLLIB::FMin_common, "fmin_common");
  add(OpenCLLIB::Radians, "radians");
  add(OpenCLLIB::Step, "step");
  add(OpenCLLIB::Smoothstep, "smoothstep");
  add(OpenCLLIB::Sign, "sign");
  add(OpenCLLIB::Cross, "cross");
  add(OpenCLLIB::Distance, "distance");
  add(OpenCLLIB::Length, "length");
  add(OpenCLLIB::Normalize, "normalize");
  add(OpenCLLIB::Fast_distance, "fast_distance");
  add(OpenCLLIB::Fast_length, "fast_length");
  add(OpenCLLIB::Fast_normalize, "fast_normalize");
  add(OpenCLLIB::Read_imagef, "read_imagef");
  add(OpenCLLIB::Read_imagei, "read_imagei");
  add(OpenCLLIB::Read_imageui, "read_imageui");
  add(OpenCLLIB::Read_imageh, "read_imageh");
  add(OpenCLLIB::Read_imagef_samplerless, "read_imagef_samplerless");
  add(OpenCLLIB::Read_imagei_samplerless, "read_imagei_samplerless");
  add(OpenCLLIB::Read_imageui_samplerless, "read_imageui_samplerless");
  add(OpenCLLIB::Read_imageh_samplerless, "read_imageh_samplerless");
  add(OpenCLLIB::Write_imagef, "write_imagef");
  add(OpenCLLIB::Write_imagei, "write_imagei");
  add(OpenCLLIB::Write_imageui, "write_imageui");
  add(OpenCLLIB::Write_imageh, "write_imageh");
  add(OpenCLLIB::Read_imagef_mipmap_lod, "read_imagef_mipmap_lod");
  add(OpenCLLIB::Read_imagei_mipmap_lod, "read_imagei_mipmap_lod");
  add(OpenCLLIB::Read_imageui_mipmap_lod, "read_imageui_mipmap_lod");
  add(OpenCLLIB::Read_imagef_mipmap_grad, "read_imagef_mipmap_gradient");
  add(OpenCLLIB::Read_imagei_mipmap_grad, "read_imagei_mipmap_gradient");
  add(OpenCLLIB::Read_imageui_mipmap_grad, "read_imageui_mipmap_gradient");
  add(OpenCLLIB::Write_imagef_mipmap_lod, "write_imagef_mipmap_lod");
  add(OpenCLLIB::Write_imagei_mipmap_lod, "write_imagei_mipmap_lod");
  add(OpenCLLIB::Write_imageui_mipmap_lod, "write_imageui_mipmap_lod");
  add(OpenCLLIB::Get_image_width, "get_image_width");
  add(OpenCLLIB::Get_image_height, "get_image_height");
  add(OpenCLLIB::Get_image_depth, "get_image_depth");
  add(OpenCLLIB::Get_image_channel_data_type, "get_image_channel_data_type");
  add(OpenCLLIB::Get_image_channel_order, "get_image_channel_order");
  add(OpenCLLIB::Get_image_dim, "get_image_dim");
  add(OpenCLLIB::Get_image_array_size, "get_image_array_size");
  add(OpenCLLIB::Get_image_num_samples, "get_image_num_samples");
  add(OpenCLLIB::Get_image_num_mip_levels, "get_image_num_mip_levels");
  add(OpenCLLIB::SAbs, "s_abs");
  add(OpenCLLIB::SAbs_diff, "s_abs_diff");
  add(OpenCLLIB::SAdd_sat, "s_add_sat");
  add(OpenCLLIB::UAdd_sat, "u_add_sat");
  add(OpenCLLIB::SHadd, "s_hadd");
  add(OpenCLLIB::UHadd, "u_hadd");
  add(OpenCLLIB::SRhadd, "s_rhadd");
  add(OpenCLLIB::URhadd, "u_rhadd");
  add(OpenCLLIB::SClamp, "s_clamp");
  add(OpenCLLIB::UClamp, "u_clamp");
  add(OpenCLLIB::Clz, "clz");
  add(OpenCLLIB::Ctz, "ctz");
  add(OpenCLLIB::SMad_hi, "s_mad_hi");
  add(OpenCLLIB::SMad_sat, "s_mad_sat");
  add(OpenCLLIB::UMad_sat, "u_mad_sat");
  add(OpenCLLIB::SMax, "s_max");
  add(OpenCLLIB::SMin, "s_min");
  add(OpenCLLIB::UMax, "u_max");
  add(OpenCLLIB::UMin, "u_min");
  add(OpenCLLIB::SMul_hi, "s_mul_hi");
  add(OpenCLLIB::Rotate, "rotate");
  add(OpenCLLIB::SSub_sat, "s_sub_sat");
  add(OpenCLLIB::USub_sat, "u_sub_sat");
  add(OpenCLLIB::U_Upsample, "u_upsample");
  add(OpenCLLIB::S_Upsample, "s_upsample");
  add(OpenCLLIB::Popcount, "popcount");
  add(OpenCLLIB::SMad24, "s_mad24");
  add(OpenCLLIB::UMad24, "u_mad24");
  add(OpenCLLIB::SMul24, "s_mul24");
  add(OpenCLLIB::UMul24, "u_mul24");
  add(OpenCLLIB::Vloadn, "vloadn");
  add(OpenCLLIB::Vstoren, "vstoren");
  add(OpenCLLIB::Vload_half, "vload_half");
  add(OpenCLLIB::Vload_halfn, "vload_halfn");
  add(OpenCLLIB::Vstore_half, "vstore_half");
  add(OpenCLLIB::Vstore_half_r, "vstore_half_r");
  add(OpenCLLIB::Vstore_halfn, "vstore_halfn");
  add(OpenCLLIB::Vstore_halfn_r, "vstore_halfn_r");
  add(OpenCLLIB::Vloada_halfn, "vloada_halfn");
  add(OpenCLLIB::Vstorea_halfn, "vstorea_halfn");
  add(OpenCLLIB::Vstorea_halfn_r, "vstorea_halfn_r");
  add(OpenCLLIB::Shuffle, "shuffle");
  add(OpenCLLIB::Shuffle2, "shuffle2");
  add(OpenCLLIB::Printf, "printf");
  add(OpenCLLIB::Prefetch, "prefetch");
  add(OpenCLLIB::Bitselect, "bitselect");
  add(OpenCLLIB::Select, "select");
  add(OpenCLLIB::UAbs, "u_abs");
  add(OpenCLLIB::UAbs_diff, "u_abs_diff");
  add(OpenCLLIB::UMul_hi, "u_mul_hi");
  add(OpenCLLIB::UMad_hi, "u_mad_hi");
}
SPIRV_DEF_NAMEMAP(OCLExtOpKind, OCLExtOpMap)

inline bool
isReadImage(SPIRVWord EntryPoint) {
  return EntryPoint >= OpenCLLIB::Read_imagef &&
      EntryPoint <= OpenCLLIB::Read_imageui;
}

inline bool
isWriteImage(SPIRVWord EntryPoint) {
  return EntryPoint >= OpenCLLIB::Write_imagef &&
      EntryPoint <= OpenCLLIB::Write_imageui;
}

inline bool
isReadOrWriteImage(SPIRVWord EntryPoint) {
  return isReadImage(EntryPoint) || isWriteImage(EntryPoint);
}

inline bool
isGLSLBuiltinSet(SPIRVExtInstSetKind Set) {
  return Set == SPIRVEIS_GLSL;
}

typedef GLSLstd450 GLSLExtOpKind;

template<> inline void
SPIRVMap<GLSLExtOpKind, std::string>::init() {
  add(GLSLstd450Round, "round");
  add(GLSLstd450RoundEven, "roundEven");
  add(GLSLstd450Trunc, "trunc");
  add(GLSLstd450FAbs, "fabs");
  add(GLSLstd450SAbs, "sabs");
  add(GLSLstd450FSign, "fsign");
  add(GLSLstd450SSign, "ssign");
  add(GLSLstd450Floor, "floor");
  add(GLSLstd450Ceil, "ceil");
  add(GLSLstd450Fract, "fract");

  add(GLSLstd450Radians, "radians");
  add(GLSLstd450Degrees, "degrees");
  add(GLSLstd450Sin, "sin");
  add(GLSLstd450Cos, "cos");
  add(GLSLstd450Tan, "tan");
  add(GLSLstd450Asin, "asin");
  add(GLSLstd450Acos, "acos");
  add(GLSLstd450Atan, "atan");
  add(GLSLstd450Sinh, "sinh");
  add(GLSLstd450Cosh, "cosh");
  add(GLSLstd450Tanh, "tanh");
  add(GLSLstd450Asinh, "asinh");
  add(GLSLstd450Acosh, "acosh");
  add(GLSLstd450Atanh, "atanh");
  add(GLSLstd450Atan2, "atan2");

  add(GLSLstd450Pow, "pow");
  add(GLSLstd450Exp, "exp");
  add(GLSLstd450Log, "log");
  add(GLSLstd450Exp2, "exp2");
  add(GLSLstd450Log2, "log2");
  add(GLSLstd450Sqrt, "sqrt");
  add(GLSLstd450InverseSqrt, "inverseSqrt");

  add(GLSLstd450Determinant, "determinant");
  add(GLSLstd450MatrixInverse, "matrixInverse");

  add(GLSLstd450Modf, "modf");
  add(GLSLstd450ModfStruct, "modfStruct");
  add(GLSLstd450FMin, "fmin");
  add(GLSLstd450UMin, "umin");
  add(GLSLstd450SMin, "smin");
  add(GLSLstd450FMax, "fmax");
  add(GLSLstd450UMax, "umax");
  add(GLSLstd450SMax, "smax");
  add(GLSLstd450FClamp, "fclamp");
  add(GLSLstd450UClamp, "uclamp");
  add(GLSLstd450SClamp, "sclamp");
  add(GLSLstd450FMix, "fmix");
  add(GLSLstd450IMix, "imix");
  add(GLSLstd450Step, "step");
  add(GLSLstd450SmoothStep, "smoothStep");

  add(GLSLstd450Fma, "fma");
  add(GLSLstd450Frexp, "frexp");
  add(GLSLstd450FrexpStruct, "frexpStruct");
  add(GLSLstd450Ldexp, "ldexp");

  add(GLSLstd450PackSnorm4x8, "packSnorm4x8");
  add(GLSLstd450PackUnorm4x8, "packUnorm4x8");
  add(GLSLstd450PackSnorm2x16, "packSnorm2x16");
  add(GLSLstd450PackUnorm2x16, "packUnorm2x16");
  add(GLSLstd450PackHalf2x16, "packHalf2x16");
  add(GLSLstd450PackDouble2x32, "packDouble2x32");
  add(GLSLstd450UnpackSnorm2x16, "unpackSnorm2x16");
  add(GLSLstd450UnpackUnorm2x16, "unpackUnorm2x16");
  add(GLSLstd450UnpackHalf2x16, "unpackHalf2x16");
  add(GLSLstd450UnpackSnorm4x8, "unpackSnorm4x8");
  add(GLSLstd450UnpackUnorm4x8, "unpackUnorm4x8");
  add(GLSLstd450UnpackDouble2x32, "unpackDouble2x32");

  add(GLSLstd450Length, "length");
  add(GLSLstd450Distance, "distance");
  add(GLSLstd450Cross, "cross");
  add(GLSLstd450Normalize, "normalize");
  add(GLSLstd450FaceForward, "faceForward");
  add(GLSLstd450Reflect, "reflect");
  add(GLSLstd450Refract, "refract");

  add(GLSLstd450FindILsb, "findILsb");
  add(GLSLstd450FindSMsb, "findSMsb");
  add(GLSLstd450FindUMsb, "findUMsb");

  add(GLSLstd450InterpolateAtCentroid, "interpolateAtCentroid");
  add(GLSLstd450InterpolateAtSample, "interpolateAtSample");
  add(GLSLstd450InterpolateAtOffset, "interpolateAtOffset");

  add(GLSLstd450NMin, "nmin");
  add(GLSLstd450NMax, "nmax");
  add(GLSLstd450NClamp, "nclamp");
}
SPIRV_DEF_NAMEMAP(GLSLExtOpKind, GLSLExtOpMap)

}

#endif
