;**********************************************************************************************************************
;*
;*  Trade secret of Advanced Micro Devices, Inc.
;*  Copyright (c) 2017, Advanced Micro Devices, Inc., (unpublished)
;*
;*  All rights reserved. This notice is intended as a precaution against inadvertent publication and does not imply
;*  publication or any waiver of confidentiality. The year included in the foregoing notice is the year of creation of
;*  the work.
;*
;**********************************************************************************************************************

;**********************************************************************************************************************
;* @file  glslInlineConstOpEmu.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL inline constant buffer operations.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

; GLSL: uniform load float16/int16/uint16 (word) from inline constant buffer
define <2 x i8> @llpc.inlineconst.load.uniform.v2i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    ; TODO: Use buffer.load.i16() to load a WORD.
    ret <2 x i8> undef
}

; GLSL: uniform load f16vec2/i16vec2/u16vec2/float/int/uint (dword) from inline constant buffer
define <4 x i8> @llpc.inlineconst.load.uniform.v4i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast i32 %1 to <4 x i8>
    ret <4 x i8> %2
}

; GLSL: uniform load f16vec3/i16vec3/u16vec3 (wordx3) from inline constant buffer
define <6 x i8> @llpc.inlineconst.load.uniform.v6i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    ; TODO: Use buffer.load.i16() to load a DWORD and a WORD.
    ret <6 x i8> undef
}

; GLSL: uniform load f16vec4/i16vec4/u16vec4/vec2/ivec2/uvec2/double/int64/uint64 (dwordx2) from inline constant buffer
define <8 x i8> @llpc.inlineconst.load.uniform.v8i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast <2 x i32> %1 to <8 x i8>
    ret <8 x i8> %2
}

; GLSL: uniform load vec3/ivec3/uvec3 (dwordx3) from inline constant buffer
define <12 x i8> @llpc.inlineconst.load.uniform.v12i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = shufflevector <4 x i32> %1, <4 x i32> %1, <3 x i32> <i32 0, i32 1, i32 2>
    %3 = bitcast <3 x i32> %2 to <12 x i8>
    ret <12 x i8> %3
}

; GLSL: uniform load vec4/ivec4/uvec4/dvec2/i64vec2/u64vec2 (dwordx4) from inline constant
define <16 x i8> @llpc.inlineconst.load.uniform.v16i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast <4 x i32> %1 to <16 x i8>
    ret <16 x i8> %2
}

; GLSL: uniform load dvec3/i64vec3/u64vec3 (dwordx6) from inline constant
define <24 x i8> @llpc.inlineconst.load.uniform.v24i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = add i32 %memberOffset, 16
    %3 = call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %desc, i32 %2, i1 %glc)
    %4 = shufflevector <2 x i32> %3, <2 x i32> %3, <4 x i32> <i32 0, i32 1, i32 undef, i32 undef>
    %5 = shufflevector <4 x i32> %1, <4 x i32> %4, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5> 
    %6 = bitcast <6 x i32> %5 to <24 x i8>
    ret <24 x i8> %6
}

; GLSL: uniform load dvec4/i64vec4/u64vec4 (dwordx8) from inline constant
define <32 x i8> @llpc.inlineconst.load.uniform.v32i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = add i32 %memberOffset, 16
    %3 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %2, i1 %glc)
    %4 = shufflevector <4 x i32> %1, <4 x i32> %3,
                       <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %5 = bitcast <8 x i32> %4 to <32 x i8>
    ret <32 x i8> %5
}

declare i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32>, i32, i1) #1
declare <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32>, i32, i1) #1
declare <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32>, i32, i1) #1

declare <4 x i32> @llpc.descriptor.load.inlinebuffer(i32 , i32 , i32) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }

!0 = !{}
