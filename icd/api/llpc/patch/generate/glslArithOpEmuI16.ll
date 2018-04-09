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
; >>>  Operators
; =====================================================================================================================

; GLSL: int16_t = int16_t % int16_t
define i16 @llpc.mod.i16(i16 %x, i16 %y) #0
{
    %1 = srem i16 %x, %y
    %2 = add i16 %1, %y
    ; Check if the signedness of x and y are the same.
    %3 = xor i16 %x, %y
    ; if negative, slt signed less than
    %4 = icmp slt i16 %3, 0
    ; Check if the remainder is not 0.
    %5 = icmp ne i16 %1, 0
    %6 = and i1 %4, %5
    %7 = select i1 %6, i16 %2, i16 %1
    ret i16 %7
}

; =====================================================================================================================
; >>>  Common Functions
; =====================================================================================================================

; GLSL: int16_t abs(int16_t)
define i16 @llpc.sabs.i16(i16 %x) #0
{
    %nx = sub i16 0, %x
    %con = icmp sgt i16 %x, %nx
    %val = select i1 %con, i16 %x, i16 %nx
    ret i16 %val
}

; GLSL: int16_t sign(int16_t)
define i16 @llpc.ssign.i16(i16 %x) #0
{
    %con1 = icmp sgt i16 %x, 0
    %ret1 = select i1 %con1, i16 1, i16 %x
    %con2 = icmp sge i16 %ret1, 0
    %ret2 = select i1 %con2, i16 %ret1, i16 -1
    ret i16 %ret2
}

; GLSL: int16_t min(int16_t, int16_t)
define i16 @llpc.sminnum.i16(i16 %x, i16 %y) #0
{
    %1 = icmp slt i16 %y, %x
    %2 = select i1 %1, i16 %y, i16 %x
    ret i16 %2
}

; GLSL: int16_t min(int16_t, int16_t)
define spir_func i16 @_Z4sminss(
    i16 %x, i16 %y) #0
{
    %cmp = icmp sle i16 %x, %y
    %val = select i1 %cmp, i16 %x, i16 %y
    ret i16 %val
}

; GLSL: i16vec2 min(i16vec2, i16vec2)
define spir_func <2 x i16> @_Z4sminDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %y) #0
{
    %cmp = icmp sle <2 x i16> %x, %y
    %val = select <2 x i1> %cmp, <2 x i16> %x, <2 x i16> %y

    ret <2 x i16> %val
}

; GLSL: i16vec3 min(i16vec3, i16vec3)
define spir_func <3 x i16> @_Z4sminDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %y) #0
{
    %cmp = icmp sle <3 x i16> %x, %y
    %val = select <3 x i1> %cmp, <3 x i16> %x, <3 x i16> %y

    ret <3 x i16> %val
}

; GLSL: i16vec4 min(i16vec4, i16vec4)
define spir_func <4 x i16> @_Z4sminDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %y) #0
{
    %cmp = icmp sle <4 x i16> %x, %y
    %val = select <4 x i1> %cmp, <4 x i16> %x, <4 x i16> %y

    ret <4 x i16> %val
}

; GLSL: uint16_t min(uint16_t, uint16_t)
define spir_func i16 @_Z4uminss(
    i16 %x, i16 %y) #0
{
    %cmp = icmp ule i16 %x, %y
    %val = select i1 %cmp, i16 %x, i16 %y

    ret i16 %val
}

; GLSL: u16vec2 min(u16vec2, u16vec2)
define spir_func <2 x i16> @_Z4uminDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %y) #0
{
    %cmp = icmp ule <2 x i16> %x, %y
    %val = select <2 x i1> %cmp, <2 x i16> %x, <2 x i16> %y

    ret <2 x i16> %val
}

; GLSL: u16vec3 min(u16vec3, u16vec3)
define spir_func <3 x i16> @_Z4uminDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %y) #0
{
    %cmp = icmp ule <3 x i16> %x, %y
    %val = select <3 x i1> %cmp, <3 x i16> %x, <3 x i16> %y

    ret <3 x i16> %val
}

