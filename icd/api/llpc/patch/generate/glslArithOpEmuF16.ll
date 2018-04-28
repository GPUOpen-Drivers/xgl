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

; GLSL: float16_t = float16_t/float16_t
define half @llpc.fdiv.f16(half %y, half %x) #0
{
    %1 = fdiv half 1.0, %x
    %2 = fmul half %y, %1
    ret half %2
}

; GLSL: float16_t = float16_t(float) (rounding mode: RTZ)
define spir_func half @_Z16convert_half_rtzf(float %x) #0
{
    %1 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %x, float 0.0) #1
    %2 = extractelement <2 x half> %1, i32 0
    ret half %2
}

; GLSL: f16vec2 = f16vec2(vec2) (rounding mode: RTZ)
define spir_func <2 x half> @_Z17convert_half2_rtzDv2_f(<2 x float> %x) #0
{
    %1 = extractelement <2 x float> %x, i32 0
    %2 = extractelement <2 x float> %x, i32 1
    %3 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %1, float %2) #1
    ret <2 x half> %3
}

; GLSL: f16vec3 = f16vec3(vec3) (rounding mode: RTZ)
define spir_func <3 x half> @_Z17convert_half3_rtzDv3_f(<3 x float> %x) #0
{
    %1 = extractelement <3 x float> %x, i32 0
    %2 = extractelement <3 x float> %x, i32 1
    %3 = extractelement <3 x float> %x, i32 2
    %4 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %1, float %2) #1
    %5 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %3, float 0.0) #1
    %6 = shufflevector <2 x half> %4, <2 x half> %5, <3 x i32> <i32 0, i32 1, i32 2>
    ret <3 x half> %6
}

; GLSL: f16vec4 = f16vec4(vec4) (rounding mode: RTZ)
define spir_func <4 x half> @_Z17convert_half4_rtzDv4_f(<4 x float> %x) #0
{
    %1 = extractelement <4 x float> %x, i32 0
    %2 = extractelement <4 x float> %x, i32 1
    %3 = extractelement <4 x float> %x, i32 2
    %4 = extractelement <4 x float> %x, i32 3
    %5 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %1, float %2) #1
    %6 = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %3, float %4) #1
    %7 = shufflevector <2 x half> %5, <2 x half> %6, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    ret <4 x half> %7
}

; GLSL: float16_t = float16_t(float) (rounding mode: RTE)
define spir_func half @_Z16convert_half_rtef(float %x) #0
{
    %1 = fptrunc float %x to half
    ret half %1
}

; GLSL: f16vec2 = f16vec2(vec2) (rounding mode: RTE)
define spir_func <2 x half> @_Z17convert_half2_rteDv2_f(<2 x float> %x) #0
{
    %1 = fptrunc <2 x float> %x to <2 x half>
    ret <2 x half> %1
}

; GLSL: f16vec3 = f16vec3(vec3) (rounding mode: RTE)
define spir_func <3 x half> @_Z17convert_half3_rteDv3_f(<3 x float> %x) #0
{
    %1 = fptrunc <3 x float> %x to <3 x half>
    ret <3 x half> %1
}

; GLSL: f16vec4 = f16vec4(vec4) (rounding mode: RTE)
define spir_func <4 x half> @_Z17convert_half4_rteDv4_f(<4 x float> %x) #0
{
    %1 = fptrunc <4 x float> %x to <4 x half>
    ret <4 x half> %1
}

; GLSL: float16_t = float16_t(float) (rounding mode: RTP)
define spir_func half @_Z16convert_half_rtpf(float %x) #0
{
    %1 = fptrunc float %x to half
    ret half %1
}

; GLSL: f16vec2 = f16vec2(vec2) (rounding mode: RTP)
define spir_func <2 x half> @_Z17convert_half2_rtpDv2_f(<2 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <2 x float> %x to <2 x half>
    ret <2 x half> %1
}

; GLSL: f16vec3 = f16vec3(vec3) (rounding mode: RTP)
define spir_func <3 x half> @_Z17convert_half3_rtpDv3_f(<3 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <3 x float> %x to <3 x half>
    ret <3 x half> %1
}

; GLSL: f16vec4 = f16vec4(vec4) (rounding mode: RTP)
define spir_func <4 x half> @_Z17convert_half4_rtpDv4_f(<4 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <4 x float> %x to <4 x half>
    ret <4 x half> %1
}

; GLSL: float16_t = float16_t(float) (rounding mode: RTN)
define spir_func half @_Z16convert_half_rtnf(float %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc float %x to half
    ret half %1
}

; GLSL: f16vec2 = f16vec2(vec2) (rounding mode: RTN)
define spir_func <2 x half> @_Z17convert_half2_rtnDv2_f(<2 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <2 x float> %x to <2 x half>
    ret <2 x half> %1
}

; GLSL: f16vec3 = f16vec3(vec3) (rounding mode: RTN)
define spir_func <3 x half> @_Z17convert_half3_rtnDv3_f(<3 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <3 x float> %x to <3 x half>
    ret <3 x half> %1
}

; GLSL: f16vec4 = f16vec4(vec4) (rounding mode: RTN)
define spir_func <4 x half> @_Z17convert_half4_rtnDv4_f(<4 x float> %x) #0
{
    ; TODO: Use s.setreg() to change HW_REG_MODE.
    %1 = fptrunc <4 x float> %x to <4 x half>
    ret <4 x half> %1
}

; =====================================================================================================================
; >>>  Angle and Trigonometry Functions
; =====================================================================================================================

; GLSL: float16_t radians(float16_t)
define spir_func half @_Z7radiansDh(half %degrees) #0
{
    ; 0x3F91DC0000000000: PI/180, 0.01744080
    %1 = fmul half %degrees, 0x3F91DC0000000000
    ret half %1
}

; GLSL: f16vec2 radians(f16vec2)
define spir_func <2 x half> @_Z7radiansDv2_Dh(<2 x half> %degrees) #0
{
    %1 = alloca <2 x half>
    %2 = load <2 x half>, <2 x half>* %1
    ; 0x3F91DC0000000000: PI/180, 0.01744080
    %3 = insertelement <2 x half> %2, half 0x3F91DC0000000000, i32 0
    %4 = insertelement <2 x half> %3, half 0x3F91DC0000000000, i32 1
    %5 = fmul <2 x half> %4, %degrees
    ret <2 x half> %5
}

; GLSL: f16vec3 radians(f16vec3)
define spir_func <3 x half> @_Z7radiansDv3_Dh(<3 x half> %degrees) #0
{
    %1 = alloca <3 x half>
    %2 = load <3 x half>, <3 x half>* %1
    ; 0x3F91DC0000000000: PI/180, 0.01744080
    %3 = insertelement <3 x half> %2, half 0x3F91DC0000000000, i32 0
    %4 = insertelement <3 x half> %3, half 0x3F91DC0000000000, i32 1
    %5 = insertelement <3 x half> %4, half 0x3F91DC0000000000, i32 2
    %6 = fmul <3 x half> %5, %degrees
    ret <3 x half> %6
}

; GLSL: f16vec4 radians(f16vec4)
define spir_func <4 x half> @_Z7radiansDv4_Dh(<4 x half> %degrees) #0
{
    %1 = alloca <4 x half>
    %2 = load <4 x half>, <4 x half>* %1
    ; 0x3F91DC0000000000: PI/180, 0.01744080
    %3 = insertelement <4 x half> %2, half 0x3F91DC0000000000, i32 0
    %4 = insertelement <4 x half> %3, half 0x3F91DC0000000000, i32 1
    %5 = insertelement <4 x half> %4, half 0x3F91DC0000000000, i32 2
    %6 = insertelement <4 x half> %5, half 0x3F91DC0000000000, i32 3
    %7 = fmul <4 x half> %6, %degrees
    ret <4 x half> %7
}

; GLSL: float16_t degrees(float16_t)
define spir_func half @_Z7degreesDh(half %radians) #0
{
    ; 0x404CA40000000000 is 57.28125000
    %1 = fmul half %radians, 0x404CA40000000000
    ret half %1
}

; GLSL: f16vec2 degrees(f16vec2)
define spir_func <2 x half> @_Z7degreesDv2_Dh(<2 x half> %radians) #0
{
    %1 = alloca <2 x half>
    %2 = load <2 x half>, <2 x half>* %1
    ; 0x404CA40000000000: 180/PI, 57.28125000
    %3 = insertelement <2 x half> %2, half 0x404CA40000000000, i32 0
    %4 = insertelement <2 x half> %3, half 0x404CA40000000000, i32 1
    %5 = fmul <2 x half> %4, %radians
    ret <2 x half> %5
}

; GLSL: f16vec3 degrees(f16vec3)
define spir_func <3 x half> @_Z7degreesDv3_Dh(<3 x half> %radians) #0
{
    %1 = alloca <3 x half>
    %2 = load <3 x half>, <3 x half>* %1
    ; 0x404CA5DC20000000: 180/PI, 57.28125000
    %3 = insertelement <3 x half> %2, half 0x404CA40000000000, i32 0
    %4 = insertelement <3 x half> %3, half 0x404CA40000000000, i32 1
    %5 = insertelement <3 x half> %4, half 0x404CA40000000000, i32 2
    %6 = fmul <3 x half> %5, %radians
    ret <3 x half> %6
}

