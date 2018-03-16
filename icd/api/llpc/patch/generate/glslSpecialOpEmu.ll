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
    call void @llvm.AMDGPU.kilp()
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
    %p0 = bitcast i32 %p0.i32 to float
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1 = bitcast i32 %p1.i32 to float

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
    %p0 = bitcast i32 %p0.i32 to float
    ; Broadcast channel 0 to whole quad (32768 = 0x8000)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32768)
    %p1 = bitcast i32 %p1.i32 to float

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
    %p0 = bitcast i32 %p0.i32 to float
    ; Swizzle channels in quad (0 -> 0, 0 -> 1, 2 -> 2, 2 -> 3) (32928 = 0x80A0)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32928)
    %p1 = bitcast i32 %p1.i32 to float

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
    %p0 = bitcast i32 %p0.i32 to float
    ; Swizzle channels in quad (0 -> 0, 1 -> 1, 0 -> 2, 1 -> 3) (32836 = 0x8044)
    %p1.i32 = call i32 @llvm.amdgcn.ds.swizzle(i32 %p.i32, i32 32836)
    %p1 = bitcast i32 %p1.i32 to float

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
define i64 @llpc.ballot(i1 %value) #0
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
    %1 = call i64 @llpc.ballot(i1 %value)
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

; GLSL: float readFirstInvocation(float)
define spir_func float @_Z26SubgroupFirstInvocationKHRf(float %value)
{
    %1 = bitcast float %value to i32
    %2 = call i32 @_Z26SubgroupFirstInvocationKHRi(i32 %1)
    %3 = bitcast i32 %2 to float

    ret float %3
}

; GLSL: bool anyInvocation(bool)
define spir_func i1 @_Z14SubgroupAnyKHRb(i1 %value)
{
    %1 = call i64 @llpc.ballot(i1 %value)
    %2 = icmp ne i64 %1, 0

    ret i1 %2
}

; GLSL: bool allInvocations(bool)
define spir_func i1 @_Z14SubgroupAllKHRb(i1 %value)
{
    %1 = call i64 @llpc.ballot(i1 %value)
    %2 = call i64 @llpc.ballot(i1 true)
    %3 = icmp eq i64 %1, %2

    ret i1 %3
}

; GLSL: bool allInvocationsEqual(bool)
define spir_func i1 @_Z19SubgroupAllEqualKHRb(i1 %value)
{
    %1 = call i64 @llpc.ballot(i1 %value)
    %2 = call i64 @llpc.ballot(i1 true)
    %3 = icmp eq i64 %1, %2
    %4 = icmp eq i64 %1, 0
    %5 = or i1 %3, %4

    ret i1 %5
}

; GLSL: int/uint writeInvocation(int/uint, int/uint, int/uint)
define spir_func i32 @_Z18WriteInvocationAMDiii(i32 %inputValue, i32 %writeValue, i32 %invocationIndex)
{
    %1 = call i32 @llvm.amdgcn.writelane(i32 %writeValue, i32 %invocationIndex, i32 %inputValue)
    ret i32 %1
}

; GLSL: float writeInvocation(float, float, uint)
define spir_func float @_Z18WriteInvocationAMDffi(float %inputValue, float %writeValue, i32 %invocationIndex)
{
    %1 = bitcast float %writeValue to i32
    %2 = bitcast float %inputValue to i32
    %3 = call i32 @_Z18WriteInvocationAMDiii(i32 %1, i32 %invocationIndex, i32 %2)
    %4 = bitcast i32 %3 to float

    ret float %4
}

; GLSL: bool subgroupElect()
define spir_func i1 @_Z20GroupNonUniformElecti(i32 %scope)
{
    %1 = call i64 @llpc.ballot(i1 true)
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
                                             i32 1, label %.inclusive
                                             i32 2, label %.exclusive ]

.reduce:
    %5 = call i64 @llvm.ctpop.i64(i64 %2)
    %6 = trunc i64 %5 to i32
    ret i32 %6

.inclusive:
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

.exclusive:
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
    %1 = mul i32 %id, 4
    %2 = call i32 @llvm.amdgcn.ds.bpermute(i32 %1, i32 %value)

    ret i32 %2
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

; GLSL: ivec/uvec subgroupAdd(ivec/uvec)
;       ivec/uvec subgroupInclusiveAdd(ivec/uvec)
;       ivec/uvec subgroupExclusiveAdd(ivec/uvec)

; GLSL: vec subgroupAdd(vec)
;       vec subgroupInclusiveAdd(vec)
;       vec subgroupExclusiveAdd(vec)

; GLSL: dvec subgroupAdd(dvec)
;       dvec subgroupInclusiveAdd(dvec)
;       dvec subgroupExclusiveAdd(dvec)

; GLSL: ivec/uvec subgroupMul(ivec/uvec)
;       ivec/uvec subgroupInclusiveMul(ivec/uvec)
;       ivec/uvec subgroupExclusiveMul(ivec/uvec)

; GLSL: vec subgroupMul(vec)
;       vec subgroupInclusiveMul(vec)
;       vec subgroupExclusiveMul(vec)

; GLSL: dvec subgroupMul(dvec)
;       dvec subgroupInclusiveMul(dvec)
;       dvec subgroupExclusiveMul(dvec)

; GLSL: ivec subgroupMin(ivec)
;       ivec subgroupInclusiveMin(ivec)
;       ivec subgroupExclusiveMin(ivec)

