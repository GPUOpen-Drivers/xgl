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
;* @file  glslArithOpEmu.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL arithmetic operations (std32).
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

; =====================================================================================================================
; >>>  Common Functions
; =====================================================================================================================

; GLSL: float fma(float, float, float)
define float @llpc.fma.f32(float %x, float %y, float %z) #0
{
    %1 = call float @llvm.fma.f32(float %x, float %y, float %z)
    ret float %1
}

declare float @llvm.fma.f32(float, float, float) #0

attributes #0 = { nounwind }