; GLSL: f16vec4 degrees(f16vec4)
define spir_func <4 x half> @_Z7degreesDv4_Dh(<4 x half> %radians) #0
{
    %1 = alloca <4 x half>
    %2 = load <4 x half>, <4 x half>* %1
    ; 0x404CA40000000000: 180/PI, 57.28125000
    %3 = insertelement <4 x half> %2, half 0x404CA40000000000, i32 0
    %4 = insertelement <4 x half> %3, half 0x404CA40000000000, i32 1
    %5 = insertelement <4 x half> %4, half 0x404CA40000000000, i32 2
    %6 = insertelement <4 x half> %5, half 0x404CA40000000000, i32 3
    %7 = fmul <4 x half> %6, %radians
    ret <4 x half> %7
}

; GLSL: float16_t tan(float16_t)
define half @llpc.tan.f16(half %angle) #0
{
    %1 = call half @llvm.sin.f16(half %angle)
    %2 = call half @llvm.cos.f16(half %angle)
    %3 = fdiv half 1.0, %2
    %4 = fmul half %1, %3
    ret half %4
}

; GLSL: float16_t asin(float16_t)
define half @llpc.asin.f16(half %x)
{
    ; asin(x) = sgn(x) * (PI/2 - sqrt(1 - |x|) * (PI/2 + |x| * (PI/4 - 1 + |x| * (p0 + |x| * p1))))
    ;   p0 = 0.086566724, p1 = -0.0310

    %1 = call half @llvm.fabs.f16(half %x)
    ; 0xBF9FC40000000000: p1 = -0.03102955
    %2 = fmul half %1, 0xBF9FC40000000000
    ; 0x3FB6280000000000: p0 = 0.08656672
    %3 = fadd half %2, 0x3FB6280000000000
    %4 = fmul half %1, %3
    ; 0xBFCB780000000000: PI/4 - 1 = -0.21460181
    %5 = fadd half %4, 0xBFCB780000000000
    %6 = fmul half %1, %5
    ; 0x3FF9200000000000: PI/2 = 1.57079637
    %7 = fadd half %6, 0x3FF9200000000000
    %8 = fsub half 1.0, %1
    %9 = call half @llvm.sqrt.f16(half %8)
    %10 = fmul half %9, %7
    ; 0x3FF9200000000000: PI/4 = 1.57079637
    %11 = fsub half 0x3FF9200000000000, %10
    %12 = fcmp ogt half %x, 0.0
    %13 = select i1 %12, half 1.0, half %x
    %14 = fcmp oge half %13, 0.0
    %15 = select i1 %14, half %13, half -1.0
    %16 = fmul half %15, %11
    ret half %16
}

; GLSL: float16_t acos(float16_t)
define half @llpc.acos.f16(half %x)
{
    ; acos(x) = PI/2 - sgn(x) * (PI/2 - sqrt(1 - |x|) * (PI/2 + |x| * (PI/4 - 1 + |x| * (p0 + |x| * p1))))
    ;   p0 = 0.08132463, p1 = -0.02363318

    %1 = call half @llvm.fabs.f16(half %x)
    ; 0xBF9FC40000000000: p1 = -0.02363318
    %2 = fmul half %1, 0xBF9FC40000000000
    ; 0x3FB6280000000000: p0 = 0.08654785
    %3 = fadd half %2, 0x3FB6280000000000
    %4 = fmul half %1, %3
    ; 0xBFCB780000000000: PI/4 - 1 = -0.21460181
    %5 = fadd half %4, 0xBFCB780000000000
    %6 = fmul half %1, %5
    ; 0x3FF9200000000000: PI/2 = 1.57079637
    %7 = fadd half %6, 0x3FF9200000000000
    %8 = fsub half 1.0, %1
    %9 = call half @llvm.sqrt.f16(half %8)
    %10 = fmul half %9, %7
    ; 0x3FF9200000000000: PI/2 = 1.57079637
    %11 = fsub half 0x3FF9200000000000, %10
    %12 = fcmp ogt half %x, 0.0
    %13 = select i1 %12, half 1.0, half %x
    %14 = fcmp oge half %13, 0.0
    %15 = select i1 %14, half %13, half -1.0
    %16 = fmul half %15, %11
    ; 0x3FF9200000000000: PI/2 = 1.57079637
    %17 = fsub half 0x3FF9200000000000, %16
    ret half %17
}

; GLSL: float16_t atan(float16_t)
define half @llpc.atan.f16(half %x)
{
    ; atan(x) = x - x^3 / 3 + x^5 / 5 - x^7 / 7 + x^9 / 9 - x^11 / 11, x <= 1.0
    ; x = min(1.0, x) / max(1.0, x), make x <= 1.0

    %1 = call half @llvm.fabs.f16(half %x)
    %2 = call half @llvm.maxnum.f16(half %1, half 1.0)
    %3 = call half @llvm.minnum.f16(half %1, half 1.0)
    %4 = fdiv half 1.0, %2

    %5 = fmul half %3, %4
    %6 = fmul half %5, %5
    %7 = fmul half %6, %5
    %8 = fmul half %7, %6
    %9 = fmul half %8, %6
    %10 = fmul half %9, %6
    %11 = fmul half %10, %6

    ; 0x3FEFFC0000000000: 0.99997932
    %12 = fmul half %5, 0x3FEFFC0000000000
    ; 0xBFD5480000000000: -0.33267564
    %13 = fmul half %7, 0xBFD5480000000000
    ; 0x3FC8D00000000000: 0.19389249
    %14 = fmul half %8, 0x3FC8D00000000000
    ; 0xBFBE080000000000: -0.11735032
    %15 = fmul half %9, 0xBFBE080000000000
    ; 0x3FAB7C0000000000: 0.05368138
    %16 = fmul half %10, 0x3FAB7C0000000000
    ; 0xBF88D80000000000: -0.01213232
    %17 = fmul half %11, 0xBF88D80000000000
    %18 = fadd half %12, %13
    %19 = fadd half %18, %14
    %20 = fadd half %19, %15
    %21 = fadd half %20, %16
    %22 = fadd half %21, %17
    %23 = fmul half %22, -2.0
    ; 0x3FF9200000000000: 1.57079637
    %24 = fadd half %23, 0x3FF9200000000000
    %25 = fcmp ogt half %1, 1.0
    %26 = select i1 %25, half 1.0, half 0.0
    %27 = fmul half %26, %24
    %28 = fadd half %22, %27
    %29 = fcmp ogt half %x, 0.0
    %30 = select i1 %29, half 1.0, half %x
    %31 = fcmp oge half %30, 0.0
    %32 = select i1 %31, half %30, half -1.0
    %33 = fmul half %28, %32
    ret half %33
}

; GLSL: float16_t atan(half, float16_t)
define half @llpc.atan2.f16(half %y, half %x)
{
    ; yox = (|x| == |y|) ? ((x == y) ? 1.0 : -1.0) : y/x
    ;
    ; p0 = sgn(y) * PI/2
    ; p1 = sgn(y) * PI
    ; atanyox = atan(yox)
    ;
    ; if (x != 0.0)
    ;     atan(y, x) = (x < 0.0) ? p1 + atanyox : atanyox
    ; else
    ;     atan(y, x) = p0

    %1 = call half @llvm.fabs.f16(half %x)    ; %1 = |x|
    %2 = call half @llvm.fabs.f16(half %y)    ; %2 = |y|
    %3 = call half @llpc.fsign.f16(half %y)   ; %3 = syn(y)

    ; 0x3FF9200000000000: PI/2 = 1.57079637
    %4 = fmul half %3, 0x3FF9200000000000     ; %4 = p0 = syn(y) * PI/2
    ; 0x4009200000000000: PI = 3.14159274
    %5 = fmul half %3, 0x4009200000000000     ; %5 = p1 = syn(y) * PI

    %6 = fcmp oeq half %1, %2                 ; %6 = (|x| == |y|)
    %7 = fcmp oeq half %x, %y                 ; %7 = (x == y)
    %8 = select i1 %7, half 1.0, half -1.0    ; %8 = (x == y) ? 1.0 : -1.0

    ; NOTE: "atan" is very sensitive to the value of y_over_x, so we have to use high accuracy division.
    %9 = fdiv half %y, %x
                                              ; %9  = y/x
    %10 = select i1 %6, half %8, half %9      ; %10 = yox = (|x| == |y|) ? ((x == y) ? 1.0 : -1.0) : y/x
    %11 = call half @llpc.atan.f16(half %10)  ; %11 = atanyox = atan(yox)

    %12 = fadd half %11, %5                   ; %12 = atanyox + p1
    %13 = fcmp olt half %x, 0.0               ; %13 = (x < 0.0)
    %14 = select i1 %13, half %12, half %11   ; %14 = (x < 0.0) ? atanyox + p1 : atanyox

    %15 = fcmp one half %x, 0.0               ; %15 = (x != 0.0)
    %16 = select i1 %15, half %14, half %4    ; %16 = atan(y, x)

    ret half %16
}

