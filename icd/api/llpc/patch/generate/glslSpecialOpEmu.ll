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
; >>> Jump Statement
; =====================================================================================================================

; GLSL: void kill()
define spir_func void @_Z4Killv() #0
{
    call void @llvm.AMDGPU.kill(float -1.0)
    ret void
}

; =====================================================================================================================
; >>>  Derivative Functions
; =====================================================================================================================

; GLSL: float dFdx(float)
define float @llpc.dpdx.f32(float %p) #0
{
    ; Broadcast channel 1 to whole quad (32853 = 0x8055)
    %p.i32 = bitcast float %p to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32853)
    %p0.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.i32)
    %p0 = bitcast i32 %p0.i32.wqm to float
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.i32)
    %p1 = bitcast i32 %p1.i32.wqm to float

    ; Calculate the delta value
    %dpdx = fsub float %p0, %p1

    ret float %dpdx
}

; GLSL: float dFdy(float)
define float @llpc.dpdy.f32(float %p) #0
{
    ; Broadcast channel 2 to whole quad (32938 = 0x80AA)
    %p.i32 = bitcast float %p to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32938)
    %p0.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.i32)
    %p0 = bitcast i32 %p0.i32.wqm to float
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.i32)
    %p1 = bitcast i32 %p1.i32.wqm to float

    ; Calculate the delta value
    %dpdy = fsub float %p0, %p1

    ret float %dpdy
}

; GLSL: float fwidth(float)
define float @llpc.fwidth.f32(float %p) #0
{
    %1 = call float @llpc.dpdx.f32(float %p)
    %2 = call float @llpc.dpdy.f32(float %p)
    %3 = call float @llvm.fabs.f32(float %1)
    %4 = call float @llvm.fabs.f32(float %2)
    %5 = fadd float %3, %4
    ret float %5
}

; GLSL: float dFdxFine(float)
define float @llpc.dpdxFine.f32(float %p) #0
{
    ; Swizzle channels in quad (1 -> 0, 1 -> 1, 3 -> 2, 3 -> 3) (33013 = 0x80F5)
    %p.i32 = bitcast float %p to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 33013)
    %p0.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.i32)
    %p0 = bitcast i32 %p0.i32.wqm to float
    ; Swizzle channels in quad (0 -> 0, 0 -> 1, 2 -> 2, 2 -> 3) (32928 = 0x80A0)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32928)
    %p1.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.i32)
    %p1 = bitcast i32 %p1.i32.wqm to float

    ; Calculate the delta value
    %dpdx = fsub float %p0, %p1

    ret float %dpdx
}

; GLSL: float dFdyFine(float)
define float @llpc.dpdyFine.f32(float %p) #0
{
    ; Swizzle channels in quad (2 -> 0, 3 -> 1, 2 -> 2, 3 -> 3) (33006 = 0x80EE)
    %p.i32 = bitcast float %p to i32
    %p0.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 33006)
    %p0.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.i32)
    %p0 = bitcast i32 %p0.i32.wqm to float
    ; Swizzle channels in quad (0 -> 0, 1 -> 1, 0 -> 2, 1 -> 3) (32836 = 0x8044)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32836)
    %p1.i32.wqm = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.i32)
    %p1 = bitcast i32 %p1.i32.wqm to float

    ; Calculate the delta value
    %dpdy = fsub float %p0, %p1

    ret float %dpdy
}

; GLSL: float fwidthFine(float)
define float @llpc.fwidthFine.f32(float %p) #0
{
    %1 = call float @llpc.dpdxFine.f32(float %p)
    %2 = call float @llpc.dpdyFine.f32(float %p)
    %3 = call float @llvm.fabs.f32(float %1)
    %4 = call float @llvm.fabs.f32(float %2)
    %5 = fadd float %3, %4
    ret float %5
}

; GLSL: float dFdxCoarse(float)
define float @llpc.dpdxCoarse.f32(float %p) #0
{
    %1 = call float @llpc.dpdx.f32(float %p)
    ret float %1
}

; GLSL: float dFdyCoarse(float)
define float @llpc.dpdyCoarse.f32(float %p) #0
{
    %1 = call float @llpc.dpdy.f32(float %p)
    ret float %1
}

; GLSL: float fwidthCoarse(float)
define float @llpc.fwidthCoarse.f32(float %p) #0
{
    %1 = call float @llpc.dpdxCoarse.f32(float %p)
    %2 = call float @llpc.dpdyCoarse.f32(float %p)
    %3 = call float @llvm.fabs.f32(float %1)
    %4 = call float @llvm.fabs.f32(float %2)
    %5 = fadd float %3, %4
    ret float %5
}

; =====================================================================================================================
; >>>  Geometry Shader Functions
; =====================================================================================================================

; GLSL: void EmitStreamVertex(int)
define spir_func void @_Z16EmitStreamVertexi(i32 %stream) #0
{
    ; TODO: Multiple output streams are not supported.
    %cmp = icmp eq i32 %stream, 0
    br i1 %cmp, label %.sendmsg, label %.end

.sendmsg:
    ; [9:8] = stream
    %1 = shl i32 %stream, 8
    ; 34 = 0x22, [3:0] = 2 (GS), [5:4] = 2 (emit)
    %2 = or i32 %1, 34
    ; BuiltInWaveId (268435466 = 0x1000000A)
    %3 = call i32 @llpc.input.import.builtin.GsWaveId(i32 268435466)
    call void @llvm.amdgcn.s.sendmsg(i32 %2, i32 %3)
    br label %.end

.end:
    ret void
}

; GLSL: void EndStreamPrimitive(int)
define spir_func void @_Z18EndStreamPrimitivei(i32 %stream) #0
{
    ; TODO: Multiple output streams are not supported.
    %cmp = icmp eq i32 %stream, 0
    br i1 %cmp, label %.sendmsg, label %.end

.sendmsg:
    ; [9:8] = stream
    %1 = shl i32 %stream, 8
    ; 18 = 0x12, [3:0] = 2 (GS), [5:4] = 1 (cut)
    %2 = or i32 %1, 18
    ; BuiltInWaveId (268435466 = 0x1000000A)
    %3 = call i32 @llpc.input.import.builtin.GsWaveId(i32 268435466)
    call void @llvm.amdgcn.s.sendmsg(i32 %2, i32 %3)
    br label %.end

.end:
    ret void
}

; GLSL: void EmitVertex()
define spir_func void @_Z10EmitVertexv() #0
{
    ; BuiltInWaveId (268435466 = 0x1000000A)
    %1 = call i32 @llpc.input.import.builtin.GsWaveId(i32 268435466)
    ; 34 = 0x22, [3:0] = 2 (GS), [5:4] = 2 (emit), [9:8] = 0 (stream = 0)
    call void @llvm.amdgcn.s.sendmsg(i32 34, i32 %1)
    ret void
}

; GLSL: void EndPrimitive()
define spir_func void @_Z12EndPrimitivev() #0
{
    ; BuiltInWaveId (268435466 = 0x1000000A)
    %1 = call i32 @llpc.input.import.builtin.GsWaveId(i32 268435466)
    ; 18 = 0x12, [3:0] = 2 (GS), [5:4] = 1 (cut), [9:8] = 0 (stream = 0)
    call void @llvm.amdgcn.s.sendmsg(i32 18, i32 %1)
    ret void
}

; =====================================================================================================================
; >>>  Shader Invocation Control Functions
; =====================================================================================================================

; GLSL: void barrier() (non compute shader)
define spir_func void @_Z17sub_group_barrierji(i32 %semantics, i32 %scope) #0
{
    call void @llvm.amdgcn.s.barrier()
    ret void
}

; GLSL: void barrier() (compute shader)
define spir_func void @_Z7barrierj(i32 %semantics) #0
{
    call void @llvm.amdgcn.s.barrier()
    ret void
}

; =====================================================================================================================
; >>>  Shader Memory Control Functions
; =====================================================================================================================

; GLSL: void memoryBarrier()
;       void memoryBarrierBuffer()
;       void memoryBarrierShared()
;       void memoryBarrierImage()
;       void groupMemoryBarrier()
define spir_func void @_Z9mem_fencej(i32 %semantics) #0
{
    ; TODO: We should choose correct waitcnt() and barrier() according to the specified memory semantics,
    ; Currently, the semantics is ignored and we sync all.
    ; 3952 = 0xF70, [3:0] = vm_cnt, [6:4] = exp_cnt, [11:8] = lgkm_cnt
    call void @llvm.amdgcn.s.waitcnt(i32 3952)
    call void @llvm.amdgcn.s.barrier()
    ret void
}

; =====================================================================================================================
; >>>  Shader Invocation Group Functions
; =====================================================================================================================

; GLSL: uint64_t ballot(bool)
define i64 @llpc.subgroup.ballot(i1 %value) #0
{
    %1 = select i1 %value, i32 1, i32 0
    ; Prevent optimization of backend compiler on the control flow
    %2 = call i32 asm sideeffect "; %1", "=v,0"(i32 %1)
    ; 33 = predicate NE
    %3 = call i64 @llvm.amdgcn.icmp.i32(i32 %2, i32 0, i32 33)

    ret i64 %3
}

; GLSL: uvec4 ballot(bool)
define spir_func <4 x i32> @_Z17SubgroupBallotKHRb(i1 %value) #0
{
    %1 = call i64 @llpc.subgroup.ballot(i1 %value)
    %2 = bitcast i64 %1 to <2 x i32>
    %3 = shufflevector <2 x i32> %2, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    ret <4 x i32> %3
}

; GLSL: int/uint readInvocation(int/uint, uint)
define spir_func i32 @_Z25SubgroupReadInvocationKHRii(i32 %value, i32 %invocationIndex)
{
    %1 = call i32 @llvm.amdgcn.readlane(i32 %value, i32 %invocationIndex)
    ret i32 %1
}

; GLSL: float readInvocation(float, uint)
define spir_func float @_Z25SubgroupReadInvocationKHRfi(float %value, i32 %invocationIndex)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z25SubgroupReadInvocationKHRii(i32 %1, i32 %invocationIndex)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: int/uint readFirstInvocation(int/uint)
define spir_func i32 @_Z26SubgroupFirstInvocationKHRi(i32 %value)
{
    %1 = call i32 @llvm.amdgcn.readfirstlane(i32 %value)
    ret i32 %1
}

; GLSL: ivec2/uvec2 readFirstInvocation(ivec2/uvec2)
define spir_func <2 x i32> @_Z26SubgroupFirstInvocationKHRDv2_i(<2 x i32> %value)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %1)
    %4 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %2)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 readFirstInvocation(ivec3/uvec3)
define spir_func <3 x i32> @_Z26SubgroupFirstInvocationKHRDv3_i(<3 x i32> %value)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %1)
    %5 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %2)
    %6 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %3)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 readFirstInvocation(ivec4/uvec4)
define spir_func <4 x i32> @_Z26SubgroupFirstInvocationKHRDv4_i(<4 x i32> %value)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %1)
    %6 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %2)
    %7 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %3)
    %8 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %4)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float readFirstInvocation(float)
define spir_func float @_Z26SubgroupFirstInvocationKHRf(float %value)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %1)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 readFirstInvocation(vec2)
define spir_func <2 x float> @_Z26SubgroupFirstInvocationKHRDv2_f(<2 x float> %value)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z26SubgroupFirstInvocationKHRDv2_i(<2 x i32> %1)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 readFirstInvocation(vec3)
define spir_func <3 x float> @_Z26SubgroupFirstInvocationKHRDv3_f(<3 x float> %value)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z26SubgroupFirstInvocationKHRDv3_i(<3 x i32> %1)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 readFirstInvocation(vec4)
define spir_func <4 x float> @_Z26SubgroupFirstInvocationKHRDv4_f(<4 x float> %value)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z26SubgroupFirstInvocationKHRDv4_i(<4 x i32> %1)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: bool anyInvocation(bool)
define spir_func i1 @_Z14SubgroupAnyKHRb(i1 %value)
{
    %1 = call i64 @llpc.subgroup.ballot(i1 %value)
    %2 = icmp ne i64 %1, 0

    ret i1 %2
}

; GLSL: bool allInvocations(bool)
define spir_func i1 @_Z14SubgroupAllKHRb(i1 %value)
{
    %1 = call i64 @llpc.subgroup.ballot(i1 %value)
    %2 = call i64 @llpc.subgroup.ballot(i1 true)
    %3 = icmp eq i64 %1, %2

    ret i1 %3
}

; GLSL: bool allInvocationsEqual(bool)
define spir_func i1 @_Z19SubgroupAllEqualKHRb(i1 %value)
{
    %1 = call i64 @llpc.subgroup.ballot(i1 %value)
    %2 = call i64 @llpc.subgroup.ballot(i1 true)
    %3 = icmp eq i64 %1, %2
    %4 = icmp eq i64 %1, 0
    %5 = or i1 %3, %4

    ret i1 %5
}

; GLSL: bool subgroupElect()
define spir_func i1 @_Z20GroupNonUniformElecti(i32 %scope)
{
    %1 = call i64 @llpc.subgroup.ballot(i1 true)
    %2 = call i64 @llvm.cttz.i64(i64 %1, i1 true)
    %3 = trunc i64 %2 to i32

    %4 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %5 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %4) #1

    %6 = icmp eq i32 %3, %5
    ret i1 %6
}

; GLSL: bool subgroupAll(bool)
define spir_func i1 @_Z18GroupNonUniformAllib(i32 %scope, i1 %value)
{
    %1 = call i1 @_Z14SubgroupAllKHRb(i1 %value)
    ret i1 %1
}

; GLSL: bool subgroupAny(bool)
define spir_func i1 @_Z18GroupNonUniformAnyib(i32 %scope, i1 %value)
{
    %1 = call i1 @_Z14SubgroupAnyKHRb(i1 %value)
    ret i1 %1
}

; GLSL: bool subgroupAllEqual(int/uint)
define spir_func i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %value)
{
    %1 = icmp ne i32 %value, 0
    %2 = call i1 @_Z19SubgroupAllEqualKHRb(i1 %1)
    ret i1 %2
}

; GLSL: bool subgroupAllEqual(ivec2/uvec2)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv2_i(i32 %scope, <2 x i32> %value)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %1)
    %4 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %2)

    %5 = and i1 %3, %4
    ret i1 %5
}

; GLSL: bool subgroupAllEqual(ivec3/uvec3)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv3_i(i32 %scope, <3 x i32> %value)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 1

    %4 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %1)
    %5 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %2)
    %6 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %3)

    %7 = and i1 %4, %5
    %8 = and i1 %7, %6
    ret i1 %8
}

; GLSL: bool subgroupAllEqual(ivec4/uvec4)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv4_i(i32 %scope, <4 x i32> %value)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %1)
    %6 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %2)
    %7 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %3)
    %8 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %4)

    %9 = and i1 %5, %6
    %10 = and i1 %9, %7
    %11 = and i1 %10, %8
    ret i1 %11
}

; GLSL: bool subgroupAllEqual(float)
define spir_func i1 @_Z23GroupNonUniformAllEqualif(i32 %scope, float %value)
{
    %1 = bitcast float %value to i32
    %2 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(vec2)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv2_f(i32 %scope, <2 x float> %value)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv2_i(i32 %scope, <2 x i32> %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(vec3)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv3_f(i32 %scope, <3 x float> %value)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv3_i(i32 %scope, <3 x i32> %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(vec4)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv4_f(i32 %scope, <4 x float> %value)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv4_i(i32 %scope, <4 x i32> %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(double)
define spir_func i1 @_Z23GroupNonUniformAllEqualid(i32 %scope, double %value)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %2)
    %5 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %3)
    %6 = and i1 %4, %5

    ret i1 %6
}

; GLSL: bool subgroupAllEqual(dvec2)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv2_d(i32 %scope, <2 x double> %value)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = shufflevector <4 x i32> %1, <4 x i32> %1, <2 x i32> <i32 0, i32 2>
    %3 = shufflevector <4 x i32> %1, <4 x i32> %1, <2 x i32> <i32 1, i32 3>

    %4 = call i1 @_Z23GroupNonUniformAllEqualiDv2_i(i32 %scope, <2 x i32> %2)
    %5 = call i1 @_Z23GroupNonUniformAllEqualiDv2_i(i32 %scope, <2 x i32> %3)
    %6 = and i1 %4, %5

    ret i1 %6
}

; GLSL: bool subgroupAllEqual(dvec3)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv3_d(i32 %scope, <3 x double> %value)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <3 x i32> <i32 0, i32 2, i32 4>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <3 x i32> <i32 1, i32 3, i32 5>

    %4 = call i1 @_Z23GroupNonUniformAllEqualiDv3_i(i32 %scope, <3 x i32> %2)
    %5 = call i1 @_Z23GroupNonUniformAllEqualiDv3_i(i32 %scope, <3 x i32> %3)
    %6 = and i1 %4, %5

    ret i1 %6
}

; GLSL: bool subgroupAllEqual(dvec4)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv4_d(i32 %scope, <4 x double> %value)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 1, i32 3, i32 5, i32 7>

    %4 = call i1 @_Z23GroupNonUniformAllEqualiDv4_i(i32 %scope, <4 x i32> %2)
    %5 = call i1 @_Z23GroupNonUniformAllEqualiDv4_i(i32 %scope, <4 x i32> %3)
    %6 = and i1 %4, %5

    ret i1 %6
}

; GLSL: bool subgroupAllEqual(bool)
define spir_func i1 @_Z23GroupNonUniformAllEqualib(i32 %scope, i1 %value)
{
    %1 = zext i1 %value to i32
    %2 = call i1 @_Z23GroupNonUniformAllEqualii(i32 %scope, i32 %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(bvec2)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv2_b(i32 %scope, <2 x i1> %value)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv2_i(i32 %scope, <2 x i32> %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(bvec3)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv3_b(i32 %scope, <3 x i1> %value)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv3_i(i32 %scope, <3 x i32> %1)

    ret i1 %2
}

; GLSL: bool subgroupAllEqual(bvec4)
define spir_func i1 @_Z23GroupNonUniformAllEqualiDv4_b(i32 %scope, <4 x i1> %value)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call i1 @_Z23GroupNonUniformAllEqualiDv4_i(i32 %scope, <4 x i32> %1)

    ret i1 %2
}

; GLSL: int/uint subgroupBroadcast(int/uint, uint)
define spir_func i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %value, i32 %id)
{
    %1 = call i32 @_Z25SubgroupReadInvocationKHRii(i32 %value, i32 %id)
    ret i32 %1
}

; GLSL: ivec2/uvec2 subgroupBroadcast(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z24GroupNonUniformBroadcastiDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupBroadcast(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z24GroupNonUniformBroadcastiDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupBroadcast(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupBroadcast(float, uint)
define spir_func float @_Z24GroupNonUniformBroadcastifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupBroadcast(vec2, uint)
define spir_func <2 x float> @_Z24GroupNonUniformBroadcastiDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupBroadcast(vec3, uint)
define spir_func <3 x float> @_Z24GroupNonUniformBroadcastiDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z24GroupNonUniformBroadcastiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupBroadcast(vec4, uint)
define spir_func <4 x float> @_Z24GroupNonUniformBroadcastiDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupBroadcast(double, uint)
define spir_func double @_Z24GroupNonUniformBroadcastidi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupBroadcast(dvec2, uint)
define spir_func <2 x double> @_Z24GroupNonUniformBroadcastiDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupBroadcast(dvec3, uint)
define spir_func <3 x double> @_Z24GroupNonUniformBroadcastiDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z24GroupNonUniformBroadcastiDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupBroadcast(dvec4, uint)
define spir_func <4 x double> @_Z24GroupNonUniformBroadcastiDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupBroadcast(bool, uint)
define spir_func i1 @_Z24GroupNonUniformBroadcastibi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z24GroupNonUniformBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupBroadcast(bvec2, uint)
define spir_func <2 x i1> @_Z24GroupNonUniformBroadcastiDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupBroadcast(bvec3, uint)
define spir_func <3 x i1> @_Z24GroupNonUniformBroadcastiDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z24GroupNonUniformBroadcastiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupBroadcast(bvec4, uint)
define spir_func <4 x i1> @_Z24GroupNonUniformBroadcastiDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: int/uint subgroupBroadcastFirst(int/uint)
define spir_func i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %value)
{
    %1 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %value)
    ret i32 %1
}

; GLSL: ivec2/uvec2 subgroupBroadcastFirst(ivec2/uvec2)
define spir_func <2 x i32> @_Z29GroupNonUniformBroadcastFirstiDv2_i(i32 %scope, <2 x i32> %value)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %1)
    %4 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %2)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupBroadcastFirst(ivec3/uvec3)
define spir_func <3 x i32> @_Z29GroupNonUniformBroadcastFirstiDv3_i(i32 %scope, <3 x i32> %value)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %1)
    %5 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %2)
    %6 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %3)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupBroadcastFirst(ivec4/uvec4)
define spir_func <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %value)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %1)
    %6 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %2)
    %7 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %3)
    %8 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %4)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupBroadcastFirst(float)
define spir_func float @_Z29GroupNonUniformBroadcastFirstif(i32 %scope, float %value)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %1)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupBroadcastFirst(vec2)
define spir_func <2 x float> @_Z29GroupNonUniformBroadcastFirstiDv2_f(i32 %scope, <2 x float> %value)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z29GroupNonUniformBroadcastFirstiDv2_i(i32 %scope, <2 x i32> %1)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupBroadcastFirst(vec3)
define spir_func <3 x float> @_Z29GroupNonUniformBroadcastFirstiDv3_f(i32 %scope, <3 x float> %value)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z29GroupNonUniformBroadcastFirstiDv3_i(i32 %scope, <3 x i32> %1)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupBroadcastFirst(vec4)
define spir_func <4 x float> @_Z29GroupNonUniformBroadcastFirstiDv4_f(i32 %scope, <4 x float> %value)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %1)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupBroadcastFirst(double)
define spir_func double @_Z29GroupNonUniformBroadcastFirstid(i32 %scope, double %value)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z29GroupNonUniformBroadcastFirstiDv2_i(i32 %scope, <2 x i32> %1)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupBroadcastFirst(dvec2)
define spir_func <2 x double> @_Z29GroupNonUniformBroadcastFirstiDv2_d(i32 %scope, <2 x double> %value)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %1)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupBroadcastFirst(dvec3)
define spir_func <3 x double> @_Z29GroupNonUniformBroadcastFirstiDv3_d(i32 %scope, <3 x double> %value)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %2)
    %5 = call <2 x i32> @_Z29GroupNonUniformBroadcastFirstiDv2_i(i32 %scope, <2 x i32> %3)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupBroadcastFirst(dvec4)
define spir_func <4 x double> @_Z29GroupNonUniformBroadcastFirstiDv4_d(i32 %scope, <4 x double> %value)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %2)
    %5 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %3)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupBroadcastFirst(bool)
define spir_func i1 @_Z29GroupNonUniformBroadcastFirstib(i32 %scope, i1 %value)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z29GroupNonUniformBroadcastFirstii(i32 %scope, i32 %1)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupBroadcastFirst(bvec2)
define spir_func <2 x i1> @_Z29GroupNonUniformBroadcastFirstiDv2_b(i32 %scope, <2 x i1> %value)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z29GroupNonUniformBroadcastFirstiDv2_i(i32 %scope, <2 x i32> %1)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupBroadcastFirst(bvec3)
define spir_func <3 x i1> @_Z29GroupNonUniformBroadcastFirstiDv3_b(i32 %scope, <3 x i1> %value)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z29GroupNonUniformBroadcastFirstiDv3_i(i32 %scope, <3 x i32> %1)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupBroadcastFirst(bvec4, uint)
define spir_func <4 x i1> @_Z29GroupNonUniformBroadcastFirstiDv4_b(i32 %scope, <4 x i1> %value)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z29GroupNonUniformBroadcastFirstiDv4_i(i32 %scope, <4 x i32> %1)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: uvec4 subgroupBallot(bool)
define spir_func <4 x i32> @_Z21GroupNonUniformBallotib(i32 %scope, i1 %value)
{
    %1 = call <4 x i32> @_Z17SubgroupBallotKHRb(i1 %value)
    ret <4 x i32> %1
}

; GLSL: bool subgroupInverseBallot(uvec4)
define spir_func i1 @_Z28GroupNonUniformInverseBallotiDv4_i(i32 %scope, <4 x i32> %value)
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1) #1
    %3 = zext i32 %2 to i64
    %4 = shl i64 1, %3

    %5 = shufflevector <4 x i32> %value, <4 x i32> %value, <2 x i32> <i32 0, i32 1>
    %6 = bitcast <2 x i32> %5 to i64
    %7 = and i64 %4, %6
    %8 = icmp ne i64 %7, 0

    ret i1 %8
}

; GLSL: bool subgroupBallotBitExtract(uvec4, uint)
define spir_func i1 @_Z31GroupNonUniformBallotBitExtractiDv4_ii(i32 %scope, <4 x i32> %value, i32 %index)
{
    %1 = zext i32 %index to i64
    %2 = shl i64 1, %1

    %3 = shufflevector <4 x i32> %value, <4 x i32> %value, <2 x i32> <i32 0, i32 1>
    %4 = bitcast <2 x i32> %3 to i64
    %5 = and i64 %2, %4
    %6 = icmp ne i64 %5, 0

    ret i1 %6
}

; GLSL: uint subgroupBallotBitCount(uvec4)
;       uint subgroupBallotInclusiveBitCount(uvec4)
;       uint subgroupBallotExclusiveBitCount(uvec4)
define spir_func i32 @_Z29GroupNonUniformBallotBitCountiiDv4_i(i32 %scope, i32 %operation, <4 x i32> %value)
{
    %1 = shufflevector <4 x i32> %value, <4 x i32> %value, <2 x i32> <i32 0, i32 1>
    %2 = bitcast <2 x i32> %1 to i64
    %3 = extractelement <2 x i32> %1, i32 0
    %4 = extractelement <2 x i32> %1, i32 1

    switch i32 %operation, label %.default [ i32 0, label %.reduce
                                             i32 1, label %.inclusiveScan
                                             i32 2, label %.exclusiveScan ]

.reduce:
    %5 = call i64 @llvm.ctpop.i64(i64 %2)
    %6 = trunc i64 %5 to i32
    ret i32 %6

.inclusiveScan:
    %7 = call i32 @llvm.amdgcn.mbcnt.lo(i32 %3, i32 0)
    %8 = call i32 @llvm.amdgcn.mbcnt.hi(i32 %4, i32 %7)
    %9 = add i32 %8, 1

    %10 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0)
    %11 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %10)
    %12 = zext i32 %11 to i64
    %13 = shl i64 1, %12

    %14 = and i64 %13, %2
    %15 = icmp ne i64 %14, 0
    %16 = select i1 %15, i32 %9, i32 %8

    ret i32 %16

.exclusiveScan:
    %17 = call i32 @llvm.amdgcn.mbcnt.lo(i32 %3, i32 0)
    %18 = call i32 @llvm.amdgcn.mbcnt.hi(i32 %4, i32 %17)

    ret i32 %18

.default:
    ret i32 0
}

; GLSL: uint subgroupBallotFindLSB(uvec4)
define spir_func i32 @_Z28GroupNonUniformBallotFindLSBiDv4_i(i32 %scope, <4 x i32> %value)
{
    %1 = shufflevector <4 x i32> %value, <4 x i32> %value, <2 x i32> <i32 0, i32 1>
    %2 = bitcast <2 x i32> %1 to i64

    %3 = call i64 @llvm.cttz.i64(i64 %2, i1 true)
    %4 = trunc i64 %3 to i32

    ret i32 %4
}

; GLSL: uint subgroupBallotFindMSB(uvec4)
define spir_func i32 @_Z28GroupNonUniformBallotFindMSBiDv4_i(i32 %scope, <4 x i32> %value)
{
    %1 = shufflevector <4 x i32> %value, <4 x i32> %value, <2 x i32> <i32 0, i32 1>
    %2 = bitcast <2 x i32> %1 to i64

    %3 = call i64 @llvm.ctlz.i64(i64 %2, i1 true)
    %4 = trunc i64 %3 to i32
    %5 = sub i32 63, %4

    ret i32 %5
}

; GLSL: int/uint subgroupShuffle(int/uint, uint)
define spir_func i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %value, i32 %id)
{
    %1 = call i32 @llvm.amdgcn.readlane(i32 %value, i32 %id)
    ret i32 %1
}

; GLSL: ivec2/uvec2 subgroupShuffle(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z22GroupNonUniformShuffleiDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupShuffle(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z22GroupNonUniformShuffleiDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupShuffle(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupShuffle(float, uint)
define spir_func float @_Z22GroupNonUniformShuffleifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupShuffle(vec2, uint)
define spir_func <2 x float> @_Z22GroupNonUniformShuffleiDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z22GroupNonUniformShuffleiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupShuffle(vec3, uint)
define spir_func <3 x float> @_Z22GroupNonUniformShuffleiDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z22GroupNonUniformShuffleiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupShuffle(vec4, uint)
define spir_func <4 x float> @_Z22GroupNonUniformShuffleiDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupShuffle(double, uint)
define spir_func double @_Z22GroupNonUniformShuffleidi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z22GroupNonUniformShuffleiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupShuffle(dvec2, uint)
define spir_func <2 x double> @_Z22GroupNonUniformShuffleiDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupShuffle(dvec3, uint)
define spir_func <3 x double> @_Z22GroupNonUniformShuffleiDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z22GroupNonUniformShuffleiDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupShuffle(dvec4, uint)
define spir_func <4 x double> @_Z22GroupNonUniformShuffleiDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupShuffle(bool, uint)
define spir_func i1 @_Z22GroupNonUniformShuffleibi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupShuffle(bvec2, uint)
define spir_func <2 x i1> @_Z22GroupNonUniformShuffleiDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z22GroupNonUniformShuffleiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupShuffle(bvec3, uint)
define spir_func <3 x i1> @_Z22GroupNonUniformShuffleiDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z22GroupNonUniformShuffleiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupShuffle(bvec4, uint)
define spir_func <4 x i1> @_Z22GroupNonUniformShuffleiDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z22GroupNonUniformShuffleiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: int/uint subgroupShuffleXor(int/uint, uint)
define spir_func i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %value, i32 %mask)
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0)
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1)
    %3 = xor i32 %2, %mask
    %4 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %value, i32 %3)

    ret i32 %4
}

; GLSL: ivec2/uvec2 subgroupShuffleXor(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z25GroupNonUniformShuffleXoriDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupShuffleXor(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z25GroupNonUniformShuffleXoriDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupShuffleXor(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupShuffleXor(float, uint)
define spir_func float @_Z25GroupNonUniformShuffleXorifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupShuffleXor(vec2, uint)
define spir_func <2 x float> @_Z25GroupNonUniformShuffleXoriDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z25GroupNonUniformShuffleXoriDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupShuffleXor(vec3, uint)
define spir_func <3 x float> @_Z25GroupNonUniformShuffleXoriDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z25GroupNonUniformShuffleXoriDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupShuffleXor(vec4, uint)
define spir_func <4 x float> @_Z25GroupNonUniformShuffleXoriDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupShuffleXor(double, uint)
define spir_func double @_Z25GroupNonUniformShuffleXoridi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z25GroupNonUniformShuffleXoriDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupShuffleXor(dvec2, uint)
define spir_func <2 x double> @_Z25GroupNonUniformShuffleXoriDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupShuffleXor(dvec3, uint)
define spir_func <3 x double> @_Z25GroupNonUniformShuffleXoriDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z25GroupNonUniformShuffleXoriDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupShuffleXor(dvec4, uint)
define spir_func <4 x double> @_Z25GroupNonUniformShuffleXoriDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupShuffleXor(bool, uint)
define spir_func i1 @_Z25GroupNonUniformShuffleXoribi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z25GroupNonUniformShuffleXoriii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupShuffleXor(bvec2, uint)
define spir_func <2 x i1> @_Z25GroupNonUniformShuffleXoriDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z25GroupNonUniformShuffleXoriDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupShuffleXor(bvec3, uint)
define spir_func <3 x i1> @_Z25GroupNonUniformShuffleXoriDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z25GroupNonUniformShuffleXoriDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupShuffleXor(bvec4, uint)
define spir_func <4 x i1> @_Z25GroupNonUniformShuffleXoriDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z25GroupNonUniformShuffleXoriDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: int/uint subgroupShuffleUp(int/uint, uint)
define spir_func i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %value, i32 %delta)
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0)
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1)
    %3 = sub i32 %2, %delta
    %4 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %value, i32 %3)

    ret i32 %4
}

; GLSL: ivec2/uvec2 subgroupShuffleUp(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z24GroupNonUniformShuffleUpiDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupShuffleUp(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z24GroupNonUniformShuffleUpiDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupShuffleUp(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupShuffleUp(float, uint)
define spir_func float @_Z24GroupNonUniformShuffleUpifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupShuffleUp(vec2, uint)
define spir_func <2 x float> @_Z24GroupNonUniformShuffleUpiDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformShuffleUpiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupShuffleUp(vec3, uint)
define spir_func <3 x float> @_Z24GroupNonUniformShuffleUpiDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z24GroupNonUniformShuffleUpiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupShuffleUp(vec4, uint)
define spir_func <4 x float> @_Z24GroupNonUniformShuffleUpiDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupShuffleUp(double, uint)
define spir_func double @_Z24GroupNonUniformShuffleUpidi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformShuffleUpiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupShuffleUp(dvec2, uint)
define spir_func <2 x double> @_Z24GroupNonUniformShuffleUpiDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupShuffleUp(dvec3, uint)
define spir_func <3 x double> @_Z24GroupNonUniformShuffleUpiDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z24GroupNonUniformShuffleUpiDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupShuffleUp(dvec4, uint)
define spir_func <4 x double> @_Z24GroupNonUniformShuffleUpiDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupShuffleUp(bool, uint)
define spir_func i1 @_Z24GroupNonUniformShuffleUpibi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z24GroupNonUniformShuffleUpiii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupShuffleUp(bvec2, uint)
define spir_func <2 x i1> @_Z24GroupNonUniformShuffleUpiDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z24GroupNonUniformShuffleUpiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupShuffleUp(bvec3, uint)
define spir_func <3 x i1> @_Z24GroupNonUniformShuffleUpiDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z24GroupNonUniformShuffleUpiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupShuffleUp(bvec4, uint)
define spir_func <4 x i1> @_Z24GroupNonUniformShuffleUpiDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z24GroupNonUniformShuffleUpiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: int/uint subgroupShuffleDown(int/uint, uint)
define spir_func i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %value, i32 %delta)
{
    %1 = call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0)
    %2 = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %1)
    %3 = add i32 %2, %delta
    %4 = call i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %value, i32 %3)

    ret i32 %4
}

