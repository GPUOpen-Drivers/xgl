;;
 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
 ;
 ;  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
 ;
 ;  Permission is hereby granted, free of charge, to any person obtaining a copy
 ;  of this software and associated documentation files (the "Software"), to deal
 ;  in the Software without restriction, including without limitation the rights
 ;  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ;  copies of the Software, and to permit persons to whom the Software is
 ;  furnished to do so, subject to the following conditions:
 ;
 ;  The above copyright notice and this permission notice shall be included in all
 ;  copies or substantial portions of the Software.
 ;
 ;  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ;  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ;  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 ;  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 ;  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 ;  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 ;  SOFTWARE.
 ;
 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024"
target triple = "spir64-unknown-unknown"

; =====================================================================================================================
; >>>  Common Built-in Variables
; =====================================================================================================================

; GLSL: in uint gl_SubGroupSize
define i32 @llpc.input.import.builtin.SubgroupSize(i32 %builtInId) #0
{
    ret i32 64
}

; GLSL: in uint gl_SubGroupInvocation
define i32 @llpc.input.import.builtin.SubgroupLocalInvocationId(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    ret i32 %2
}

; GLSL: in uint64_t gl_SubGroupEqMask
define i64 @llpc.input.import.builtin.SubgroupEqMaskKHR(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    %3 = zext i32 %2 to i64

    ; 1 << threadId
    %4 = shl i64 1, %3

    ret i64 %4
}

; GLSL: in uint64_t gl_SubGroupGeMask
define i64 @llpc.input.import.builtin.SubgroupGeMaskKHR(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    %3 = zext i32 %2 to i64

    ; 0xFFFFFFFF'FFFFFFFF << threadId
    %4 = shl i64 -1, %3

    ret i64 %4
}