; GLSL: float16_t sinh(float16_t)
define half @llpc.sinh.f16(half %x) #0
{
    ; (e^x - e^(-x)) / 2.0
    ; e^x = 2^(x * 1.442695)
    ; 0x3FF7140000000000: 1/log(2) = 1.442695
    ; e^x = 2^(x*(1/log(2))) = 2^(x*1.442695))
    %1 = fmul half %x, 0x3FF7140000000000
    %2 = fsub half 0.0, %1
    %3 = call half @llvm.exp2.f16(half %1)
    %4 = call half @llvm.exp2.f16(half %2)
    %5 = fsub half %3, %4
    %6 = fmul half %5, 0.5
    ret half %6
}

; GLSL: float16_t cosh(float16_t)
define half @llpc.cosh.f16(half %x) #0
{
    ; (e^x + e^(-x)) / 2.0
    ; e^x = 2^(x * 1.442695)
    ; 0x3FF7140000000000: 1/log(2) = 1.442695
    ; e^x = 2^(x*(1/log(2))) = 2^(x*1.442695))
    %1 = fmul half %x, 0x3FF7140000000000
    %2 = fsub half 0.0, %1
    %3 = call half @llvm.exp2.f16(half %1)
    %4 = call half @llvm.exp2.f16(half %2)
    %5 = fadd half %3, %4
    %6 = fmul half %5, 0.5
    ret half %6
}

; GLSL: float16_t tanh(float16_t)
define half @llpc.tanh.f16(half %x) #0
{
    ; sinh(x) / cosh(x)
    ; (e^x - e^(-x))/(e^x + e^(-x))
    ; 0x3FF7140000000000: 1/log(2) = 1.442695
    ; e^x = 2^(x*(1/log(2))) = 2^(x*1.442695))
    %1 = fmul half %x, 0x3FF7140000000000
    %2 = fsub half 0.0, %1
    %3 = call half @llvm.exp2.f16(half %1)
    %4 = call half @llvm.exp2.f16(half %2)
    %5 = fsub half %3, %4
    %6 = fadd half %3, %4
    %7 = fdiv half %5, %6
    ret half %7
}

; GLSL: float16_t asinh(float16_t)
define half @llpc.asinh.f16(half %x) #0
{
    ; ln(x + sqrt(x*x + 1))
    ;             / ln(x + sqrt(x^2 + 1))      when x >= 0
    ;  asinh(x) =
    ;             \ -ln((sqrt(x^2 + 1)- x))    when x < 0
    %1 = fmul half %x, %x
    %2 = fadd half %1, 1.0
    %3 = call half @llvm.sqrt.f16(half %2)
    %4 = fcmp oge half %x, 0.0
    %5 = select i1 %4, half 1.0, half -1.0
    %6 = fmul half %x, %5
    %7 = fadd half %3, %6
    ; In(x) = log2(x) * 0.6931471824646
    %8 = call half @llvm.log2.f16(half %7)
    ; 0x3FE62C0000000000: 0.6931471824646
    %9 = fmul half %8, 0x3FE62C0000000000
    %10 =fmul half %9, %5
    ret half %10
}

; GLSL: float16_t acosh(float16_t)
define half @llpc.acosh.f16(half %x) #0
{
    ; ln(x + sqrt(x*x - 1))
    ; x should >= 1, undefined < 1
    %1 = fmul half %x, %x
    %2 = fsub half %1, 1.0
    %3 = call half @llvm.sqrt.f16(half %2)
    %4 = fadd half %x, %3
    ; In(x) = log2(x) * 0.6931471824646
    %5 = call half @llvm.log2.f16(half %4)
    ; 0x3FE62C0000000000: 0.6931471824646
    %6 = fmul half %5, 0x3FE62C0000000000
    ret half %6
}

; GLSL: float16_t atanh(float16_t)
define half @llpc.atanh.f16(half %x) #0
{
    ; In((x + 1)/( 1 - x)) * 0.5f;
    ; "Ox"O <1, undefined "Ox"O >= 1
    %1 = fadd half %x, 1.0
    %2 = fsub half 1.0, %x
    %3 = fdiv half 1.0, %2
    %4 = fmul half %1, %3
    ; In(x) = log2(x)/(log2(e))
    ; 1.0f/log2(e) = 0.6931471824646
    %5 = call half @llvm.log2.f16(half %4)
    ; 0x3FD62C0000000000: 1.0f/(2*log2(e)) = 0.34657359
    %6 = fmul half %5, 0x3FD62C0000000000
    ret half %6
}

; =====================================================================================================================
; >>>  Exponential Functions
; =====================================================================================================================

; GLSL: float16_t pow(float16_t, float16_t)
define half @llpc.pow.f16(half %x, half %y) #0
{
    ; pow(x, y) = exp2(y * log2(x))
    %1 = call half @llvm.log2.f16(half %x)
    %2 = fmul half %y, %1
    %3 = call half @llvm.exp2.f16(half %2)
    ret half %3
}

; GLSL: float16_t exp(float16_t)
define half @llpc.exp.f16(half %x) #0
{
    ; 0x3FF7140000000000: 1.442695
    %1 = fmul half %x, 0x3FF7140000000000
    %2 = call half @llvm.exp2.f16(half %1)
    ret half %2
}

; GLSL: float16_t log(float16_t)
define half @llpc.log.f16(half %x) #0
{
    ; 0x3FE62C0000000000: 0.6931471824646
    %1 = call half @llvm.log2.f16(half %x)
    %2 = fmul half %1, 0x3FE62C0000000000
    ret half %2
}

; GLSL: float16_t inversesqrt(float16_t)
define half @llpc.inverseSqrt.f16(half %x) #0
{
    %1 = call half @llvm.sqrt.f16(half %x)
    %2 = fdiv half 1.0, %1
    ret half %2
}

; =====================================================================================================================
; >>>  Common Functions
; =====================================================================================================================

; GLSL: float16_t abs(float16_t)
define half @llpc.fsign.f16(half %x) #0
{
    %con1 = fcmp ogt half %x, 0.0
    %ret1 = select i1 %con1, half 1.0, half %x
    %con2 = fcmp oge half %ret1, 0.0
    %ret2 = select i1 %con2, half %ret1,half -1.0
    ret half %ret2
}

; GLSL: float16_t round(float16_t)
define half @llpc.round.f16(half %x)
{
    %1 = call half @llvm.rint.f16(half %x)
    ret half %1
}

; GLSL: float16_t fract(float16_t)
define half @llpc.fract.f16(half %x) #0
{
    %1 = call half @llvm.amdgcn.fract.f16(half %x)
    ret half %1
}

; GLSL: float16_t mod(float16_t, float16_t)
define half @llpc.mod.f16(half %x, half %y) #0
{
    %1 = fdiv half 1.0,%y
    %2 = fmul half %x, %1
    %3 = call half @llvm.floor.f16(half %2)
    %4 = fmul half %y, %3
    %5 = fsub half %x, %4
    ret half %5
}

; GLSL: float16_t modf(float16_t, out float16_t)
define spir_func half @_Z4modfDhPDh(
    half %x, half* %i) #0
{
    %1 = call half @llvm.trunc.f16(half %x)
    %2 = fsub half %x, %1

    store half %1, half* %i
    ret half %2
}

; GLSL: f16vec2 modf(f16vec2, out f16vec2)
define spir_func <2 x half> @_Z4modfDv2_DhPDv2_Dh(
    <2 x half> %x, <2 x half>* %i) #0
{
    %x0 = extractelement <2 x half> %x, i32 0
    %x1 = extractelement <2 x half> %x, i32 1

    %1 = call half @llvm.trunc.f16(half %x0)
    %2 = fsub half %x0, %1

    %3 = call half @llvm.trunc.f16(half %x1)
    %4 = fsub half %x1, %3

    %5 = insertelement <2 x half> undef, half %1, i32 0
    %6 = insertelement <2 x half> %5, half %3, i32 1

    %7 = insertelement <2 x half> undef, half %2, i32 0
    %8 = insertelement <2 x half> %7, half %4, i32 1

    store <2 x half> %6, <2 x half>* %i
    ret <2 x half> %8
}

; GLSL: f16vec3 modf(f16vec3, out f16vec3)
define spir_func <3 x half> @_Z4modfDv3_DhPDv3_Dh(
    <3 x half> %x, <3 x half>* %i) #0
{
    %x0 = extractelement <3 x half> %x, i32 0
    %x1 = extractelement <3 x half> %x, i32 1
    %x2 = extractelement <3 x half> %x, i32 2

    %1 = call half @llvm.trunc.f16(half %x0)
    %2 = fsub half %x0, %1

    %3 = call half @llvm.trunc.f16(half %x1)
    %4 = fsub half %x1, %3

    %5 = call half @llvm.trunc.f16(half %x2)
    %6 = fsub half %x2, %5

    %7 = insertelement <3 x half> undef, half %1, i32 0
    %8 = insertelement <3 x half> %7, half %3, i32 1
    %9 = insertelement <3 x half> %8, half %5, i32 2

    %10 = insertelement <3 x half> undef, half %2, i32 0
    %11 = insertelement <3 x half> %10, half %4, i32 1
    %12 = insertelement <3 x half> %11, half %6, i32 2

    store <3 x half> %9, <3 x half>* %i
    ret <3 x half> %12
}

