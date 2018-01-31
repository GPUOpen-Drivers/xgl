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
;* @file  glslNullFsEmul.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for null fragment shader.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

;
; GLSL source code
;
; #version 450
;
; layout (location = 0) in float fragIn;
; layout (location = 0) out float fragOut;
;
; void main()
; {
;     fragOut = fragIn;
; }
;

define dllexport spir_func void @main() #0 !spirv.ExecutionModel !5
{
.entry:
    %0 = tail call float @llpc.input.import.generic.f32(i32 0, i32 0, i32 0, i32 1) #0
    tail call void @llpc.output.export.generic.f32(i32 0, i32 0, float %0) #0
    ret void
}

declare float @llpc.input.import.generic.f32(i32, i32, i32, i32) #0
declare void @llpc.output.export.generic.f32(i32, i32, float) #0

attributes #0 = { nounwind }

!opencl.kernels = !{}
!spirv.EntryPoints = !{!0}
!opencl.enable.FP_CONTRACT = !{}
!spirv.Source = !{!2}
!opencl.used.extensions = !{!3}
!opencl.used.optional.core.features = !{!3}
!spirv.Generator = !{!4}

!0 = !{void ()* @main, !1}
!1 = !{!"spirv.ExecutionMode.Fragment", i32 1, i32 0, i32 0}
!2 = !{i32 2, i32 450}
!3 = !{}
!4 = !{i16 8, i16 1}
!5 = !{i32 4}