; GLSL: u16vec4 min(u16vec4, u16vec4)
define spir_func <4 x i16> @_Z4uminDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %y) #0
{
    %cmp = icmp ule <4 x i16> %x, %y
    %val = select <4 x i1> %cmp, <4 x i16> %x, <4 x i16> %y

    ret <4 x i16> %val
}

; GLSL: int16_t max(int16_t, int16_t)
define spir_func i16 @_Z4smaxss(
    i16 %x, i16 %y) #0
{
    %cmp = icmp sge i16 %x, %y
    %val = select i1 %cmp, i16 %x, i16 %y
    ret i16 %val
}

; GLSL: i16vec2 max(i16vec2, i16vec2)
define spir_func <2 x i16> @_Z4smaxDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %y) #0
{
    %cmp = icmp sge <2 x i16> %x, %y
    %val = select <2 x i1> %cmp, <2 x i16> %x, <2 x i16> %y

    ret <2 x i16> %val
}

; GLSL: i16vec3 max(i16vec3, i16vec3)
define spir_func <3 x i16> @_Z4smaxDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %y) #0
{
    %cmp = icmp sge <3 x i16> %x, %y
    %val = select <3 x i1> %cmp, <3 x i16> %x, <3 x i16> %y

    ret <3 x i16> %val
}

; GLSL: i16vec4 max(i16vec4, i16vec4)
define spir_func <4 x i16> @_Z4smaxDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %y) #0
{
    %cmp = icmp sge <4 x i16> %x, %y
    %val = select <4 x i1> %cmp, <4 x i16> %x, <4 x i16> %y

    ret <4 x i16> %val
}


; GLSL: uint16_t max(uint16_t, uint16_t)
define spir_func i16 @_Z4umaxss(
    i16 %x, i16 %y) #0
{
    %cmp = icmp uge i16 %x, %y
    %val = select i1 %cmp, i16 %x, i16 %y

    ret i16 %val
}

; GLSL: u16vec2 max(u16vec2, u16vec2)
define spir_func <2 x i16> @_Z4umaxDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %y) #0
{
    %cmp = icmp uge <2 x i16> %x, %y
    %val = select <2 x i1> %cmp, <2 x i16> %x, <2 x i16> %y

    ret <2 x i16> %val
}

; GLSL: u16vec3 max(u16vec3, u16vec3)
define spir_func <3 x i16> @_Z4umaxDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %y) #0
{
    %cmp = icmp uge <3 x i16> %x, %y
    %val = select <3 x i1> %cmp, <3 x i16> %x, <3 x i16> %y

    ret <3 x i16> %val
}

; GLSL: u16vec4 max(u16vec4, u16vec4)
define spir_func <4 x i16> @_Z4umaxDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %y) #0
{
    %cmp = icmp uge <4 x i16> %x, %y
    %val = select <4 x i1> %cmp, <4 x i16> %x, <4 x i16> %y

    ret <4 x i16> %val
}

; GLSL: uint16_t clamp(uint16_t, uint16_t, uint16_t)
define spir_func i16 @_Z6uclampsss(
    i16 %x, i16 %minVal, i16 %maxVal) #0
{
    %1 = call i16 @_Z4umaxss(i16 %x, i16 %minVal)
    %2 = call i16 @_Z4uminss(i16 %1, i16 %maxVal)
    ret i16 %2
}

; GLSL: u16vec2 clamp(u16vec2, u16vec2, u16vec2)
define spir_func <2 x i16> @_Z6uclampDv2_sDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %minVal, <2 x i16> %maxVal) #0
{
    %1 = call <2 x i16> @_Z4umaxDv2_sDv2_s(<2 x i16> %x, <2 x i16> %minVal)
    %2 = call <2 x i16> @_Z4uminDv2_sDv2_s(<2 x i16> %1, <2 x i16> %maxVal)
    ret <2 x i16> %2
}