; GLSL: f16vec4 modf(f16vec4, out f16vec4)
define spir_func <4 x half> @_Z4modfDv4_DhPDv4_Dh(
    <4 x half> %x, <4 x half>* %i) #0
{
    %x0 = extractelement <4 x half> %x, i32 0
    %x1 = extractelement <4 x half> %x, i32 1
    %x2 = extractelement <4 x half> %x, i32 2
    %x3 = extractelement <4 x half> %x, i32 3

    %1 = call half @llvm.trunc.f16(half %x0)
    %2 = fsub half %x0, %1

    %3 = call half @llvm.trunc.f16(half %x1)
    %4 = fsub half %x1, %3

    %5 = call half @llvm.trunc.f16(half %x2)
    %6 = fsub half %x2, %5

    %7 = call half @llvm.trunc.f16(half %x3)
    %8 = fsub half %x3, %7

    %9 = insertelement <4 x half> undef, half %1, i32 0
    %10 = insertelement <4 x half> %9, half %3, i32 1
    %11 = insertelement <4 x half> %10, half %5, i32 2
    %12 = insertelement <4 x half> %11, half %7, i32 3

    %13 = insertelement <4 x half> undef, half %2, i32 0
    %14 = insertelement <4 x half> %13, half %4, i32 1
    %15 = insertelement <4 x half> %14, half %6, i32 2
    %16 = insertelement <4 x half> %15, half %8, i32 3

    store <4 x half> %12, <4 x half>* %i
    ret <4 x half> %16
}

; GLSL: float16_t mix(float16_t, float16_t, float16_t)
define spir_func half @_Z4fmixDhDhDh(
    half %x, half %y, half %a) #0
{
    %1 = fsub half %y, %x
    %2 = tail call half @llvm.fmuladd.f16(half %1, half %a, half %x)

    ret half %2
}

; GLSL: f16vec2 mix(f16vec2, f16vec2, f16vec2)
define spir_func <2 x half> @_Z4fmixDv2_DhDv2_DhDv2_Dh(
    <2 x half> %x, <2 x half> %y, <2 x half> %a) #0
{
    %1 = fsub <2 x half> %y, %x
    %2 = tail call <2 x half> @llvm.fmuladd.v2f16(<2 x half> %1, <2 x half> %a, <2 x half> %x)

    ret <2 x half> %2
}

; GLSL: f16vec3 mix(f16vec3, f16vec3, f16vec3)
define spir_func <3 x half> @_Z4fmixDv3_DhDv3_DhDv3_Dh(
    <3 x half> %x, <3 x half> %y, <3 x half> %a) #0
{
    %1 = fsub <3 x half> %y, %x
    %2 = tail call <3 x half> @llvm.fmuladd.v3f16(<3 x half> %1, <3 x half> %a, <3 x half> %x)

    ret <3 x half> %2
}

; GLSL: f16vec4 mix(f16vec4, f16vec4, f16vec4)
define spir_func <4 x half> @_Z4fmixDv4_DhDv4_DhDv4_Dh(
    <4 x half> %x, <4 x half> %y, <4 x half> %a) #0
{
    %1 = fsub <4 x half> %y, %x
    %2 = tail call <4 x half> @llvm.fmuladd.v4f16(<4 x half> %1, <4 x half> %a, <4 x half> %x)

    ret <4 x half> %2
}

; GLSL: float16_t min(float16_t, float16_t)
define spir_func half @_Z4fminDhDh(
    half %x, half %y) #0
{
    %1 = call half @llvm.minnum.f16(half %x, half %y)

    ret half %1
}

; GLSL: f16vec2 min(f16vec2, f16vec2)
define spir_func <2 x half> @_Z4fminDv2_DhDv2_Dh(
    <2 x half> %x, <2 x half> %y) #0
{
    %1 = call <2 x half> @llvm.minnum.v2f16(<2 x half> %x, <2 x half> %y)

    ret <2 x half> %1
}

; GLSL: f16vec3 min(f16vec3, f16vec3)
define spir_func <3 x half> @_Z4fminDv3_DhDv3_Dh(
    <3 x half> %x, <3 x half> %y) #0
{
    %1 = call <3 x half> @llvm.minnum.v3f16(<3 x half> %x, <3 x half> %y)

    ret <3 x half> %1
}

; GLSL: f16vec4 min(f16vec4, f16vec4)
define spir_func <4 x half> @_Z4fminDv4_DhDv4_Dh(
    <4 x half> %x, <4 x half> %y) #0
{
    %1 = call <4 x half> @llvm.minnum.v4f16(<4 x half> %x, <4 x half> %y)

    ret <4 x half> %1
}

; GLSL: float16_t max(float16_t, float16_t)
define spir_func half @_Z4fmaxDhDh(
    half %x, half %y) #0
{
    %1 = call half @llvm.maxnum.f16(half %x, half %y)

    ret half %1
}

; GLSL: f16vec2 max(f16vec2, f16vec2)
define spir_func <2 x half> @_Z4fmaxDv2_DhDv2_Dh(
    <2 x half> %x, <2 x half> %y) #0
{
    %1 = call <2 x half> @llvm.maxnum.v2f16(<2 x half> %x, <2 x half> %y)

    ret <2 x half> %1
}

; GLSL: f16vec3 max(f16vec3, f16vec3)
define spir_func <3 x half> @_Z4fmaxDv3_DhDv3_Dh(
    <3 x half> %x, <3 x half> %y) #0
{
    %1 = call <3 x half> @llvm.maxnum.v3f16(<3 x half> %x, <3 x half> %y)

    ret <3 x half> %1
}

; GLSL: f16vec4 max(f16vec4, f16vec4)
define spir_func <4 x half> @_Z4fmaxDv4_DhDv4_Dh(
    <4 x half> %x, <4 x half> %y) #0
{
    %1 = call <4 x half> @llvm.maxnum.v4f16(<4 x half> %x, <4 x half> %y)
    ret <4 x half> %1
}

; GLSL: float16_t clamp(float16_t, float16_t ,float16_t)
define half @_Z6fclampDhDhDh(half %x, half %minVal, half %maxVal) #0
{
    %1 = call half @llvm.maxnum.f16(half %x, half %minVal)
    %2 = call half @llvm.minnum.f16(half %1, half %maxVal)

    ret half %2
}

; GLSL: f16vec2 clamp(f16vec2, f16vec2 ,f16vec2)
define spir_func <2 x half> @_Z6fclampDv2_DhDv2_DhDv2_Dh(
    <2 x half> %x, <2 x half> %minVal, <2 x half> %maxVal) #0
{
    %1 = call <2 x half> @llvm.maxnum.v2f16(<2 x half> %x, <2 x half> %minVal)
    %2 = call <2 x half> @llvm.minnum.v2f16(<2 x half> %1, <2 x half> %maxVal)

    ret <2 x half> %2
}

; GLSL: f16vec3 clamp(f16vec3, f16vec3 ,f16vec3)
define spir_func <3 x half> @_Z6fclampDv3_DhDv3_DhDv3_Dh(
    <3 x half> %x, <3 x half> %minVal, <3 x half> %maxVal) #0
{
    %1 = call <3 x half> @llvm.maxnum.v3f16(<3 x half> %x, <3 x half> %minVal)
    %2 = call <3 x half> @llvm.minnum.v3f16(<3 x half> %1, <3 x half> %maxVal)

    ret <3 x half> %2
}

; GLSL: f16vec4 clamp(f16vec4, f16vec4 ,f16vec4)
define spir_func <4 x half> @_Z6fclampDv4_DhDv4_DhDv4_Dh(
    <4 x half> %x, <4 x half> %minVal, <4 x half> %maxVal) #0
{
    %1 = call <4 x half> @llvm.maxnum.v4f16(<4 x half> %x, <4 x half> %minVal)
    %2 = call <4 x half> @llvm.minnum.v4f16(<4 x half> %1, <4 x half> %maxVal)

    ret <4 x half> %2
}

; GLSL: float16_t step(float16_t, float16_t)
define half @llpc.step.f16(half %edge, half %x) #0
{
    %1 = fcmp olt half %x, %edge
    %2 = select i1 %1, half 0.0, half 1.0
    ret half %2
}

; GLSL: float16_t smoothstep(float16_t, float16_t, float16_t)
define half @llpc.smoothStep.f16(half %edge0, half %edge1, half %x) #0
{
    %1 = fsub half %x, %edge0
    %2 = fsub half %edge1, %edge0
    %3 = fdiv half 1.0, %2
    %4 = fmul half %1, %3
    %5 = call half @llvm.amdgcn.fmed3.f16(half %4, half 0.0, half 1.0)
    %6 = fmul half %5, %5
    %7 = fmul half -2.0, %5
    %8 = fadd half 3.0, %7
    %9 = fmul half %6, %8
    ret half %9
}

; GLSL: bool isinf(float16_t)
define i1 @llpc.isinf.f16(half %x) #0
{
    ; 0x004: negative infinity; 0x200: positive infinity
    %1 = call i1 @llvm.amdgcn.class.f16(half %x, i32 516)
    ret i1 %1
}

; GLSL: bool isnan(float16_t)
define i1 @llpc.isnan.f16(half %x) #0
{
    ; 0x001: signaling NaN, 0x002: quiet NaN
    %1 = call i1 @llvm.amdgcn.class.f16(half %x, i32 3)
    ret i1 %1
}

