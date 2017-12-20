;**********************************************************************************************************************
;*
;* Trade secret of Advanced Micro Devices, Inc.
;* Copyright (c) 2017, Advanced Micro Devices, Inc., (unpublished)
;*
;* All rights reserved. This notice is intended as a precaution against inadvertent publication and does not imply
;* publication or any waiver of confidentiality. The year included in the foregoing notice is the year of creation of
;* the work.
;*
;**********************************************************************************************************************

;**********************************************************************************************************************
;* @file glslPushConstOpEmu.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL push constant (spilled) operations.
;**********************************************************************************************************************

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

; GLSL: load float/int/uint (dword)
define <4 x i8> @llpc.pushconst.load.v4i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <4 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <4 x i8>, <4 x i8> addrspace(2)* %2, align 4
    ret <4 x i8> %3
}

; GLSL: load vec2/ivec2/uvec2/double/int64/uint64 (dwordx2)
define <8 x i8> @llpc.pushconst.load.v8i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <8 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <8 x i8>, <8 x i8> addrspace(2)* %2, align 8
    ret <8 x i8> %3
}

; GLSL: load vec3/ivec3/uvec3 (dwordx3)
define <12 x i8> @llpc.pushconst.load.v12i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <12 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <12 x i8>, <12 x i8> addrspace(2)* %2, align 4
    ret <12 x i8> %3
}

; GLSL: load vec4/ivec4/uvec4/dvec2/i64vec2/u64vec2 (dwordx4)
define <16 x i8> @llpc.pushconst.load.v16i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <16 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <16 x i8>, <16 x i8> addrspace(2)* %2, align 4
    ret <16 x i8> %3
}

; GLSL: load dvec3/i64vec3/u64vec3 (dwordx6)
define <24 x i8> @llpc.pushconst.load.v24i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <24 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <24 x i8>, <24 x i8> addrspace(2)* %2, align 4
    ret <24 x i8> %3
}

; GLSL: load dvec4/i64vec4/u64vec4 (dwordx8)
define <32 x i8> @llpc.pushconst.load.v32i8(i32 %memberOffset, i1 %glc, i1 %slc) #0
{
    %spillTablePtr = call [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable()
    %1 = getelementptr [512 x i8], [512 x i8] addrspace(2)* %spillTablePtr, i32 0, i32 %memberOffset
    %2 = bitcast i8 addrspace(2)* %1 to <32 x i8> addrspace(2)*, !amdgpu.uniform !0
    %3 = load <32 x i8>, <32 x i8> addrspace(2)* %2, align 4
    ret <32 x i8> %3
}

declare [512 x i8] addrspace(2)* @llpc.descriptor.load.spilltable() #0

attributes #0 = { nounwind }

!0 = !{}