; GLSL: u16vec3 clamp(u16vec3, u16vec3, u16vec3)
define spir_func <3 x i16> @_Z6uclampDv3_sDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %minVal, <3 x i16> %maxVal) #0
{
    %1 = call <3 x i16> @_Z4umaxDv3_sDv3_s(<3 x i16> %x, <3 x i16> %minVal)
    %2 = call <3 x i16> @_Z4uminDv3_sDv3_s(<3 x i16> %1, <3 x i16> %maxVal)
    ret <3 x i16> %2
}

; GLSL: u16vec4 clamp(u16vec4, u16vec4, u16vec4)
define spir_func <4 x i16> @_Z6uclampDv4_sDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %minVal, <4 x i16> %maxVal) #0
{
    %1 = call <4 x i16> @_Z4umaxDv4_sDv4_s(<4 x i16> %x, <4 x i16> %minVal)
    %2 = call <4 x i16> @_Z4uminDv4_sDv4_s(<4 x i16> %1, <4 x i16> %maxVal)
    ret <4 x i16> %2
}

; GLSL: int16_t clamp(int16_t, int16_t, int16_t)
define spir_func i16 @_Z6sclampsss(
    i16 %x, i16 %minVal, i16 %maxVal) #0
{
    %1 = call i16 @_Z4smaxss(i16 %x, i16 %minVal)
    %2 = call i16 @_Z4sminss(i16 %1, i16 %maxVal)
    ret i16 %2
}

; GLSL: i16vec2 clamp(i16vec2, i16vec2, i16vec2)
define spir_func <2 x i16> @_Z6sclampDv2_sDv2_sDv2_s(
    <2 x i16> %x, <2 x i16> %minVal, <2 x i16> %maxVal) #0
{
    %1 = call <2 x i16> @_Z4smaxDv2_sDv2_s(<2 x i16> %x, <2 x i16> %minVal)
    %2 = call <2 x i16> @_Z4sminDv2_sDv2_s(<2 x i16> %1, <2 x i16> %maxVal)
    ret <2 x i16> %2
}

; GLSL: i16vec3 clamp(i16vec3, i16vec3, i16vec3)
define spir_func <3 x i16> @_Z6sclampDv3_sDv3_sDv3_s(
    <3 x i16> %x, <3 x i16> %minVal, <3 x i16> %maxVal) #0
{
    %1 = call <3 x i16> @_Z4smaxDv3_sDv3_s(<3 x i16> %x, <3 x i16> %minVal)
    %2 = call <3 x i16> @_Z4sminDv3_sDv3_s(<3 x i16> %1, <3 x i16> %maxVal)
    ret <3 x i16> %2
}

; GLSL: i16vec4 clamp(i16vec4, i16vec4, i16vec4)
define spir_func <4 x i16> @_Z6sclampDv4_sDv4_sDv4_s(
    <4 x i16> %x, <4 x i16> %minVal, <4 x i16> %maxVal) #0
{
    %1 = call <4 x i16> @_Z4smaxDv4_sDv4_s(<4 x i16> %x, <4 x i16> %minVal)
    %2 = call <4 x i16> @_Z4sminDv4_sDv4_s(<4 x i16> %1, <4 x i16> %maxVal)
    ret <4 x i16> %2
}

; half ldexp()  =>  llvm.amdgcn.ldexp.f16
define spir_func half @_Z5ldexpDhs(
    half %x, i16 %exp) #0
{
    %exps = sext i16 %exp to i32
    %1 = call half @llvm.amdgcn.ldexp.f16(half %x, i32 %exps)

    ret half %1
}

