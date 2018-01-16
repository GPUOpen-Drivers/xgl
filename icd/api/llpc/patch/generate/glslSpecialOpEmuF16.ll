;**********************************************************************************************************************
;*
;*  Trade secret of Advanced Micro Devices, Inc.
;*  Copyright (c) 2018, Advanced Micro Devices, Inc., (unpublished)
;*
;*  All rights reserved. This notice is intended as a precaution against inadvertent publication and does not imply
;*  publication or any waiver of confidentiality. The year included in the foregoing notice is the year of creation of
;*  the work.
;*
;**********************************************************************************************************************

;**********************************************************************************************************************
;* @file  glslSpecialOpEmuF16.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL special graphics-specific operations (float16).
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

; =====================================================================================================================
; >>>  Derivative Functions
; =====================================================================================================================

; GLSL: float16_t dFdx(float16_t)
define half @llpc.dpdx.f16(half %p) #0
{
    ; Broadcast channel 1 to whole quad (32853 = 0x8055)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32853)
    %p0.i16 = trunc i32 %p0.i32 to i16
    %p0 = bitcast i16 %p0.i16 to half
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1.i16 = trunc i32 %p1.i32 to i16
    %p1 = bitcast i16 %p1.i16 to half

    ; Calculate the delta value
    %dpdx = fsub half %p0, %p1

    ret half %dpdx
}

; GLSL: float16_t dFdy(float16_t)
define half @llpc.dpdy.f16(half %p) #0
{
    ; Broadcast channel 2 to whole quad (32938 = 0x80AA)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32938)
    %p0.i16 = trunc i32 %p0.i32 to i16
    %p0 = bitcast i16 %p0.i16 to half
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1.i16 = trunc i32 %p1.i32 to i16
    %p1 = bitcast i16 %p1.i16 to half

    ; Calculate the delta value
    %dpdy = fsub half %p0, %p1

    ret half %dpdy
}

; GLSL: float16_t fwidth(float16_t)
define half @llpc.fwidth.f16(half %p) #0
{
    %1 = call half @llpc.dpdx.f16(half %p)
    %2 = call half @llpc.dpdy.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

; GLSL: float16_t dFdxFine(float16_t)
define half @llpc.dpdxFine.f16(half %p) #0
{
    ; Swizzle channels in quad (1 -> 0, 1 -> 1, 3 -> 2, 3 -> 3) (33013 = 0x80F5)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 33013)
    %p0.i16 = trunc i32 %p0.i32 to i16
    %p0 = bitcast i16 %p0.i16 to half
    ; Swizzle channels in quad (0 -> 0, 0 -> 1, 2 -> 2, 2 -> 3) (32928 = 0x80A0)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32928)
    %p1.i16 = trunc i32 %p1.i32 to i16
    %p1 = bitcast i16 %p1.i16 to half

    ; Calculate the delta value
    %dpdx = fsub half %p0, %p1

    ret half %dpdx
}

; GLSL: float16_t dFdyFine(float16_t)
define half @llpc.dpdyFine.f16(half %p) #0
{
    ; Swizzle channels in quad (2 -> 0, 3 -> 1, 2 -> 2, 3 -> 3) (33006 = 0x80EE)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 33006)
    %p0.i16 = trunc i32 %p0.i32 to i16
    %p0 = bitcast i16 %p0.i16 to half
    ; Swizzle channels in quad (0 -> 0, 1 -> 1, 0 -> 2, 1 -> 3) (32836 = 0x8044)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32836)
    %p1.i16 = trunc i32 %p1.i32 to i16
    %p1 = bitcast i16 %p1.i16 to half

    ; Calculate the delta value
    %dpdy = fsub half %p0, %p1

    ret half %dpdy
}

; GLSL: float16_t fwidthFine(float16_t)
define half @llpc.fwidthFine.f16(half %p) #0
{
    %1 = call half @llpc.dpdxFine.f16(half %p)
    %2 = call half @llpc.dpdyFine.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

; GLSL: float16_t dFdxCoarse(float16_t)
define half @llpc.dpdxCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdx.f16(half %p)
    ret half %1
}

; GLSL: float16_t dFdyCoarse(float16_t)
define half @llpc.dpdyCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdy.f16(half %p)
    ret half %1
}

; GLSL: float16_t fwidthCoarse(float16_t)
define half @llpc.fwidthCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdxCoarse.f16(half %p)
    %2 = call half @llpc.dpdyCoarse.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

declare half @llvm.fabs.f16(half) #0
declare i32 @llvm.amdgcn.ds.swizzle(i32, i32) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone convergent }
attributes #3 = { convergent nounwind }