; GLSL: float16_t fma(float16_t, float16_t, float16_t)
define spir_func half @_Z3fmaDhDhDh(
    half %a, half %b, half %c) #0
{
    %1 = tail call half @llvm.fmuladd.f16(half %a, half %b, half %c)

    ret half %1
}

; GLSL: f16vec2 fma(f16vec2, f16vec2, f16vec2)
define spir_func <2 x half> @_Z3fmaDv2_DhDv2_DhDv2_Dh(
    <2 x half> %a, <2 x half> %b, <2 x half> %c) #0
{
    %1 = tail call <2 x half> @llvm.fmuladd.v2f16(<2 x half> %a, <2 x half> %b, <2 x half> %c)

    ret <2 x half>  %1
}

; GLSL: f16vec3 fma(f16vec3, f16vec3, f16vec3)
define spir_func <3 x half> @_Z3fmaDv3_DhDv3_DhDv3_Dh(
    <3 x half> %a, <3 x half> %b, <3 x half> %c) #0
{
    %1 = tail call <3 x half> @llvm.fmuladd.v3f16(<3 x half> %a, <3 x half> %b, <3 x half> %c)

    ret <3 x half>  %1
}

; GLSL: f16vec4 fma(f16vec4, f16vec4, f16vec4)
define spir_func <4 x half> @_Z3fmaDv4_DhDv4_DhDv4_Dh(
    <4 x half> %a, <4 x half> %b, <4 x half> %c) #0
{
    %1 = tail call <4 x half> @llvm.fmuladd.v4f16(<4 x half> %a, <4 x half> %b, <4 x half> %c)

    ret <4 x half> %1
}

; =====================================================================================================================
; >>>  Geometric Functions
; =====================================================================================================================
; GLSL: float16_t length(float16_t)
define spir_func half @_Z6lengthDh(half %x) #0
{
    %end = call half @llvm.fabs.f16(half %x)
    ret half %end
}

; GLSL: float16_t length(f16vec2)
define spir_func half @_Z6lengthDv2_Dh(<2 x half> %x) #0
{
    %x.x = extractelement <2 x half> %x, i32 0
    %x.y = extractelement <2 x half> %x, i32 1

    %v0 = fmul half %x.x, %x.x
    %v1 = fmul half %x.y, %x.y
    %sqr = fadd half %v0, %v1
    %end = call half @llvm.sqrt.f16(half %sqr)

    ret half %end
}

; GLSL: float16_t length(f16vec3)
define spir_func half @_Z6lengthDv3_Dh(<3 x half> %x) #0
{
    %x.x = extractelement <3 x half> %x, i32 0
    %x.y = extractelement <3 x half> %x, i32 1
    %x.z = extractelement <3 x half> %x, i32 2

    %v0 = fmul half %x.x, %x.x
    %v1 = fmul half %x.y, %x.y
    %vl = fadd half %v0, %v1
    %v2 = fmul half %x.z, %x.z
    %sqr = fadd half %vl, %v2
    %end = call half @llvm.sqrt.f16(half %sqr)

    ret half %end
}

; GLSL: float16_t length(f16vec4)
define spir_func half @_Z6lengthDv4_Dh(<4 x half> %x) #0
{
    %x.x = extractelement <4 x half> %x, i32 0
    %x.y = extractelement <4 x half> %x, i32 1
    %x.z = extractelement <4 x half> %x, i32 2
    %x.w = extractelement <4 x half> %x, i32 3

    %v0 = fmul half %x.x, %x.x
    %v1 = fmul half %x.y, %x.y
    %vl = fadd half %v0, %v1
    %v2 = fmul half %x.z, %x.z
    %v3 = fmul half %x.w, %x.w
    %vm = fadd half %v2, %v3
    %sqr = fadd half %vl, %vm
    %end = call half @llvm.sqrt.f16(half %sqr)

    ret half %end
}

; GLSL: float16_t distance(float16_t, float16_t)
define spir_func half @_Z8distanceDhDh(half %p0, half %p1) #0
{
    %subtra = fsub half %p0 ,%p1
    %1 = call half @llvm.fabs.f16(half %subtra)
    ret half %1
}

; ; GLSL: float16_t distance(f16vec2, f16vec2)
define spir_func half @_Z8distanceDv2_DhDv2_Dh(<2 x half> %p0, <2 x half> %p1) #0
{
    %subtra = fsub <2 x half> %p0 ,%p1
    %1 = call half @_Z6lengthDv2_Dh(<2 x half> %subtra)
    ret half %1
}

; GLSL: float16_t distance(f16vec3, f16vec3)
define spir_func half @_Z8distanceDv3_DhDv3_Dh(<3 x half> %p0, <3 x half> %p1) #0
{
    %subtra = fsub <3 x half> %p0 ,%p1
    %1 = call half @_Z6lengthDv3_Dh(<3 x half> %subtra)
    ret half %1
}

; GLSL: float16_t distance(f16vec4, f16vec4)
define spir_func half @_Z8distanceDv4_DhDv4_Dh(<4 x half> %p0, <4 x half> %p1) #0
{
    %subtra = fsub <4 x half> %p0 ,%p1
    %1 = call half @_Z6lengthDv4_Dh(<4 x half> %subtra)
    ret half %1
}

; GLSL: float16_t dot(float16_t, float16_t)
define spir_func half @_Z3dotDhDh(half %x, half %y) #0
{
    %1 = fmul half %x, %y
    ret half %1
}

; GLSL half dot(f16vec2, f16vec2)
define spir_func half @_Z3dotDv2_DhDv2_Dh(<2 x half> %x, <2 x half> %y) #0
{
    %x.x = extractelement <2 x half> %x, i32 0
    %x.y = extractelement <2 x half> %x, i32 1

    %y.x = extractelement <2 x half> %y, i32 0
    %y.y = extractelement <2 x half> %y, i32 1

    %v0 = fmul half %x.x, %y.x
    %v1 = fmul half %x.y, %y.y
    %end = fadd half %v0, %v1

    ret half %end
}

; GLSL half dot(f16vec3, f16vec3)
define spir_func half @_Z3dotDv3_DhDv3_Dh(<3 x half> %x, <3 x half> %y) #0
{
    %x.x = extractelement <3 x half> %x, i32 0
    %x.y = extractelement <3 x half> %x, i32 1
    %x.z = extractelement <3 x half> %x, i32 2

    %y.x = extractelement <3 x half> %y, i32 0
    %y.y = extractelement <3 x half> %y, i32 1
    %y.z = extractelement <3 x half> %y, i32 2

    %v0 = fmul half %x.x, %y.x
    %v1 = fmul half %x.y, %y.y
    %vl = fadd half %v0, %v1
    %v2 = fmul half %x.z, %y.z
    %end = fadd half %v2, %vl

    ret half %end
}

; GLSL half dot(f16vec4, f16vec4)
define spir_func half @_Z3dotDv4_DhDv4_Dh(<4 x half> %x, <4 x half> %y) #0
{
    %x.x = extractelement <4 x half> %x, i32 0
    %x.y = extractelement <4 x half> %x, i32 1
    %x.z = extractelement <4 x half> %x, i32 2
    %x.w = extractelement <4 x half> %x, i32 3

    %y.x = extractelement <4 x half> %y, i32 0
    %y.y = extractelement <4 x half> %y, i32 1
    %y.z = extractelement <4 x half> %y, i32 2
    %y.w = extractelement <4 x half> %y, i32 3

    %v0 = fmul half %x.x, %y.x
    %v1 = fmul half %x.y, %y.y
    %vl = fadd half %v0, %v1
    %v2 = fmul half %x.z, %y.z
    %v3 = fmul half %x.w, %y.w
    %vm = fadd half %v2, %v3
    %end = fadd half %vl,%vm

    ret half %end
}

; GLSL: f16vec3 cross(f16vec3, f16vec3)
define spir_func <3 x half> @_Z5crossDv3_DhDv3_Dh(<3 x half> %x, <3 x half> %y) #0
{
    %x.x = extractelement <3 x half> %x, i32 0
    %x.y = extractelement <3 x half> %x, i32 1
    %x.z = extractelement <3 x half> %x, i32 2

    %y.x = extractelement <3 x half> %y, i32 0
    %y.y = extractelement <3 x half> %y, i32 1
    %y.z = extractelement <3 x half> %y, i32 2

    %l0 = fmul half %x.y, %y.z
    %l1 = fmul half %x.z, %y.x
    %l2 = fmul half %x.x, %y.y

    %r0 = fmul half %y.y, %x.z
    %r1 = fmul half %y.z, %x.x
    %r2 = fmul half %y.x, %x.y

    %1 = fsub half %l0, %r0
    %2 = fsub half %l1, %r1
    %3 = fsub half %l2, %r2

    %4 = alloca <3 x half>
    %5 = load <3 x half>, <3 x half>* %4
    %6 = insertelement <3 x half> %5, half %1, i32 0
    %7 = insertelement <3 x half> %6, half %2, i32 1
    %8 = insertelement <3 x half> %7, half %3, i32 2

    ret <3 x half> %8
}

; GLSL: float16_t normalize(float16_t)
define spir_func half @_Z9normalizeDh(half %x) #0
{
    %1 = fcmp ogt half %x, 0.0
    %2 = select i1 %1, half 1.0, half -1.0
    ret half %2
}

; GLSL: f16vec2 normalize(f16vec2)
define spir_func <2 x half> @_Z9normalizeDv2_Dh(<2 x half> %x) #0
{
    %length = call half @_Z6lengthDv2_Dh(<2 x half> %x)
    %rsq = fdiv half 1.0, %length

    %x.x = extractelement <2 x half> %x, i32 0
    %x.y = extractelement <2 x half> %x, i32 1

    %1 = fmul half %x.x, %rsq
    %2 = fmul half %x.y, %rsq

    %3 = alloca <2 x half>
    %4 = load <2 x half>, <2 x half>* %3
    %5 = insertelement <2 x half> %4, half %1, i32 0
    %6 = insertelement <2 x half> %5, half %2, i32 1

    ret <2 x half> %6
}

; GLSL: f16vec3 normalize(f16vec3)
define spir_func <3 x half> @_Z9normalizeDv3_Dh(<3 x half> %x) #0
{
    %length = call half @_Z6lengthDv3_Dh(<3 x half> %x)
    %rsq = fdiv half 1.0, %length

    %x.x = extractelement <3 x half> %x, i32 0
    %x.y = extractelement <3 x half> %x, i32 1
    %x.z = extractelement <3 x half> %x, i32 2

    %1 = fmul half %x.x, %rsq
    %2 = fmul half %x.y, %rsq
    %3 = fmul half %x.z, %rsq

    %4 = alloca <3 x half>
    %5 = load <3 x half>, <3 x half>* %4
    %6 = insertelement <3 x half> %5, half %1, i32 0
    %7 = insertelement <3 x half> %6, half %2, i32 1
    %8 = insertelement <3 x half> %7, half %3, i32 2

    ret <3 x half> %8
}

; GLSL: f16vec4 normalize(f16vec4)
define spir_func <4 x half> @_Z9normalizeDv4_Dh(<4 x half> %x) #0
{
    %length = call half @_Z6lengthDv4_Dh(<4 x half> %x)
    %rsq = fdiv half 1.0, %length

    %x.x = extractelement <4 x half> %x, i32 0
    %x.y = extractelement <4 x half> %x, i32 1
    %x.z = extractelement <4 x half> %x, i32 2
    %x.w = extractelement <4 x half> %x, i32 3

    %1 = fmul half %x.x, %rsq
    %2 = fmul half %x.y, %rsq
    %3 = fmul half %x.z, %rsq
    %4 = fmul half %x.w, %rsq

    %5 = alloca <4 x half>
    %6 = load <4 x half>, <4 x half>* %5
    %7 = insertelement <4 x half> %6, half %1, i32 0
    %8 = insertelement <4 x half> %7, half %2, i32 1
    %9 = insertelement <4 x half> %8, half %3, i32 2
    %10 = insertelement <4 x half> %9, half %4, i32 3

    ret <4 x half> %10
}

; GLSL: float16_t faceforward(float16_t, float16_t, float16_t)
define spir_func half @_Z11faceForwardDhDhDh( half %N, half %I,  half %Nref) #0
{
    %dotv = fmul half %I, %Nref
    ; Compare if dot < 0.0
    %con = fcmp olt half %dotv, 0.0

    %NN = fsub half 0.0, %N

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,half %N, half %NN

    ret half %1
}

; GLSL: f16vec2 faceforward(f16vec2, f16vec2, f16vec2)
define spir_func <2 x half> @_Z11faceForwardDv2_DhDv2_DhDv2_Dh(<2 x half> %N, <2 x half> %I, <2 x half> %Nref) #0
{
    %dotv = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %I, <2 x half> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt half %dotv, 0.0

    %N.x = extractelement <2 x half> %N, i32 0
    %N.y = extractelement <2 x half> %N, i32 1

    %NN.x = fsub half 0.0, %N.x
    %NN.y = fsub half 0.0, %N.y

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,half %N.x, half %NN.x
    %2 = select i1 %con ,half %N.y, half %NN.y

    %3 = alloca <2 x half>
    %4 = load <2 x half>, <2 x half>* %3
    %5 = insertelement <2 x half> %4, half %1, i32 0
    %6 = insertelement <2 x half> %5, half %2, i32 1

    ret <2 x half> %6
}

; GLSL: f16vec3 faceforward(f16vec3, f16vec3, f16vec3)
define spir_func <3 x half> @_Z11faceForwardDv3_DhDv3_DhDv3_Dh(<3 x half> %N, <3 x half> %I, <3 x half> %Nref) #0
{
    %dotv = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %I, <3 x half> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt half %dotv, 0.0

    %N.x = extractelement <3 x half> %N, i32 0
    %N.y = extractelement <3 x half> %N, i32 1
    %N.z = extractelement <3 x half> %N, i32 2

    %NN.x = fsub half 0.0, %N.x
    %NN.y = fsub half 0.0, %N.y
    %NN.z = fsub half 0.0, %N.z

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,half %N.x, half %NN.x
    %2 = select i1 %con ,half %N.y, half %NN.y
    %3 = select i1 %con ,half %N.z, half %NN.z

    %4 = alloca <3 x half>
    %5 = load <3 x half>, <3 x half>* %4
    %6 = insertelement <3 x half> %5, half %1, i32 0
    %7 = insertelement <3 x half> %6, half %2, i32 1
    %8 = insertelement <3 x half> %7, half %3, i32 2

    ret <3 x half> %8
}

; GLSL: f16vec4 faceforward(f16vec4, f16vec4, f16vec4)
define spir_func <4 x half> @_Z11faceForwardDv4_DhDv4_DhDv4_Dh(<4 x half> %N, <4 x half> %I, <4 x half> %Nref) #0
{
    %dotv = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %I, <4 x half> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt half %dotv, 0.0

    %N.x = extractelement <4 x half> %N, i32 0
    %N.y = extractelement <4 x half> %N, i32 1
    %N.z = extractelement <4 x half> %N, i32 2
    %N.w = extractelement <4 x half> %N, i32 3

    %NN.x = fsub half 0.0, %N.x
    %NN.y = fsub half 0.0, %N.y
    %NN.z = fsub half 0.0, %N.z
    %NN.w = fsub half 0.0, %N.w

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,half %N.x,  half %NN.x
    %2 = select i1 %con ,half %N.y,  half %NN.y
    %3 = select i1 %con ,half %N.z,  half %NN.z
    %4 = select i1 %con ,half %N.w,  half %NN.w

    %5 = alloca <4 x half>
    %6 = load <4 x half>, <4 x half>* %5
    %7 = insertelement <4 x half> %6, half %1, i32 0
    %8 = insertelement <4 x half> %7, half %2, i32 1
    %9 = insertelement <4 x half> %8, half %3, i32 2
    %10 = insertelement <4 x half> %9, half %4, i32 3

    ret <4 x half> %10
}

; GLSL: float16_t reflect(float16_t, float16_t)
define spir_func half @_Z7reflectDhDh(half %I, half %N) #0
{
    %dotin = fmul half %I, %N
    %dot = fmul half %dotin, 2.0

    ; 2 * dot(N, I) * N
    %right = fmul half %dot, %N
    %end = fsub half %I, %right

    ret half %end
}

; GLSL: f16vec2 reflect(f16vec2, f16vec2)
define spir_func <2 x half> @_Z7reflectDv2_DhDv2_Dh(<2 x half> %I, <2 x half> %N) #0
{
    %dotin = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %I, <2 x half> %N)
    %dot = fmul half %dotin, 2.0

    %1 = alloca <2 x half>
    %2 = load <2 x half>, <2 x half>* %1
    %3 = insertelement <2 x half> %2, half %dot, i32 0
    %dotv = insertelement <2 x half> %3, half %dot, i32 1

    ; 2 * dot(N, I) * N
    %right = fmul <2 x half> %dotv, %N
    %end = fsub <2 x half> %I, %right

    ret <2 x half> %end
}

; GLSL: f16vec3 reflect(f16vec3, f16vec3)
define spir_func <3 x half> @_Z7reflectDv3_DhDv3_Dh(<3 x half> %I, <3 x half> %N) #0
{
    %dotin = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %I, <3 x half> %N)
    %dot = fmul half %dotin, 2.0

    %1 = alloca <3 x half>
    %2 = load <3 x half>, <3 x half>* %1
    %3 = insertelement <3 x half> %2, half %dot, i32 0
    %4 = insertelement <3 x half> %3, half %dot, i32 1
    %dotv = insertelement <3 x half> %4, half %dot, i32 2

    ; 2 * dot(N, I) * N
    %right = fmul <3 x half> %dotv, %N
    %end = fsub <3 x half> %I, %right

    ret <3 x half> %end
}

; GLSL: f16vec4 reflect(f16vec4, f16vec4)
define spir_func <4 x half> @_Z7reflectDv4_DhDv4_Dh(<4 x half> %I, <4 x half> %N) #0
{
    %dotin = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %I, <4 x half> %N)
    %dot = fmul half %dotin, 2.0

    %1 = alloca <4 x half>
    %2 = load <4 x half>, <4 x half>* %1
    %3 = insertelement <4 x half> %2, half %dot, i32 0
    %4 = insertelement <4 x half> %3, half %dot, i32 1
    %5 = insertelement <4 x half> %4, half %dot, i32 2
    %dotv = insertelement <4 x half> %5, half %dot, i32 3

    ; 2 * dot(N, I) * N
    %right = fmul <4 x half> %dotv, %N
    %end = fsub <4 x half> %I, %right

    ret <4 x half> %end
}

; GLSL: float16_t refract(float16_t, float16_t, float16_t)
define spir_func half @_Z7refractDhDhDh(half %I, half %N, half %eta) #0
{
    %dotin = fmul half %I, %N
    %dotinsqr = fmul half %dotin, %dotin
    %e1 = fsub half 1.0, %dotinsqr
    %e2 = fmul half %eta, %eta
    %e3 = fmul half %e1, %e2
    %k = fsub half 1.0, %e3
    %ksqrt = call half @llvm.sqrt.f16(half %k)
    %etadot = fmul half %eta, %dotin
    %innt = fadd half %etadot, %ksqrt

    %N0 = fmul half %innt, %N
    %I0 = fmul half %I, %eta
    %S = fsub half %I0, %N0
    ; Compare k < 0
    %con = fcmp olt half %k, 0.0
    %1 = select i1 %con, half 0.0, half %S

    ret half %1
}

; GLSL: f16vec2 refract(f16vec2, f16vec2, float16_t)
define spir_func <2 x half> @_Z7refractDv2_DhDv2_DhDh(<2 x half> %I, <2 x half> %N, half %eta) #0
{
    %dotin = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %I, <2 x half> %N)
    %dotinsqr = fmul half %dotin, %dotin
    %e1 = fsub half 1.0, %dotinsqr
    %e2 = fmul half %eta, %eta
    %e3 = fmul half %e1, %e2
    %k = fsub half 1.0, %e3
    %ksqrt = call half @llvm.sqrt.f16(half %k)
    %etadot = fmul half %eta, %dotin
    %innt = fadd half %etadot, %ksqrt

    %I.x = extractelement <2 x half> %I, i32 0
    %I.y = extractelement <2 x half> %I, i32 1

    %N.x = extractelement <2 x half> %N, i32 0
    %N.y = extractelement <2 x half> %N, i32 1

    %I0 = fmul half %I.x, %eta
    %I1 = fmul half %I.y, %eta

    %N0 = fmul half %N.x, %innt
    %N1 = fmul half %N.y, %innt

    %S0 = fsub half %I0, %N0
    %S1 = fsub half %I1, %N1

    ; Compare k < 0
    %con = fcmp olt half %k, 0.0

    %1 = select i1 %con, half 0.0, half %S0
    %2 = select i1 %con, half 0.0, half %S1

    %3 = alloca <2 x half>
    %4 = load <2 x half>, <2 x half>* %3
    %5 = insertelement <2 x half> %4, half %1, i32 0
    %6 = insertelement <2 x half> %5, half %2, i32 1

    ret <2 x half> %6
}

; GLSL: f16vec3 refract(f16vec3, f16vec3, float16_t)
define spir_func <3 x half> @_Z7refractDv3_DhDv3_DhDh(<3 x half> %I, <3 x half> %N, half %eta) #0
{
    %dotin = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %I, <3 x half> %N)
    %dotinsqr = fmul half %dotin, %dotin
    %e1 = fsub half 1.0, %dotinsqr
    %e2 = fmul half %eta, %eta
    %e3 = fmul half %e1, %e2
    %k = fsub half 1.0, %e3
    %ksqrt = call half @llvm.sqrt.f16(half %k)
    %etadot = fmul half %eta, %dotin
    %innt = fadd half %etadot, %ksqrt

    %I.x = extractelement <3 x half> %I, i32 0
    %I.y = extractelement <3 x half> %I, i32 1
    %I.z = extractelement <3 x half> %I, i32 2

    %N.x = extractelement <3 x half> %N, i32 0
    %N.y = extractelement <3 x half> %N, i32 1
    %N.z = extractelement <3 x half> %N, i32 2

    %I0 = fmul half %I.x, %eta
    %I1 = fmul half %I.y, %eta
    %I2 = fmul half %I.z, %eta

    %N0 = fmul half %N.x, %innt
    %N1 = fmul half %N.y, %innt
    %N2 = fmul half %N.z, %innt

    %S0 = fsub half %I0, %N0
    %S1 = fsub half %I1, %N1
    %S2 = fsub half %I2, %N2

    ; Compare k < 0
    %con = fcmp olt half %k, 0.0

    %1 = select i1 %con, half 0.0, half %S0
    %2 = select i1 %con, half 0.0, half %S1
    %3 = select i1 %con, half 0.0, half %S2

    %4 = alloca <3 x half>
    %5 = load <3 x half>, <3 x half>* %4
    %6 = insertelement <3 x half> %5, half %1, i32 0
    %7 = insertelement <3 x half> %6, half %2, i32 1
    %8 = insertelement <3 x half> %7, half %3, i32 2

    ret <3 x half> %8
}

; GLSL: f16vec4 refract(f16vec4, f16vec4, float16_t)
define spir_func <4 x half> @_Z7refractDv4_DhDv4_DhDh(<4 x half> %I, <4 x half> %N, half %eta) #0
{
    %dotin = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %I, <4 x half> %N)
    %dotinsqr = fmul half %dotin, %dotin
    %e1 = fsub half 1.0, %dotinsqr
    %e2 = fmul half %eta, %eta
    %e3 = fmul half %e1, %e2
    %k = fsub half 1.0, %e3
    %ksqrt = call half @llvm.sqrt.f16(half %k)
    %etadot = fmul half %eta, %dotin
    %innt = fadd half %etadot, %ksqrt

    %I.x = extractelement <4 x half> %I, i32 0
    %I.y = extractelement <4 x half> %I, i32 1
    %I.z = extractelement <4 x half> %I, i32 2
    %I.w = extractelement <4 x half> %I, i32 3

    %N.x = extractelement <4 x half> %N, i32 0
    %N.y = extractelement <4 x half> %N, i32 1
    %N.z = extractelement <4 x half> %N, i32 2
    %N.w = extractelement <4 x half> %N, i32 3

    %I0 = fmul half %I.x, %eta
    %I1 = fmul half %I.y, %eta
    %I2 = fmul half %I.z, %eta
    %I3 = fmul half %I.w, %eta

    %N0 = fmul half %N.x, %innt
    %N1 = fmul half %N.y, %innt
    %N2 = fmul half %N.z, %innt
    %N3 = fmul half %N.w, %innt

    %S0 = fsub half %I0, %N0
    %S1 = fsub half %I1, %N1
    %S2 = fsub half %I2, %N2
    %S3 = fsub half %I3, %N3

    ; Compare k < 0
    %con = fcmp olt half %k, 0.0

    %1 = select i1 %con, half 0.0, half %S0
    %2 = select i1 %con, half 0.0, half %S1
    %3 = select i1 %con, half 0.0, half %S2
    %4 = select i1 %con, half 0.0, half %S3

    %5 = alloca <4 x half>
    %6 = load <4 x half>, <4 x half>* %5
    %7 = insertelement <4 x half> %6, half %1, i32 0
    %8 = insertelement <4 x half> %7, half %2, i32 1
    %9 = insertelement <4 x half> %8, half %3, i32 2
    %10 = insertelement <4 x half> %9, half %4, i32 3

    ret <4 x half> %10
}

; GLSL: float16_t frexp(float16_t, out int16_t)
define spir_func { half, i16 } @_Z11frexpStructDhs(
    half %x) #0
{
    %1 = call half @llvm.amdgcn.frexp.mant.f16(half %x)
    %2 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x)

    %3 = insertvalue { half, i16 } undef, half %1, 0
    %4 = insertvalue { half, i16 } %3, i16 %2, 1

    ret { half, i16 } %4
}

; GLSL: f16vec2 frexp(f16vec2, out i16vec2)
define spir_func { <2 x half>, <2 x i16> } @_Z11frexpStructDv2_DhDv2_s(
    <2 x half> %x) #0
{
    %x0 = extractelement <2 x half> %x, i32 0
    %x1 = extractelement <2 x half> %x, i32 1

    %1 = call half @llvm.amdgcn.frexp.mant.f16(half %x0)
    %2 = call half @llvm.amdgcn.frexp.mant.f16(half %x1)

    %3 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x0)
    %4 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x1)

    %5 = insertelement <2 x half> undef, half %1, i32 0
    %6 = insertelement <2 x half> %5, half %2, i32 1

    %7 = insertelement <2 x i16> undef, i16 %3, i32 0
    %8 = insertelement <2 x i16> %7, i16 %4, i32 1

    %9 = insertvalue { <2 x half>, <2 x i16> } undef, <2 x half> %6, 0
    %10 = insertvalue { <2 x half>, <2 x i16> } %9, <2 x i16> %8, 1

    ret { <2 x half>, <2 x i16> } %10
}

; GLSL: f16vec3 frexp(f16vec3, out i16vec3)
define spir_func { <3 x half>, <3 x i16> } @_Z11frexpStructDv3_DhDv3_s(
    <3 x half> %x) #0
{
    %x0 = extractelement <3 x half> %x, i32 0
    %x1 = extractelement <3 x half> %x, i32 1
    %x2 = extractelement <3 x half> %x, i32 2

    %1 = call half @llvm.amdgcn.frexp.mant.f16(half %x0)
    %2 = call half @llvm.amdgcn.frexp.mant.f16(half %x1)
    %3 = call half @llvm.amdgcn.frexp.mant.f16(half %x2)

    %4 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x0)
    %5 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x1)
    %6 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x2)

    %7 = insertelement <3 x half> undef, half %1, i32 0
    %8 = insertelement <3 x half> %7, half %2, i32 1
    %9 = insertelement <3 x half> %8, half %3, i32 2

    %10 = insertelement <3 x i16> undef, i16 %4, i32 0
    %11 = insertelement <3 x i16> %10, i16 %5, i32 1
    %12 = insertelement <3 x i16> %11, i16 %6, i32 2

    %13 = insertvalue { <3 x half>, <3 x i16> } undef, <3 x half> %9, 0
    %14 = insertvalue { <3 x half>, <3 x i16> } %13, <3 x i16> %12, 1

    ret { <3 x half>, <3 x i16> } %14
}

; GLSL: f16vec4 frexp(f16vec4, out i16vec4)
define spir_func { <4 x half>, <4 x i16> } @_Z11frexpStructDv4_DhDv4_s(
    <4 x half> %x) #0
{
    %x0 = extractelement <4 x half> %x, i32 0
    %x1 = extractelement <4 x half> %x, i32 1
    %x2 = extractelement <4 x half> %x, i32 2
    %x3 = extractelement <4 x half> %x, i32 3

    %1 = call half @llvm.amdgcn.frexp.mant.f16(half %x0)
    %2 = call half @llvm.amdgcn.frexp.mant.f16(half %x1)
    %3 = call half @llvm.amdgcn.frexp.mant.f16(half %x2)
    %4 = call half @llvm.amdgcn.frexp.mant.f16(half %x3)

    %5 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x0)
    %6 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x1)
    %7 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x2)
    %8 = call i16 @llvm.amdgcn.frexp.exp.i16.f16(half %x3)

    %9  = insertelement <4 x half> undef, half %1, i32 0
    %10 = insertelement <4 x half> %9, half %2, i32 1
    %11 = insertelement <4 x half> %10, half %3, i32 2
    %12 = insertelement <4 x half> %11, half %4, i32 3

    %13 = insertelement <4 x i16> undef, i16 %5, i32 0
    %14 = insertelement <4 x i16> %13, i16 %6, i32 1
    %15 = insertelement <4 x i16> %14, i16 %7, i32 2
    %16 = insertelement <4 x i16> %15, i16 %8, i32 3

    %17 = insertvalue { <4 x half>, <4 x i16> } undef, <4 x half> %12, 0
    %18 = insertvalue { <4 x half>, <4 x i16> } %17, <4 x i16> %16, 1

    ret { <4 x half>, <4 x i16> } %18
}

; GLSL: float16_t frexp(float16_t, out int)
define spir_func { half, i32 } @_Z11frexpStructDhi(
    half %x) #0
{
    %1 = call { half, i16 } @_Z11frexpStructDhs(half %x)

    %2 = extractvalue { half, i16 } %1, 0
    %3 = extractvalue { half, i16 } %1, 1
    %4 = sext i16 %3 to i32

    %5 = insertvalue { half, i32 } undef, half %2, 0
    %6 = insertvalue { half, i32 } %5, i32 %4, 1

    ret { half, i32 } %6
}

; GLSL: f16vec2 frexp(f16vec2, out ivec2)
define spir_func { <2 x half>, <2 x i32> } @_Z11frexpStructDv2_DhDv2_i(
    <2 x half> %x) #0
{
    %1 = call { <2 x half>, <2 x i16> } @_Z11frexpStructDv2_DhDv2_s(<2 x half> %x)

    %2 = extractvalue { <2 x half>, <2 x i16> } %1, 0
    %3 = extractvalue { <2 x half>, <2 x i16> } %1, 1
    %4 = sext <2 x i16> %3 to <2 x i32>

    %5 = insertvalue { <2 x half>, <2 x i32> } undef, <2 x half> %2, 0
    %6 = insertvalue { <2 x half>, <2 x i32> } %5, <2 x i32> %4, 1

    ret { <2 x half>, <2 x i32> } %6
}

; GLSL: f16vec3 frexp(f16vec3, out ivec3)
define spir_func { <3 x half>, <3 x i32> } @_Z11frexpStructDv3_DhDv3_i(
    <3 x half> %x) #0
{
    %1 = call { <3 x half>, <3 x i16> } @_Z11frexpStructDv3_DhDv3_s(<3 x half> %x)

    %2 = extractvalue { <3 x half>, <3 x i16> } %1, 0
    %3 = extractvalue { <3 x half>, <3 x i16> } %1, 1
    %4 = sext <3 x i16> %3 to <3 x i32>

    %5 = insertvalue { <3 x half>, <3 x i32> } undef, <3 x half> %2, 0
    %6 = insertvalue { <3 x half>, <3 x i32> } %5, <3 x i32> %4, 1

    ret { <3 x half>, <3 x i32> } %6
}

; GLSL: f16vec4 frexp(f16vec4, out ivec4)
define spir_func { <4 x half>, <4 x i32> } @_Z11frexpStructDv4_DhDv4_i(
    <4 x half> %x) #0
{
    %1 = call { <4 x half>, <4 x i16> } @_Z11frexpStructDv4_DhDv4_s(<4 x half> %x)

    %2 = extractvalue { <4 x half>, <4 x i16> } %1, 0
    %3 = extractvalue { <4 x half>, <4 x i16> } %1, 1
    %4 = sext <4 x i16> %3 to <4 x i32>

    %5 = insertvalue { <4 x half>, <4 x i32> } undef, <4 x half> %2, 0
    %6 = insertvalue { <4 x half>, <4 x i32> } %5, <4 x i32> %4, 1

    ret { <4 x half>, <4 x i32> } %6
}

; =====================================================================================================================
; >>>  Functions of Extension AMD_shader_trinary_minmax
; =====================================================================================================================

; GLSL: float16_t min3(float16_t, float16_t, float16_t)
define half @llpc.fmin3.f16(half %x, half %y, half %z)
{
    ; min(x, y)
    %1 = call half @llvm.minnum.f16(half %x, half %y)

    ; min(min(x, y), z)
    %2 = call half @llvm.minnum.f16(half %1, half %z)

    ret half %2
}

; GLSL: float16_t max3(float16_t, float16_t, float16_t)
define half @llpc.fmax3.f16(half %x, half %y, half %z)
{
    ; max(x, y)
    %1 = call half @llvm.maxnum.f16(half %x, half %y)

    ; max(max(x, y), z)
    %2 = call half @llvm.maxnum.f16(half %1, half %z)

    ret half %2
}

; GLSL: float16_t mid3(float16_t, float16_t, float16_t)
define half @llpc.fmid3.f16(half %x, half %y, half %z)
{
    %1 = call half @llvm.amdgcn.fmed3.f16(half %x, half %y, half %z)
    ret half %1
}

declare half @llvm.trunc.f16(half) #0
declare half @llvm.fabs.f16(half) #0
declare half @llvm.sqrt.f16(half) #0
declare half @llvm.floor.f16(half) #0
declare half @llvm.exp2.f16(half) #0
declare half @llvm.log2.f16(half) #0
declare half @llvm.sin.f16(half) #0
declare half @llvm.cos.f16(half) #0

declare half @llvm.minnum.f16(half, half) #0
declare <2 x half> @llvm.minnum.v2f16(<2 x half>, <2 x half>) #0
declare <3 x half> @llvm.minnum.v3f16(<3 x half>, <3 x half>) #0
declare <4 x half> @llvm.minnum.v4f16(<4 x half>, <4 x half>) #0

declare half @llvm.maxnum.f16(half, half) #0
declare <2 x half> @llvm.maxnum.v2f16(<2 x half>, <2 x half>) #0
declare <3 x half> @llvm.maxnum.v3f16(<3 x half>, <3 x half>) #0
declare <4 x half> @llvm.maxnum.v4f16(<4 x half>, <4 x half>) #0

declare half @llvm.fmuladd.f16(half, half, half) #0
declare <2 x half> @llvm.fmuladd.v2f16(<2 x half>, <2 x half>, <2 x half>) #0
declare <3 x half> @llvm.fmuladd.v3f16(<3 x half>, <3 x half>, <3 x half>) #0
declare <4 x half> @llvm.fmuladd.v4f16(<4 x half>, <4 x half>, <4 x half>) #0

declare i1 @llvm.amdgcn.class.f16(half, i32) #1
declare half @llvm.amdgcn.fract.f16(half) #1
declare half @llvm.amdgcn.fmed3.f16(half, half, half) #1
declare half @llvm.rint.f16(half) #0
declare i16 @llvm.amdgcn.frexp.exp.i16.f16(half) #1
declare half @llvm.amdgcn.frexp.mant.f16(half) #1
declare <2 x half> @llvm.amdgcn.cvt.pkrtz(float, float) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