; <2 x half> ldexp()  =>  llvm.amdgcn.ldexp.f16
define spir_func <2 x half> @_Z5ldexpDv2_DhDv2_s(
    <2 x half> %x, <2 x i16> %exp) #0
{
    ; Extract components from source vectors
    %x0 = extractelement <2 x half> %x, i16 0
    %x1 = extractelement <2 x half> %x, i16 1

    %exp0 = extractelement <2 x i16> %exp, i16 0
    %exp1 = extractelement <2 x i16> %exp, i16 1
    %exp0s = sext i16 %exp0 to i32
    %exp1s = sext i16 %exp1 to i32

    ; Call LLVM/LLPC instrinsic, do component-wise computation
    %1 = call half @llvm.amdgcn.ldexp.f16(half %x0, i32 %exp0s)
    %2 = call half @llvm.amdgcn.ldexp.f16(half %x1, i32 %exp1s)

    ; Insert computed components into the destination vector
    %3 = alloca <2 x half>
    %4 = load <2 x half>, <2 x half>* %3
    %5 = insertelement <2 x half> %4, half %1, i16 0
    %6 = insertelement <2 x half> %5, half %2, i16 1

    ret <2 x half> %6
}

; <3 x half> ldexp()  =>  llvm.amdgcn.ldexp.f16
define spir_func <3 x half> @_Z5ldexpDv3_DhDv3_s(
    <3 x half> %x, <3 x i16> %exp) #0
{
    ; Extract components from source vectors
    %x0 = extractelement <3 x half> %x, i16 0
    %x1 = extractelement <3 x half> %x, i16 1
    %x2 = extractelement <3 x half> %x, i16 2

    %exp0 = extractelement <3 x i16> %exp, i16 0
    %exp1 = extractelement <3 x i16> %exp, i16 1
    %exp2 = extractelement <3 x i16> %exp, i16 2

    %exp0s = sext i16 %exp0 to i32
    %exp1s = sext i16 %exp1 to i32
    %exp2s = sext i16 %exp2 to i32

    ; Call LLVM/LLPC instrinsic, do component-wise computation
    %1 = call half @llvm.amdgcn.ldexp.f16(half %x0, i32 %exp0s)
    %2 = call half @llvm.amdgcn.ldexp.f16(half %x1, i32 %exp1s)
    %3 = call half @llvm.amdgcn.ldexp.f16(half %x2, i32 %exp2s)

    ; Insert computed components into the destination vector
    %4 = alloca <3 x half>
    %5 = load <3 x half>, <3 x half>* %4
    %6 = insertelement <3 x half> %5, half %1, i16 0
    %7 = insertelement <3 x half> %6, half %2, i16 1
    %8 = insertelement <3 x half> %7, half %3, i16 2

    ret <3 x half> %8
}

; <4 x half> ldexp()  =>  llvm.amdgcn.ldexp.f16
define spir_func <4 x half> @_Z5ldexpDv4_DhDv4_s(
    <4 x half> %x, <4 x i16> %exp) #0
{
    ; Extract components from source vectors
    %x0 = extractelement <4 x half> %x, i16 0
    %x1 = extractelement <4 x half> %x, i16 1
    %x2 = extractelement <4 x half> %x, i16 2
    %x3 = extractelement <4 x half> %x, i16 3

    %exp0 = extractelement <4 x i16> %exp, i16 0
    %exp1 = extractelement <4 x i16> %exp, i16 1
    %exp2 = extractelement <4 x i16> %exp, i16 2
    %exp3 = extractelement <4 x i16> %exp, i16 3

    %exp0s = sext i16 %exp0 to i32
    %exp1s = sext i16 %exp1 to i32
    %exp2s = sext i16 %exp2 to i32
    %exp3s = sext i16 %exp3 to i32

    ; Call LLVM/LLPC instrinsic, do component-wise computation
    %1 = call half @llvm.amdgcn.ldexp.f16(half %x0, i32 %exp0s)
    %2 = call half @llvm.amdgcn.ldexp.f16(half %x1, i32 %exp1s)
    %3 = call half @llvm.amdgcn.ldexp.f16(half %x2, i32 %exp2s)
    %4 = call half @llvm.amdgcn.ldexp.f16(half %x3, i32 %exp3s)

    ; Insert computed components into the destination vector
    %5 = alloca <4 x half>
    %6 = load <4 x half>, <4 x half>* %5
    %7 = insertelement <4 x half> %6, half %1, i16 0
    %8 = insertelement <4 x half> %7, half %2, i16 1
    %9 = insertelement <4 x half> %8, half %3, i16 2
    %10 = insertelement <4 x half> %9, half %4, i16 3

    ret <4 x half> %10
}