; GLSL: ivec2/uvec2 subgroupShuffleDown(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z26GroupNonUniformShuffleDowniDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupShuffleDown(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z26GroupNonUniformShuffleDowniDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupShuffleDown(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupShuffleDown(float, uint)
define spir_func float @_Z26GroupNonUniformShuffleDownifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupShuffleDown(vec2, uint)
define spir_func <2 x float> @_Z26GroupNonUniformShuffleDowniDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z26GroupNonUniformShuffleDowniDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupShuffleDown(vec3, uint)
define spir_func <3 x float> @_Z26GroupNonUniformShuffleDowniDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z26GroupNonUniformShuffleDowniDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupShuffleDown(vec4, uint)
define spir_func <4 x float> @_Z26GroupNonUniformShuffleDowniDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupShuffleDown(double, uint)
define spir_func double @_Z26GroupNonUniformShuffleDownidi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z26GroupNonUniformShuffleDowniDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupShuffleDown(dvec2, uint)
define spir_func <2 x double> @_Z26GroupNonUniformShuffleDowniDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupShuffleDown(dvec3, uint)
define spir_func <3 x double> @_Z26GroupNonUniformShuffleDowniDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z26GroupNonUniformShuffleDowniDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupShuffleDown(dvec4, uint)
define spir_func <4 x double> @_Z26GroupNonUniformShuffleDowniDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupShuffleDown(bool, uint)
define spir_func i1 @_Z26GroupNonUniformShuffleDownibi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z26GroupNonUniformShuffleDowniii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupShuffleDown(bvec2, uint)
define spir_func <2 x i1> @_Z26GroupNonUniformShuffleDowniDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z26GroupNonUniformShuffleDowniDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupShuffleDown(bvec3, uint)
define spir_func <3 x i1> @_Z26GroupNonUniformShuffleDowniDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z26GroupNonUniformShuffleDowniDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupShuffleDown(bvec4, uint)
define spir_func <4 x i1> @_Z26GroupNonUniformShuffleDowniDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z26GroupNonUniformShuffleDowniDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: x [binary] y (32-bit)
define spir_func i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %x, i32 %y)
{
.entry:
    switch i32 %binaryOp, label %.end [i32 0,  label %.iadd
                                       i32 1,  label %.imul
                                       i32 2,  label %.smin
                                       i32 3,  label %.smax
                                       i32 4,  label %.umin
                                       i32 5,  label %.umax
                                       i32 6,  label %.and
                                       i32 7,  label %.or
                                       i32 8,  label %.xor
                                       i32 9,  label %.fmul
                                       i32 10, label %.fmin
                                       i32 11, label %.fmax
                                       i32 12, label %.fadd ]
.iadd:
    %0 = add i32 %x, %y
    br label %.end
.imul:
    %1 = mul i32 %x, %y
    br label %.end
.smin:
    %2 = call i32 @llpc.sminnum.i32(i32 %x, i32 %y)
    br label %.end
.smax:
    %3 = call i32 @llpc.smaxnum.i32(i32 %x, i32 %y)
    br label %.end
.umin:
    %4 = call i32 @llpc.uminnum.i32(i32 %x, i32 %y)
    br label %.end
.umax:
    %5 = call i32 @llpc.umaxnum.i32(i32 %x, i32 %y)
    br label %.end
.and:
    %6 = and i32 %x, %y
    br label %.end
.or:
    %7 = or i32 %x, %y
    br label %.end
.xor:
    %8 = xor i32 %x, %y
    br label %.end
.fmul:
    %x.fmul.f32 = bitcast i32 %x to float
    %y.fmul.f32 = bitcast i32 %y to float
    %fmul.f32 = fmul float %x.fmul.f32, %y.fmul.f32
    %9 = bitcast float %fmul.f32 to i32
    br label %.end
.fmin:
    %x.fmin.f32 = bitcast i32 %x to float
    %y.fmin.f32 = bitcast i32 %y to float
    %fmin.f32 = call float @llvm.minnum.f32(float %x.fmin.f32, float %y.fmin.f32)
    %10 = bitcast float %fmin.f32 to i32
    br label %.end
.fmax:
    %x.fmax.f32 = bitcast i32 %x to float
    %y.fmax.f32 = bitcast i32 %y to float
    %fmax.f32 = call float @llvm.maxnum.f32(float %x.fmax.f32, float %y.fmax.f32)
    %11 = bitcast float %fmax.f32 to i32
    br label %.end
.fadd:
    %x.fadd.f32 = bitcast i32 %x to float
    %y.fadd.f32 = bitcast i32 %y to float
    %fadd.f32 = fadd float %x.fadd.f32, %y.fadd.f32
    %12 = bitcast float %fadd.f32 to i32
    br label %.end
.end:
    %result = phi i32 [undef, %.entry], [%0, %.iadd], [%1, %.imul], [%2, %.smin],
              [%3, %.smax], [%4, %.umin], [%5, %.umax], [%6, %.and], [%7, %.or],
              [%8, %.xor], [%9, %.fmul], [%10, %.fmin], [%11, %.fmax], [%12, %.fadd]
    ret i32 %result
}

; GLSL: x [binary] y (int64_t/uint64_t)
define spir_func i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %x, i64 %y)
{
.entry:
    %x64 = bitcast i64 %x to double
    %y64 = bitcast i64 %y to double
    switch i32 %binaryOp, label %.default [i32 0,  label %.iadd
                                           i32 1,  label %.imul
                                           i32 2,  label %.smin
                                           i32 3,  label %.smax
                                           i32 4,  label %.umin
                                           i32 5,  label %.umax
                                           i32 6,  label %.and
                                           i32 7,  label %.or
                                           i32 8,  label %.xor
                                           i32 9,  label %.fmul
                                           i32 10, label %.fmin
                                           i32 11, label %.fmax
                                           i32 12, label %.fadd ]

.iadd:
    %i0 = add i64 %x, %y
    ret i64 %i0
.imul:
    %i1 = mul i64 %x, %y
    ret i64 %i1
.smin:
    %i2 = call i64 @llpc.sminnum.i64(i64 %x, i64 %y)
    ret i64 %i2
.smax:
    %i3 = call i64 @llpc.smaxnum.i64(i64 %x, i64 %y)
    ret i64 %i3
.umin:
    %i4 = call i64 @llpc.uminnum.i64(i64 %x, i64 %y)
    ret i64 %i4
.umax:
    %i5 = call i64 @llpc.umaxnum.i64(i64 %x, i64 %y)
    ret i64 %i5
.and:
    ret i64 0
.or:
    ret i64 0
.xor:
    ret i64 0
.fadd:
    %f0 = fadd double %x64, %y64
    %iv0 = bitcast double %f0 to i64
    ret i64 %iv0
.fmul:
    %f1 = fmul double %x64, %y64
    %iv1 = bitcast double %f1 to i64
    ret i64 %iv1
.fmin:
    %f2 = call double @llvm.minnum.f64(double %x64, double %y64)
    %iv2 = bitcast double %f2 to i64
    ret i64 %iv2
.fmax:
    %f3 = call double @llvm.maxnum.f64(double %x64, double %y64)
    %iv3 = bitcast double %f3 to i64
    ret i64 %iv3
.default:
    ret i64 0
}

; GLSL: identity (32-bit)
define spir_func i32 @llpc.subgroup.identity.i32(i32 %binaryOp)
{
.entry:
    switch i32 %binaryOp, label %.end [i32 0,  label %.iadd
                                       i32 1,  label %.imul
                                       i32 2,  label %.smin
                                       i32 3,  label %.smax
                                       i32 4,  label %.umin
                                       i32 5,  label %.umax
                                       i32 6,  label %.and
                                       i32 7,  label %.or
                                       i32 8,  label %.xor
                                       i32 9,  label %.fmul
                                       i32 10, label %.fmin
                                       i32 11, label %.fmax
                                       i32 12, label %.fadd ]

.iadd:
    ret i32 0
.imul:
    ret i32 1
.smin:
    ; 0x7FFF FFFF
    ret i32 2147483647
.smax:
    ; 0x80000000
    ret i32 2147483648
.umin:
    ; 0xFFFF FFFF
    ret i32 4294967295
.umax:
    ret i32 0
.and:
    ; 0xFFFF FFFF
    ret i32 4294967295
.or:
    ret i32 0
.xor:
    ret i32 0
.fmul:
    ; 0x3F800000, 1.0
    ret i32 1065353216
.fmin:
    ; 0x7F800000, +1.#INF00E+000
    ret i32 2139095040
.fmax:
    ; 0xFF800000  -1.#INF00E+000
    ret i32 4286578688
.fadd:
    ret i32 0
.end:
    ret i32 0
}

; GLSL: identity (64-bit)
define spir_func i64 @llpc.subgroup.identity.i64(i32 %binaryOp)
{
.entry:
    switch i32 %binaryOp, label %.default [i32 0,  label %.iadd
                                           i32 1,  label %.imul
                                           i32 2,  label %.smin
                                           i32 3,  label %.smax
                                           i32 4,  label %.umin
                                           i32 5,  label %.umax
                                           i32 6,  label %.and
                                           i32 7,  label %.or
                                           i32 8,  label %.xor
                                           i32 9,  label %.fmul
                                           i32 10, label %.fmin
                                           i32 11, label %.fmax
                                           i32 12, label %.fadd ]

.iadd:
    ret i64 0
.imul:
    ret i64 1
.smin:
    ; 0x7FFF FFFF FFFF FFFF
    ret i64 9223372036854775807
.smax:
    ; 0x8000 0000 0000 0000
    ret i64 -9223372036854775808
.umin:
    ; 0xFFFF FFFF FFFF FFFF
    ret i64 -1
.umax:
    ret i64 0
.and:
    ret i64 0
.or:
    ret i64 0
.xor:
    ret i64 0
.fadd:
    ret i64 0
.fmul:
    ; ‭3FF0,0000,0000,0000, 1.0
    ret i64 4607182418800017408
.fmin:
    ; 7FF0,0000,0000,0000, 1.#INF00E+000
    ret i64 9218868437227405312
.fmax:
    ; FFF0,0000,0000,0000, -1.#INF00E+000
    ret i64 -4503599627370496
.default:
    ret i64 0
}

; Emulate ISA: v_cndmask_b32 (32-bit)
define spir_func i32 @llpc.cndmask.i32(i64 %tidmask, i64 %mask, i32 %src0, i32 %src1)
{
    %1 = and i64 %tidmask, %mask
    %2 = icmp ne i64 %1, 0
    %3 = select i1 %2, i32 %src1, i32 %src0

    ret i32 %3
}

; Emulate ISA: v_cndmask_b32 (64-bit)
define spir_func i64 @llpc.cndmask.i64(i64 %tidmask, i64 %mask, i64 %src0, i64 %src1)
{
    %1 = and i64 %tidmask, %mask
    %2 = icmp ne i64 %1, 0
    %3 = select i1 %2, i64 %src1, i64 %src0

    ret i64 %3
}

; Performs ds_swizzle on 64-bit data
define spir_func i64 @llpc.swizzle.i64(i64 %value, i32 %offset)
{
    %value.v2 = bitcast i64 %value to <2 x i32>
    %1 = extractelement <2 x i32> %value.v2, i32 0
    %2 = extractelement <2 x i32> %value.v2, i32 1
    %3 = call i32 @llvm.amdgcn.ds.swizzle(i32 %1, i32 %offset)
    %4 = call i32 @llvm.amdgcn.ds.swizzle(i32 %2, i32 %offset)
    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1
    %7 = bitcast <2 x i32> %6 to i64

    ret i64 %7
}

; Performs readlane on 64-bit data
define spir_func i64 @llpc.readlane.i64(i64 %value, i32 %id)
{
    %value.v2 = bitcast i64 %value to <2 x i32>
    %1 = extractelement <2 x i32> %value.v2, i32 0
    %2 = extractelement <2 x i32> %value.v2, i32 1
    %3 = call i32 @llvm.amdgcn.readlane(i32 %1, i32 %id)
    %4 = call i32 @llvm.amdgcn.readlane(i32 %2, i32 %id)
    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1
    %7 = bitcast <2 x i32> %6 to i64

    ret i64 %7
}

; GLSL: int/uint/float subgroupXXX(int/uint/float)
define spir_func i32 @llpc.subgroup.reduce.i32(i32 %binaryOp, i32 %value)
{
    ; ds_swizzle work in 32 consecutive lanes/threads BIT mode
    ; log2(64) = 6 , so there are 6 iteration of binary ops needed

    ; 1055 ,bit mode, xor mask = 1 ->(SWAP, 1)
    %i1.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 1055)
    %i1.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i1.1)
    %i1.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %value, i32 %i1.2)

    ; 2079 ,bit mode, xor mask = 2 ->(SWAP, 2)
    %i2.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i1.3, i32 2079)
    %i2.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i2.1)
    %i2.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i1.3, i32 %i2.2)

    ; 4127 ,bit mode, xor mask = 4 ->(SWAP, 4)
    %i3.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i2.3, i32 4127)
    %i3.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i3.1)
    %i3.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i2.3, i32 %i3.2)

    ; 8223 ,bit mode, xor mask = 8 ->(SWAP, 8)
    %i4.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i3.3, i32 8223)
    %i4.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i4.1)
    %i4.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i3.3, i32 %i4.2)

    ; 16415 ,bit mode, xor mask = 16 >(SWAP, 16)
    %i5.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i4.3, i32 16415)
    %i5.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i5.1)
    %i5.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i4.3, i32 %i5.2)
    %i5.4 = call i32 @llvm.amdgcn.wwm.i32(i32 %i5.3)

    %i6.1 = call i32 @llvm.amdgcn.readlane(i32 %i5.4, i32 31)
    %i6.2 = call i32 @llvm.amdgcn.readlane(i32 %i5.4, i32 63)
    %i6.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i6.1, i32 %i6.2)
    ret i32 %i6.3
}

; GLSL: int64_t/uint64_t subgroupXXX(int64_t/uint64_t)
define spir_func i64 @llpc.subgroup.reduce.i64(i32 %binaryOp, i64 %value)
{
    ; ds_swizzle work in 32 consecutive lanes/threads BIT mode
    ; log2(64) = 6 , so there are 6 iteration of binary ops needed

    ; 1055 ,bit mode, xor mask = 1 ->(SWAP, 1)
    %i1.1 = call i64 @llpc.swizzle.i64(i64 %value, i32 1055)
    %i1.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i1.1)
    %i1.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %value, i64 %i1.2)

    ; 2079 ,bit mode, xor mask = 2 ->(SWAP, 2)
    %i2.1 = call i64 @llpc.swizzle.i64(i64 %i1.3, i32 2079)
    %i2.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i2.1)
    %i2.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i1.3, i64 %i2.2)

    ; 4127 ,bit mode, xor mask = 4 ->(SWAP, 4)
    %i3.1 = call i64 @llpc.swizzle.i64(i64 %i2.3, i32 4127)
    %i3.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i3.1)
    %i3.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i2.3, i64 %i3.2)

    ; 8223 ,bit mode, xor mask = 8 ->(SWAP, 8)
    %i4.1 = call i64 @llpc.swizzle.i64(i64 %i3.3, i32 8223)
    %i4.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i4.1)
    %i4.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i3.3, i64 %i4.2)

    ; 16415 ,bit mode, xor mask = 16 >(SWAP, 16)
    %i5.1 = call i64 @llpc.swizzle.i64(i64 %i4.3, i32 16415)
    %i5.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i5.1)
    %i5.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i4.3, i64 %i5.2)
    %i5.4 = call i64 @llvm.amdgcn.wwm.i64(i64 %i5.3)

    %i6.1 = call i64 @llpc.readlane.i64(i64 %i5.4, i32 31)
    %i6.2 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i6.1, i64 %i5.4)
    %i6.3 = call i64 @llvm.amdgcn.wwm.i64(i64 %i6.2)
    %i6.4 = call i64 @llpc.readlane.i64(i64 %i6.3, i32 63)

    ret i64 %i6.4
}

; GLSL: int/uint/float subgroupExclusiveXXX(int/uint/float)
define spir_func i32 @llpc.subgroup.exclusiveScan.i32(i32 %binaryOp, i32 %value)
{
    %tid.lo =  call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %tid = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %tid.lo) #1
    %tid.64 = zext i32 %tid to i64
    %tmask = shl i64 1, %tid.64
    %tidmask = call i64 @llvm.amdgcn.wwm.i64(i64 %tmask)

    ; ds_swizzle work in 32 consecutive lanes/threads BIT mode
    ; 11 iteration of binary ops needed
    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i1.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 1055)
    %i1.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i1.1)
    %i1.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %value, i32 %i1.2)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA, update lanes/threads according to mask
    %i1.4 = call i32 @llpc.cndmask.i32(i64 %tidmask , i64 -6148914691236517206, i32 %value, i32 %i1.3)
    %i1.5 = call i32 @llvm.amdgcn.wwm.i32(i32 %i1.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i2.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i1.5, i32 2079)
    %i2.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i2.1)
    %i2.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i1.4, i32 %i2.2)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i2.4 = call i32 @llpc.cndmask.i32(i64 %tidmask ,i64 -8608480567731124088, i32 %i1.4, i32 %i2.3)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i3.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i2.4, i32 4127)
    %i3.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i3.1)
    %i3.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i2.4, i32 %i3.2)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i3.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9187201950435737472, i32 %i2.4, i32 %i3.3)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i4.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i3.4, i32 8223)
    %i4.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i4.1)
    %i4.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i3.4, i32 %i4.2)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i4.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223231297218904064, i32 %i3.4, i32 %i4.3)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i5.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i4.4, i32 16415)
    %i5.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i5.1)
    %i5.3 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i4.4, i32 %i5.2)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i5.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372034707292160, i32 %i4.4, i32 %i5.3)

    ; From now on, scan would be downward
    %identity = call i32 @llpc.subgroup.identity.i32(i32 %binaryOp)
    %i6.1 = call i32 @llvm.amdgcn.readlane(i32 %i5.4, i32 31)
    %i6.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i6.1)
    ; -9223372036854775808 = 0x8000,0000,0000,0000
    %i6.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372036854775808, i32 %i5.4, i32 %i6.2)
    ; 2147483648 = 0x0000,0000,8000,0000
    %i6.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 2147483648, i32 %i6.3, i32 %identity)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i7.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i6.4, i32 16415)
    %i7.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i7.1)
    ; 140737488388096 = 0x0000,8000,0000,8000
    %i7.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 140737488388096, i32 %i6.4, i32 %i7.2)
    %i7.4 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i7.2, i32 %i7.3)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i7.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372034707292160, i32 %i7.3, i32 %i7.4)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i8.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i7.5, i32 8223)
    %i8.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i8.1)
    ; 36029346783166592 = 0x0080,0080,0080,0080
    %i8.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 36029346783166592, i32 %i7.5, i32 %i8.2)
    %i8.4 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i8.2, i32 %i8.3)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i8.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223231297218904064, i32 %i8.3, i32 %i8.4)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i9.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i8.5, i32 4127)
    %i9.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i9.1)
    ; 578721382704613384 = 0x0808,0808,0808,0808
    %i9.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 578721382704613384, i32 %i8.5, i32 %i9.2)
    %i9.4 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i9.2, i32 %i9.3)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i9.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9187201950435737472, i32 %i9.3, i32 %i9.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i10.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i9.5, i32 2079)
    %i10.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i10.1)
    ; 2459565876494606882 = 0x2222,2222,2222,2222
    %i10.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 2459565876494606882, i32 %i9.5, i32 %i10.2)
    %i10.4 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i10.2, i32 %i10.3)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i10.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -8608480567731124088, i32 %i10.3, i32 %i10.4)

    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i11.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i10.5, i32 1055)
    %i11.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i11.1)
    ; 6148914691236517205 = 0x5555,5555,5555,5555
    %i11.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 6148914691236517205, i32 %i10.5, i32 %i11.2)
    %i11.4 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %i11.2, i32 %i11.3)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA
    %i11.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -6148914691236517206, i32 %i11.3, i32 %i11.4)
    %i11.6 = call i32 @llvm.amdgcn.wwm.i32(i32 %i11.5)

    ret i32 %i11.6
}

; GLSL: int64_t/uint64_t subgroupExclusiveXXX(int64_t/uint64_t)
define spir_func i64 @llpc.subgroup.exclusiveScan.i64(i32 %binaryOp, i64 %value)
{
    %tid.lo =  call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %tid = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %tid.lo) #1
    %tid.64 = zext i32 %tid to i64
    %tmask = shl i64 1, %tid.64
    %tidmask = call i64 @llvm.amdgcn.wwm.i64(i64 %tmask)

    ; ds_swizzle work in 32 consecutive lanes/threads BIT mode
    ; 11 iteration of binary ops needed
    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i1.1 = call i64 @llpc.swizzle.i64(i64 %value, i32 1055)
    %i1.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i1.1)
    %i1.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %value, i64 %i1.2)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA, update lanes/threads according to mask
    %i1.4 = call i64 @llpc.cndmask.i64(i64 %tidmask , i64 -6148914691236517206, i64 %value, i64 %i1.3)
    %i1.5 = call i64 @llvm.amdgcn.wwm.i64(i64 %i1.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i2.1 = call i64 @llpc.swizzle.i64(i64 %i1.5, i32 2079)
    %i2.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i2.1)
    %i2.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i1.4, i64 %i2.2)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i2.4 = call i64 @llpc.cndmask.i64(i64 %tidmask ,i64 -8608480567731124088, i64 %i1.4, i64 %i2.3)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i3.1 = call i64 @llpc.swizzle.i64(i64 %i2.4, i32 4127)
    %i3.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i3.1)
    %i3.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i2.4, i64 %i3.2)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i3.4 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9187201950435737472, i64 %i2.4, i64 %i3.3)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i4.1 = call i64 @llpc.swizzle.i64(i64 %i3.4, i32 8223)
    %i4.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i4.1)
    %i4.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i3.4, i64 %i4.2)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i4.4 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9223231297218904064, i64 %i3.4, i64 %i4.3)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i5.1 = call i64 @llpc.swizzle.i64(i64 %i4.4, i32 16415)
    %i5.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i5.1)
    %i5.3 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i4.4, i64 %i5.2)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i5.4 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9223372034707292160, i64 %i4.4, i64 %i5.3)

    ; From now on, scan would be downward
    %identity.i64 = call i64 @llpc.subgroup.identity.i64(i32 %binaryOp)
    %i6.1 = call i64 @llpc.readlane.i64(i64 %i5.4, i32 31)
    %i6.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i6.1)
    ; -9223372036854775808 = 0x8000,0000,0000,0000
    %i6.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9223372036854775808, i64 %i5.4, i64 %i6.2)
    ; 2147483648 = 0x0000,0000,8000,0000
    %i6.4 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 2147483648, i64 %i6.3, i64 %identity.i64)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i7.1 = call i64 @llpc.swizzle.i64(i64 %i6.4, i32 16415)
    %i7.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i7.1)
    ; 140737488388096 = 0x0000,8000,0000,8000
    %i7.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 140737488388096, i64 %i6.4, i64 %i7.2)
    %i7.4 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i7.2, i64 %i7.3)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i7.5 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9223372034707292160, i64 %i7.3, i64 %i7.4)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i8.1 = call i64 @llpc.swizzle.i64(i64 %i7.5, i32 8223)
    %i8.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i8.1)
    ; 36029346783166592 = 0x0080,0080,0080,0080
    %i8.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 36029346783166592, i64 %i7.5, i64 %i8.2)
    %i8.4 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i8.2, i64 %i8.3)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i8.5 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9223231297218904064, i64 %i8.3, i64 %i8.4)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i9.1 = call i64 @llpc.swizzle.i64(i64 %i8.5, i32 4127)
    %i9.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i9.1)
    ; 578721382704613384 = 0x0808,0808,0808,0808
    %i9.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 578721382704613384, i64 %i8.5, i64 %i9.2)
    %i9.4 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i9.2, i64 %i9.3)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i9.5 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -9187201950435737472, i64 %i9.3, i64 %i9.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i10.1 = call i64 @llpc.swizzle.i64(i64 %i9.5, i32 2079)
    %i10.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i10.1)
    ; 2459565876494606882 = 0x2222,2222,2222,2222
    %i10.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 2459565876494606882, i64 %i9.5, i64 %i10.2)
    %i10.4 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i10.2, i64 %i10.3)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i10.5 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -8608480567731124088, i64 %i10.3, i64 %i10.4)

    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i11.1 = call i64 @llpc.swizzle.i64(i64 %i10.5, i32 1055)
    %i11.2 = call i64 @llvm.amdgcn.wwm.i64(i64 %i11.1)
    ; 6148914691236517205 = 0x5555,5555,5555,5555
    %i11.3 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 6148914691236517205, i64 %i10.5, i64 %i11.2)
    %i11.4 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %i11.2, i64 %i11.3)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA
    %i11.5 = call i64 @llpc.cndmask.i64(i64 %tidmask, i64 -6148914691236517206, i64 %i11.3, i64 %i11.4)
    %i11.6 = call i64 @llvm.amdgcn.wwm.i64(i64 %i11.5)

    ret i64 %i11.6
}

; GLSL: int/uint/float subgroupInclusiveXXX(int/uint/float)
define spir_func i32 @llpc.subgroup.inclusiveScan.i32(i32 %binaryOp, i32 %value)
{
    %1 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 %binaryOp, i32 %value)
    %2 = call i32 @llpc.subgroup.arithmetic.i32(i32 %binaryOp, i32 %1, i32 %value)

    ret i32 %2
}

; GLSL: int64_t/uint64_t subgroupInclusiveXXX(int64_t/uint64_t)
define spir_func i64 @llpc.subgroup.inclusiveScan.i64(i32 %binaryOp, i64 %value)
{
    %1 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 %binaryOp, i64 %value)
    %2 = call i64 @llpc.subgroup.arithmetic.i64(i32 %binaryOp, i64 %1, i64 %value)

    ret i64 %2
}

; Set values to all inactive lanes (32 bit)
define spir_func i32 @llpc.subgroup.set.inactive.i32(i32 %binaryOp, i32 %value)
{
    ; Get identity value of binary operations
    %identity = call i32 @llpc.subgroup.identity.i32(i32 %binaryOp)
    ; Set identity value for the inactive threads
    %activeValue = call i32 @llvm.amdgcn.set.inactive.i32(i32 %value, i32 %identity)

    ret i32 %activeValue
}

; Set values to all inactive lanes (64 bit)
define spir_func i64 @llpc.subgroup.set.inactive.i64(i32 %binaryOp, i64 %value)
{
    %identity = call i64 @llpc.subgroup.identity.i64(i32 %binaryOp)
    %activeValue = call i64 @llvm.amdgcn.set.inactive.i64(i64 %value, i64 %identity)

    ret i64 %activeValue
}

; GLSL: int/uint subgroupAdd(int/uint)
define spir_func i32 @_Z31sub_group_reduce_add_nonuniformi(i32 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 0, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupAdd(ivec2/uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_add_nonuniformDv2_i(<2 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupAdd(ivec3/uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_add_nonuniformDv3_i(<3 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupAdd(ivec4/uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_add_nonuniformDv4_i(<4 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 0 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupInclusiveAdd(int/uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_add_nonuniformi(i32 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupInclusiveAdd(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_i(<2 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupInclusiveAdd(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_i(<3 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupInclusiveAdd(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_i(<4 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 0 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupExclusiveAdd(int/uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_add_nonuniformi(i32 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupExclusiveAdd(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_i(<2 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupExclusiveAdd(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_i(<3 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupExclusiveAdd(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_i(<4 x i32> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 0, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 0 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: float subgroupAdd(float)
define spir_func float @_Z31sub_group_reduce_add_nonuniformf(float %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 12, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupAdd(vec2)
define spir_func <2 x float> @_Z31sub_group_reduce_add_nonuniformDv2_f(<2 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupAdd(vec3)
define spir_func <3 x float> @_Z31sub_group_reduce_add_nonuniformDv3_f(<3 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupAdd(vec4)
define spir_func <4 x float> @_Z31sub_group_reduce_add_nonuniformDv4_f(<4 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 12 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupInclusiveAdd(float)
define spir_func float @_Z39sub_group_scan_inclusive_add_nonuniformf(float %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupInclusiveAdd(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_f(<2 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupInclusiveAdd(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_f(<3 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupInclusiveAdd(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_f(<4 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 12 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupExclusiveAdd(float)
define spir_func float @_Z39sub_group_scan_exclusive_add_nonuniformf(float %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupExclusiveAdd(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_f(<2 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupExclusiveAdd(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_f(<3 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupExclusiveAdd(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_f(<4 x float> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 12, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 12 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: double subgroupAdd(double)
define spir_func double @_Z31sub_group_reduce_add_nonuniformd(double %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %1)
    %3 = call i64 @llpc.subgroup.reduce.i64(i32 12, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupAdd(dvec2)
define spir_func <2 x double> @_Z31sub_group_reduce_add_nonuniformDv2_d(<2 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)

    %6 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %4)
    %7 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupAdd(dvec3)
define spir_func <3 x double> @_Z31sub_group_reduce_add_nonuniformDv3_d(<3 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)

    %8 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %6)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupAdd(dvec4)
define spir_func <4 x double> @_Z31sub_group_reduce_add_nonuniformDv4_d(<4 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %5)

    %10 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %8)
    %13 = call i64 @llpc.subgroup.reduce.i64(i32 12 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupInclusiveAdd(double)
define spir_func double @_Z39sub_group_scan_inclusive_add_nonuniformd(double %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %1)
    %3 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupInclusiveAdd(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_d(<2 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)

    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %4)
    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupInclusiveAdd(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_d(<3 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)

    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %6)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupInclusiveAdd(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_d(<4 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %5)

    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %8)
    %13 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 12 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupExclusiveAdd(double)
define spir_func double @_Z39sub_group_scan_exclusive_add_nonuniformd(double %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %1)
    %3 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupExclusiveAdd(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_d(<2 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)

    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %4)
    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupExclusiveAdd(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_d(<3 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)

    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %6)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupExclusiveAdd(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_d(<4 x double> %value)
{
    ; 12 = arithmetic fadd
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 12, i64 %5)

    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %8)
    %13 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 12 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: int/uint subgroupMul(int/uint)
define spir_func i32 @_Z31sub_group_reduce_mul_nonuniformi(i32 %value)
{
    ; 1 = arithmetic imul
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 1, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupMul(ivec2/uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_mul_nonuniformDv2_i(<2 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupMul(ivec3/uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_mul_nonuniformDv3_i(<3 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupMul(ivec4/uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_mul_nonuniformDv4_i(<4 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 1 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupInclusiveMul(int/uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_mul_nonuniformi(i32 %value)
{
    ; 1 = arithmetic imul
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupInclusiveMul(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_mul_nonuniformDv2_i(<2 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupInclusiveMul(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_mul_nonuniformDv3_i(<3 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupInclusiveMul(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_mul_nonuniformDv4_i(<4 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 1 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupExclusiveMul(int/uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_mul_nonuniformi(i32 %value)
{
    ; 1 = arithmetic imul
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupExclusiveMul(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_mul_nonuniformDv2_i(<2 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupExclusiveMul(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_mul_nonuniformDv3_i(<3 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupExclusiveMul(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_mul_nonuniformDv4_i(<4 x i32> %value)
{
    ; 1 = arithmetic imul

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 1, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 1 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: float subgroupMul(float)
define spir_func float @_Z31sub_group_reduce_mul_nonuniformf(float %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 9, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupMul(vec2)
define spir_func <2 x float> @_Z31sub_group_reduce_mul_nonuniformDv2_f(<2 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupMul(vec3)
define spir_func <3 x float> @_Z31sub_group_reduce_mul_nonuniformDv3_f(<3 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupMul(vec4)
define spir_func <4 x float> @_Z31sub_group_reduce_mul_nonuniformDv4_f(<4 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 9 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupInclusiveMul(float)
define spir_func float @_Z39sub_group_scan_inclusive_mul_nonuniformf(float %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupInclusiveMul(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_inclusive_mul_nonuniformDv2_f(<2 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupInclusiveMul(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_inclusive_mul_nonuniformDv3_f(<3 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupInclusiveMul(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_inclusive_mul_nonuniformDv4_f(<4 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 9 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupExclusiveMul(float)
define spir_func float @_Z39sub_group_scan_exclusive_mul_nonuniformf(float %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupExclusiveMul(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_exclusive_mul_nonuniformDv2_f(<2 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupExclusiveMul(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_exclusive_mul_nonuniformDv3_f(<3 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupExclusiveMul(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_exclusive_mul_nonuniformDv4_f(<4 x float> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 9, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 9 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: double subgroupMul(double)
define spir_func double @_Z31sub_group_reduce_mul_nonuniformd(double %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %1)
    %3 = call i64 @llpc.subgroup.reduce.i64(i32 9, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupMul(dvec2)
define spir_func <2 x double> @_Z31sub_group_reduce_mul_nonuniformDv2_d(<2 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)

    %6 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %4)
    %7 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupMul(dvec3)
define spir_func <3 x double> @_Z31sub_group_reduce_mul_nonuniformDv3_d(<3 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)

    %8 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %6)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupMul(dvec4)
define spir_func <4 x double> @_Z31sub_group_reduce_mul_nonuniformDv4_d(<4 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %5)

    %10 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %8)
    %13 = call i64 @llpc.subgroup.reduce.i64(i32 9 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupInclusiveMul(double)
define spir_func double @_Z39sub_group_scan_inclusive_mul_nonuniformd(double %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %1)
    %3 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupInclusiveMul(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_inclusive_mul_nonuniformDv2_d(<2 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)

    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %4)
    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupInclusiveMul(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_inclusive_mul_nonuniformDv3_d(<3 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)

    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %6)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupInclusiveMul(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_inclusive_mul_nonuniformDv4_d(<4 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %5)

    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %8)
    %13 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 9 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupExclusiveMul(double)
define spir_func double @_Z39sub_group_scan_exclusive_mul_nonuniformd(double %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %1)
    %3 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupExclusiveMul(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_exclusive_mul_nonuniformDv2_d(<2 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)

    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %4)
    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupExclusiveMul(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_exclusive_mul_nonuniformDv3_d(<3 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)

    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %6)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupExclusiveMul(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_exclusive_mul_nonuniformDv4_d(<4 x double> %value)
{
    ; 9 = arithmetic fmul
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 9, i64 %5)

    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %8)
    %13 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 9 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: int subgroupMin(int)
define spir_func i32 @_Z31sub_group_reduce_min_nonuniformi(i32 %value)
{
    ; 2 = arithmetic imin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 2, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupMin(ivec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_min_nonuniformDv2_i(<2 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupMin(ivec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_min_nonuniformDv3_i(<3 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupMin(ivec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_min_nonuniformDv4_i(<4 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 2 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int subgroupInclusiveMin(int)
define spir_func i32 @_Z39sub_group_scan_inclusive_min_nonuniformi(i32 %value)
{
    ; 2 = arithmetic imin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupInclusiveMin(ivec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_i(<2 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupInclusiveMin(ivec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_i(<3 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupInclusiveMin(ivec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_i(<4 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 2 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int subgroupExclusiveMin(int)
define spir_func i32 @_Z39sub_group_scan_exclusive_min_nonuniformi(i32 %value)
{
    ; 2 = arithmetic imin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupExclusiveMin(ivec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_i(<2 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupExclusiveMin(ivec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_i(<3 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupExclusiveMin(ivec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_i(<4 x i32> %value)
{
    ; 2 = arithmetic imin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 2, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 2 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupMin(uint)
define spir_func i32 @_Z31sub_group_reduce_min_nonuniformj(i32 %value)
{
    ; 4 = arithmetic jmin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 4, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupMin(uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_min_nonuniformDv2_j(<2 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupMin(uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_min_nonuniformDv3_j(<3 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupMin(uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_min_nonuniformDv4_j(<4 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 4 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupInclusiveMin(uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_min_nonuniformj(i32 %value)
{
    ; 4 = arithmetic jmin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupInclusiveMin(uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_j(<2 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupInclusiveMin(uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_j(<3 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupInclusiveMin(uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_j(<4 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 4 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupExclusiveMin(uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_min_nonuniformj(i32 %value)
{
    ; 4 = arithmetic jmin
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupExclusiveMin(uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_j(<2 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupExclusiveMin(uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_j(<3 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupExclusiveMin(uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_j(<4 x i32> %value)
{
    ; 4 = arithmetic jmin

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 4, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 4 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: float subgroupMin(float)
define spir_func float @_Z31sub_group_reduce_min_nonuniformf(float %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 10, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupMin(vec2)
define spir_func <2 x float> @_Z31sub_group_reduce_min_nonuniformDv2_f(<2 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupMin(vec3)
define spir_func <3 x float> @_Z31sub_group_reduce_min_nonuniformDv3_f(<3 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupMin(vec4)
define spir_func <4 x float> @_Z31sub_group_reduce_min_nonuniformDv4_f(<4 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 10 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupInclusiveMin(float)
define spir_func float @_Z39sub_group_scan_inclusive_min_nonuniformf(float %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupInclusiveMin(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_f(<2 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupInclusiveMin(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_f(<3 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupInclusiveMin(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_f(<4 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 10 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupExclusiveMin(float)
define spir_func float @_Z39sub_group_scan_exclusive_min_nonuniformf(float %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupExclusiveMin(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_f(<2 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupExclusiveMin(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_f(<3 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupExclusiveMin(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_f(<4 x float> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 10, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 10 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: double subgroupMin(double)
define spir_func double @_Z31sub_group_reduce_min_nonuniformd(double %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %1)
    %3 = call i64 @llpc.subgroup.reduce.i64(i32 10, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupMin(dvec2)
define spir_func <2 x double> @_Z31sub_group_reduce_min_nonuniformDv2_d(<2 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)

    %6 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %4)
    %7 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupMin(dvec3)
define spir_func <3 x double> @_Z31sub_group_reduce_min_nonuniformDv3_d(<3 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)

    %8 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %6)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupMin(dvec4)
define spir_func <4 x double> @_Z31sub_group_reduce_min_nonuniformDv4_d(<4 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %5)

    %10 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %8)
    %13 = call i64 @llpc.subgroup.reduce.i64(i32 10 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupInclusiveMin(double)
define spir_func double @_Z39sub_group_scan_inclusive_min_nonuniformd(double %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %1)
    %3 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupInclusiveMin(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_d(<2 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)

    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %4)
    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupInclusiveMin(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_d(<3 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)

    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %6)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupInclusiveMin(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_d(<4 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %5)

    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %8)
    %13 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 10 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupExclusiveMin(double)
define spir_func double @_Z39sub_group_scan_exclusive_min_nonuniformd(double %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %1)
    %3 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupExclusiveMin(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_d(<2 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)

    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %4)
    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupExclusiveMin(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_d(<3 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)

    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %6)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupExclusiveMin(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_d(<4 x double> %value)
{
    ; 10 = arithmetic fmin
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 10, i64 %5)

    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %8)
    %13 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 10 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: int subgroupMax(int)
define spir_func i32 @_Z31sub_group_reduce_max_nonuniformi(i32 %value)
{
    ; 3 = arithmetic imax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 3, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupMax(ivec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_max_nonuniformDv2_i(<2 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupMax(ivec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_max_nonuniformDv3_i(<3 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupMax(ivec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_max_nonuniformDv4_i(<4 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 3 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int subgroupInclusiveMax(int)
define spir_func i32 @_Z39sub_group_scan_inclusive_max_nonuniformi(i32 %value)
{
    ; 3 = arithmetic imax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupInclusiveMax(ivec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_i(<2 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupInclusiveMax(ivec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_i(<3 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupInclusiveMax(ivec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_i(<4 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 3 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int subgroupExclusiveMax(int)
define spir_func i32 @_Z39sub_group_scan_exclusive_max_nonuniformi(i32 %value)
{
    ; 3 = arithmetic imax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3, i32 %1)

    ret i32 %2
}

; GLSL: ivec2 subgroupExclusiveMax(ivec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_i(<2 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3 subgroupExclusiveMax(ivec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_i(<3 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4 subgroupExclusiveMax(ivec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_i(<4 x i32> %value)
{
    ; 3 = arithmetic imax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 3, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 3 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupMax(uint)
define spir_func i32 @_Z31sub_group_reduce_max_nonuniformj(i32 %value)
{
    ; 5 = arithmetic jmax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 5, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupMax(uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_max_nonuniformDv2_j(<2 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupMax(uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_max_nonuniformDv3_j(<3 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupMax(uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_max_nonuniformDv4_j(<4 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 5 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupInclusiveMax(uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_max_nonuniformj(i32 %value)
{
    ; 5 = arithmetic jmax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupInclusiveMax(uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_j(<2 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupInclusiveMax(uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_j(<3 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupInclusiveMax(uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_j(<4 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 5 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: uint subgroupExclusiveMax(uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_max_nonuniformj(i32 %value)
{
    ; 5 = arithmetic jmax
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5, i32 %1)

    ret i32 %2
}

; GLSL: uvec2 subgroupExclusiveMax(uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_j(<2 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: uvec3 subgroupExclusiveMax(uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_j(<3 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: uvec4 subgroupExclusiveMax(uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_j(<4 x i32> %value)
{
    ; 5 = arithmetic jmax

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 5, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 5 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: float subgroupMax(float)
define spir_func float @_Z31sub_group_reduce_max_nonuniformf(float %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 11, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupMax(vec2)
define spir_func <2 x float> @_Z31sub_group_reduce_max_nonuniformDv2_f(<2 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupMax(vec3)
define spir_func <3 x float> @_Z31sub_group_reduce_max_nonuniformDv3_f(<3 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupMax(vec4)
define spir_func <4 x float> @_Z31sub_group_reduce_max_nonuniformDv4_f(<4 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 11 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupInclusiveMax(float)
define spir_func float @_Z39sub_group_scan_inclusive_max_nonuniformf(float %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupInclusiveMax(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_f(<2 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupInclusiveMax(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_f(<3 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupInclusiveMax(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_f(<4 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 11 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: float subgroupExclusiveMax(float)
define spir_func float @_Z39sub_group_scan_exclusive_max_nonuniformf(float %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast float %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 subgroupExclusiveMax(vec2)
define spir_func <2 x float> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_f(<2 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x float> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = bitcast <2 x i32> %9 to <2 x float>

    ret <2 x float> %10
}

; GLSL: vec3 subgroupExclusiveMax(vec3)
define spir_func <3 x float> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_f(<3 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x float> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = bitcast <3 x i32> %13 to <3 x float>

    ret <3 x float> %14
}

; GLSL: vec4 subgroupExclusiveMax(vec4)
define spir_func <4 x float> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_f(<4 x float> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x float> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 11, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 11 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = bitcast <4 x i32> %17 to <4 x float>

    ret <4 x float> %18
}

; GLSL: double subgroupMax(double)
define spir_func double @_Z31sub_group_reduce_max_nonuniformd(double %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %1)
    %3 = call i64 @llpc.subgroup.reduce.i64(i32 11, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupMax(dvec2)
define spir_func <2 x double> @_Z31sub_group_reduce_max_nonuniformDv2_d(<2 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)

    %6 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %4)
    %7 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupMax(dvec3)
define spir_func <3 x double> @_Z31sub_group_reduce_max_nonuniformDv3_d(<3 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)

    %8 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %6)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupMax(dvec4)
define spir_func <4 x double> @_Z31sub_group_reduce_max_nonuniformDv4_d(<4 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %5)

    %10 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %8)
    %13 = call i64 @llpc.subgroup.reduce.i64(i32 11 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupInclusiveMax(double)
define spir_func double @_Z39sub_group_scan_inclusive_max_nonuniformd(double %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %1)
    %3 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupInclusiveMax(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_d(<2 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)

    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %4)
    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupInclusiveMax(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_d(<3 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)

    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %6)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupInclusiveMax(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_d(<4 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %5)

    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %8)
    %13 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 11 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: double subgroupExclusiveMax(double)
define spir_func double @_Z39sub_group_scan_exclusive_max_nonuniformd(double %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast double %value to i64
    %2 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %1)
    %3 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11, i64 %2)
    %4 = bitcast i64 %3 to double

    ret double %4
}

; GLSL: dvec2 subgroupExclusiveMax(dvec2)
define spir_func <2 x double> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_d(<2 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <2 x double> %value to <2 x i64>

    %2 = extractelement <2 x i64> %1, i32 0
    %3 = extractelement <2 x i64> %1, i32 1

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)

    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %4)
    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %5)

    %8 = insertelement <2 x i64> undef, i64 %6, i64 0
    %9 = insertelement <2 x i64> %8, i64 %7, i64 1
    %10 = bitcast <2 x i64> %9 to <2 x double>

    ret <2 x double> %10
}

; GLSL: dvec3 subgroupExclusiveMax(dvec3)
define spir_func <3 x double> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_d(<3 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <3 x double> %value to <3 x i64>

    %2 = extractelement <3 x i64> %1, i32 0
    %3 = extractelement <3 x i64> %1, i32 1
    %4 = extractelement <3 x i64> %1, i32 2

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)

    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %6)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %7)

    %11 = insertelement <3 x i64> undef, i64 %8, i64 0
    %12 = insertelement <3 x i64> %11, i64 %9, i64 1
    %13 = insertelement <3 x i64> %12, i64 %10, i64 2
    %14 = bitcast <3 x i64> %13 to <3 x double>

    ret <3 x double> %14
}

; GLSL: dvec4 subgroupExclusiveMax(dvec4)
define spir_func <4 x double> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_d(<4 x double> %value)
{
    ; 11 = arithmetic fmax
    %1 = bitcast <4 x double> %value to <4 x i64>

    %2 = extractelement <4 x i64> %1, i32 0
    %3 = extractelement <4 x i64> %1, i32 1
    %4 = extractelement <4 x i64> %1, i32 2
    %5 = extractelement <4 x i64> %1, i32 3

    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %4)
    %9 = call i64 @llpc.subgroup.set.inactive.i64(i32 11, i64 %5)

    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %8)
    %13 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 11 ,i64 %9)

    %14 = insertelement <4 x i64> undef, i64 %10, i64 0
    %15 = insertelement <4 x i64> %14, i64 %11, i64 1
    %16 = insertelement <4 x i64> %15, i64 %12, i64 2
    %17 = insertelement <4 x i64> %16, i64 %13, i64 3
    %18 = bitcast <4 x i64> %17 to <4 x double>

    ret <4 x double> %18
}

; GLSL: int/uint subgroupAnd(int/uint)
define spir_func i32 @_Z31sub_group_reduce_and_nonuniformi(i32 %value)
{
    ; 6 = arithmetic iand
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 6, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupAnd(ivec2/uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_and_nonuniformDv2_i(<2 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupAnd(ivec3/uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_and_nonuniformDv3_i(<3 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupAnd(ivec4/uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_and_nonuniformDv4_i(<4 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupInclusiveAnd(int/uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_and_nonuniformi(i32 %value)
{
    ; 6 = arithmetic iand
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupInclusiveAnd(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_and_nonuniformDv2_i(<2 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupInclusiveAnd(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_and_nonuniformDv3_i(<3 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupInclusiveAnd(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_and_nonuniformDv4_i(<4 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupExclusiveAnd(int/uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_and_nonuniformi(i32 %value)
{
    ; 6 = arithmetic iand
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupExclusiveAnd(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_and_nonuniformDv2_i(<2 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupExclusiveAnd(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_and_nonuniformDv3_i(<3 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupExclusiveAnd(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_and_nonuniformDv4_i(<4 x i32> %value)
{
    ; 6 = arithmetic iand

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: bool subgroupAnd(bool)
define spir_func i1 @_Z31sub_group_reduce_and_nonuniformb(i1 %value)
{
    ; 6 = arithmetic band
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 6, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupAnd(bvec2)
define spir_func <2 x i1> @_Z31sub_group_reduce_and_nonuniformDv2_b(<2 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupAnd(bvec3)
define spir_func <3 x i1> @_Z31sub_group_reduce_and_nonuniformDv3_b(<3 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupAnd(bvec4)
define spir_func <4 x i1> @_Z31sub_group_reduce_and_nonuniformDv4_b(<4 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 6 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupInclusiveAnd(bool)
define spir_func i1 @_Z39sub_group_scan_inclusive_and_nonuniformb(i1 %value)
{
    ; 6 = arithmetic band
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupInclusiveAnd(bvec2)
define spir_func <2 x i1> @_Z39sub_group_scan_inclusive_and_nonuniformDv2_b(<2 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupInclusiveAnd(bvec3)
define spir_func <3 x i1> @_Z39sub_group_scan_inclusive_and_nonuniformDv3_b(<3 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupInclusiveAnd(bvec4)
define spir_func <4 x i1> @_Z39sub_group_scan_inclusive_and_nonuniformDv4_b(<4 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 6 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupExclusiveAnd(bool)
define spir_func i1 @_Z39sub_group_scan_exclusive_and_nonuniformb(i1 %value)
{
    ; 6 = arithmetic band
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupExclusiveAnd(bvec2)
define spir_func <2 x i1> @_Z39sub_group_scan_exclusive_and_nonuniformDv2_b(<2 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupExclusiveAnd(bvec3)
define spir_func <3 x i1> @_Z39sub_group_scan_exclusive_and_nonuniformDv3_b(<3 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupExclusiveAnd(bvec4)
define spir_func <4 x i1> @_Z39sub_group_scan_exclusive_and_nonuniformDv4_b(<4 x i1> %value)
{
    ; 6 = arithmetic band
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 6, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 6 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: int/uint subgroupOr(int/uint)
define spir_func i32 @_Z30sub_group_reduce_or_nonuniformi(i32 %value)
{
    ; 7 = arithmetic ior
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 7, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupOr(ivec2/uvec2)
define spir_func <2 x i32> @_Z30sub_group_reduce_or_nonuniformDv2_i(<2 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupOr(ivec3/uvec3)
define spir_func <3 x i32> @_Z30sub_group_reduce_or_nonuniformDv3_i(<3 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupOr(ivec4/uvec4)
define spir_func <4 x i32> @_Z30sub_group_reduce_or_nonuniformDv4_i(<4 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupInclusiveOr(int/uint)
define spir_func i32 @_Z38sub_group_scan_inclusive_or_nonuniformi(i32 %value)
{
    ; 7 = arithmetic ior
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupInclusiveOr(ivec2/uvec2)
define spir_func <2 x i32> @_Z38sub_group_scan_inclusive_or_nonuniformDv2_i(<2 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupInclusiveOr(ivec3/uvec3)
define spir_func <3 x i32> @_Z38sub_group_scan_inclusive_or_nonuniformDv3_i(<3 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupInclusiveOr(ivec4/uvec4)
define spir_func <4 x i32> @_Z38sub_group_scan_inclusive_or_nonuniformDv4_i(<4 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupExclusiveOr(int/uint)
define spir_func i32 @_Z38sub_group_scan_exclusive_or_nonuniformi(i32 %value)
{
    ; 7 = arithmetic ior
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupExclusiveOr(ivec2/uvec2)
define spir_func <2 x i32> @_Z38sub_group_scan_exclusive_or_nonuniformDv2_i(<2 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupExclusiveOr(ivec3/uvec3)
define spir_func <3 x i32> @_Z38sub_group_scan_exclusive_or_nonuniformDv3_i(<3 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupExclusiveOr(ivec4/uvec4)
define spir_func <4 x i32> @_Z38sub_group_scan_exclusive_or_nonuniformDv4_i(<4 x i32> %value)
{
    ; 7 = arithmetic ior

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: bool subgroupOr(bool)
define spir_func i1 @_Z30sub_group_reduce_or_nonuniformb(i1 %value)
{
    ; 7 = arithmetic bor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 7, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupOr(bvec2)
define spir_func <2 x i1> @_Z30sub_group_reduce_or_nonuniformDv2_b(<2 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupOr(bvec3)
define spir_func <3 x i1> @_Z30sub_group_reduce_or_nonuniformDv3_b(<3 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupOr(bvec4)
define spir_func <4 x i1> @_Z30sub_group_reduce_or_nonuniformDv4_b(<4 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 7 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupInclusiveOr(bool)
define spir_func i1 @_Z38sub_group_scan_inclusive_or_nonuniformb(i1 %value)
{
    ; 7 = arithmetic bor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupInclusiveOr(bvec2)
define spir_func <2 x i1> @_Z38sub_group_scan_inclusive_or_nonuniformDv2_b(<2 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupInclusiveOr(bvec3)
define spir_func <3 x i1> @_Z38sub_group_scan_inclusive_or_nonuniformDv3_b(<3 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupInclusiveOr(bvec4)
define spir_func <4 x i1> @_Z38sub_group_scan_inclusive_or_nonuniformDv4_b(<4 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 7 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupExclusiveOr(bool)
define spir_func i1 @_Z38sub_group_scan_exclusive_or_nonuniformb(i1 %value)
{
    ; 7 = arithmetic bor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupExclusiveOr(bvec2)
define spir_func <2 x i1> @_Z38sub_group_scan_exclusive_or_nonuniformDv2_b(<2 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupExclusiveOr(bvec3)
define spir_func <3 x i1> @_Z38sub_group_scan_exclusive_or_nonuniformDv3_b(<3 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupExclusiveOr(bvec4)
define spir_func <4 x i1> @_Z38sub_group_scan_exclusive_or_nonuniformDv4_b(<4 x i1> %value)
{
    ; 7 = arithmetic bor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 7, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 7 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: int/uint subgroupXor(int/uint)
define spir_func i32 @_Z31sub_group_reduce_xor_nonuniformi(i32 %value)
{
    ; 8 = arithmetic ixor
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %value)
    %2 = call i32 @llpc.subgroup.reduce.i32(i32 8, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupXor(ivec2/uvec2)
define spir_func <2 x i32> @_Z31sub_group_reduce_xor_nonuniformDv2_i(<2 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)

    %5 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %3)
    %6 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupXor(ivec3/uvec3)
define spir_func <3 x i32> @_Z31sub_group_reduce_xor_nonuniformDv3_i(<3 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %7 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %4)
    %8 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupXor(ivec4/uvec4)
define spir_func <4 x i32> @_Z31sub_group_reduce_xor_nonuniformDv4_i(<4 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %9 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %5)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupInclusiveXor(int/uint)
define spir_func i32 @_Z39sub_group_scan_inclusive_xor_nonuniformi(i32 %value)
{
    ; 8 = arithmetic ixor
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %value)
    %2 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupInclusiveXor(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_inclusive_xor_nonuniformDv2_i(<2 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)

    %5 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %3)
    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupInclusiveXor(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_inclusive_xor_nonuniformDv3_i(<3 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %4)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupInclusiveXor(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_inclusive_xor_nonuniformDv4_i(<4 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %5)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: int/uint subgroupExclusiveXor(int/uint)
define spir_func i32 @_Z39sub_group_scan_exclusive_xor_nonuniformi(i32 %value)
{
    ; 8 = arithmetic ixor
    %1 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %value)
    %2 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8, i32 %1)

    ret i32 %2
}

; GLSL: ivec2/uvec2 subgroupExclusiveXor(ivec2/uvec2)
define spir_func <2 x i32> @_Z39sub_group_scan_exclusive_xor_nonuniformDv2_i(<2 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)

    %5 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %3)
    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %4)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 subgroupExclusiveXor(ivec3/uvec3)
define spir_func <3 x i32> @_Z39sub_group_scan_exclusive_xor_nonuniformDv3_i(<3 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %4)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %6)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 subgroupExclusiveXor(ivec4/uvec4)
define spir_func <4 x i32> @_Z39sub_group_scan_exclusive_xor_nonuniformDv4_i(<4 x i32> %value)
{
    ; 8 = arithmetic ixor

    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %5)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %8)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: bool subgroupXor(bool)
define spir_func i1 @_Z31sub_group_reduce_xor_nonuniformb(i1 %value)
{
    ; 8 = arithmetic bxor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i32(i32 8, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupXor(bvec2)
define spir_func <2 x i1> @_Z31sub_group_reduce_xor_nonuniformDv2_b(<2 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupXor(bvec3)
define spir_func <3 x i1> @_Z31sub_group_reduce_xor_nonuniformDv3_b(<3 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupXor(bvec4)
define spir_func <4 x i1> @_Z31sub_group_reduce_xor_nonuniformDv4_b(<4 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i32(i32 8 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupInclusiveXor(bool)
define spir_func i1 @_Z39sub_group_scan_inclusive_xor_nonuniformb(i1 %value)
{
    ; 8 = arithmetic bxor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupInclusiveXor(bvec2)
define spir_func <2 x i1> @_Z39sub_group_scan_inclusive_xor_nonuniformDv2_b(<2 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupInclusiveXor(bvec3)
define spir_func <3 x i1> @_Z39sub_group_scan_inclusive_xor_nonuniformDv3_b(<3 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupInclusiveXor(bvec4)
define spir_func <4 x i1> @_Z39sub_group_scan_inclusive_xor_nonuniformDv4_b(<4 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i32(i32 8 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: bool subgroupExclusiveXor(bool)
define spir_func i1 @_Z39sub_group_scan_exclusive_xor_nonuniformb(i1 %value)
{
    ; 8 = arithmetic bxor
    %1 = zext i1 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8, i32 %2)
    %4 = trunc i32 %3 to i1

    ret i1 %4
}

; GLSL: bvec2 subgroupExclusiveXor(bvec2)
define spir_func <2 x i1> @_Z39sub_group_scan_exclusive_xor_nonuniformDv2_b(<2 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <2 x i1> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i1>

    ret <2 x i1> %10
}

; GLSL: bvec3 subgroupExclusiveXor(bvec3)
define spir_func <3 x i1> @_Z39sub_group_scan_exclusive_xor_nonuniformDv3_b(<3 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <3 x i1> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i1>

    ret <3 x i1> %14
}

; GLSL: bvec4 subgroupExclusiveXor(bvec4)
define spir_func <4 x i1> @_Z39sub_group_scan_exclusive_xor_nonuniformDv4_b(<4 x i1> %value)
{
    ; 8 = arithmetic bxor
    %1 = zext <4 x i1> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i32(i32 8, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i32(i32 8 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i1>

    ret <4 x i1> %18
}

; GLSL: int64_t/uint64_t subgroupAdd(int64_t/uint64_t)
define spir_func i64 @_Z31sub_group_reduce_add_nonuniforml(i64 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %value)
    %2 = call i64 @llpc.subgroup.reduce.i64(i32 0, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2/u64vec2 subgroupAdd(i64vec2/u64vec2)
define spir_func <2 x i64> @_Z31sub_group_reduce_add_nonuniformDv2_l(<2 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)

    %5 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %3)
    %6 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3/u64vec3 subgroupAdd(i64vec3/u64vec3)
define spir_func <3 x i64> @_Z31sub_group_reduce_add_nonuniformDv3_l(<3 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)

    %7 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %4)
    %8 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4/u64vec4 subgroupAdd(i64vec4/u64vec4)
define spir_func <4 x i64> @_Z31sub_group_reduce_add_nonuniformDv4_l(<4 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %4)

    %9 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %5)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 0 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t/uint64_t subgroupInclusiveAdd(int64_t/uint64_t)
define spir_func i64 @_Z39sub_group_scan_inclusive_add_nonuniforml(i64 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %value)
    %2 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2/u64vec2 subgroupInclusiveAdd(i64vec2/u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_l(<2 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)

    %5 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %3)
    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3/u64vec3 subgroupInclusiveAdd(i64vec3/u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_l(<3 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)

    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %4)
    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4/u64vec4 subgroupInclusiveAdd(i64vec4/u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_l(<4 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %4)

    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %5)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 0 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t/uint64_t subgroupExclusiveAdd(int64_t/uint64_t)
define spir_func i64 @_Z39sub_group_scan_exclusive_add_nonuniforml(i64 %value)
{
    ; 0 = arithmetic iadd
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %value)
    %2 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2/u64vec2 subgroupExclusiveAdd(i64vec2/u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_l(<2 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)

    %5 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %3)
    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3/u64vec3 subgroupExclusiveAdd(i64vec3/u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_l(<3 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)

    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %4)
    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4/u64vec4 subgroupExclusiveAdd(i64vec4/u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_l(<4 x i64> %value)
{
    ; 0 = arithmetic iadd

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 0, i64 %4)

    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %5)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 0 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupMin(int64_t)
define spir_func i64 @_Z31sub_group_reduce_min_nonuniforml(i64 %value)
{
    ; 2 = arithmetic smin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %value)
    %2 = call i64 @llpc.subgroup.reduce.i64(i32 2, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupMin(i64vec2)
define spir_func <2 x i64> @_Z31sub_group_reduce_min_nonuniformDv2_l(<2 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)

    %5 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %3)
    %6 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupMin(i64vec3)
define spir_func <3 x i64> @_Z31sub_group_reduce_min_nonuniformDv3_l(<3 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)

    %7 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %4)
    %8 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupMin(i64vec4)
define spir_func <4 x i64> @_Z31sub_group_reduce_min_nonuniformDv4_l(<4 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %4)

    %9 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %5)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 2 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupInclusiveMin(int64_t)
define spir_func i64 @_Z39sub_group_scan_inclusive_min_nonuniforml(i64 %value)
{
    ; 2 = arithmetic smin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %value)
    %2 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupInclusiveMin(i64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_l(<2 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)

    %5 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %3)
    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupInclusiveMin(i64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_l(<3 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)

    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %4)
    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupInclusiveMin(i64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_l(<4 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %4)

    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %5)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 2 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupExclusiveMin(int64_t)
define spir_func i64 @_Z39sub_group_scan_exclusive_min_nonuniforml(i64 %value)
{
    ; 2 = arithmetic smin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %value)
    %2 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupExclusiveMin(i64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_l(<2 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)

    %5 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %3)
    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupExclusiveMin(i64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_l(<3 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)

    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %4)
    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupExclusiveMin(i64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_l(<4 x i64> %value)
{
    ; 2 = arithmetic smin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 2, i64 %4)

    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %5)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 2 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupMin(uint64_t)
define spir_func i64 @_Z31sub_group_reduce_min_nonuniformm(i64 %value)
{
    ; 4 = arithmetic umin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %value)
    %2 = call i64 @llpc.subgroup.reduce.i64(i32 4, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupMin(u64vec2)
define spir_func <2 x i64> @_Z31sub_group_reduce_min_nonuniformDv2_m(<2 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)

    %5 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %3)
    %6 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupMin(u64vec3)
define spir_func <3 x i64> @_Z31sub_group_reduce_min_nonuniformDv3_m(<3 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)

    %7 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %4)
    %8 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupMin(u64vec4)
define spir_func <4 x i64> @_Z31sub_group_reduce_min_nonuniformDv4_m(<4 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %4)

    %9 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %5)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 4 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupInclusiveMin(uint64_t)
define spir_func i64 @_Z39sub_group_scan_inclusive_min_nonuniformm(i64 %value)
{
    ; 4 = arithmetic umin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %value)
    %2 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupInclusiveMin(u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_m(<2 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)

    %5 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %3)
    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupInclusiveMin(u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_m(<3 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)

    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %4)
    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupInclusiveMin(u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_m(<4 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %4)

    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %5)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 4 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupExclusiveMin(uint64_t)
define spir_func i64 @_Z39sub_group_scan_exclusive_min_nonuniformm(i64 %value)
{
    ; 4 = arithmetic umin
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %value)
    %2 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupExclusiveMin(u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_m(<2 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)

    %5 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %3)
    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupExclusiveMin(u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_m(<3 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)

    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %4)
    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupExclusiveMin(u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_m(<4 x i64> %value)
{
    ; 4 = arithmetic umin

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 4, i64 %4)

    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %5)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 4 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupMax(int64_t)
define spir_func i64 @_Z31sub_group_reduce_max_nonuniforml(i64 %value)
{
    ; 3 = arithmetic smax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %value)
    %2 = call i64 @llpc.subgroup.reduce.i64(i32 3, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupMax(i64vec2)
define spir_func <2 x i64> @_Z31sub_group_reduce_max_nonuniformDv2_l(<2 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)

    %5 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %3)
    %6 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupMax(i64vec3)
define spir_func <3 x i64> @_Z31sub_group_reduce_max_nonuniformDv3_l(<3 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)

    %7 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %4)
    %8 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupMax(i64vec4)
define spir_func <4 x i64> @_Z31sub_group_reduce_max_nonuniformDv4_l(<4 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %4)

    %9 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %5)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 3 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupInclusiveMax(int64_t)
define spir_func i64 @_Z39sub_group_scan_inclusive_max_nonuniforml(i64 %value)
{
    ; 3 = arithmetic smax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %value)
    %2 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupInclusiveMax(i64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_l(<2 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)

    %5 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %3)
    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupInclusiveMax(i64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_l(<3 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)

    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %4)
    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupInclusiveMax(i64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_l(<4 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %4)

    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %5)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 3 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int64_t subgroupExclusiveMax(int64_t)
define spir_func i64 @_Z39sub_group_scan_exclusive_max_nonuniforml(i64 %value)
{
    ; 3 = arithmetic smax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %value)
    %2 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3, i64 %1)

    ret i64 %2
}

; GLSL: i64vec2 subgroupExclusiveMax(i64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_l(<2 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)

    %5 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %3)
    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: i64vec3 subgroupExclusiveMax(i64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_l(<3 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)

    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %4)
    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: i64vec4 subgroupExclusiveMax(i64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_l(<4 x i64> %value)
{
    ; 3 = arithmetic smax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 3, i64 %4)

    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %5)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 3 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupMax(uint64_t)
define spir_func i64 @_Z31sub_group_reduce_max_nonuniformm(i64 %value)
{
    ; 5 = arithmetic umax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %value)
    %2 = call i64 @llpc.subgroup.reduce.i64(i32 5, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupMax(u64vec2)
define spir_func <2 x i64> @_Z31sub_group_reduce_max_nonuniformDv2_m(<2 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)

    %5 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %3)
    %6 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupMax(u64vec3)
define spir_func <3 x i64> @_Z31sub_group_reduce_max_nonuniformDv3_m(<3 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)

    %7 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %4)
    %8 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %5)
    %9 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupMax(u64vec4)
define spir_func <4 x i64> @_Z31sub_group_reduce_max_nonuniformDv4_m(<4 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %4)

    %9 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %5)
    %10 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %6)
    %11 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %7)
    %12 = call i64 @llpc.subgroup.reduce.i64(i32 5 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupInclusiveMax(uint64_t)
define spir_func i64 @_Z39sub_group_scan_inclusive_max_nonuniformm(i64 %value)
{
    ; 5 = arithmetic umax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %value)
    %2 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupInclusiveMax(u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_m(<2 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)

    %5 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %3)
    %6 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupInclusiveMax(u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_m(<3 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)

    %7 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %4)
    %8 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %5)
    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupInclusiveMax(u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_m(<4 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %4)

    %9 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %5)
    %10 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %6)
    %11 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %7)
    %12 = call i64 @llpc.subgroup.inclusiveScan.i64(i32 5 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: uint64_t subgroupExclusiveMax(uint64_t)
define spir_func i64 @_Z39sub_group_scan_exclusive_max_nonuniformm(i64 %value)
{
    ; 5 = arithmetic umax
    %1 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %value)
    %2 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5, i64 %1)

    ret i64 %2
}

; GLSL: u64vec2 subgroupExclusiveMax(u64vec2)
define spir_func <2 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_m(<2 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <2 x i64> %value, i32 0
    %2 = extractelement <2 x i64> %value, i32 1

    %3 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)

    %5 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %3)
    %6 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %4)

    %7 = insertelement <2 x i64> undef, i64 %5, i64 0
    %8 = insertelement <2 x i64> %7, i64 %6, i64 1

    ret <2 x i64> %8
}

; GLSL: u64vec3 subgroupExclusiveMax(u64vec3)
define spir_func <3 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_m(<3 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <3 x i64> %value, i32 0
    %2 = extractelement <3 x i64> %value, i32 1
    %3 = extractelement <3 x i64> %value, i32 2

    %4 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)

    %7 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %4)
    %8 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %5)
    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %6)

    %10 = insertelement <3 x i64> undef, i64 %7, i64 0
    %11 = insertelement <3 x i64> %10, i64 %8, i64 1
    %12 = insertelement <3 x i64> %11, i64 %9, i64 2

    ret <3 x i64> %12
}

; GLSL: u64vec4 subgroupExclusiveMax(u64vec4)
define spir_func <4 x i64> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_m(<4 x i64> %value)
{
    ; 5 = arithmetic umax

    %1 = extractelement <4 x i64> %value, i32 0
    %2 = extractelement <4 x i64> %value, i32 1
    %3 = extractelement <4 x i64> %value, i32 2
    %4 = extractelement <4 x i64> %value, i32 3

    %5 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %1)
    %6 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %2)
    %7 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %3)
    %8 = call i64 @llpc.subgroup.set.inactive.i64(i32 5, i64 %4)

    %9 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %5)
    %10 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %6)
    %11 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %7)
    %12 = call i64 @llpc.subgroup.exclusiveScan.i64(i32 5 ,i64 %8)

    %13 = insertelement <4 x i64> undef, i64 %9, i64 0
    %14 = insertelement <4 x i64> %13, i64 %10, i64 1
    %15 = insertelement <4 x i64> %14, i64 %11, i64 2
    %16 = insertelement <4 x i64> %15, i64 %12, i64 3

    ret <4 x i64> %16
}

; GLSL: int/uint subgroupQuadBroadcast(int/uint, uint)
define spir_func i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %value, i32 %id)
{
    ; id should be constant of 0 ~ 3
.entry:
    switch i32 %id, label %.end [ i32 0, label %.id0
                                  i32 1, label %.id1
                                  i32 2, label %.id2
                                  i32 3, label %.id3 ]
.id0:
    ; QUAD_PERM 0,0,0,0
    %value.id0 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32768)
    br label %.end
.id1:
    ; QUAD_PERM 1,1,1,1
    %value.id1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32853)
    br label %.end
.id2:
    ; QUAD_PERM 2,2,2,2
    %value.id2 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32938)
    br label %.end
.id3:
    ; QUAD_PERM 3,3,3,3
    %value.id3 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 33023)
    br label %.end
.end:
    %result = phi i32 [undef, %.entry],[%value.id0, %.id0],[%value.id1, %.id1], [%value.id2, %.id2], [%value.id3, %.id3]
    ret i32 %result
}

; GLSL: ivec2/uvec2 subgroupQuadBroadcast(ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z28GroupNonUniformQuadBroadcastiDv2_ii(i32 %scope, <2 x i32> %value, i32 %id)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %4 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %2, i32 %id)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupQuadBroadcast(ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z28GroupNonUniformQuadBroadcastiDv3_ii(i32 %scope, <3 x i32> %value, i32 %id)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %5 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %2, i32 %id)
    %6 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %3, i32 %id)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupQuadBroadcast(ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %value, i32 %id)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %6 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %2, i32 %id)
    %7 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %3, i32 %id)
    %8 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %4, i32 %id)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupQuadBroadcast(float, uint)
define spir_func float @_Z28GroupNonUniformQuadBroadcastifi(i32 %scope, float %value, i32 %id)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupQuadBroadcast(vec2, uint)
define spir_func <2 x float> @_Z28GroupNonUniformQuadBroadcastiDv2_fi(i32 %scope, <2 x float> %value, i32 %id)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z28GroupNonUniformQuadBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupQuadBroadcast(vec3, uint)
define spir_func <3 x float> @_Z28GroupNonUniformQuadBroadcastiDv3_fi(i32 %scope, <3 x float> %value, i32 %id)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z28GroupNonUniformQuadBroadcastiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupQuadBroadcast(vec4, uint)
define spir_func <4 x float> @_Z28GroupNonUniformQuadBroadcastiDv4_fi(i32 %scope, <4 x float> %value, i32 %id)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupQuadBroadcast(double, uint)
define spir_func double @_Z28GroupNonUniformQuadBroadcastidi(i32 %scope, double %value, i32 %id)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z28GroupNonUniformQuadBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupQuadBroadcast(dvec2, uint)
define spir_func <2 x double> @_Z28GroupNonUniformQuadBroadcastiDv2_di(i32 %scope, <2 x double> %value, i32 %id)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupQuadBroadcast(dvec3, uint)
define spir_func <3 x double> @_Z28GroupNonUniformQuadBroadcastiDv3_di(i32 %scope, <3 x double> %value, i32 %id)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <2 x i32> @_Z28GroupNonUniformQuadBroadcastiDv2_ii(i32 %scope, <2 x i32> %3, i32 %id)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupQuadBroadcast(dvec4, uint)
define spir_func <4 x double> @_Z28GroupNonUniformQuadBroadcastiDv4_di(i32 %scope, <4 x double> %value, i32 %id)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %2, i32 %id)
    %5 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %3, i32 %id)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupQuadBroadcast(bool, uint)
define spir_func i1 @_Z28GroupNonUniformQuadBroadcastibi(i32 %scope, i1 %value, i32 %id)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %1, i32 %id)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupQuadBroadcast(bvec2, uint)
define spir_func <2 x i1> @_Z28GroupNonUniformQuadBroadcastiDv2_bi(i32 %scope, <2 x i1> %value, i32 %id)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z28GroupNonUniformQuadBroadcastiDv2_ii(i32 %scope, <2 x i32> %1, i32 %id)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupQuadBroadcast(bvec3, uint)
define spir_func <3 x i1> @_Z28GroupNonUniformQuadBroadcastiDv3_bi(i32 %scope, <3 x i1> %value, i32 %id)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z28GroupNonUniformQuadBroadcastiDv3_ii(i32 %scope, <3 x i32> %1, i32 %id)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupQuadBroadcast(bvec4, uint)
define spir_func <4 x i1> @_Z28GroupNonUniformQuadBroadcastiDv4_bi(i32 %scope, <4 x i1> %value, i32 %id)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z28GroupNonUniformQuadBroadcastiDv4_ii(i32 %scope, <4 x i32> %1, i32 %id)
    %3 = trunc <4 x i32> %2 to <4 x i1>

    ret <4 x i1> %3
}

; GLSL: int/uint subgroupQuadSwapHorizontal(int/uint)
;       int/uint subgroupQuadSwapVertical(int/uint)
;       int/uint subgroupQuadSwapDiagonal(int/uint)
define spir_func i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %value, i32 %direction)
{
    ; direction 0 is Horizontal
    ; direction 1 is Vertical
    ; direction 2 is Diagonal
.entry:
    switch i32 %direction, label %.end [ i32 0, label %.horizonal
                                         i32 1, label %.vertical
                                         i32 2, label %.diagonal ]
.horizonal:
    ; QUAD_PERM [ 0->1, 1->0, 2->3, 3->2], 0b1011,0001
    %value.dir0 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32945)
    br label %.end
.vertical:
    ; QUAD_PERM [ 0->2, 1->3, 2->0, 3->1], 0b0100,1110
    %value.dir1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32846)
    br label %.end
.diagonal:
    ; QUAD_PERM [ 0->3, 1->2, 2->1, 3->0], 0b0001,1011
    %value.dir2 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 32795)
    br label %.end
.end:
    %result = phi i32 [undef, %.entry], [%value.dir0, %.horizonal], [%value.dir1, %.vertical], [%value.dir2, %.diagonal]
    ret i32 %result
}

; GLSL: ivec2/uvec2 subgroupQuadSwapHorizontal(ivec2/uvec2)
;       ivec2/uvec2 subgroupQuadSwapVertical(ivec2/uvec2)
;       ivec2/uvec2 subgroupQuadSwapDiagonal(ivec2/uvec2)
define spir_func <2 x i32> @_Z23GroupNonUniformQuadSwapiDv2_ii(i32 %scope, <2 x i32> %value, i32 %direction)
{
    %1 = extractelement <2 x i32> %value, i32 0
    %2 = extractelement <2 x i32> %value, i32 1

    %3 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %1, i32 %direction)
    %4 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %2, i32 %direction)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 subgroupQuadSwapHorizontal(ivec3/uvec3)
;       ivec3/uvec3 subgroupQuadSwapVertical(ivec3/uvec3)
;       ivec3/uvec3 subgroupQuadSwapDiagonal(ivec3/uvec3)
define spir_func <3 x i32> @_Z23GroupNonUniformQuadSwapiDv3_ii(i32 %scope, <3 x i32> %value, i32 %direction)
{
    %1 = extractelement <3 x i32> %value, i32 0
    %2 = extractelement <3 x i32> %value, i32 1
    %3 = extractelement <3 x i32> %value, i32 2

    %4 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %1, i32 %direction)
    %5 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %2, i32 %direction)
    %6 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %3, i32 %direction)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 subgroupQuadSwapHorizontal(ivec4/uvec4)
;       ivec4/uvec4 subgroupQuadSwapVertical(ivec4/uvec4)
;       ivec4/uvec4 subgroupQuadSwapDiagonal(ivec4/uvec4)
define spir_func <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %value, i32 %direction)
{
    %1 = extractelement <4 x i32> %value, i32 0
    %2 = extractelement <4 x i32> %value, i32 1
    %3 = extractelement <4 x i32> %value, i32 2
    %4 = extractelement <4 x i32> %value, i32 3

    %5 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %1, i32 %direction)
    %6 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %2, i32 %direction)
    %7 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %3, i32 %direction)
    %8 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %4, i32 %direction)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float subgroupQuadSwapHorizontal(float)
;       float subgroupQuadSwapVertical(float)
;       float subgroupQuadSwapDiagonal(float)
define spir_func float @_Z23GroupNonUniformQuadSwapifi(i32 %scope, float %value, i32 %direction)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %1, i32 %direction)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 subgroupQuadSwapHorizontal(vec2)
;       vec2 subgroupQuadSwapVertical(vec2)
;       vec2 subgroupQuadSwapDiagonal(vec2)
define spir_func <2 x float> @_Z23GroupNonUniformQuadSwapiDv2_fi(i32 %scope, <2 x float> %value, i32 %direction)
{
    %1 = bitcast <2 x float> %value to <2 x i32>
    %2 = call <2 x i32> @_Z23GroupNonUniformQuadSwapiDv2_ii(i32 %scope, <2 x i32> %1, i32 %direction)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 subgroupQuadSwapHorizontal(vec3)
;       vec3 subgroupQuadSwapVertical(vec3)
;       vec3 subgroupQuadSwapDiagonal(vec3)
define spir_func <3 x float> @_Z23GroupNonUniformQuadSwapiDv3_fi(i32 %scope, <3 x float> %value, i32 %direction)
{
    %1 = bitcast <3 x float> %value to <3 x i32>
    %2 = call <3 x i32> @_Z23GroupNonUniformQuadSwapiDv3_ii(i32 %scope, <3 x i32> %1, i32 %direction)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 subgroupQuadSwapHorizontal(vec4)
;       vec4 subgroupQuadSwapVertical(vec4)
;       vec4 subgroupQuadSwapDiagonal(vec4)
define spir_func <4 x float> @_Z23GroupNonUniformQuadSwapiDv4_fi(i32 %scope, <4 x float> %value, i32 %direction)
{
    %1 = bitcast <4 x float> %value to <4 x i32>
    %2 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %1, i32 %direction)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: double subgroupQuadSwapHorizontal(double)
;       double subgroupQuadSwapVertical(double)
;       double subgroupQuadSwapDiagonal(double)
define spir_func double @_Z23GroupNonUniformQuadSwapidi(i32 %scope, double %value, i32 %direction)
{
    %1 = bitcast double %value to <2 x i32>
    %2 = call <2 x i32> @_Z23GroupNonUniformQuadSwapiDv2_ii(i32 %scope, <2 x i32> %1, i32 %direction)
    %3 = bitcast <2 x i32> %2 to double

    ret double %3
}

; GLSL: dvec2 subgroupQuadSwapHorizontal(dvec2)
;       dvec2 subgroupQuadSwapVertical(dvec2)
;       dvec2 subgroupQuadSwapDiagonal(dvec2)
define spir_func <2 x double> @_Z23GroupNonUniformQuadSwapiDv2_di(i32 %scope, <2 x double> %value, i32 %direction)
{
    %1 = bitcast <2 x double> %value to <4 x i32>
    %2 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %1, i32 %direction)
    %3 = bitcast <4 x i32> %2 to <2 x double>

    ret <2 x double> %3
}

; GLSL: dvec3 subgroupQuadSwapHorizontal(dvec3)
;       dvec3 subgroupQuadSwapVertical(dvec3)
;       dvec3 subgroupQuadSwapDiagonal(dvec3)
define spir_func <3 x double> @_Z23GroupNonUniformQuadSwapiDv3_di(i32 %scope, <3 x double> %value, i32 %direction)
{
    %1 = bitcast <3 x double> %value to <6 x i32>
    %2 = shufflevector <6 x i32> %1, <6 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <6 x i32> %1, <6 x i32> %1, <2 x i32> <i32 4, i32 5>

    %4 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %2, i32 %direction)
    %5 = call <2 x i32> @_Z23GroupNonUniformQuadSwapiDv2_ii(i32 %scope, <2 x i32> %3, i32 %direction)
    %6 = shufflevector <2 x i32> %5, <2 x i32> <i32 0, i32 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>

    %7 = shufflevector <4 x i32> %4, <4 x i32> %6, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    %8 = bitcast <6 x i32> %7 to <3 x double>

    ret <3 x double> %8
}

; GLSL: dvec4 subgroupQuadSwapHorizontal(dvec4)
;       dvec4 subgroupQuadSwapVertical(dvec4)
;       dvec4 subgroupQuadSwapDiagonal(dvec4)
define spir_func <4 x double> @_Z23GroupNonUniformQuadSwapiDv4_di(i32 %scope, <4 x double> %value, i32 %direction)
{
    %1 = bitcast <4 x double> %value to <8 x i32>
    %2 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %3 = shufflevector <8 x i32> %1, <8 x i32> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>

    %4 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %2, i32 %direction)
    %5 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %3, i32 %direction)

    %6 = shufflevector <4 x i32> %4, <4 x i32> %5, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %7 = bitcast <8 x i32> %6 to <4 x double>

    ret <4 x double> %7
}

; GLSL: bool subgroupQuadSwapHorizontal(bool)
;       bool subgroupQuadSwapVertical(bool)
;       bool subgroupQuadSwapDiagonal(bool)
define spir_func i1 @_Z23GroupNonUniformQuadSwapibi(i32 %scope, i1 %value, i32 %direction)
{
    %1 = zext i1 %value to i32
    %2 = call i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %1, i32 %direction)
    %3 = trunc i32 %2 to i1

    ret i1 %3
}

; GLSL: bvec2 subgroupQuadSwapHorizontal(bvec2)
;       bvec2 subgroupQuadSwapVertical(bvec2)
;       bvec2 subgroupQuadSwapDiagonal(bvec2)
define spir_func <2 x i1> @_Z23GroupNonUniformQuadSwapiDv2_bi(i32 %scope, <2 x i1> %value, i32 %direction)
{
    %1 = zext <2 x i1> %value to <2 x i32>
    %2 = call <2 x i32> @_Z23GroupNonUniformQuadSwapiDv2_ii(i32 %scope, <2 x i32> %1, i32 %direction)
    %3 = trunc <2 x i32> %2 to <2 x i1>

    ret <2 x i1> %3
}

; GLSL: bvec3 subgroupQuadSwapHorizontal(bvec3)
;       bvec3 subgroupQuadSwapVertical(bvec3)
;       bvec3 subgroupQuadSwapDiagonal(bvec3)
define spir_func <3 x i1> @_Z23GroupNonUniformQuadSwapiDv3_bi(i32 %scope, <3 x i1> %value, i32 %direction)
{
    %1 = zext <3 x i1> %value to <3 x i32>
    %2 = call <3 x i32> @_Z23GroupNonUniformQuadSwapiDv3_ii(i32 %scope, <3 x i32> %1, i32 %direction)
    %3 = trunc <3 x i32> %2 to <3 x i1>

    ret <3 x i1> %3
}

; GLSL: bvec4 subgroupQuadSwapHorizontal(bvec4)
;       bvec4 subgroupQuadSwapVertical(bvec4)
;       bvec4 subgroupQuadSwapDiagonal(bvec4)
define spir_func <4 x i1> @_Z23GroupNonUniformQuadSwapiDv4_bi(i32 %scope, <4 x i1> %value, i32 %direction)
{
    %1 = zext <4 x i1> %value to <4 x i32>
    %2 = call <4 x i32> @_Z23GroupNonUniformQuadSwapiDv4_ii(i32 %scope, <4 x i32> %1, i32 %direction)
    %3 = trunc <4 x i32> %2 to <4 x i1>
    ret <4 x i1> %3
}

; GLSL: int/uint swizzleInvocations(int/uint, uvec4)
define spir_func i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %data, <4 x i32> %offset)
{
    %1 = extractelement <4 x i32> %offset, i32 0
    %2 = extractelement <4 x i32> %offset, i32 1
    %3 = extractelement <4 x i32> %offset, i32 2
    %4 = extractelement <4 x i32> %offset, i32 3

    %5 = and i32 %1, 3
    %6 = and i32 %2, 3
    %7 = and i32 %3, 3
    %8 = and i32 %4, 3

    ; [7:6] = offset[3], [5:4] = offset[2], [3:2] = offset[1], [1:0] = offset[0]
    %9  = shl i32 %6, 2
    %10 = shl i32 %7, 4
    %11 = shl i32 %8, 6

    %12 = or i32 %5,  %9
    %13 = or i32 %12, %10
    %14 = or i32 %13, %11

    ; 32768 = 0x8000, [15] = 1 (QUAD_PERMUTE)
    %15 = or i32 %14, 32768
    %16 = call i32 @llvm.amdgcn.ds.swizzle(i32 %data, i32 %15)

    ret i32 %16
}

; GLSL: ivec2/uvec2 swizzleInvocations(ivec2/uvec2, uvec4)
define spir_func <2 x i32> @_Z21SwizzleInvocationsAMDDv2_iDv4_i(<2 x i32> %data, <4 x i32> %offset)
{
    %1 = extractelement <2 x i32> %data, i32 0
    %2 = extractelement <2 x i32> %data, i32 1

    %3 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %1, <4 x i32> %offset)
    %4 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %2, <4 x i32> %offset)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 swizzleInvocations(ivec3/uvec3, uvec4)
define spir_func <3 x i32> @_Z21SwizzleInvocationsAMDDv3_iDv4_i(<3 x i32> %data, <4 x i32> %offset)
{
    %1 = extractelement <3 x i32> %data, i32 0
    %2 = extractelement <3 x i32> %data, i32 1
    %3 = extractelement <3 x i32> %data, i32 2

    %4 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %1, <4 x i32> %offset)
    %5 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %2, <4 x i32> %offset)
    %6 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %3, <4 x i32> %offset)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 swizzleInvocations(ivec4/uvec4, uvec4)
define spir_func <4 x i32> @_Z21SwizzleInvocationsAMDDv4_iDv4_i(<4 x i32> %data, <4 x i32> %offset)
{
    %1 = extractelement <4 x i32> %data, i32 0
    %2 = extractelement <4 x i32> %data, i32 1
    %3 = extractelement <4 x i32> %data, i32 2
    %4 = extractelement <4 x i32> %data, i32 3

    %5 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %1, <4 x i32> %offset)
    %6 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %2, <4 x i32> %offset)
    %7 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %3, <4 x i32> %offset)
    %8 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %4, <4 x i32> %offset)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float swizzleInvocations(float, uvec4)
define spir_func float @_Z21SwizzleInvocationsAMDfDv4_i(float %data, <4 x i32> %offset)
{
    %1 = bitcast float %data to i32
    %2 = call i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %1, <4 x i32> %offset)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 swizzleInvocations(vec2, uvec4)
define spir_func <2 x float> @_Z21SwizzleInvocationsAMDDv2_fDv4_i(<2 x float> %data, <4 x i32> %offset)
{
    %1 = bitcast <2 x float> %data to <2 x i32>
    %2 = call <2 x i32> @_Z21SwizzleInvocationsAMDDv2_iDv4_i(<2 x i32> %1, <4 x i32> %offset)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 swizzleInvocations(vec3, uvec4)
define spir_func <3 x float> @_Z21SwizzleInvocationsAMDDv3_fDv4_i(<3 x float> %data, <4 x i32> %offset)
{
    %1 = bitcast <3 x float> %data to <3 x i32>
    %2 = call <3 x i32> @_Z21SwizzleInvocationsAMDDv3_iDv4_i(<3 x i32> %1, <4 x i32> %offset)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 swizzleInvocations(vec4, uvec4)
define spir_func <4 x float> @_Z21SwizzleInvocationsAMDDv4_fDv4_i(<4 x float> %data, <4 x i32> %offset)
{
    %1 = bitcast <4 x float> %data to <4 x i32>
    %2 = call <4 x i32> @_Z21SwizzleInvocationsAMDDv4_iDv4_i(<4 x i32> %1, <4 x i32> %offset)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: int/uint swizzleInvocationsMasked(int/uint, uvec3)
define spir_func i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %data, <3 x i32> %mask)
{
    %1 = extractelement <3 x i32> %mask, i32 0 ; AND mask
    %2 = extractelement <3 x i32> %mask, i32 1 ; OR mask
    %3 = extractelement <3 x i32> %mask, i32 2 ; XOR mask

    %4 = and i32 %1, 31
    %5 = and i32 %2, 31
    %6 = and i32 %3, 31

    ; [14:10] = XOR mask, [9:5] = OR mask, [4:0] = AND mask, [15] = 0 (BITMASK_PERMUTE)
    %7 = shl i32 %5, 5
    %8 = shl i32 %6, 10

    %9  = or i32 %4, %7
    %10 = or i32 %9, %8

    %11 = call i32 @llvm.amdgcn.ds.swizzle(i32 %data, i32 %10)

    ret i32 %11
}

; GLSL: ivec2/uvec2 swizzleInvocationsMasked(ivec2/uvec2, uvec3)
define spir_func <2 x i32> @_Z27SwizzleInvocationsMaskedAMDDv2_iDv3_i(<2 x i32> %data, <3 x i32> %mask)
{
    %1 = extractelement <2 x i32> %data, i32 0
    %2 = extractelement <2 x i32> %data, i32 1

    %3 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %1, <3 x i32> %mask)
    %4 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %2, <3 x i32> %mask)

    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1

    ret <2 x i32> %6
}

; GLSL: ivec3/uvec3 swizzleInvocationsMasked(ivec3/uvec3, uvec3)
define spir_func <3 x i32> @_Z27SwizzleInvocationsMaskedAMDDv3_iDv3_i(<3 x i32> %data, <3 x i32> %mask)
{
    %1 = extractelement <3 x i32> %data, i32 0
    %2 = extractelement <3 x i32> %data, i32 1
    %3 = extractelement <3 x i32> %data, i32 2

    %4 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %1, <3 x i32> %mask)
    %5 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %2, <3 x i32> %mask)
    %6 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %3, <3 x i32> %mask)

    %7 = insertelement <3 x i32> undef, i32 %4, i32 0
    %8 = insertelement <3 x i32> %7, i32 %5, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2

    ret <3 x i32> %9
}

; GLSL: ivec4/uvec4 swizzleInvocationsMasked(ivec4/uvec4, uvec3)
define spir_func <4 x i32> @_Z27SwizzleInvocationsMaskedAMDDv4_iDv3_i(<4 x i32> %data, <3 x i32> %mask)
{
    %1 = extractelement <4 x i32> %data, i32 0
    %2 = extractelement <4 x i32> %data, i32 1
    %3 = extractelement <4 x i32> %data, i32 2
    %4 = extractelement <4 x i32> %data, i32 3

    %5 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %1, <3 x i32> %mask)
    %6 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %2, <3 x i32> %mask)
    %7 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %3, <3 x i32> %mask)
    %8 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %4, <3 x i32> %mask)

    %9 = insertelement <4 x i32> undef, i32 %5, i32 0
    %10 = insertelement <4 x i32> %9, i32 %6, i32 1
    %11 = insertelement <4 x i32> %10, i32 %7, i32 2
    %12 = insertelement <4 x i32> %11, i32 %8, i32 3

    ret <4 x i32> %12
}

; GLSL: float swizzleInvocationsMasked(float, uvec3)
define spir_func float @_Z27SwizzleInvocationsMaskedAMDfDv3_i(float %data, <3 x i32> %mask)
{
    %1 = bitcast float %data to i32
    %2 = call i32 @_Z27SwizzleInvocationsMaskedAMDiDv3_i(i32 %1, <3 x i32> %mask)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: vec2 swizzleInvocationsMasked(vec2, uvec3)
define spir_func <2 x float> @_Z27SwizzleInvocationsMaskedAMDDv2_fDv3_i(<2 x float> %data, <3 x i32> %mask)
{
    %1 = bitcast <2 x float> %data to <2 x i32>
    %2 = call <2 x i32> @_Z27SwizzleInvocationsMaskedAMDDv2_iDv3_i(<2 x i32> %1, <3 x i32> %mask)
    %3 = bitcast <2 x i32> %2 to <2 x float>

    ret <2 x float> %3
}

; GLSL: vec3 swizzleInvocationsMasked(vec3, uvec3)
define spir_func <3 x float> @_Z27SwizzleInvocationsMaskedAMDDv3_fDv3_i(<3 x float> %data, <3 x i32> %mask)
{
    %1 = bitcast <3 x float> %data to <3 x i32>
    %2 = call <3 x i32> @_Z27SwizzleInvocationsMaskedAMDDv3_iDv3_i(<3 x i32> %1, <3 x i32> %mask)
    %3 = bitcast <3 x i32> %2 to <3 x float>

    ret <3 x float> %3
}

; GLSL: vec4 swizzleInvocationsMasked(vec4, uvec3)
define spir_func <4 x float> @_Z27SwizzleInvocationsMaskedAMDDv4_fDv3_i(<4 x float> %data, <3 x i32> %mask)
{
    %1 = bitcast <4 x float> %data to <4 x i32>
    %2 = call <4 x i32> @_Z27SwizzleInvocationsMaskedAMDDv4_iDv3_i(<4 x i32> %1, <3 x i32> %mask)
    %3 = bitcast <4 x i32> %2 to <4 x float>

    ret <4 x float> %3
}

; GLSL: int/uint writeInvocation(int/uint, int/uint, uint)
define spir_func i32 @_Z18WriteInvocationAMDiii(i32 %inputValue, i32 %writeValue, i32 %invocationIndex)
{
    %1 = call i32 @llvm.amdgcn.writelane(i32 %writeValue, i32 %invocationIndex, i32 %inputValue)
    ret i32 %1
}

; GLSL: ivec2/uvec2 writeInvocation(ivec2/uvec2, ivec2/uvec2, uint)
define spir_func <2 x i32> @_Z18WriteInvocationAMDDv2_iDv2_ii(
    <2 x i32> %inputValue, <2 x i32> %writeValue, i32 %invocationIndex)
{
    %1 = extractelement <2 x i32> %inputValue, i32 0
    %2 = extractelement <2 x i32> %inputValue, i32 1

    %3 = extractelement <2 x i32> %writeValue, i32 0
    %4 = extractelement <2 x i32> %writeValue, i32 1

    %5 = call i32 @_Z18WriteInvocationAMDiii(i32 %1, i32 %3, i32 %invocationIndex)
    %6 = call i32 @_Z18WriteInvocationAMDiii(i32 %2, i32 %4, i32 %invocationIndex)

    %7 = insertelement <2 x i32> undef, i32 %5, i32 0
    %8 = insertelement <2 x i32> %7, i32 %6, i32 1

    ret <2 x i32> %8
}

; GLSL: ivec3/uvec3 writeInvocation(ivec3/uvec3, ivec3/uvec3, uint)
define spir_func <3 x i32> @_Z18WriteInvocationAMDDv3_iDv3_ii(
    <3 x i32> %inputValue, <3 x i32> %writeValue, i32 %invocationIndex)
{
    %1 = extractelement <3 x i32> %inputValue, i32 0
    %2 = extractelement <3 x i32> %inputValue, i32 1
    %3 = extractelement <3 x i32> %inputValue, i32 2

    %4 = extractelement <3 x i32> %writeValue, i32 0
    %5 = extractelement <3 x i32> %writeValue, i32 1
    %6 = extractelement <3 x i32> %writeValue, i32 2

    %7 = call i32 @_Z18WriteInvocationAMDiii(i32 %1, i32 %4, i32 %invocationIndex)
    %8 = call i32 @_Z18WriteInvocationAMDiii(i32 %2, i32 %5, i32 %invocationIndex)
    %9 = call i32 @_Z18WriteInvocationAMDiii(i32 %3, i32 %6, i32 %invocationIndex)

    %10 = insertelement <3 x i32> undef, i32 %7, i32 0
    %11 = insertelement <3 x i32> %10, i32 %8, i32 1
    %12 = insertelement <3 x i32> %11, i32 %9, i32 2

    ret <3 x i32> %12
}

; GLSL: ivec4/uvec4 writeInvocation(ivec4/uvec4, ivec4/uvec4, uint)
define spir_func <4 x i32> @_Z18WriteInvocationAMDDv4_iDv4_ii(
    <4 x i32> %inputValue, <4 x i32> %writeValue, i32 %invocationIndex)
{
    %1 = extractelement <4 x i32> %inputValue, i32 0
    %2 = extractelement <4 x i32> %inputValue, i32 1
    %3 = extractelement <4 x i32> %inputValue, i32 2
    %4 = extractelement <4 x i32> %inputValue, i32 3

    %5 = extractelement <4 x i32> %writeValue, i32 0
    %6 = extractelement <4 x i32> %writeValue, i32 1
    %7 = extractelement <4 x i32> %writeValue, i32 2
    %8 = extractelement <4 x i32> %writeValue, i32 3

    %9  = call i32 @_Z18WriteInvocationAMDiii(i32 %1, i32 %5, i32 %invocationIndex)
    %10 = call i32 @_Z18WriteInvocationAMDiii(i32 %2, i32 %6, i32 %invocationIndex)
    %11 = call i32 @_Z18WriteInvocationAMDiii(i32 %3, i32 %7, i32 %invocationIndex)
    %12 = call i32 @_Z18WriteInvocationAMDiii(i32 %4, i32 %8, i32 %invocationIndex)

    %13 = insertelement <4 x i32> undef, i32 %9, i32 0
    %14 = insertelement <4 x i32> %13, i32 %10, i32 1
    %15 = insertelement <4 x i32> %14, i32 %11, i32 2
    %16 = insertelement <4 x i32> %15, i32 %12, i32 3

    ret <4 x i32> %16
}

; GLSL: float writeInvocation(float, float, uint)
define spir_func float @_Z18WriteInvocationAMDffi(float %inputValue, float %writeValue, i32 %invocationIndex)
{
    %1 = bitcast float %inputValue to i32
    %2 = bitcast float %writeValue to i32
    %3 = call i32 @_Z18WriteInvocationAMDiii(i32 %1, i32 %2, i32 %invocationIndex)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: vec2 writeInvocation(vec2, vec2, uint)
define spir_func <2 x float> @_Z18WriteInvocationAMDDv2_fDv2_fi(
    <2 x float> %inputValue, <2 x float> %writeValue, i32 %invocationIndex)
{
    %1 = bitcast <2 x float> %inputValue to <2 x i32>
    %2 = bitcast <2 x float> %writeValue to <2 x i32>
    %3 = call <2 x i32> @_Z18WriteInvocationAMDDv2_iDv2_ii(<2 x i32> %1, <2 x i32> %2, i32 %invocationIndex)
    %4 = bitcast <2 x i32> %3 to <2 x float>

    ret <2 x float> %4
}

; GLSL: vec3 writeInvocation(vec3, vec3, uint)
define spir_func <3 x float> @_Z18WriteInvocationAMDDv3_fDv3_fi(
    <3 x float> %inputValue, <3 x float> %writeValue, i32 %invocationIndex)
{
    %1 = bitcast <3 x float> %inputValue to <3 x i32>
    %2 = bitcast <3 x float> %writeValue to <3 x i32>
    %3 = call <3 x i32> @_Z18WriteInvocationAMDDv3_iDv3_ii(<3 x i32> %1, <3 x i32> %2, i32 %invocationIndex)
    %4 = bitcast <3 x i32> %3 to <3 x float>

    ret <3 x float> %4
}

; GLSL: vec4 writeInvocation(vec4, vec4, uint)
define spir_func <4 x float> @_Z18WriteInvocationAMDDv4_fDv4_fi(
    <4 x float> %inputValue, <4 x float> %writeValue, i32 %invocationIndex)
{
    %1 = bitcast <4 x float> %inputValue to <4 x i32>
    %2 = bitcast <4 x float> %writeValue to <4 x i32>
    %3 = call <4 x i32> @_Z18WriteInvocationAMDDv4_iDv4_ii(<4 x i32> %1, <4 x i32> %2, i32 %invocationIndex)
    %4 = bitcast <4 x i32> %3 to <4 x float>

    ret <4 x float> %4
}

; GLSL: uint mbcnt(uint64_t)
define spir_func i32 @_Z8MbcntAMDl(i64 %mask)
{
    %1 = bitcast i64 %mask to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llvm.amdgcn.mbcnt.lo(i32 %2, i32 0)
    %5 = call i32 @llvm.amdgcn.mbcnt.hi(i32 %3, i32 %4)

    ret i32 %5
}

; =====================================================================================================================
; >>>  Interpolation Functions
; =====================================================================================================================

; Adjust interpolation I/J according to specified offsets X/Y
define float @llpc.input.interpolate.adjustij.f32(float %ij, float %offsetX, float %offsetY)
{
    ; Calculate DpDx, DpDy for %ij
    %1 = call float @llpc.dpdxFine.f32(float %ij)
    %2 = call float @llpc.dpdyFine.f32(float %ij)

    ; Adjust %ij by offset
    %3 = fmul float %offsetX, %1
    %4 = fadd float %ij, %3
    %5 = fmul float %offsetY, %2
    %6 = fadd float %4, %5

    ret float %6
}

; Evaluate interpolation I/J for GLSL function interpolateAtOffset()
define <2 x float> @llpc.input.interpolate.evalij.offset.v2f32(<2 x float> %offset) #0
{
    ; BuiltInInterpPullMode 268435459 = 0x10000003
    %1 = call <3 x float> @llpc.input.import.builtin.InterpPullMode(i32 268435459)
    ; Extract Pull Model I/W, J/W, 1/W
    %2 = extractelement <3 x float> %1, i32 0
    %3 = extractelement <3 x float> %1, i32 1
    %4 = extractelement <3 x float> %1, i32 2

    ; Extract offset to scalar
    %5 = extractelement <2 x float> %offset, i32 0
    %6 = extractelement <2 x float> %offset, i32 1

    ; Adjust each coefficient by offset
    %7 = call float @llpc.input.interpolate.adjustij.f32(float %2, float %5, float %6)
    %8 = call float @llpc.input.interpolate.adjustij.f32(float %3, float %5, float %6)
    %9 = call float @llpc.input.interpolate.adjustij.f32(float %4, float %5, float %6)

    ; Get final I, J
    %10 = fmul float %7, %9
    %11 = fmul float %8, %9

    %12 = insertelement <2 x float> undef, float %10, i32 0
    %13 = insertelement <2 x float> %12, float %11, i32 1

    ret <2 x float> %13
}

; Evaluate interpolation I/J for GLSL function interpolateAtOffset() with "noperspective" qualifier specified
; on interpolant
define <2 x float> @llpc.input.interpolate.evalij.offset.noperspective.v2f32(<2 x float> %offset) #0
{
    ; BuiltInInterpLinearCenter 268435461 = 0x10000005
    %1 = call <2 x float> @llpc.input.import.builtin.InterpLinearCenter(i32 268435461)
    ; Extract I, J
    %2 = extractelement <2 x float> %1, i32 0
    %3 = extractelement <2 x float> %1, i32 1

    ; Extract offset to scalar
    %4 = extractelement <2 x float> %offset, i32 0
    %5 = extractelement <2 x float> %offset, i32 1

    ; Adjust I,J by offset
    %6 = call float @llpc.input.interpolate.adjustij.f32(float %2, float %4, float %5)
    %7 = call float @llpc.input.interpolate.adjustij.f32(float %3, float %4, float %5)

    %8 = insertelement <2 x float> undef, float %6, i32 0
    %9 = insertelement <2 x float> %8, float %7, i32 1

    ret <2 x float> %9
}

; Evaluate interpolation I/J for GLSL function interpolateAtSample()
define <2 x float> @llpc.input.interpolate.evalij.sample(i32 %sample) #0
{
    ; BuiltInSamplePosOffset 268435463 = 0x10000007
    %1 = call <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 268435463, i32 %sample)
    %2 = call <2 x float> @llpc.input.interpolate.evalij.offset.v2f32(<2 x float> %1)
    ret <2 x float> %2
}

; Evaluate interpolation I/J for GLSL function interpolateAtSample() with "noperspective" qualifier specified
; on interpolant
define <2 x float> @llpc.input.interpolate.evalij.sample.noperspective(i32 %sample) #0
{
    ; BuiltInSamplePosOffset 268435463 = 0x10000007
    %1 = call <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 268435463, i32 %sample)
    %2 = call <2 x float> @llpc.input.interpolate.evalij.offset.noperspective.v2f32(<2 x float> %1)
    ret <2 x float> %2
}

; =====================================================================================================================
; >>>  Functions of Extension AMD_gcn_shader
; =====================================================================================================================

; GLSL: float cubeFaceIndex(vec3)
define spir_func float @_Z16CubeFaceIndexAMDDv3_f(<3 x float> %coord)
{
    %1 = extractelement <3 x float> %coord, i32 0
    %2 = extractelement <3 x float> %coord, i32 1
    %3 = extractelement <3 x float> %coord, i32 2

    %4 = call float @llvm.amdgcn.cubeid(float %1, float %2, float %3)
    ret float %4
}

; GLSL: vec2 cubeFaceCoord(vec3)
define spir_func <2 x float> @_Z16CubeFaceCoordAMDDv3_f(<3 x float> %coord)
{
    %1 = extractelement <3 x float> %coord, i32 0
    %2 = extractelement <3 x float> %coord, i32 1
    %3 = extractelement <3 x float> %coord, i32 2

    %4 = call float @llvm.amdgcn.cubema(float %1, float %2, float %3)
    %5 = fdiv float 1.0, %4

    %6 = call float @llvm.amdgcn.cubesc(float %1, float %2, float %3)
    %7 = fmul float %5, %6
    %8 = fadd float %7, 0.5

    %9  = call float @llvm.amdgcn.cubetc(float %1, float %2, float %3)
    %10 = fmul float %5, %9
    %11 = fadd float %10, 0.5

    %12 = insertelement <2 x float> undef, float %8, i32 0
    %13 = insertelement <2 x float> %12, float %11, i32 1

    ret <2 x float> %13
}

; GLSL: uint64_t time()
define spir_func i64 @_Z7TimeAMDv()
{
    %1 = call i64 @llvm.amdgcn.s.memtime() #0
    ; Prevent optimization of backend compiler on the control flow
    %2 = call i64 asm sideeffect "; %1", "=v,0"(i64 %1)
    ret i64 %2
}

declare void @llvm.AMDGPU.kill(float) #0
declare float @llvm.fabs.f32(float) #0
declare i32 @llvm.amdgcn.ds.swizzle(i32, i32) #2
declare void @llvm.amdgcn.s.waitcnt(i32) #0
declare void @llvm.amdgcn.s.barrier() #3
declare void @llvm.amdgcn.s.sendmsg(i32, i32) #0
declare <3 x float> @llpc.input.import.builtin.InterpPullMode(i32) #0
declare <2 x float> @llpc.input.import.builtin.InterpLinearCenter(i32) #0
declare <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32, i32) #0
declare i32 @llpc.input.import.builtin.GsWaveId(i32) #0
declare i32 @llvm.amdgcn.readlane(i32, i32) #2
declare i32 @llvm.amdgcn.readfirstlane(i32) #2
declare i32 @llvm.amdgcn.writelane(i32, i32, i32) #2
declare i64 @llvm.amdgcn.icmp.i32(i32, i32, i32) #2
declare i32 @llvm.amdgcn.mbcnt.lo(i32, i32) #1
declare i32 @llvm.amdgcn.mbcnt.hi(i32, i32) #1
declare i1 @llvm.amdgcn.ps.live() #1
declare i64 @llvm.cttz.i64(i64, i1) #0
declare i64 @llvm.ctlz.i64(i64, i1) #0
declare i64 @llvm.ctpop.i64(i64) #0
declare float @llvm.amdgcn.cubeid(float, float, float) #1
declare float @llvm.amdgcn.cubesc(float, float, float) #1
declare float @llvm.amdgcn.cubema(float, float, float) #1
declare float @llvm.amdgcn.cubetc(float, float, float) #1
declare i64 @llvm.amdgcn.s.memtime() #0
declare i32 @llvm.amdgcn.wwm.i32(i32) #1
declare i64 @llvm.amdgcn.wwm.i64(i64) #1
declare <2 x i32> @llvm.amdgcn.wwm.v2i32(<2 x i32>) #1
declare i32 @llvm.amdgcn.set.inactive.i32(i32, i32) #2
declare i64 @llvm.amdgcn.set.inactive.i64(i64, i64) #2
declare <2 x i32> @llvm.amdgcn.set.inactive.v2i32(<2 x i32>, <2 x i32>) #2
declare <3 x i32> @llvm.amdgcn.set.inactive.v3i32(<3 x i32>, <3 x i32>) #2
declare <4 x i32> @llvm.amdgcn.set.inactive.v4i32(<4 x i32>, <4 x i32>) #2
declare i32 @llpc.sminnum.i32(i32, i32) #0
declare i32 @llpc.smaxnum.i32(i32, i32) #0
declare i32 @llpc.uminnum.i32(i32, i32) #0
declare i32 @llpc.umaxnum.i32(i32, i32) #0
declare i64 @llpc.sminnum.i64(i64, i64) #0
declare i64 @llpc.smaxnum.i64(i64, i64) #0
declare i64 @llpc.uminnum.i64(i64, i64) #0
declare i64 @llpc.umaxnum.i64(i64, i64) #0
declare float @llvm.minnum.f32(float, float) #0
declare float @llvm.maxnum.f32(float, float) #0
declare double @llvm.minnum.f64(double, double) #0
declare double @llvm.maxnum.f64(double, double) #0
declare i32 @llvm.amdgcn.wqm.i32(i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readnone convergent }
attributes #3 = { convergent nounwind }
attributes #4 = { nounwind readonly }