; GLSL: uvec subgroupMin(uvec)
;       uvec subgroupInclusiveMin(uvec)
;       uvec subgroupExclusiveMin(uvec)

; GLSL: vec subgroupMin(vec)
;       vec subgroupInclusiveMin(vec)
;       vec subgroupExclusiveMin(vec)

; GLSL: dvec subgroupMin(dvec)
;       dvec subgroupInclusiveMin(dvec)
;       dvec subgroupExclusiveMin(dvec)

; GLSL: ivec subgroupMax(ivec)
;       ivec subgroupInclusiveMax(ivec)
;       ivec subgroupExclusiveMax(ivec)

; GLSL: uvec subgroupMax(uvec)
;       uvec subgroupInclusiveMax(uvec)
;       uvec subgroupExclusiveMax(uvec)

; GLSL: vec subgroupMax(vec)
;       vec subgroupInclusiveMax(vec)
;       vec subgroupExclusiveMax(vec)

; GLSL: dvec subgroupMax(dvec)
;       dvec subgroupInclusiveMax(dvec)
;       dvec subgroupExclusiveMax(dvec)

; GLSL: ivec/uvec subgroupAnd(ivec/uvec)
;       ivec/uvec subgroupInclusiveAnd(ivec/uvec)
;       ivec/uvec subgroupExclusiveAnd(ivec/uvec)

; GLSL: ivec/uvec subgroupOr(ivec/uvec)
;       ivec/uvec subgroupInclusiveOr(ivec/uvec)
;       ivec/uvec subgroupExclusiveOr(ivec/uvec)

; GLSL: ivec/uvec subgroupXor(ivec/uvec)
;       ivec/uvec subgroupInclusiveXor(ivec/uvec)
;       ivec/uvec subgroupExclusiveXor(ivec/uvec)

; GLSL: bvec subgroupAnd(bvec)
;       bvec subgroupInclusiveAnd(bvec)
;       bvec subgroupExclusiveAnd(bvec)

; GLSL: bvec subgroupOr(bvec)
;       bvec subgroupInclusiveOr(bvec)
;       bvec subgroupExclusiveOr(bvec)

; GLSL: bvec subgroupXor(bvec)
;       bvec subgroupInclusiveXor(bvec)
;       bvec subgroupExclusiveXor(bvec)

; GLSL: gvec subgroupQuadBroadcast(gvec, uint)

; GLSL: gvec subgroupQuadSwapHorizontal(gvec)
;       gvec subgroupQuadSwapVertical(gvec)
;       gvec subgroupQuadSwapDiagonal(gvec)

; =====================================================================================================================
; >>>  Interpolation Functions
; =====================================================================================================================

; Adjust interpolation I/J according to specified offsets X/Y
define float @llpc.input.interpolate.adjustij(float %ij, float %offsetX, float %offsetY)
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
define <2 x float> @llpc.input.interpolate.evalij.offset(<2 x float> %offset) #0
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
    %7 = call float @llpc.input.interpolate.adjustij(float %2, float %5, float %6)
    %8 = call float @llpc.input.interpolate.adjustij(float %3, float %5, float %6)
    %9 = call float @llpc.input.interpolate.adjustij(float %4, float %5, float %6)

    ; Get final I, J
    %10 = fmul float %7, %9
    %11 = fmul float %8, %9

    %12 = insertelement <2 x float> undef, float %10, i32 0
    %13 = insertelement <2 x float> %12, float %11, i32 1

    ret <2 x float> %13
}

; Evaluate interpolation I/J for GLSL function interpolateAtOffset() with "noperspective" qualifier specified
; on interpolant
define <2 x float> @llpc.input.interpolate.evalij.offset.noperspective(<2 x float> %offset) #0
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
    %6 = call float @llpc.input.interpolate.adjustij(float %2, float %4, float %5)
    %7 = call float @llpc.input.interpolate.adjustij(float %3, float %4, float %5)

    %8 = insertelement <2 x float> undef, float %6, i32 0
    %9 = insertelement <2 x float> %8, float %7, i32 1

    ret <2 x float> %9
}

; Evaluate interpolation I/J for GLSL function interpolateAtSample()
define <2 x float> @llpc.input.interpolate.evalij.sample(i32 %sample) #0
{
    ; BuiltInSamplePosOffset 268435463 = 0x10000007
    %1 = call <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 268435463, i32 %sample)
    %2 = call <2 x float> @llpc.input.interpolate.evalij.offset(<2 x float> %1)
    ret <2 x float> %2
}

; Evaluate interpolation I/J for GLSL function interpolateAtSample() with "noperspective" qualifier specified
; on interpolant
define <2 x float> @llpc.input.interpolate.evalij.sample.noperspective(i32 %sample) #0
{
    ; BuiltInSamplePosOffset 268435463 = 0x10000007
    %1 = call <2 x float> @llpc.input.import.builtin.SamplePosOffset(i32 268435463, i32 %sample)
    %2 = call <2 x float> @llpc.input.interpolate.evalij.offset.noperspective(<2 x float> %1)
    ret <2 x float> %2
}

declare void @llvm.AMDGPU.kilp() #0
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
declare i64 @llvm.cttz.i64(i64, i1) #0
declare i64 @llvm.ctlz.i64(i64, i1) #0
declare i64 @llvm.ctpop.i64(i64) #0
declare i32 @llvm.amdgcn.ds.bpermute(i32, i32) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readnone convergent }
attributes #3 = { convergent nounwind }