; GLSL: in uint64_t gl_SubGroupGtMask
define i64 @llpc.input.import.builtin.SubgroupGtMaskKHR(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    %3 = zext i32 %2 to i64

    ; 0xFFFFFFFF'FFFFFFFF << threadId
    %4 = shl i64 -1, %3

    ; 1 << threadId
    %5 = shl i64 1, %3

    ; (0xFFFFFFFF'FFFFFFFF << threadId) ^ (1 << threadId)
    %6 = xor i64 %4, %5

    ret i64 %6
}

; GLSL: in uint64_t gl_SubGroupLeMask
define i64 @llpc.input.import.builtin.SubgroupLeMaskKHR(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    %3 = zext i32 %2 to i64

    ; 0xFFFFFFFF'FFFFFFFF << threadId
    %4 = shl i64 -1, %3

    ; 1 << threadId
    %5 = shl i64 1, %3

    ; (0xFFFFFFFF'FFFFFFFF << threadId) ^ (1 << threadId)
    %6 = xor i64 %4, %5

    ; ~((0xFFFFFFFF'FFFFFFFF << threadId) ^ (1 << threadId))
    %7 = xor i64 %6, -1

    ret i64 %7
}

; GLSL: in uint64_t gl_SubGroupLtMask
define i64 @llpc.input.import.builtin.SubgroupLtMaskKHR(i32 %builtInId) #0
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1

    %3 = zext i32 %2 to i64

    ; 0xFFFFFFFF'FFFFFFFF << threadId
    %4 = shl i64 -1, %3

    ; ~(0xFFFFFFFF'FFFFFFFF << threadId)
    %5 = xor i64 %4, -1

    ret i64 %5
}

; =====================================================================================================================
; >>>  Compute Shader Built-in Variables
; =====================================================================================================================

; GLSL: in uvec3 gl_GloablInvocationID
define <3 x i32> @llpc.input.import.builtin.GlobalInvocationId(i32 %builtInId) #0
{
    %1 = call <3 x i32> @llpc.input.import.builtin.WorkgroupSize(i32 25)
    %2 = call <3 x i32> @llpc.input.import.builtin.WorkgroupId(i32 26)
    %3 = call <3 x i32> @llpc.input.import.builtin.LocalInvocationId(i32 27)

    %4 = mul <3 x i32> %1, %2
    %5 = add <3 x i32> %4, %3

    ret <3 x i32> %5
}

; GLSL: in uint gl_LocationInvocationIndex
define i32 @llpc.input.import.builtin.LocalInvocationIndex(i32 %builtInId) #0
{
    %1 = call <3 x i32> @llpc.input.import.builtin.WorkgroupSize(i32 25)
    %2 = call <3 x i32> @llpc.input.import.builtin.LocalInvocationId(i32 27)

    %3 = extractelement <3 x i32> %1, i32 0 ; gl_WorkGroupSize.x
    %4 = extractelement <3 x i32> %1, i32 1 ; gl_WorkGroupSize.y

    %5 = extractelement <3 x i32> %2, i32 0 ; gl_LocalInvocationID.x
    %6 = extractelement <3 x i32> %2, i32 1 ; gl_LocalInvocationID.y
    %7 = extractelement <3 x i32> %2, i32 2 ; gl_LocalInvocationID.z

    %8 = mul i32 %4, %7
    %9 = add i32 %8, %6
    %10 = mul i32 %3, %9
    %11 = add i32 %10, %5

    ret i32 %11
}

; GLSL: in uint gl_SubgroupID
define i32 @llpc.input.import.builtin.SubgroupId(i32 %builtInId) #0
{
    ; gl_SubgroupID = gl_LocationInvocationIndex / gl_SubgroupSize
    %1 = call i32 @llpc.input.import.builtin.LocalInvocationIndex(i32 29)
    %2 = call i32 @llpc.input.import.builtin.SubgroupSize(i32 36)
    %3 = udiv i32 %1, %2

    ret i32 %3
}

; =====================================================================================================================
; >>>  Fragment Shader Built-in Variables
; =====================================================================================================================

; Gets the offset of sample position relative to the pixel center for the specified sample ID
define <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 %builtInId, i32 %sampleId) #0
{
    ; BuiltInNumSamples (268435464 = 0x10000008)
    %1 = call i32 @llpc.input.import.builtin.NumSamples(i32 268435464)
    ; BuiltInSamplePatternIdx (268435465 = 0x10000009)
    %2 = call i32 @llpc.input.import.builtin.SamplePatternIdx(i32 268435465)
    %3 = add i32 %2, %sampleId

    ; offset = (sampleCount > sampleId) ? (samplePatternOffset + sampleId) : 0
    %4 = icmp ugt i32 %1, %sampleId
    %5 = select i1 %4, i32 %3, i32 0

    ; Load sample position descriptor (GlobalTable (268435456 = 0x10000000), SAMPLEPOS (12))
    %6 = shl i32 %5, 4
    %7 = call <8 x i8> @llpc.buffer.load.v8i8(i32 268435456, i32 12, i32 0, i32 %6, i1 1, i1 0, i1 0)
    %8 = bitcast <8 x i8> %7 to <2 x float>
    ret <2 x float> %8
}

; GLSL: in vec2 gl_SamplePosition
define <2 x float> @llpc.input.import.builtin.SamplePosition(i32 %builtInId) #0
{
    %1 = call i32 @llpc.input.import.builtin.SampleId(i32 18)
    ; BuiltInSamplePosOffset (268435463 = 0x10000007)
    %2 = call <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 268435463, i32 %1) #0
    %3 = fadd <2 x float> %2, <float 0.5, float 0.5>
    ret <2 x float> %3
}

declare <3 x i32> @llpc.input.import.builtin.WorkgroupSize(i32) #0
declare <3 x i32> @llpc.input.import.builtin.WorkgroupId(i32) #0
declare <3 x i32> @llpc.input.import.builtin.LocalInvocationId(i32) #0
declare i32 @llpc.input.import.builtin.NumSamples(i32) #0
declare i32 @llpc.input.import.builtin.SamplePatternIdx(i32) #0
declare i32 @llpc.input.import.builtin.SampleId(i32) #0
declare <8 x i8> @llpc.buffer.load.uniform.v8i8(i32, i32, i32, i32, i1, i1, i1)
declare <8 x i8> @llpc.buffer.load.v8i8(i32, i32, i32, i32, i1, i1, i1)
declare i32 @llvm.amdgcn.mbcnt.lo(i32, i32) #1
declare i32 @llvm.amdgcn.mbcnt.hi(i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