; =====================================================================================================================
; >>>  Functions of Extension AMD_shader_trinary_minmax
; =====================================================================================================================

; GLSL: int16_t min3(int16_t, int16_t, int16_t)
define i16 @llpc.smin3.i16(i16 %x, i16 %y, i16 %z)
{
    ; min(x, y)
    %1 = icmp slt i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; min(min(x, y), z)
    %3 = icmp slt i16 %2, %z
    %4 = select i1 %3, i16 %2, i16 %z

    ret i16 %4
}

; GLSL: int16_t max3(int16_t, int16_t, int16_t)
define i16 @llpc.smax3.i16(i16 %x, i16 %y, i16 %z)
{
    ; max(x, y)
    %1 = icmp sgt i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; max(max(x, y), z)
    %3 = icmp sgt i16 %2, %z
    %4 = select i1 %3, i16 %2, i16 %z

    ret i16 %4
}

; GLSL: int16_t mid3(int16_t, int16_t, int16_t)
define i16 @llpc.smid3.i16(i16 %x, i16 %y, i16 %z)
{
    ; min(x, y)
    %1 = icmp slt i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; max(x, y)
    %3 = icmp sgt i16 %x, %y
    %4 = select i1 %3, i16 %x, i16 %y

    ; min(max(x, y), z)
    %5 = icmp slt i16 %4, %z
    %6 = select i1 %5, i16 %4, i16 %z

    ; max(min(x, y), min(max(x, y), z))
    %7 = icmp sgt i16 %2, %6
    %8 = select i1 %7, i16 %2, i16 %6

    ret i16 %8
}

; GLSL: uint16_t min3(uint16_t, uint16_t, uint16_t)
define i16 @llpc.umin3.i16(i16 %x, i16 %y, i16 %z)
{
    ; min(x, y)
    %1 = icmp ult i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; min(min(x, y), z)
    %3 = icmp ult i16 %2, %z
    %4 = select i1 %3, i16 %2, i16 %z

    ret i16 %4
}

; GLSL: uint16_t max3(uint16_t, uint16_t, uint16_t)
define i16 @llpc.umax3.i16(i16 %x, i16 %y, i16 %z)
{
    ; max(x, y)
    %1 = icmp ugt i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; max(max(x, y), z)
    %3 = icmp ugt i16 %2, %z
    %4 = select i1 %3, i16 %2, i16 %z

    ret i16 %4
}

; GLSL: uint16_t mid3(uint16_t, uint16_t, uint16_t)
define i16 @llpc.umid3.i16(i16 %x, i16 %y, i16 %z)
{
    ; min(x, y)
    %1 = icmp ult i16 %x, %y
    %2 = select i1 %1, i16 %x, i16 %y

    ; max(x, y)
    %3 = icmp ugt i16 %x, %y
    %4 = select i1 %3, i16 %x, i16 %y

    ; min(max(x, y), z)
    %5 = icmp ult i16 %4, %z
    %6 = select i1 %5, i16 %4, i16 %z

    ; max(min(x, y), min(max(x, y), z))
    %7 = icmp ugt i16 %2, %6
    %8 = select i1 %7, i16 %2, i16 %6

    ret i16 %8
}

declare half @llvm.amdgcn.ldexp.f16(half, i32) #1
declare i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x) #1
declare half @llvm.amdgcn.frexp.mant.f16(half) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }