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

; GLSL: double = double/double
define double @llpc.fdiv.f64(double %y, double %x) #0
{
    %1 = fdiv double 1.0, %x
    %2 = fmul double %y, %1
    ret double %2
}

; =====================================================================================================================
; >>>  Exponential Functions
; =====================================================================================================================

; GLSL: double inversesqrt(double)
define double @llpc.inverseSqrt.f64(double %x) #0
{
    %1 = call double @llvm.sqrt.f64(double %x)
    %2 = fdiv double 1.0, %1
    ret double %2
}

; =====================================================================================================================
; >>>  Common Functions
; =====================================================================================================================

; GLSL: double abs(double)
define double @llpc.fsign.f64(double %x) #0
{
    %con1 = fcmp ogt double %x, 0.0
    %ret1 = select i1 %con1, double 1.0, double %x
    %con2 = fcmp oge double %ret1, 0.0
    %ret2 = select i1 %con2, double %ret1,double -1.0
    ret double %ret2
}

; GLSL: double round(double)
define double @llpc.round.f64(double %x)
{
    %1 = call double @llvm.rint.f64(double %x)
    ret double %1
}

; GLSL: double fract(double)
define double @llpc.fract.f64(double %x) #0
{
    %1 = call double @llvm.amdgcn.fract.f64(double %x)
    ret double %1
}

; GLSL: double mod(double, double)
define double @llpc.mod.f64(double %x, double %y) #0
{
    %1 = fdiv double 1.0,%y
    %2 = fmul double %x, %1
    %3 = call double @llvm.floor.f64(double %2)
    %4 = fmul double %y, %3
    %5 = fsub double %x, %4
    ret double %5
}

; GLSL: double modf(double, out double)
define spir_func double @_Z4modfdPd(
    double %x, double* %i) #0
{
    %1 = call double @llvm.trunc.f64(double %x)
    %2 = fsub double %x, %1

    store double %1, double* %i
    ret double %2
}

; GLSL: dvec2 modf(dvec2, out dvec2)
define spir_func <2 x double> @_Z4modfDv2_dPDv2_d(
    <2 x double> %x, <2 x double>* %i) #0
{
    %x0 = extractelement <2 x double> %x, i32 0
    %x1 = extractelement <2 x double> %x, i32 1

    %1 = call double @llvm.trunc.f64(double %x0)
    %2 = fsub double %x0, %1

    %3 = call double @llvm.trunc.f64(double %x1)
    %4 = fsub double %x1, %3

    %5 = insertelement <2 x double> undef, double %1, i32 0
    %6 = insertelement <2 x double> %5, double %3, i32 1

    %7 = insertelement <2 x double> undef, double %2, i32 0
    %8 = insertelement <2 x double> %7, double %4, i32 1

    store <2 x double> %6, <2 x double>* %i
    ret <2 x double> %8
}

; GLSL: dvec3 modf(dvec3, out dvec3)
define spir_func <3 x double> @_Z4modfDv3_dPDv3_d(
    <3 x double> %x, <3 x double>* %i) #0
{
    %x0 = extractelement <3 x double> %x, i32 0
    %x1 = extractelement <3 x double> %x, i32 1
    %x2 = extractelement <3 x double> %x, i32 2

    %1 = call double @llvm.trunc.f64(double %x0)
    %2 = fsub double %x0, %1

    %3 = call double @llvm.trunc.f64(double %x1)
    %4 = fsub double %x1, %3

    %5 = call double @llvm.trunc.f64(double %x2)
    %6 = fsub double %x2, %5

    %7 = insertelement <3 x double> undef, double %1, i32 0
    %8 = insertelement <3 x double> %7, double %3, i32 1
    %9 = insertelement <3 x double> %8, double %5, i32 2

    %10 = insertelement <3 x double> undef, double %2, i32 0
    %11 = insertelement <3 x double> %10, double %4, i32 1
    %12 = insertelement <3 x double> %11, double %6, i32 2

    store <3 x double> %9, <3 x double>* %i
    ret <3 x double> %12
}

; GLSL: dvec4 modf(dvec4, out dvec4)
define spir_func <4 x double> @_Z4modfDv4_dPDv4_d(
    <4 x double> %x, <4 x double>* %i) #0
{
    %x0 = extractelement <4 x double> %x, i32 0
    %x1 = extractelement <4 x double> %x, i32 1
    %x2 = extractelement <4 x double> %x, i32 2
    %x3 = extractelement <4 x double> %x, i32 3

    %1 = call double @llvm.trunc.f64(double %x0)
    %2 = fsub double %x0, %1

    %3 = call double @llvm.trunc.f64(double %x1)
    %4 = fsub double %x1, %3

    %5 = call double @llvm.trunc.f64(double %x2)
    %6 = fsub double %x2, %5

    %7 = call double @llvm.trunc.f64(double %x3)
    %8 = fsub double %x3, %7

    %9 = insertelement <4 x double> undef, double %1, i32 0
    %10 = insertelement <4 x double> %9, double %3, i32 1
    %11 = insertelement <4 x double> %10, double %5, i32 2
    %12 = insertelement <4 x double> %11, double %7, i32 3

    %13 = insertelement <4 x double> undef, double %2, i32 0
    %14 = insertelement <4 x double> %13, double %4, i32 1
    %15 = insertelement <4 x double> %14, double %6, i32 2
    %16 = insertelement <4 x double> %15, double %8, i32 3

    store <4 x double> %12, <4 x double>* %i
    ret <4 x double> %16
}

; GLSL: double clamp(double, double ,double)
define double @llpc.fclamp.f64(double %x, double %minVal, double %maxVal) #0
{
    %1 = call double @llvm.maxnum.f64(double %x, double %minVal)
    %2 = call double @llvm.minnum.f64(double %1, double %maxVal)
    ret double %2
}

; GLSL: double mix(double, double, double)
define double @llpc.fmix.f64(double %x, double %y, double %a) #0
{
    %1 = fsub double %y, %x
    %2 = fmul double %1, %a
    %3 = fadd double %2, %x
    ret double %3
}

; GLSL: double step(double, double)
define double @llpc.step.f64(double %edge, double %x) #0
{
    %1 = fcmp olt double %x, %edge
    %2 = select i1 %1, double 0.0, double 1.0
    ret double %2
}

; GLSL: double smoothstep(double, double, double)
define double @llpc.smoothStep.f64(double %edge0, double %edge1, double %x) #0
{
    %1 = fsub double %x, %edge0
    %2 = fsub double %edge1, %edge0
    %3 = fdiv double 1.0, %2
    %4 = fmul double %1, %3
    %5 = call double @llpc.fclamp.f64(double %4, double 0.0, double 1.0)
    %6 = fmul double %5, %5
    %7 = fmul double -2.0, %5
    %8 = fadd double 3.0, %7
    %9 = fmul double %6, %8
    ret double %9
}

; GLSL: bool isinf(double)
define i1 @llpc.isinf.f64(double %x) #0
{
    ; 0x004: negative infinity; 0x200: positive infinity
    %1 = call i1 @llvm.amdgcn.class.f64(double %x, i32 516)
    ret i1 %1
}

; GLSL: bool isnan(double)
define i1 @llpc.isnan.f64(double %x) #0
{
    ; 0x001: signaling NaN, 0x002: quiet NaN
    %1 = call i1 @llvm.amdgcn.class.f64(double %x, i32 3)
    ret i1 %1
}

; GLSL: double frexp(double, out int)
define spir_func {double, i32} @_Z11frexpStructd(
    double %x) #0
{
    %1 = call double @llvm.amdgcn.frexp.mant.f64(double %x)
    %2 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x)

    %3 = insertvalue {double, i32} undef, double %1, 0
    %4 = insertvalue {double, i32} %3, i32 %2, 1

    ret {double, i32} %4
}

; GLSL: dvec2 frexp(dvec2, out ivec2)
define spir_func {<2 x double>, <2 x i32>} @_Z11frexpStructDv2_d(
    <2 x double> %x) #0
{
    %x0 = extractelement <2 x double> %x, i32 0
    %x1 = extractelement <2 x double> %x, i32 1

    %1 = call double @llvm.amdgcn.frexp.mant.f64(double %x0)
    %2 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x0)

    %3 = call double @llvm.amdgcn.frexp.mant.f64(double %x1)
    %4 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x1)

    %5 = insertelement <2 x double> undef, double %1, i32 0
    %6 = insertelement <2 x double> %5, double %3, i32 1

    %7 = insertelement <2 x i32> undef, i32 %2, i32 0
    %8 = insertelement <2 x i32> %7, i32 %4, i32 1

    %9 = insertvalue {<2 x double>, <2 x i32>} undef, <2 x double> %6, 0
    %10 = insertvalue {<2 x double>, <2 x i32>} %9, <2 x i32> %8, 1

    ret {<2 x double>, <2 x i32>} %10
}

; GLSL: dvec3 frexp(dvec3, out ivec3)
define spir_func {<3 x double>, <3 x i32>} @_Z11frexpStructDv3_d(
    <3 x double> %x) #0
{
    %x0 = extractelement <3 x double> %x, i32 0
    %x1 = extractelement <3 x double> %x, i32 1
    %x2 = extractelement <3 x double> %x, i32 2

    %1 = call double @llvm.amdgcn.frexp.mant.f64(double %x0)
    %2 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x0)

    %3 = call double @llvm.amdgcn.frexp.mant.f64(double %x1)
    %4 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x1)

    %5 = call double @llvm.amdgcn.frexp.mant.f64(double %x2)
    %6 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x2)

    %7 = insertelement <3 x double> undef, double %1, i32 0
    %8 = insertelement <3 x double> %7, double %3, i32 1
    %9 = insertelement <3 x double> %8, double %5, i32 2

    %10 = insertelement <3 x i32> undef, i32 %2, i32 0
    %11 = insertelement <3 x i32> %10, i32 %4, i32 1
    %12 = insertelement <3 x i32> %11, i32 %6, i32 2

    %13 = insertvalue {<3 x double>, <3 x i32>} undef, <3 x double> %9, 0
    %14 = insertvalue {<3 x double>, <3 x i32>} %13, <3 x i32> %12, 1

    ret {<3 x double>, <3 x i32>} %14
}

; GLSL: dvec4 frexp(dvec4, out ivec4)
define spir_func {<4 x double>, <4 x i32>} @_Z11frexpStructDv4_d(
    <4 x double> %x) #0
{
    %x0 = extractelement <4 x double> %x, i32 0
    %x1 = extractelement <4 x double> %x, i32 1
    %x2 = extractelement <4 x double> %x, i32 2
    %x3 = extractelement <4 x double> %x, i32 3

    %1 = call double @llvm.amdgcn.frexp.mant.f64(double %x0)
    %2 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x0)

    %3 = call double @llvm.amdgcn.frexp.mant.f64(double %x1)
    %4 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x1)

    %5 = call double @llvm.amdgcn.frexp.mant.f64(double %x2)
    %6 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x2)

    %7 = call double @llvm.amdgcn.frexp.mant.f64(double %x3)
    %8 = call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x3)

    %9 = insertelement <4 x double> undef, double %1, i32 0
    %10 = insertelement <4 x double> %9, double %3, i32 1
    %11 = insertelement <4 x double> %10, double %5, i32 2
    %12 = insertelement <4 x double> %11, double %7, i32 3

    %13 = insertelement <4 x i32> undef, i32 %2, i32 0
    %14 = insertelement <4 x i32> %13, i32 %4, i32 1
    %15 = insertelement <4 x i32> %14, i32 %6, i32 2
    %16 = insertelement <4 x i32> %15, i32 %8, i32 3

    %17 = insertvalue {<4 x double>, <4 x i32>} undef, <4 x double> %12, 0
    %18 = insertvalue {<4 x double>, <4 x i32>} %17, <4 x i32> %16, 1

    ret {<4 x double>, <4 x i32>} %18
}

; =====================================================================================================================
; >>>  Floating-point Pack and Unpack Functions
; =====================================================================================================================

; GLSL: double packDouble2x32(uvec2)
define double @_Z14packDouble2x32Dv2_i(<2 x i32> %v) #0
{
    %v0 =  extractelement <2 x i32> %v, i32 0
    %v1 =  extractelement <2 x i32> %v, i32 1

    %itgm =  zext i32 %v1 to i64
    %itgshf = shl i64 %itgm, 32
    %itgl =  zext i32 %v0 to i64
    %itg64 = or i64 %itgshf,%itgl
    %end = bitcast i64 %itg64 to double

    ret double %end
}

; GLSL: uvec2 unpackDouble2x32(double)
define <2 x i32> @_Z16unpackDouble2x32d(double %v) #0
{
    %itg64 = bitcast double %v to i64
    %il = trunc i64 %itg64 to i32
    %im0 = lshr i64 %itg64, 32
    %im = trunc i64 %im0 to i32
    %vec2ptr = alloca <2 x i32>
    %vec2 = load <2 x i32>, <2 x i32>* %vec2ptr

    %vec20 = insertelement <2 x i32> %vec2, i32 %il, i32 0
    %vec21 = insertelement <2 x i32> %vec20, i32 %im, i32 1

    ret <2 x i32> %vec21
}

; =====================================================================================================================
; >>>  Geometric Functions
; =====================================================================================================================
; GLSL: double length(double)
define spir_func double @_Z6lengthd(double %x) #0
{
    %end = call double @llvm.fabs.f64(double %x)
    ret double %end
}

; GLSL: double length(dvec2)
define spir_func double @_Z6lengthDv2_d(<2 x double> %x) #0
{
    %x.x = extractelement <2 x double> %x, i32 0
    %x.y = extractelement <2 x double> %x, i32 1

    %v0 = fmul double %x.x, %x.x
    %v1 = fmul double %x.y, %x.y
    %sqr = fadd double %v0, %v1
    %end = call double @llvm.sqrt.f64(double %sqr)

    ret double %end
}

; GLSL: double length(dvec3)
define spir_func double @_Z6lengthDv3_d(<3 x double> %x) #0
{
    %x.x = extractelement <3 x double> %x, i32 0
    %x.y = extractelement <3 x double> %x, i32 1
    %x.z = extractelement <3 x double> %x, i32 2

    %v0 = fmul double %x.x, %x.x
    %v1 = fmul double %x.y, %x.y
    %vl = fadd double %v0, %v1
    %v2 = fmul double %x.z, %x.z
    %sqr = fadd double %vl, %v2
    %end = call double @llvm.sqrt.f64(double %sqr)

    ret double %end
}

; GLSL: double length(dvec4)
define spir_func double @_Z6lengthDv4_d(<4 x double> %x) #0
{
    %x.x = extractelement <4 x double> %x, i32 0
    %x.y = extractelement <4 x double> %x, i32 1
    %x.z = extractelement <4 x double> %x, i32 2
    %x.w = extractelement <4 x double> %x, i32 3

    %v0 = fmul double %x.x, %x.x
    %v1 = fmul double %x.y, %x.y
    %vl = fadd double %v0, %v1
    %v2 = fmul double %x.z, %x.z
    %v3 = fmul double %x.w, %x.w
    %vm = fadd double %v2, %v3
    %sqr = fadd double %vl, %vm
    %end = call double @llvm.sqrt.f64(double %sqr)

    ret double %end
}

; GLSL: double distance(double, double)
define spir_func double @_Z8distancedd(double %p0, double %p1) #0
{
    %subtra = fsub double %p0 ,%p1
    %1 = call double @llvm.fabs.f64(double %subtra)
    ret double %1
}

; Function Attrs: nounwind
define spir_func double @_Z8distanceDv2_dDv2_d(<2 x double> %p0, <2 x double> %p1) #0
{
    %subtra = fsub <2 x double> %p0 ,%p1
    %1 = call double @_Z6lengthDv2_d(<2 x double> %subtra)
    ret double %1
}

; GLSL: double distance(dvec3, dvec3)
define spir_func double @_Z8distanceDv3_dDv3_d(<3 x double> %p0, <3 x double> %p1) #0
{
    %subtra = fsub <3 x double> %p0 ,%p1
    %1 = call double @_Z6lengthDv3_d(<3 x double> %subtra)
    ret double %1
}

; GLSL: double distance(dvec4, dvec4)
define spir_func double @_Z8distanceDv4_dDv4_d(<4 x double> %p0, <4 x double> %p1) #0
{
    %subtra = fsub <4 x double> %p0 ,%p1
    %1 = call double @_Z6lengthDv4_d(<4 x double> %subtra)
    ret double %1
}

; GLSL: double dot(double, double)
define spir_func double @_Z3dotdd(double %x, double %y) #0
{
    %1 = fmul double %x, %y
    ret double %1
}

; GLSL double dot(dvec2, dvec2)
define spir_func double @_Z3dotDv2_dDv2_d(<2 x double> %x, <2 x double> %y) #0
{
    %x.x = extractelement <2 x double> %x, i32 0
    %x.y = extractelement <2 x double> %x, i32 1

    %y.x = extractelement <2 x double> %y, i32 0
    %y.y = extractelement <2 x double> %y, i32 1

    %v0 = fmul double %x.x, %y.x
    %v1 = fmul double %x.y, %y.y
    %end = fadd double %v0, %v1

    ret double %end
}

; GLSL double dot(dvec3, dvec3)
define spir_func double @_Z3dotDv3_dDv3_d(<3 x double> %x, <3 x double> %y) #0
{
    %x.x = extractelement <3 x double> %x, i32 0
    %x.y = extractelement <3 x double> %x, i32 1
    %x.z = extractelement <3 x double> %x, i32 2

    %y.x = extractelement <3 x double> %y, i32 0
    %y.y = extractelement <3 x double> %y, i32 1
    %y.z = extractelement <3 x double> %y, i32 2

    %v0 = fmul double %x.x, %y.x
    %v1 = fmul double %x.y, %y.y
    %vl = fadd double %v0, %v1
    %v2 = fmul double %x.z, %y.z
    %end = fadd double %v2, %vl

    ret double %end
}

; GLSL double dot(dvec4, dvec4)
define spir_func double @_Z3dotDv4_dDv4_d(<4 x double> %x, <4 x double> %y) #0
{
    %x.x = extractelement <4 x double> %x, i32 0
    %x.y = extractelement <4 x double> %x, i32 1
    %x.z = extractelement <4 x double> %x, i32 2
    %x.w = extractelement <4 x double> %x, i32 3

    %y.x = extractelement <4 x double> %y, i32 0
    %y.y = extractelement <4 x double> %y, i32 1
    %y.z = extractelement <4 x double> %y, i32 2
    %y.w = extractelement <4 x double> %y, i32 3

    %v0 = fmul double %x.x, %y.x
    %v1 = fmul double %x.y, %y.y
    %vl = fadd double %v0, %v1
    %v2 = fmul double %x.z, %y.z
    %v3 = fmul double %x.w, %y.w
    %vm = fadd double %v2, %v3
    %end = fadd double %vl,%vm

    ret double %end
}

; GLSL: dvec3 cross(dvec3, dvec3)
define spir_func <3 x double> @_Z5crossDv3_dDv3_d(<3 x double> %x, <3 x double> %y) #0
{
    %x.x = extractelement <3 x double> %x, i32 0
    %x.y = extractelement <3 x double> %x, i32 1
    %x.z = extractelement <3 x double> %x, i32 2

    %y.x = extractelement <3 x double> %y, i32 0
    %y.y = extractelement <3 x double> %y, i32 1
    %y.z = extractelement <3 x double> %y, i32 2

    %l0 = fmul double %x.y, %y.z
    %l1 = fmul double %x.z, %y.x
    %l2 = fmul double %x.x, %y.y

    %r0 = fmul double %y.y, %x.z
    %r1 = fmul double %y.z, %x.x
    %r2 = fmul double %y.x, %x.y

    %1 = fsub double %l0, %r0
    %2 = fsub double %l1, %r1
    %3 = fsub double %l2, %r2

    %4 = alloca <3 x double>
    %5 = load <3 x double>, <3 x double>* %4
    %6 = insertelement <3 x double> %5, double %1, i32 0
    %7 = insertelement <3 x double> %6, double %2, i32 1
    %8 = insertelement <3 x double> %7, double %3, i32 2

    ret <3 x double> %8
}

; GLSL: double normalize(double)
define spir_func double @_Z9normalized(double %x) #0
{
    %1 = call double @llvm.fabs.f64(double %x)
    ret double %1
}

; GLSL: dvec2 normalize(dvec2)
define spir_func <2 x double> @_Z9normalizeDv2_d(<2 x double> %x) #0
{
    %length = call double @_Z6lengthDv2_d(<2 x double> %x)
    %rsq = fdiv double 1.0, %length

    %x.x = extractelement <2 x double> %x, i32 0
    %x.y = extractelement <2 x double> %x, i32 1

    %1 = fmul double %x.x, %rsq
    %2 = fmul double %x.y, %rsq

    %3 = alloca <2 x double>
    %4 = load <2 x double>, <2 x double>* %3
    %5 = insertelement <2 x double> %4, double %1, i32 0
    %6 = insertelement <2 x double> %5, double %2, i32 1

    ret <2 x double> %6
}

; GLSL: dvec3 normalize(dvec3)
define spir_func <3 x double> @_Z9normalizeDv3_d(<3 x double> %x) #0
{
    %length = call double @_Z6lengthDv3_d(<3 x double> %x)
    %rsq = fdiv double 1.0, %length

    %x.x = extractelement <3 x double> %x, i32 0
    %x.y = extractelement <3 x double> %x, i32 1
    %x.z = extractelement <3 x double> %x, i32 2

    %1 = fmul double %x.x, %rsq
    %2 = fmul double %x.y, %rsq
    %3 = fmul double %x.z, %rsq

    %4 = alloca <3 x double>
    %5 = load <3 x double>, <3 x double>* %4
    %6 = insertelement <3 x double> %5, double %1, i32 0
    %7 = insertelement <3 x double> %6, double %2, i32 1
    %8 = insertelement <3 x double> %7, double %3, i32 2

    ret <3 x double> %8
}

; GLSL: dvec4 normalize(dvec4)
define spir_func <4 x double> @_Z9normalizeDv4_d(<4 x double> %x) #0
{
    %length = call double @_Z6lengthDv4_d(<4 x double> %x)
    %rsq = fdiv double 1.0, %length

    %x.x = extractelement <4 x double> %x, i32 0
    %x.y = extractelement <4 x double> %x, i32 1
    %x.z = extractelement <4 x double> %x, i32 2
    %x.w = extractelement <4 x double> %x, i32 3

    %1 = fmul double %x.x, %rsq
    %2 = fmul double %x.y, %rsq
    %3 = fmul double %x.z, %rsq
    %4 = fmul double %x.w, %rsq

    %5 = alloca <4 x double>
    %6 = load <4 x double>, <4 x double>* %5
    %7 = insertelement <4 x double> %6, double %1, i32 0
    %8 = insertelement <4 x double> %7, double %2, i32 1
    %9 = insertelement <4 x double> %8, double %3, i32 2
    %10 = insertelement <4 x double> %9, double %4, i32 3

    ret <4 x double> %10
}

; GLSL: double faceforward(double, double, double)
define spir_func double @_Z11faceForwardddd( double %N, double %I,  double %Nref) #0
{
    %dotv = fmul double %I, %Nref
    ; Compare if dot < 0.0
    %con = fcmp olt double %dotv, 0.0

    %NN = fsub double 0.0, %N

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,double %N, double %NN

    ret double %1
}

; GLSL: dvec2 faceforward(dvec2, dvec2, dvec2)
define spir_func <2 x double> @_Z11faceForwardDv2_dDv2_dDv2_d(<2 x double> %N, <2 x double> %I, <2 x double> %Nref) #0
{
    %dotv = call double @_Z3dotDv2_dDv2_d(<2 x double> %I, <2 x double> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt double %dotv, 0.0

    %N.x = extractelement <2 x double> %N, i32 0
    %N.y = extractelement <2 x double> %N, i32 1

    %NN.x = fsub double 0.0, %N.x
    %NN.y = fsub double 0.0, %N.y

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,double %N.x, double %NN.x
    %2 = select i1 %con ,double %N.y, double %NN.y

    %3 = alloca <2 x double>
    %4 = load <2 x double>, <2 x double>* %3
    %5 = insertelement <2 x double> %4, double %1, i32 0
    %6 = insertelement <2 x double> %5, double %2, i32 1

    ret <2 x double> %6
}

; GLSL: dvec3 faceforward(dvec3, dvec3, dvec3)
define spir_func <3 x double> @_Z11faceForwardDv3_dDv3_dDv3_d(<3 x double> %N, <3 x double> %I, <3 x double> %Nref) #0
{
    %dotv = call double @_Z3dotDv3_dDv3_d(<3 x double> %I, <3 x double> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt double %dotv, 0.0

    %N.x = extractelement <3 x double> %N, i32 0
    %N.y = extractelement <3 x double> %N, i32 1
    %N.z = extractelement <3 x double> %N, i32 2

    %NN.x = fsub double 0.0, %N.x
    %NN.y = fsub double 0.0, %N.y
    %NN.z = fsub double 0.0, %N.z

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,double %N.x, double %NN.x
    %2 = select i1 %con ,double %N.y, double %NN.y
    %3 = select i1 %con ,double %N.z, double %NN.z

    %4 = alloca <3 x double>
    %5 = load <3 x double>, <3 x double>* %4
    %6 = insertelement <3 x double> %5, double %1, i32 0
    %7 = insertelement <3 x double> %6, double %2, i32 1
    %8 = insertelement <3 x double> %7, double %3, i32 2

    ret <3 x double> %8
}

; GLSL: dvec4 faceforward(dvec4, dvec4, dvec4)
define spir_func <4 x double> @_Z11faceForwardDv4_dDv4_dDv4_d(<4 x double> %N, <4 x double> %I, <4 x double> %Nref) #0
{
    %dotv = call double @_Z3dotDv4_dDv4_d(<4 x double> %I, <4 x double> %Nref)
    ; Compare if dot < 0.0
    %con = fcmp olt double %dotv, 0.0

    %N.x = extractelement <4 x double> %N, i32 0
    %N.y = extractelement <4 x double> %N, i32 1
    %N.z = extractelement <4 x double> %N, i32 2
    %N.w = extractelement <4 x double> %N, i32 3

    %NN.x = fsub double 0.0, %N.x
    %NN.y = fsub double 0.0, %N.y
    %NN.z = fsub double 0.0, %N.z
    %NN.w = fsub double 0.0, %N.w

    ; dot < 0.0, return N, otherwise return -N
    %1 = select i1 %con ,double %N.x,  double %NN.x
    %2 = select i1 %con ,double %N.y,  double %NN.y
    %3 = select i1 %con ,double %N.z,  double %NN.z
    %4 = select i1 %con ,double %N.w,  double %NN.w

    %5 = alloca <4 x double>
    %6 = load <4 x double>, <4 x double>* %5
    %7 = insertelement <4 x double> %6, double %1, i32 0
    %8 = insertelement <4 x double> %7, double %2, i32 1
    %9 = insertelement <4 x double> %8, double %3, i32 2
    %10 = insertelement <4 x double> %9, double %4, i32 3

    ret <4 x double> %10
}

; GLSL: double reflect(double, double)
define spir_func double @_Z7reflectdd(double %I, double %N) #0
{
    %dotin = fmul double %I, %N
    %dot = fmul double %dotin, 2.0

    ; 2 * dot(N, I) * N
    %right = fmul double %dot, %N
    %end = fsub double %I, %right

    ret double %end
}

; GLSL: dvec2 reflect(dvec2, dvec2)
define spir_func <2 x double> @_Z7reflectDv2_dDv2_d(<2 x double> %I, <2 x double> %N) #0
{
    %dotin = call double @_Z3dotDv2_dDv2_d(<2 x double> %I, <2 x double> %N)
    %dot = fmul double %dotin, 2.0

    %1 = alloca <2 x double>
    %2 = load <2 x double>, <2 x double>* %1
    %3 = insertelement <2 x double> %2, double %dot, i32 0
    %dotv = insertelement <2 x double> %3, double %dot, i32 1

    ; 2 * dot(N, I) * N
    %right = fmul <2 x double> %dotv, %N
    %end = fsub <2 x double> %I, %right

    ret <2 x double> %end
}

; GLSL: dvec3 reflect(dvec3, dvec3)
define spir_func <3 x double> @_Z7reflectDv3_dDv3_d(<3 x double> %I, <3 x double> %N) #0
{
    %dotin = call double @_Z3dotDv3_dDv3_d(<3 x double> %I, <3 x double> %N)
    %dot = fmul double %dotin, 2.0

    %1 = alloca <3 x double>
    %2 = load <3 x double>, <3 x double>* %1
    %3 = insertelement <3 x double> %2, double %dot, i32 0
    %4 = insertelement <3 x double> %3, double %dot, i32 1
    %dotv = insertelement <3 x double> %4, double %dot, i32 2

    ; 2 * dot(N, I) * N
    %right = fmul <3 x double> %dotv, %N
    %end = fsub <3 x double> %I, %right

    ret <3 x double> %end
}

; GLSL: dvec4 reflect(dvec4, dvec4)
define spir_func <4 x double> @_Z7reflectDv4_dDv4_d(<4 x double> %I, <4 x double> %N) #0
{
    %dotin = call double @_Z3dotDv4_dDv4_d(<4 x double> %I, <4 x double> %N)
    %dot = fmul double %dotin, 2.0

    %1 = alloca <4 x double>
    %2 = load <4 x double>, <4 x double>* %1
    %3 = insertelement <4 x double> %2, double %dot, i32 0
    %4 = insertelement <4 x double> %3, double %dot, i32 1
    %5 = insertelement <4 x double> %4, double %dot, i32 2
    %dotv = insertelement <4 x double> %5, double %dot, i32 3

    ; 2 * dot(N, I) * N
    %right = fmul <4 x double> %dotv, %N
    %end = fsub <4 x double> %I, %right

    ret <4 x double> %end
}

; GLSL: double refract(double, double, double)
define spir_func double @_Z7refractddd(double %I, double %N, double %eta) #0
{
    %dotin = fmul double %I, %N
    %dotinsqr = fmul double %dotin, %dotin
    %e1 = fsub double 1.0, %dotinsqr
    %e2 = fmul double %eta, %eta
    %e3 = fmul double %e1, %e2
    %k = fsub double 1.0, %e3
    %ksqrt = call double @llvm.sqrt.f64(double %k)
    %etadot = fmul double %eta, %dotin
    %innt = fadd double %etadot, %ksqrt

    %N0 = fmul double %innt, %N
    %I0 = fmul double %I, %eta
    %S = fsub double %I0, %N0
    ; Compare k < 0
    %con = fcmp olt double %k, 0.0
    %1 = select i1 %con, double 0.0, double %S

    ret double %1
}

; GLSL: dvec2 refract(dvec2, dvec2, double)
define spir_func <2 x double> @_Z7refractDv2_dDv2_dd(<2 x double> %I, <2 x double> %N, double %eta) #0
{
    %dotin = call double @_Z3dotDv2_dDv2_d(<2 x double> %I, <2 x double> %N)
    %dotinsqr = fmul double %dotin, %dotin
    %e1 = fsub double 1.0, %dotinsqr
    %e2 = fmul double %eta, %eta
    %e3 = fmul double %e1, %e2
    %k = fsub double 1.0, %e3
    %ksqrt = call double @llvm.sqrt.f64(double %k)
    %etadot = fmul double %eta, %dotin
    %innt = fadd double %etadot, %ksqrt

    %I.x = extractelement <2 x double> %I, i32 0
    %I.y = extractelement <2 x double> %I, i32 1

    %N.x = extractelement <2 x double> %N, i32 0
    %N.y = extractelement <2 x double> %N, i32 1

    %I0 = fmul double %I.x, %eta
    %I1 = fmul double %I.y, %eta

    %N0 = fmul double %N.x, %innt
    %N1 = fmul double %N.y, %innt

    %S0 = fsub double %I0, %N0
    %S1 = fsub double %I1, %N1

    ; Compare k < 0
    %con = fcmp olt double %k, 0.0

    %1 = select i1 %con, double 0.0, double %S0
    %2 = select i1 %con, double 0.0, double %S1

    %3 = alloca <2 x double>
    %4 = load <2 x double>, <2 x double>* %3
    %5 = insertelement <2 x double> %4, double %1, i32 0
    %6 = insertelement <2 x double> %5, double %2, i32 1

    ret <2 x double> %6
}

; GLSL: dvec3 refract(dvec3, dvec3, double)
define spir_func <3 x double> @_Z7refractDv3_dDv3_dd(<3 x double> %I, <3 x double> %N, double %eta) #0
{
    %dotin = call double @_Z3dotDv3_dDv3_d(<3 x double> %I, <3 x double> %N)
    %dotinsqr = fmul double %dotin, %dotin
    %e1 = fsub double 1.0, %dotinsqr
    %e2 = fmul double %eta, %eta
    %e3 = fmul double %e1, %e2
    %k = fsub double 1.0, %e3
    %ksqrt = call double @llvm.sqrt.f64(double %k)
    %etadot = fmul double %eta, %dotin
    %innt = fadd double %etadot, %ksqrt

    %I.x = extractelement <3 x double> %I, i32 0
    %I.y = extractelement <3 x double> %I, i32 1
    %I.z = extractelement <3 x double> %I, i32 2

    %N.x = extractelement <3 x double> %N, i32 0
    %N.y = extractelement <3 x double> %N, i32 1
    %N.z = extractelement <3 x double> %N, i32 2

    %I0 = fmul double %I.x, %eta
    %I1 = fmul double %I.y, %eta
    %I2 = fmul double %I.z, %eta

    %N0 = fmul double %N.x, %innt
    %N1 = fmul double %N.y, %innt
    %N2 = fmul double %N.z, %innt

    %S0 = fsub double %I0, %N0
    %S1 = fsub double %I1, %N1
    %S2 = fsub double %I2, %N2

    ; Compare k < 0
    %con = fcmp olt double %k, 0.0

    %1 = select i1 %con, double 0.0, double %S0
    %2 = select i1 %con, double 0.0, double %S1
    %3 = select i1 %con, double 0.0, double %S2

    %4 = alloca <3 x double>
    %5 = load <3 x double>, <3 x double>* %4
    %6 = insertelement <3 x double> %5, double %1, i32 0
    %7 = insertelement <3 x double> %6, double %2, i32 1
    %8 = insertelement <3 x double> %7, double %3, i32 2

    ret <3 x double> %8
}

; GLSL: dvec4 refract(dvec4, dvec4, double)
define spir_func <4 x double> @_Z7refractDv4_dDv4_dd(<4 x double> %I, <4 x double> %N, double %eta) #0
{
    %dotin = call double @_Z3dotDv4_dDv4_d(<4 x double> %I, <4 x double> %N)
    %dotinsqr = fmul double %dotin, %dotin
    %e1 = fsub double 1.0, %dotinsqr
    %e2 = fmul double %eta, %eta
    %e3 = fmul double %e1, %e2
    %k = fsub double 1.0, %e3
    %ksqrt = call double @llvm.sqrt.f64(double %k)
    %etadot = fmul double %eta, %dotin
    %innt = fadd double %etadot, %ksqrt

    %I.x = extractelement <4 x double> %I, i32 0
    %I.y = extractelement <4 x double> %I, i32 1
    %I.z = extractelement <4 x double> %I, i32 2
    %I.w = extractelement <4 x double> %I, i32 3

    %N.x = extractelement <4 x double> %N, i32 0
    %N.y = extractelement <4 x double> %N, i32 1
    %N.z = extractelement <4 x double> %N, i32 2
    %N.w = extractelement <4 x double> %N, i32 3

    %I0 = fmul double %I.x, %eta
    %I1 = fmul double %I.y, %eta
    %I2 = fmul double %I.z, %eta
    %I3 = fmul double %I.w, %eta

    %N0 = fmul double %N.x, %innt
    %N1 = fmul double %N.y, %innt
    %N2 = fmul double %N.z, %innt
    %N3 = fmul double %N.w, %innt

    %S0 = fsub double %I0, %N0
    %S1 = fsub double %I1, %N1
    %S2 = fsub double %I2, %N2
    %S3 = fsub double %I3, %N3

    ; Compare k < 0
    %con = fcmp olt double %k, 0.0

    %1 = select i1 %con, double 0.0, double %S0
    %2 = select i1 %con, double 0.0, double %S1
    %3 = select i1 %con, double 0.0, double %S2
    %4 = select i1 %con, double 0.0, double %S3

    %5 = alloca <4 x double>
    %6 = load <4 x double>, <4 x double>* %5
    %7 = insertelement <4 x double> %6, double %1, i32 0
    %8 = insertelement <4 x double> %7, double %2, i32 1
    %9 = insertelement <4 x double> %8, double %3, i32 2
    %10 = insertelement <4 x double> %9, double %4, i32 3

    ret <4 x double> %10
}

declare double @llvm.trunc.f64(double ) #0
declare double @llvm.fabs.f64(double) #0
declare double @llvm.sqrt.f64(double) #0
declare double @llvm.floor.f64(double) #0
declare double @llvm.minnum.f64(double, double) #0
declare double @llvm.maxnum.f64(double, double) #0
declare double @llvm.amdgcn.frexp.mant.f64(double) #1
declare i1 @llvm.amdgcn.class.f64(double, i32) #1
declare i32 @llvm.amdgcn.frexp.exp.i32.f64(double %x) #1
declare double @llvm.amdgcn.fract.f64(double) #1
declare double @llvm.amdgcn.fmed3.f64(double %x, double %minVal, double %maxVal) #1
declare double @llvm.rint.f64(double) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
