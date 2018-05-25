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

; GLSL: dmat2 = outerProduct(dvec2, dvec2)
define spir_func [2 x <2 x double>] @_Z12OuterProductDv2_dDv2_d(
    <2 x double> %c, <2 x double> %r) #0
{
    %m = alloca [2 x <2 x double>]
    %m0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 1

    %c0 = extractelement <2 x double> %c, i32 0
    %c1 = extractelement <2 x double> %c, i32 1

    %r0 = extractelement <2 x double> %r, i32 0
    %r1 = extractelement <2 x double> %r, i32 1

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c0, %r1
    store double %3, double* %m10
    %4 = fmul double %c1, %r1
    store double %4, double* %m11
    %5 = load [2 x <2 x double>], [2 x <2 x double>]* %m

    ret [2 x <2 x double>] %5
}

; GLSL: dmat3 = outerProduct(dvec3, dvec3)
define spir_func [3 x <3 x double>] @_Z12OuterProductDv3_dDv3_d(
    <3 x double> %c, <3 x double> %r) #0
{
    %m = alloca [3 x <3 x double>]
    %m0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 2

    %c0 = extractelement <3 x double> %c, i32 0
    %c1 = extractelement <3 x double> %c, i32 1
    %c2 = extractelement <3 x double> %c, i32 2

    %r0 = extractelement <3 x double> %r, i32 0
    %r1 = extractelement <3 x double> %r, i32 1
    %r2 = extractelement <3 x double> %r, i32 2

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c0, %r1
    store double %4, double* %m10
    %5 = fmul double %c1, %r1
    store double %5, double* %m11
    %6 = fmul double %c2, %r1
    store double %6, double* %m12
    %7 = fmul double %c0, %r2
    store double %7, double* %m20
    %8 = fmul double %c1, %r2
    store double %8, double* %m21
    %9 = fmul double %c2, %r2
    store double %9, double* %m22
    %10 = load [3 x <3 x double>], [3 x <3 x double>]* %m

    ret [3 x <3 x double>] %10
}

; GLSL: dmat4 = outerProduct(dvec4, dvec4)
define spir_func [4 x <4 x double>] @_Z12OuterProductDv4_dDv4_d(
    <4 x double> %c, <4 x double> %r) #0
{
    %m = alloca [4 x <4 x double>]
    %m0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 3

    %m3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <4 x double>, <4 x double>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <4 x double>, <4 x double>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <4 x double>, <4 x double>* %m3, i32 0, i32 2
    %m33 = getelementptr inbounds <4 x double>, <4 x double>* %m3, i32 0, i32 3

    %c0 = extractelement <4 x double> %c, i32 0
    %c1 = extractelement <4 x double> %c, i32 1
    %c2 = extractelement <4 x double> %c, i32 2
    %c3 = extractelement <4 x double> %c, i32 3

    %r0 = extractelement <4 x double> %r, i32 0
    %r1 = extractelement <4 x double> %r, i32 1
    %r2 = extractelement <4 x double> %r, i32 2
    %r3 = extractelement <4 x double> %r, i32 3

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c3, %r0
    store double %4, double* %m03
    %5 = fmul double %c0, %r1
    store double %5, double* %m10
    %6 = fmul double %c1, %r1
    store double %6, double* %m11
    %7 = fmul double %c2, %r1
    store double %7, double* %m12
    %8 = fmul double %c3, %r1
    store double %8, double* %m13
    %9 = fmul double %c0, %r2
    store double %9, double* %m20
    %10 = fmul double %c1, %r2
    store double %10, double* %m21
    %11 = fmul double %c2, %r2
    store double %11, double* %m22
    %12 = fmul double %c3, %r2
    store double %12, double* %m23
    %13 = fmul double %c0, %r3
    store double %13, double* %m30
    %14 = fmul double %c1, %r3
    store double %14, double* %m31
    %15 = fmul double %c2, %r3
    store double %15, double* %m32
    %16 = fmul double %c3, %r3
    store double %16, double* %m33
    %17 = load [4 x <4 x double>], [4 x <4 x double>]* %m

    ret [4 x <4 x double>] %17
}

; GLSL: dmat2x3 = outerProduct(dvec3, dvec2)
define spir_func [2 x <3 x double>] @_Z12OuterProductDv3_dDv2_d(
    <3 x double> %c, <2 x double> %r) #0
{
    %m = alloca [2 x <3 x double>]
    %m0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 2

    %c0 = extractelement <3 x double> %c, i32 0
    %c1 = extractelement <3 x double> %c, i32 1
    %c2 = extractelement <3 x double> %c, i32 2

    %r0 = extractelement <2 x double> %r, i32 0
    %r1 = extractelement <2 x double> %r, i32 1

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c0, %r1
    store double %4, double* %m10
    %5 = fmul double %c1, %r1
    store double %5, double* %m11
    %6 = fmul double %c2, %r1
    store double %6, double* %m12
    %7 = load [2 x <3 x double>], [2 x <3 x double>]* %m

    ret [2 x <3 x double>] %7
}

; GLSL: dmat3x2 = outerProduct(dvec2, dvec3)
define spir_func [3 x <2 x double>] @_Z12OuterProductDv2_dDv3_d(
    <2 x double> %c, <3 x double> %r) #0
{
    %m = alloca [3 x <2 x double>]
    %m0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x double>, <2 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x double>, <2 x double>* %m2, i32 0, i32 1

    %c0 = extractelement <2 x double> %c, i32 0
    %c1 = extractelement <2 x double> %c, i32 1

    %r0 = extractelement <3 x double> %r, i32 0
    %r1 = extractelement <3 x double> %r, i32 1
    %r2 = extractelement <3 x double> %r, i32 2

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c0, %r1
    store double %3, double* %m10
    %4 = fmul double %c1, %r1
    store double %4, double* %m11
    %5 = fmul double %c0, %r2
    store double %5, double* %m20
    %6 = fmul double %c1, %r2
    store double %6, double* %m21
    %7 = load [3 x <2 x double>], [3 x <2 x double>]* %m

    ret [3 x <2 x double>] %7
}

; GLSL: dmat2x4 = outerProduct(dvec4, dvec2)
define spir_func [2 x <4 x double>] @_Z12OuterProductDv4_dDv2_d(
    <4 x double> %c, <2 x double> %r) #0
{
    %m = alloca [2 x <4 x double>]
    %m0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 3

    %c0 = extractelement <4 x double> %c, i32 0
    %c1 = extractelement <4 x double> %c, i32 1
    %c2 = extractelement <4 x double> %c, i32 2
    %c3 = extractelement <4 x double> %c, i32 3

    %r0 = extractelement <2 x double> %r, i32 0
    %r1 = extractelement <2 x double> %r, i32 1

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c3, %r0
    store double %4, double* %m03
    %5 = fmul double %c0, %r1
    store double %5, double* %m10
    %6 = fmul double %c1, %r1
    store double %6, double* %m11
    %7 = fmul double %c2, %r1
    store double %7, double* %m12
    %8 = fmul double %c3, %r1
    store double %8, double* %m13
    %9 = load [2 x <4 x double>], [2 x <4 x double>]* %m

    ret [2 x <4 x double>] %9
}

; GLSL: dmat4x2 = outerProduct(dvec2, dvec4)
define spir_func [4 x <2 x double>] @_Z12OuterProductDv2_dDv4_d(
    <2 x double> %c, <4 x double> %r) #0
{
    %m = alloca [4 x <2 x double>]
    %m0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x double>, <2 x double>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x double>, <2 x double>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x double>, <2 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x double>, <2 x double>* %m2, i32 0, i32 1

    %m3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <2 x double>, <2 x double>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <2 x double>, <2 x double>* %m3, i32 0, i32 1

    %c0 = extractelement <2 x double> %c, i32 0
    %c1 = extractelement <2 x double> %c, i32 1

    %r0 = extractelement <4 x double> %r, i32 0
    %r1 = extractelement <4 x double> %r, i32 1
    %r2 = extractelement <4 x double> %r, i32 2
    %r3 = extractelement <4 x double> %r, i32 3

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c0, %r1
    store double %3, double* %m10
    %4 = fmul double %c1, %r1
    store double %4, double* %m11
    %5 = fmul double %c0, %r2
    store double %5, double* %m20
    %6 = fmul double %c1, %r2
    store double %6, double* %m21
    %7 = fmul double %c0, %r3
    store double %7, double* %m30
    %8 = fmul double %c1, %r3
    store double %8, double* %m31
    %9 = load [4 x <2 x double>], [4 x <2 x double>]* %m

    ret [4 x <2 x double>] %9
}

; GLSL: dmat3x4 = outerProduct(dvec4, dvec3)
define spir_func [3 x <4 x double>] @_Z12OuterProductDv4_dDv3_d(
    <4 x double> %c, <3 x double> %r) #0
{
    %m = alloca [3 x <4 x double>]
    %m0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x double>, <4 x double>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x double>, <4 x double>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x double>, <4 x double>* %m2, i32 0, i32 3

    %c0 = extractelement <4 x double> %c, i32 0
    %c1 = extractelement <4 x double> %c, i32 1
    %c2 = extractelement <4 x double> %c, i32 2
    %c3 = extractelement <4 x double> %c, i32 3

    %r0 = extractelement <3 x double> %r, i32 0
    %r1 = extractelement <3 x double> %r, i32 1
    %r2 = extractelement <3 x double> %r, i32 2

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c3, %r0
    store double %4, double* %m03
    %5 = fmul double %c0, %r1
    store double %5, double* %m10
    %6 = fmul double %c1, %r1
    store double %6, double* %m11
    %7 = fmul double %c2, %r1
    store double %7, double* %m12
    %8 = fmul double %c3, %r1
    store double %8, double* %m13
    %9 = fmul double %c0, %r2
    store double %9, double* %m20
    %10 = fmul double %c1, %r2
    store double %10, double* %m21
    %11 = fmul double %c2, %r2
    store double %11, double* %m22
    %12 = fmul double %c3, %r2
    store double %12, double* %m23
    %13 = load [3 x <4 x double>], [3 x <4 x double>]* %m

    ret [3 x <4 x double>] %13
}

; GLSL: dmat4x3 = outerProduct(dvec3, dvec4)
define spir_func [4 x <3 x double>] @_Z12OuterProductDv3_dDv4_d(
    <3 x double> %c, <4 x double> %r) #0
{
    %m = alloca [4 x <3 x double>]
    %m0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x double>, <3 x double>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x double>, <3 x double>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x double>, <3 x double>* %m2, i32 0, i32 2

    %m3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <3 x double>, <3 x double>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <3 x double>, <3 x double>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <3 x double>, <3 x double>* %m3, i32 0, i32 2

    %c0 = extractelement <3 x double> %c, i32 0
    %c1 = extractelement <3 x double> %c, i32 1
    %c2 = extractelement <3 x double> %c, i32 2

    %r0 = extractelement <4 x double> %r, i32 0
    %r1 = extractelement <4 x double> %r, i32 1
    %r2 = extractelement <4 x double> %r, i32 2
    %r3 = extractelement <4 x double> %r, i32 3

    %1 = fmul double %c0, %r0
    store double %1, double* %m00
    %2 = fmul double %c1, %r0
    store double %2, double* %m01
    %3 = fmul double %c2, %r0
    store double %3, double* %m02
    %4 = fmul double %c0, %r1
    store double %4, double* %m10
    %5 = fmul double %c1, %r1
    store double %5, double* %m11
    %6 = fmul double %c2, %r1
    store double %6, double* %m12
    %7 = fmul double %c0, %r2
    store double %7, double* %m20
    %8 = fmul double %c1, %r2
    store double %8, double* %m21
    %9 = fmul double %c2, %r2
    store double %9, double* %m22
    %10 = fmul double %c0, %r3
    store double %10, double* %m30
    %11 = fmul double %c1, %r3
    store double %11, double* %m31
    %12 = fmul double %c2, %r3
    store double %12, double* %m32
    %13 = load [4 x <3 x double>], [4 x <3 x double>]* %m

    ret [4 x <3 x double>] %13
}

; GLSL: dmat2 = transpose(dmat2)
define spir_func [2 x <2 x double>] @_Z9TransposeDv2_Dv2_d(
    [2 x <2 x double>] %m) #0
{
    %nm = alloca [2 x <2 x double>]
    %nm0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    %nmv = load [2 x <2 x double>], [2 x <2 x double>]* %nm
    ret [2 x <2 x double>] %nmv
}

; GLSL: dmat3 = transpose(dmat3)
define spir_func [3 x <3 x double>] @_Z9TransposeDv3_Dv3_d(
    [3 x <3 x double>] %m) #0
{
    %nm = alloca [3 x <3 x double>]
    %nm0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x double>] %m, 2
    %m2v0 = extractelement <3 x double> %m2v, i32 0
    %m2v1 = extractelement <3 x double> %m2v, i32 1
    %m2v2 = extractelement <3 x double> %m2v, i32 2

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    store double %m2v2, double* %nm22
    %nmv = load [3 x <3 x double>], [3 x <3 x double>]* %nm
    ret [3 x <3 x double>] %nmv
}

; GLSL: dmat4 = transpose(dmat4)
define spir_func [4 x <4 x double>] @_Z9TransposeDv4_Dv4_d(
    [4 x <4 x double>] %m) #0
{
    %nm = alloca [4 x <4 x double>]
    %nm0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m2v0 = extractelement <4 x double> %m2v, i32 0
    %m2v1 = extractelement <4 x double> %m2v, i32 1
    %m2v2 = extractelement <4 x double> %m2v, i32 2
    %m2v3 = extractelement <4 x double> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x double>] %m, 3
    %m3v0 = extractelement <4 x double> %m3v, i32 0
    %m3v1 = extractelement <4 x double> %m3v, i32 1
    %m3v2 = extractelement <4 x double> %m3v, i32 2
    %m3v3 = extractelement <4 x double> %m3v, i32 3

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m3v0, double* %nm03
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    store double %m3v1, double* %nm13
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    store double %m2v2, double* %nm22
    store double %m3v2, double* %nm23
    store double %m0v3, double* %nm30
    store double %m1v3, double* %nm31
    store double %m2v3, double* %nm32
    store double %m3v3, double* %nm33
    %nmv = load [4 x <4 x double>], [4 x <4 x double>]* %nm
    ret [4 x <4 x double>] %nmv
}

; GLSL: dmat2x3 = transpose(dmat3x2)
define spir_func [2 x <3 x double>] @_Z9TransposeDv3_Dv2_d(
    [3 x <2 x double>] %m) #0
{
    %nm = alloca [2 x <3 x double>]
    %nm0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x double>] %m, 2
    %m2v0 = extractelement <2 x double> %m2v, i32 0
    %m2v1 = extractelement <2 x double> %m2v, i32 1

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    %nmv = load [2 x <3 x double>], [2 x <3 x double>]* %nm
    ret [2 x <3 x double>] %nmv
}

; GLSL: dmat3x2 = transpose(dmat2x3)
define spir_func [3 x <2 x double>] @_Z9TransposeDv2_Dv3_d(
    [2 x <3 x double>] %m) #0
{
    %nm = alloca [3 x <2 x double>]
    %nm0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    %nmv = load [3 x <2 x double>], [3 x <2 x double>]* %nm
    ret [3 x <2 x double>] %nmv
}

; GLSL: dmat2x4 = transpose(dmat4x2)
define spir_func [2 x <4 x double>] @_Z9TransposeDv4_Dv2_d(
    [4 x <2 x double>] %m) #0
{
    %nm = alloca [2 x <4 x double>]
    %nm0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m2v0 = extractelement <2 x double> %m2v, i32 0
    %m2v1 = extractelement <2 x double> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x double>] %m, 3
    %m3v0 = extractelement <2 x double> %m3v, i32 0
    %m3v1 = extractelement <2 x double> %m3v, i32 1

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m3v0, double* %nm03
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    store double %m3v1, double* %nm13
    %nmv = load [2 x <4 x double>], [2 x <4 x double>]* %nm
    ret [2 x <4 x double>] %nmv
}

; GLSL: dmat4x2 = transpose(dmat2x4)
define spir_func [4 x <2 x double>] @_Z9TransposeDv2_Dv4_d(
    [2 x <4 x double>] %m) #0
{
    %nm = alloca [4 x <2 x double>]
    %nm0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x double>, <2 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x double>, <2 x double>* %nm3, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    store double %m0v3, double* %nm30
    store double %m1v3, double* %nm31
    %nmv = load [4 x <2 x double>], [4 x <2 x double>]* %nm
    ret [4 x <2 x double>] %nmv
}

; GLSL: dmat3x4 = transpose(dmat4x3)
define spir_func [3 x <4 x double>] @_Z9TransposeDv4_Dv3_d(
    [4 x <3 x double>] %m) #0
{
    %nm = alloca [3 x <4 x double>]
    %nm0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m2v0 = extractelement <3 x double> %m2v, i32 0
    %m2v1 = extractelement <3 x double> %m2v, i32 1
    %m2v2 = extractelement <3 x double> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x double>] %m, 3
    %m3v0 = extractelement <3 x double> %m3v, i32 0
    %m3v1 = extractelement <3 x double> %m3v, i32 1
    %m3v2 = extractelement <3 x double> %m3v, i32 2

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m3v0, double* %nm03
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    store double %m3v1, double* %nm13
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    store double %m2v2, double* %nm22
    store double %m3v2, double* %nm23
    %nmv = load [3 x <4 x double>], [3 x <4 x double>]* %nm
    ret [3 x <4 x double>] %nmv
}

; GLSL: dmat4x3 = transpose(dmat3x4)
define spir_func [4 x <3 x double>] @_Z9TransposeDv3_Dv4_d(
    [3 x <4 x double>] %m) #0
{
    %nm = alloca [4 x <3 x double>]
    %nm0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x double>] %m, 2
    %m2v0 = extractelement <4 x double> %m2v, i32 0
    %m2v1 = extractelement <4 x double> %m2v, i32 1
    %m2v2 = extractelement <4 x double> %m2v, i32 2
    %m2v3 = extractelement <4 x double> %m2v, i32 3

    store double %m0v0, double* %nm00
    store double %m1v0, double* %nm01
    store double %m2v0, double* %nm02
    store double %m0v1, double* %nm10
    store double %m1v1, double* %nm11
    store double %m2v1, double* %nm12
    store double %m0v2, double* %nm20
    store double %m1v2, double* %nm21
    store double %m2v2, double* %nm22
    store double %m0v3, double* %nm30
    store double %m1v3, double* %nm31
    store double %m2v3, double* %nm32
    %nmv = load [4 x <3 x double>], [4 x <3 x double>]* %nm
    ret [4 x <3 x double>] %nmv
}

; GLSL: dmat2 = dmat2 * double
define spir_func [2 x <2 x double>] @_Z17MatrixTimesScalarDv2_Dv2_dd(
    [2 x <2 x double>] %m, double %s) #0
{
    %nm = alloca [2 x <2 x double>]
    %nm0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m1v0, %s
    store double %3, double* %nm10
    %4 = fmul double %m1v1, %s
    store double %4, double* %nm11
    %5 = load [2 x <2 x double>], [2 x <2 x double>]* %nm

    ret [2 x <2 x double>] %5
}

; GLSL: dmat3 = dmat3 * double
define spir_func [3 x <3 x double>] @_Z17MatrixTimesScalarDv3_Dv3_dd(
    [3 x <3 x double>] %m, double %s) #0
{
    %nm = alloca [3 x <3 x double>]
    %nm0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x double>] %m, 2
    %m2v0 = extractelement <3 x double> %m2v, i32 0
    %m2v1 = extractelement <3 x double> %m2v, i32 1
    %m2v2 = extractelement <3 x double> %m2v, i32 2

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m1v0, %s
    store double %4, double* %nm10
    %5 = fmul double %m1v1, %s
    store double %5, double* %nm11
    %6 = fmul double %m1v2, %s
    store double %6, double* %nm12
    %7 = fmul double %m2v0, %s
    store double %7, double* %nm20
    %8 = fmul double %m2v1, %s
    store double %8, double* %nm21
    %9 = fmul double %m2v2, %s
    store double %9, double* %nm22
    %10 = load [3 x <3 x double>], [3 x <3 x double>]* %nm

    ret [3 x <3 x double>] %10
}

; GLSL: dmat4 = dmat4 * double
define spir_func [4 x <4 x double>] @_Z17MatrixTimesScalarDv4_Dv4_dd(
    [4 x <4 x double>] %m, double %s) #0
{
    %nm = alloca [4 x <4 x double>]
    %nm0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x double>, <4 x double>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m2v0 = extractelement <4 x double> %m2v, i32 0
    %m2v1 = extractelement <4 x double> %m2v, i32 1
    %m2v2 = extractelement <4 x double> %m2v, i32 2
    %m2v3 = extractelement <4 x double> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x double>] %m, 3
    %m3v0 = extractelement <4 x double> %m3v, i32 0
    %m3v1 = extractelement <4 x double> %m3v, i32 1
    %m3v2 = extractelement <4 x double> %m3v, i32 2
    %m3v3 = extractelement <4 x double> %m3v, i32 3

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m0v3, %s
    store double %4, double* %nm03
    %5 = fmul double %m1v0, %s
    store double %5, double* %nm10
    %6 = fmul double %m1v1, %s
    store double %6, double* %nm11
    %7 = fmul double %m1v2, %s
    store double %7, double* %nm12
    %8 = fmul double %m1v3, %s
    store double %8, double* %nm13
    %9 = fmul double %m2v0, %s
    store double %9, double* %nm20
    %10 = fmul double %m2v1, %s
    store double %10, double* %nm21
    %11 = fmul double %m2v2, %s
    store double %11, double* %nm22
    %12 = fmul double %m2v3, %s
    store double %12, double* %nm23
    %13 = fmul double %m3v0, %s
    store double %13, double* %nm30
    %14 = fmul double %m3v1, %s
    store double %14, double* %nm31
    %15 = fmul double %m3v2, %s
    store double %15, double* %nm32
    %16 = fmul double %m3v3, %s
    store double %16, double* %nm33
    %17 = load [4 x <4 x double>], [4 x <4 x double>]* %nm

    ret [4 x <4 x double>] %17
}

; GLSL: dmat3x2 = dmat3x2 * double
define spir_func [3 x <2 x double>] @_Z17MatrixTimesScalarDv3_Dv2_dd(
    [3 x <2 x double>] %m, double %s) #0
{
    %nm = alloca [3 x <2 x double>]
    %nm0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 1

    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x double>] %m, 2
    %m2v0 = extractelement <2 x double> %m2v, i32 0
    %m2v1 = extractelement <2 x double> %m2v, i32 1

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m1v0, %s
    store double %3, double* %nm10
    %4 = fmul double %m1v1, %s
    store double %4, double* %nm11
    %5 = fmul double %m2v0, %s
    store double %5, double* %nm20
    %6 = fmul double %m2v1, %s
    store double %6, double* %nm21
    %7 = load [3 x <2 x double>], [3 x <2 x double>]* %nm

    ret [3 x <2 x double>] %7
}

; GLSL: dmat2x3 = dmat2x3 * double
define spir_func [2 x <3 x double>] @_Z17MatrixTimesScalarDv2_Dv3_dd(
    [2 x <3 x double>] %m, double %s) #0
{
    %nm = alloca [2 x <3 x double>]
    %nm0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m1v0, %s
    store double %4, double* %nm10
    %5 = fmul double %m1v1, %s
    store double %5, double* %nm11
    %6 = fmul double %m1v2, %s
    store double %6, double* %nm12
    %7 = load [2 x <3 x double>], [2 x <3 x double>]* %nm

    ret [2 x <3 x double>] %7
}

; GLSL: dmat4x2 = dmat4x2 * double
define spir_func [4 x <2 x double>] @_Z17MatrixTimesScalarDv4_Dv2_dd(
    [4 x <2 x double>] %m, double %s) #0
{
    %nm = alloca [4 x <2 x double>]
    %nm0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x double>, <2 x double>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x double>, <2 x double>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x double>, <2 x double>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x double>, <2 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x double>, <2 x double>* %nm3, i32 0, i32 1

    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m2v0 = extractelement <2 x double> %m2v, i32 0
    %m2v1 = extractelement <2 x double> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x double>] %m, 3
    %m3v0 = extractelement <2 x double> %m3v, i32 0
    %m3v1 = extractelement <2 x double> %m3v, i32 1

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m1v0, %s
    store double %3, double* %nm10
    %4 = fmul double %m1v1, %s
    store double %4, double* %nm11
    %5 = fmul double %m2v0, %s
    store double %5, double* %nm20
    %6 = fmul double %m2v1, %s
    store double %6, double* %nm21
    %7 = fmul double %m3v0, %s
    store double %7, double* %nm30
    %8 = fmul double %m3v1, %s
    store double %8, double* %nm31
    %9 = load [4 x <2 x double>], [4 x <2 x double>]* %nm

    ret [4 x <2 x double>] %9
}

; GLSL: dmat2x4 = dmat2x4 * double
define spir_func [2 x <4 x double>] @_Z17MatrixTimesScalarDv2_Dv4_dd(
    [2 x <4 x double>] %m, double %s) #0
{
    %nm = alloca [2 x <4 x double>]
    %nm0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m0v3, %s
    store double %4, double* %nm03
    %5 = fmul double %m1v0, %s
    store double %5, double* %nm10
    %6 = fmul double %m1v1, %s
    store double %6, double* %nm11
    %7 = fmul double %m1v2, %s
    store double %7, double* %nm12
    %8 = fmul double %m1v3, %s
    store double %8, double* %nm13
    %9 = load [2 x <4 x double>], [2 x <4 x double>]* %nm

    ret [2 x <4 x double>] %9
}

; GLSL: dmat4x3 = dmat4x3 * double
define spir_func [4 x <3 x double>] @_Z17MatrixTimesScalarDv4_Dv3_dd(
    [4 x <3 x double>] %m, double %s) #0
{
    %nm = alloca [4 x <3 x double>]
    %nm0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x double>, <3 x double>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x double>, <3 x double>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x double>, <3 x double>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x double>, <3 x double>* %nm3, i32 0, i32 2

    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m2v0 = extractelement <3 x double> %m2v, i32 0
    %m2v1 = extractelement <3 x double> %m2v, i32 1
    %m2v2 = extractelement <3 x double> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x double>] %m, 3
    %m3v0 = extractelement <3 x double> %m3v, i32 0
    %m3v1 = extractelement <3 x double> %m3v, i32 1
    %m3v2 = extractelement <3 x double> %m3v, i32 2

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m1v0, %s
    store double %4, double* %nm10
    %5 = fmul double %m1v1, %s
    store double %5, double* %nm11
    %6 = fmul double %m1v2, %s
    store double %6, double* %nm12
    %7 = fmul double %m2v0, %s
    store double %7, double* %nm20
    %8 = fmul double %m2v1, %s
    store double %8, double* %nm21
    %9 = fmul double %m2v2, %s
    store double %9, double* %nm22
    %10 = fmul double %m3v0, %s
    store double %10, double* %nm30
    %11 = fmul double %m3v1, %s
    store double %11, double* %nm31
    %12 = fmul double %m3v2, %s
    store double %12, double* %nm32
    %13 = load [4 x <3 x double>], [4 x <3 x double>]* %nm

    ret [4 x <3 x double>] %13
}

; GLSL: dmat3x4 = dmat3x4 * double
define spir_func [3 x <4 x double>] @_Z17MatrixTimesScalarDv3_Dv4_dd(
    [3 x <4 x double>] %m, double %s) #0
{
    %nm = alloca [3 x <4 x double>]
    %nm0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x double>, <4 x double>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x double>, <4 x double>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x double>, <4 x double>* %nm2, i32 0, i32 3

    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x double>] %m, 2
    %m2v0 = extractelement <4 x double> %m2v, i32 0
    %m2v1 = extractelement <4 x double> %m2v, i32 1
    %m2v2 = extractelement <4 x double> %m2v, i32 2
    %m2v3 = extractelement <4 x double> %m2v, i32 3

    %1 = fmul double %m0v0, %s
    store double %1, double* %nm00
    %2 = fmul double %m0v1, %s
    store double %2, double* %nm01
    %3 = fmul double %m0v2, %s
    store double %3, double* %nm02
    %4 = fmul double %m0v3, %s
    store double %4, double* %nm03
    %5 = fmul double %m1v0, %s
    store double %5, double* %nm10
    %6 = fmul double %m1v1, %s
    store double %6, double* %nm11
    %7 = fmul double %m1v2, %s
    store double %7, double* %nm12
    %8 = fmul double %m1v3, %s
    store double %8, double* %nm13
    %9 = fmul double %m2v0, %s
    store double %9, double* %nm20
    %10 = fmul double %m2v1, %s
    store double %10, double* %nm21
    %11 = fmul double %m2v2, %s
    store double %11, double* %nm22
    %12 = fmul double %m2v3, %s
    store double %12, double* %nm23
    %13 = load [3 x <4 x double>], [3 x <4 x double>]* %nm

    ret [3 x <4 x double>] %13
}

; GLSL: dvec2 = dvec2 * dmat2
define spir_func <2 x double> @_Z17VectorTimesMatrixDv2_dDv2_Dv2_d(
    <2 x double> %c, [2 x <2 x double>] %m) #0
{
    %nv = alloca <2 x double>
    %nvp0 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %1 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m0v, <2 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [2 x <2 x double>] %m, 1
    %2 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m1v, <2 x double> %c)
    store double %2, double* %nvp1

    %3 = load <2 x double>, <2 x double>* %nv

    ret <2 x double> %3
}

; GLSL: dvec3 = dvec3 * dmat3
define spir_func <3 x double> @_Z17VectorTimesMatrixDv3_dDv3_Dv3_d(
    <3 x double> %c, [3 x <3 x double>] %m) #0
{
    %nv = alloca <3 x double>
    %nvp0 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %1 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m0v, <3 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %2 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m1v, <3 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [3 x <3 x double>] %m, 2
    %3 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m2v, <3 x double> %c)
    store double %3, double* %nvp2

    %4 = load <3 x double>, <3 x double>* %nv

    ret <3 x double> %4
}

; GLSL: dvec4 = dvec4 * dmat4
define spir_func <4 x double> @_Z17VectorTimesMatrixDv4_dDv4_Dv4_d(
    <4 x double> %c, [4 x <4 x double>] %m) #0
{
    %nv = alloca <4 x double>
    %nvp0 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %1 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m0v, <4 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %2 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m1v, <4 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %3 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m2v, <4 x double> %c)
    store double %3, double* %nvp2
    %m3v = extractvalue [4 x <4 x double>] %m, 3
    %4 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m3v, <4 x double> %c)
    store double %4, double* %nvp3

    %5 = load <4 x double>, <4 x double>* %nv

    ret <4 x double> %5
}

; GLSL: dvec3 = dvec2 * dmat3x2
define spir_func <3 x double> @_Z17VectorTimesMatrixDv2_dDv3_Dv2_d(
    <2 x double> %c, [3 x <2 x double>] %m) #0
{
    %nv = alloca <3 x double>
    %nvp0 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %1 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m0v, <2 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %2 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m1v, <2 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [3 x <2 x double>] %m, 2
    %3 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m2v, <2 x double> %c)
    store double %3, double* %nvp2

    %4 = load <3 x double>, <3 x double>* %nv

    ret <3 x double> %4
}

; GLSL: dvec4 = dvec2 * dmat4x2
define spir_func <4 x double> @_Z17VectorTimesMatrixDv2_dDv4_Dv2_d(
    <2 x double> %c, [4 x <2 x double>] %m) #0
{
    %nv = alloca <4 x double>
    %nvp0 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %1 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m0v, <2 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %2 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m1v, <2 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %3 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m2v, <2 x double> %c)
    store double %3, double* %nvp2
    %m3v = extractvalue [4 x <2 x double>] %m, 3
    %4 = call double @_Z3dotDv2_dDv2_d(<2 x double> %m3v, <2 x double> %c)
    store double %4, double* %nvp3

    %5 = load <4 x double>, <4 x double>* %nv

    ret <4 x double> %5
}

; GLSL: dvec2 = dvec3 * dmat2x3
define spir_func <2 x double> @_Z17VectorTimesMatrixDv3_dDv2_Dv3_d(
    <3 x double> %c, [2 x <3 x double>] %m) #0
{
    %nv = alloca <2 x double>
    %nvp0 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %1 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m0v, <3 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [2 x <3 x double>] %m, 1
    %2 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m1v, <3 x double> %c)
    store double %2, double* %nvp1

    %3 = load <2 x double>, <2 x double>* %nv

    ret <2 x double> %3
}

; GLSL: dvec4 = dvec3 * dmat4x3
define spir_func <4 x double> @_Z17VectorTimesMatrixDv3_dDv4_Dv3_d(
    <3 x double> %c, [4 x <3 x double>] %m) #0
{
    %nv = alloca <4 x double>
    %nvp0 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x double>, <4 x double>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %1 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m0v, <3 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %2 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m1v, <3 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %3 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m2v, <3 x double> %c)
    store double %3, double* %nvp2
    %m3v = extractvalue [4 x <3 x double>] %m, 3
    %4 = call double @_Z3dotDv3_dDv3_d(<3 x double> %m3v, <3 x double> %c)
    store double %4, double* %nvp3

    %5 = load <4 x double>, <4 x double>* %nv

    ret <4 x double> %5
}

; GLSL: dvec2 = dvec4 * dmat2x4
define spir_func <2 x double> @_Z17VectorTimesMatrixDv4_dDv2_Dv4_d(
    <4 x double> %c, [2 x <4 x double>] %m) #0
{
    %nv = alloca <2 x double>
    %nvp0 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x double>, <2 x double>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %1 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m0v, <4 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [2 x <4 x double>] %m, 1
    %2 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m1v, <4 x double> %c)
    store double %2, double* %nvp1

    %3 = load <2 x double>, <2 x double>* %nv

    ret <2 x double> %3
}

; GLSL: dvec3 = dvec4 * dmat3x4
define spir_func <3 x double> @_Z17VectorTimesMatrixDv4_dDv3_Dv4_d(
    <4 x double> %c, [3 x <4 x double>] %m) #0
{
    %nv = alloca <3 x double>
    %nvp0 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x double>, <3 x double>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %1 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m0v, <4 x double> %c)
    store double %1, double* %nvp0
    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %2 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m1v, <4 x double> %c)
    store double %2, double* %nvp1
    %m2v = extractvalue [3 x <4 x double>] %m, 2
    %3 = call double @_Z3dotDv4_dDv4_d(<4 x double> %m2v, <4 x double> %c)
    store double %3, double* %nvp2

    %4 = load <3 x double>, <3 x double>* %nv

    ret <3 x double> %4
}

; GLSL: dvec2 = dmat2 * dvec2
define spir_func <2 x double> @_Z17MatrixTimesVectorDv2_Dv2_dDv2_d(
    [2 x <2 x double>] %m, <2 x double> %r) #0
{
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m1v = extractvalue [2 x <2 x double>] %m, 1

    %r0 = shufflevector <2 x double> %r, <2 x double> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %r0
    %r1 = shufflevector <2 x double> %r, <2 x double> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %r1
    %3 = fadd <2 x double> %2, %1

    ret <2 x double> %3
}

; GLSL: dvec3 = dmat3 * dvec3
define spir_func <3 x double> @_Z17MatrixTimesVectorDv3_Dv3_dDv3_d(
    [3 x <3 x double>] %m, <3 x double> %r) #0
{
    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m2v = extractvalue [3 x <3 x double>] %m, 2

    %r0 = shufflevector <3 x double> %r, <3 x double> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %r0
    %r1 = shufflevector <3 x double> %r, <3 x double> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %r1
    %3 = fadd <3 x double> %2, %1
    %r2 = shufflevector <3 x double> %r, <3 x double> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %r2
    %5 = fadd <3 x double> %4, %3

    ret <3 x double> %5
}

; GLSL: dvec4 = dmat4 * dvec4
define spir_func <4 x double> @_Z17MatrixTimesVectorDv4_Dv4_dDv4_d(
    [4 x <4 x double>] %m, <4 x double> %r) #0
{
    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m3v = extractvalue [4 x <4 x double>] %m, 3

    %r0 = shufflevector <4 x double> %r, <4 x double> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %r0
    %r1 = shufflevector <4 x double> %r, <4 x double> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %r1
    %3 = fadd <4 x double> %2, %1
    %r2 = shufflevector <4 x double> %r, <4 x double> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %r2
    %5 = fadd <4 x double> %4, %3
    %r3 = shufflevector <4 x double> %r, <4 x double> %r, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x double> %m3v, %r3
    %7 = fadd <4 x double> %6, %5

    ret <4 x double> %7
}

; GLSL: dvec2 = dmat3x2 * dvec3
define spir_func <2 x double> @_Z17MatrixTimesVectorDv3_Dv2_dDv3_d(
    [3 x <2 x double>] %m, <3 x double> %r) #0
{
    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m2v = extractvalue [3 x <2 x double>] %m, 2

    %r0 = shufflevector <3 x double> %r, <3 x double> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %r0
    %r1 = shufflevector <3 x double> %r, <3 x double> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %r1
    %3 = fadd <2 x double> %2, %1
    %r2 = shufflevector <3 x double> %r, <3 x double> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %r2
    %5 = fadd <2 x double> %4, %3

    ret <2 x double> %5
}

; GLSL: dvec3 = dmat2x3 * dvec2
define spir_func <3 x double> @_Z17MatrixTimesVectorDv2_Dv3_dDv2_d(
    [2 x <3 x double>] %m, <2 x double> %r) #0
{
    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m1v = extractvalue [2 x <3 x double>] %m, 1

    %r0 = shufflevector <2 x double> %r, <2 x double> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %r0
    %r1 = shufflevector <2 x double> %r, <2 x double> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %r1
    %3 = fadd <3 x double> %2, %1

    ret <3 x double> %3
}

; GLSL: dvec2 = dmat4x2 * dvec4
define spir_func <2 x double> @_Z17MatrixTimesVectorDv4_Dv2_dDv4_d(
    [4 x <2 x double>] %m, <4 x double> %r) #0
{
    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m3v = extractvalue [4 x <2 x double>] %m, 3

    %r0 = shufflevector <4 x double> %r, <4 x double> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %r0
    %r1 = shufflevector <4 x double> %r, <4 x double> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %r1
    %3 = fadd <2 x double> %2, %1
    %r2 = shufflevector <4 x double> %r, <4 x double> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %r2
    %5 = fadd <2 x double> %4, %3
    %r3 = shufflevector <4 x double> %r, <4 x double> %r, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x double> %m3v, %r3
    %7 = fadd <2 x double> %6, %5

    ret <2 x double> %7
}

; GLSL: dvec4 = dmat2x4 * dvec2
define spir_func <4 x double> @_Z17MatrixTimesVectorDv2_Dv4_dDv2_d(
    [2 x <4 x double>] %m, <2 x double> %r) #0
{
    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m1v = extractvalue [2 x <4 x double>] %m, 1

    %r0 = shufflevector <2 x double> %r, <2 x double> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %r0
    %r1 = shufflevector <2 x double> %r, <2 x double> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %r1
    %3 = fadd <4 x double> %2, %1

    ret <4 x double> %3
}

; GLSL: dvec3 = dmat4x3 * dvec4
define spir_func <3 x double> @_Z17MatrixTimesVectorDv4_Dv3_dDv4_d(
    [4 x <3 x double>] %m, <4 x double> %r) #0
{
    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m3v = extractvalue [4 x <3 x double>] %m, 3

    %r0 = shufflevector <4 x double> %r, <4 x double> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %r0
    %r1 = shufflevector <4 x double> %r, <4 x double> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %r1
    %3 = fadd <3 x double> %2, %1
    %r2 = shufflevector <4 x double> %r, <4 x double> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %r2
    %5 = fadd <3 x double> %4, %3
    %r3 = shufflevector <4 x double> %r, <4 x double> %r, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x double> %m3v, %r3
    %7 = fadd <3 x double> %6, %5

    ret <3 x double> %7
}

; GLSL: dvec4 = dmat3x4 * dvec3
define spir_func <4 x double> @_Z17MatrixTimesVectorDv3_Dv4_dDv3_d(
    [3 x <4 x double>] %m, <3 x double> %r) #0
{
    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m2v = extractvalue [3 x <4 x double>] %m, 2

    %r0 = shufflevector <3 x double> %r, <3 x double> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %r0
    %r1 = shufflevector <3 x double> %r, <3 x double> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %r1
    %3 = fadd <4 x double> %2, %1
    %r2 = shufflevector <3 x double> %r, <3 x double> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %r2
    %5 = fadd <4 x double> %4, %3

    ret <4 x double> %5
}

; GLSL: dmat2 = dmat2 * dmat2
define spir_func [2 x <2 x double>] @_Z17MatrixTimesMatrixDv2_Dv2_dDv2_Dv2_d(
    [2 x <2 x double>] %m, [2 x <2 x double>] %rm) #0
{
    %nm = alloca [2 x <2 x double>]
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m1v = extractvalue [2 x <2 x double>] %m, 1

    %rm0v = extractvalue [2 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %nm0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %3, <2 x double>* %nm0

    %rm1v = extractvalue [2 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x double> %m1v, %rm1v1
    %6 = fadd <2 x double> %5, %4

    %nm1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %6, <2 x double>* %nm1

    %7 = load [2 x <2 x double>], [2 x <2 x double>]* %nm

    ret [2 x <2 x double>] %7
}

; GLSL: dmat3x2 = dmat2 * dmat3x2
define spir_func [3 x <2 x double>] @_Z17MatrixTimesMatrixDv2_Dv2_dDv3_Dv2_d(
    [2 x <2 x double>] %m, [3 x <2 x double>] %rm) #0
{
    %nm = alloca [3 x <2 x double>]
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m1v = extractvalue [2 x <2 x double>] %m, 1

    %rm0v = extractvalue [3 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %nm0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %3, <2 x double>* %nm0

    %rm1v = extractvalue [3 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x double> %m1v, %rm1v1
    %6 = fadd <2 x double> %5, %4

    %nm1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %6, <2 x double>* %nm1

    %rm2v = extractvalue [3 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x double> %m1v, %rm2v1
    %9 = fadd <2 x double> %8, %7

    %nm2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %9, <2 x double>* %nm2

    %10 = load [3 x <2 x double>], [3 x <2 x double>]* %nm

    ret [3 x <2 x double>] %10
}

; GLSL: dmat4x2 = dmat2 * dmat4x2
define spir_func [4 x <2 x double>] @_Z17MatrixTimesMatrixDv2_Dv2_dDv4_Dv2_d(
    [2 x <2 x double>] %m, [4 x <2 x double>] %rm) #0
{
    %nm = alloca [4 x <2 x double>]
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m1v = extractvalue [2 x <2 x double>] %m, 1

    %rm0v = extractvalue [4 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %nm0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %3, <2 x double>* %nm0

    %rm1v = extractvalue [4 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x double> %m1v, %rm1v1
    %6 = fadd <2 x double> %5, %4

    %nm1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %6, <2 x double>* %nm1

    %rm2v = extractvalue [4 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x double> %m1v, %rm2v1
    %9 = fadd <2 x double> %8, %7

    %nm2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %9, <2 x double>* %nm2

    %rm3v = extractvalue [4 x <2 x double>] %rm, 3
    %rm3v0 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <2 x i32> <i32 0, i32 0>
    %10 = fmul <2 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <2 x i32> <i32 1, i32 1>
    %11 = fmul <2 x double> %m1v, %rm3v1
    %12 = fadd <2 x double> %11, %10

    %nm3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 3
    store <2 x double> %12, <2 x double>* %nm3

    %13 = load [4 x <2 x double>], [4 x <2 x double>]* %nm

    ret [4 x <2 x double>] %13
}

; GLSL: dmat3 = dmat3 * dmat3
define spir_func [3 x <3 x double>] @_Z17MatrixTimesMatrixDv3_Dv3_dDv3_Dv3_d(
    [3 x <3 x double>] %m, [3 x <3 x double>] %rm) #0
{
    %nm = alloca [3 x <3 x double>]
    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m2v = extractvalue [3 x <3 x double>] %m, 2

    %rm0v = extractvalue [3 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %nm0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %5, <3 x double>* %nm0

    %rm1v = extractvalue [3 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x double> %m1v, %rm1v1
    %8 = fadd <3 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x double> %m2v, %rm1v2
    %10 = fadd <3 x double> %9, %8

    %nm1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %10, <3 x double>* %nm1

    %rm2v = extractvalue [3 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x double> %m1v, %rm2v1
    %13 = fadd <3 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x double> %m2v, %rm2v2
    %15 = fadd <3 x double> %14, %13

    %nm2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %15, <3 x double>* %nm2

    %16 = load [3 x <3 x double>], [3 x <3 x double>]* %nm

    ret [3 x <3 x double>] %16
}

; GLSL: dmat2x3 = dmat3 * dmat2x3
define spir_func [2 x <3 x double>] @_Z17MatrixTimesMatrixDv3_Dv3_dDv2_Dv3_d(
    [3 x <3 x double>] %m, [2 x <3 x double>] %rm) #0
{
    %nm = alloca [2 x <3 x double>]
    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m2v = extractvalue [3 x <3 x double>] %m, 2

    %rm0v = extractvalue [2 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %nm0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %5, <3 x double>* %nm0

    %rm1v = extractvalue [2 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x double> %m1v, %rm1v1
    %8 = fadd <3 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x double> %m2v, %rm1v2
    %10 = fadd <3 x double> %9, %8

    %nm1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %10, <3 x double>* %nm1

    %11 = load [2 x <3 x double>], [2 x <3 x double>]* %nm

    ret [2 x <3 x double>] %11
}

; GLSL: dmat4x3 = dmat3 * dmat4x3
define spir_func [4 x <3 x double>] @_Z17MatrixTimesMatrixDv3_Dv3_dDv4_Dv3_d(
    [3 x <3 x double>] %m, [4 x <3 x double>] %rm) #0
{
    %nm = alloca [4 x <3 x double>]
    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m2v = extractvalue [3 x <3 x double>] %m, 2

    %rm0v = extractvalue [4 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %nm0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %5, <3 x double>* %nm0

    %rm1v = extractvalue [4 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x double> %m1v, %rm1v1
    %8 = fadd <3 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x double> %m2v, %rm1v2
    %10 = fadd <3 x double> %9, %8

    %nm1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %10, <3 x double>* %nm1

    %rm2v = extractvalue [4 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x double> %m1v, %rm2v1
    %13 = fadd <3 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x double> %m2v, %rm2v2
    %15 = fadd <3 x double> %14, %13

    %nm2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %15, <3 x double>* %nm2

    %rm3v = extractvalue [4 x <3 x double>] %rm, 3
    %rm3v0 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %16 = fmul <3 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %17 = fmul <3 x double> %m1v, %rm3v1
    %18 = fadd <3 x double> %17, %16

    %rm3v2 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %19 = fmul <3 x double> %m2v, %rm3v2
    %20 = fadd <3 x double> %19, %18

    %nm3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 3
    store <3 x double> %20, <3 x double>* %nm3

    %21 = load [4 x <3 x double>], [4 x <3 x double>]* %nm

    ret [4 x <3 x double>] %21
}

; GLSL: dmat4 = dmat4 * dmat4
define spir_func [4 x <4 x double>] @_Z17MatrixTimesMatrixDv4_Dv4_dDv4_Dv4_d(
    [4 x <4 x double>] %m, [4 x <4 x double>] %rm) #0
{
    %nm = alloca [4 x <4 x double>]
    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m3v = extractvalue [4 x <4 x double>] %m, 3

    %rm0v = extractvalue [4 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x double> %m3v, %rm0v3
    %7 = fadd <4 x double> %6, %5

    %nm0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %7, <4 x double>* %nm0

    %rm1v = extractvalue [4 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x double> %m1v, %rm1v1
    %10 = fadd <4 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x double> %m2v, %rm1v2
    %12 = fadd <4 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x double> %m3v, %rm1v3
    %14 = fadd <4 x double> %13, %12

    %nm1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %14, <4 x double>* %nm1

    %rm2v = extractvalue [4 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x double> %m1v, %rm2v1
    %17 = fadd <4 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x double> %m2v, %rm2v2
    %19 = fadd <4 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x double> %m3v, %rm2v3
    %21 = fadd <4 x double> %20, %19

    %nm2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %21, <4 x double>* %nm2

    %rm3v = extractvalue [4 x <4 x double>] %rm, 3
    %rm3v0 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %22 = fmul <4 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %23 = fmul <4 x double> %m1v, %rm3v1
    %24 = fadd <4 x double> %23, %22

    %rm3v2 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %25 = fmul <4 x double> %m2v, %rm3v2
    %26 = fadd <4 x double> %25, %24

    %rm3v3 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %27 = fmul <4 x double> %m3v, %rm3v3
    %28 = fadd <4 x double> %27, %26

    %nm3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 3
    store <4 x double> %28, <4 x double>* %nm3

    %29 = load [4 x <4 x double>], [4 x <4 x double>]* %nm

    ret [4 x <4 x double>] %29
}

; GLSL: dmat2x4 = dmat4 * dmat2x4
define spir_func [2 x <4 x double>] @_Z17MatrixTimesMatrixDv4_Dv4_dDv2_Dv4_d(
    [4 x <4 x double>] %m, [2 x <4 x double>] %rm) #0
{
    %nm = alloca [2 x <4 x double>]
    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m3v = extractvalue [4 x <4 x double>] %m, 3

    %rm0v = extractvalue [2 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x double> %m3v, %rm0v3
    %7 = fadd <4 x double> %6, %5

    %nm0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %7, <4 x double>* %nm0

    %rm1v = extractvalue [2 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x double> %m1v, %rm1v1
    %10 = fadd <4 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x double> %m2v, %rm1v2
    %12 = fadd <4 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x double> %m3v, %rm1v3
    %14 = fadd <4 x double> %13, %12

    %nm1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %14, <4 x double>* %nm1

    %15 = load [2 x <4 x double>], [2 x <4 x double>]* %nm

    ret [2 x <4 x double>] %15
}

; GLSL: dmat3x4 = dmat4 * dmat3x4
define spir_func [3 x <4 x double>] @_Z17MatrixTimesMatrixDv4_Dv4_dDv3_Dv4_d(
    [4 x <4 x double>] %m, [3 x <4 x double>] %rm) #0
{
    %nm = alloca [3 x <4 x double>]
    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m3v = extractvalue [4 x <4 x double>] %m, 3

    %rm0v = extractvalue [3 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x double> %m3v, %rm0v3
    %7 = fadd <4 x double> %6, %5

    %nm0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %7, <4 x double>* %nm0

    %rm1v = extractvalue [3 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x double> %m1v, %rm1v1
    %10 = fadd <4 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x double> %m2v, %rm1v2
    %12 = fadd <4 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x double> %m3v, %rm1v3
    %14 = fadd <4 x double> %13, %12

    %nm1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %14, <4 x double>* %nm1

    %rm2v = extractvalue [3 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x double> %m1v, %rm2v1
    %17 = fadd <4 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x double> %m2v, %rm2v2
    %19 = fadd <4 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x double> %m3v, %rm2v3
    %21 = fadd <4 x double> %20, %19

    %nm2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %21, <4 x double>* %nm2

    %22 = load [3 x <4 x double>], [3 x <4 x double>]* %nm

    ret [3 x <4 x double>] %22
}

; GLSL: dmat2 = dmat3x2 * dmat2x3
define spir_func [2 x <2 x double>] @_Z17MatrixTimesMatrixDv3_Dv2_dDv2_Dv3_d(
    [3 x <2 x double>] %m, [2 x <3 x double>] %rm) #0
{
    %nm = alloca [2 x <2 x double>]
    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m2v = extractvalue [3 x <2 x double>] %m, 2

    %rm0v = extractvalue [2 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %nm0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %5, <2 x double>* %nm0

    %rm1v = extractvalue [2 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x double> %m1v, %rm1v1
    %8 = fadd <2 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x double> %m2v, %rm1v2
    %10 = fadd <2 x double> %9, %8

    %nm1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %10, <2 x double>* %nm1

    %11 = load [2 x <2 x double>], [2 x <2 x double>]* %nm

    ret [2 x <2 x double>] %11
}

; GLSL: dmat3x2 = dmat3x2 * dmat3
define spir_func [3 x <2 x double>] @_Z17MatrixTimesMatrixDv3_Dv2_dDv3_Dv3_d(
    [3 x <2 x double>] %m, [3 x <3 x double>] %rm) #0
{
    %nm = alloca [3 x <2 x double>]
    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m2v = extractvalue [3 x <2 x double>] %m, 2

    %rm0v = extractvalue [3 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %nm0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %5, <2 x double>* %nm0

    %rm1v = extractvalue [3 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x double> %m1v, %rm1v1
    %8 = fadd <2 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x double> %m2v, %rm1v2
    %10 = fadd <2 x double> %9, %8

    %nm1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %10, <2 x double>* %nm1

    %rm2v = extractvalue [3 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x double> %m1v, %rm2v1
    %13 = fadd <2 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x double> %m2v, %rm2v2
    %15 = fadd <2 x double> %14, %13

    %nm2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %15, <2 x double>* %nm2

    %16 = load [3 x <2 x double>], [3 x <2 x double>]* %nm

    ret [3 x <2 x double>] %16
}

; GLSL: dmat4x2 = dmat3x2 * dmat4x3
define spir_func [4 x <2 x double>] @_Z17MatrixTimesMatrixDv3_Dv2_dDv4_Dv3_d(
    [3 x <2 x double>] %m, [4 x <3 x double>] %rm) #0
{
    %nm = alloca [4 x <2 x double>]
    %m0v = extractvalue [3 x <2 x double>] %m, 0
    %m1v = extractvalue [3 x <2 x double>] %m, 1
    %m2v = extractvalue [3 x <2 x double>] %m, 2

    %rm0v = extractvalue [4 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %nm0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %5, <2 x double>* %nm0

    %rm1v = extractvalue [4 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x double> %m1v, %rm1v1
    %8 = fadd <2 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x double> %m2v, %rm1v2
    %10 = fadd <2 x double> %9, %8

    %nm1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %10, <2 x double>* %nm1

    %rm2v = extractvalue [4 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x double> %m1v, %rm2v1
    %13 = fadd <2 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x double> %m2v, %rm2v2
    %15 = fadd <2 x double> %14, %13

    %nm2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %15, <2 x double>* %nm2

    %rm3v = extractvalue [4 x <3 x double>] %rm, 3
    %rm3v0 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <2 x i32> <i32 0, i32 0>
    %16 = fmul <2 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <2 x i32> <i32 1, i32 1>
    %17 = fmul <2 x double> %m1v, %rm3v1
    %18 = fadd <2 x double> %17, %16

    %rm3v2 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <2 x i32> <i32 2, i32 2>
    %19 = fmul <2 x double> %m2v, %rm3v2
    %20 = fadd <2 x double> %19, %18

    %nm3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 3
    store <2 x double> %20, <2 x double>* %nm3

    %21 = load [4 x <2 x double>], [4 x <2 x double>]* %nm

    ret [4 x <2 x double>] %21
}

; GLSL: dmat2x3 = dmat2x3 * dmat2
define spir_func [2 x <3 x double>] @_Z17MatrixTimesMatrixDv2_Dv3_dDv2_Dv2_d(
    [2 x <3 x double>] %m, [2 x <2 x double>] %rm) #0
{
    %nm = alloca [2 x <3 x double>]
    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m1v = extractvalue [2 x <3 x double>] %m, 1

    %rm0v = extractvalue [2 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %nm0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %3, <3 x double>* %nm0

    %rm1v = extractvalue [2 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x double> %m1v, %rm1v1
    %6 = fadd <3 x double> %5, %4

    %nm1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %6, <3 x double>* %nm1

    %7 = load [2 x <3 x double>], [2 x <3 x double>]* %nm

    ret [2 x <3 x double>] %7
}

; GLSL: dmat3 = dmat2x3 * dmat3x2
define spir_func [3 x <3 x double>] @_Z17MatrixTimesMatrixDv2_Dv3_dDv3_Dv2_d(
    [2 x <3 x double>] %m, [3 x <2 x double>] %rm) #0
{
    %nm = alloca [3 x <3 x double>]
    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m1v = extractvalue [2 x <3 x double>] %m, 1

    %rm0v = extractvalue [3 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %nm0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %3, <3 x double>* %nm0

    %rm1v = extractvalue [3 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x double> %m1v, %rm1v1
    %6 = fadd <3 x double> %5, %4

    %nm1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %6, <3 x double>* %nm1

    %rm2v = extractvalue [3 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x double> %m1v, %rm2v1
    %9 = fadd <3 x double> %8, %7

    %nm2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %9, <3 x double>* %nm2

    %10 = load [3 x <3 x double>], [3 x <3 x double>]* %nm

    ret [3 x <3 x double>] %10
}

; GLSL: dmat4x3 = dmat2x3 * dmat4x2
define spir_func [4 x <3 x double>] @_Z17MatrixTimesMatrixDv2_Dv3_dDv4_Dv2_d(
    [2 x <3 x double>] %m, [4 x <2 x double>] %rm) #0
{
    %nm = alloca [4 x <3 x double>]
    %m0v = extractvalue [2 x <3 x double>] %m, 0
    %m1v = extractvalue [2 x <3 x double>] %m, 1

    %rm0v = extractvalue [4 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %nm0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %3, <3 x double>* %nm0

    %rm1v = extractvalue [4 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x double> %m1v, %rm1v1
    %6 = fadd <3 x double> %5, %4

    %nm1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %6, <3 x double>* %nm1

    %rm2v = extractvalue [4 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x double> %m1v, %rm2v1
    %9 = fadd <3 x double> %8, %7

    %nm2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %9, <3 x double>* %nm2

    %rm3v = extractvalue [4 x <2 x double>] %rm, 3
    %rm3v0 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %10 = fmul <3 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %11 = fmul <3 x double> %m1v, %rm3v1
    %12 = fadd <3 x double> %11, %10

    %nm3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 3
    store <3 x double> %12, <3 x double>* %nm3

    %13 = load [4 x <3 x double>], [4 x <3 x double>]* %nm

    ret [4 x <3 x double>] %13
}

; GLSL: dmat2 = dmat4x2 * dmat2x4
define spir_func [2 x <2 x double>] @_Z17MatrixTimesMatrixDv4_Dv2_dDv2_Dv4_d(
    [4 x <2 x double>] %m, [2 x <4 x double>] %rm) #0
{
    %nm = alloca [2 x <2 x double>]
    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m3v = extractvalue [4 x <2 x double>] %m, 3

    %rm0v = extractvalue [2 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x double> %m3v, %rm0v3
    %7 = fadd <2 x double> %6, %5

    %nm0 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %7, <2 x double>* %nm0

    %rm1v = extractvalue [2 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x double> %m1v, %rm1v1
    %10 = fadd <2 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x double> %m2v, %rm1v2
    %12 = fadd <2 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x double> %m3v, %rm1v3
    %14 = fadd <2 x double> %13, %12

    %nm1 = getelementptr inbounds [2 x <2 x double>], [2 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %14, <2 x double>* %nm1

    %15 = load [2 x <2 x double>], [2 x <2 x double>]* %nm

    ret [2 x <2 x double>] %15
}

; GLSL: dmat3x2 = dmat4x2 * dmat3x4
define spir_func [3 x <2 x double>] @_Z17MatrixTimesMatrixDv4_Dv2_dDv3_Dv4_d(
    [4 x <2 x double>] %m, [3 x <4 x double>] %rm) #0
{
    %nm = alloca [3 x <2 x double>]
    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m3v = extractvalue [4 x <2 x double>] %m, 3

    %rm0v = extractvalue [3 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x double> %m3v, %rm0v3
    %7 = fadd <2 x double> %6, %5

    %nm0 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %7, <2 x double>* %nm0

    %rm1v = extractvalue [3 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x double> %m1v, %rm1v1
    %10 = fadd <2 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x double> %m2v, %rm1v2
    %12 = fadd <2 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x double> %m3v, %rm1v3
    %14 = fadd <2 x double> %13, %12

    %nm1 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %14, <2 x double>* %nm1

    %rm2v = extractvalue [3 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x double> %m1v, %rm2v1
    %17 = fadd <2 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x double> %m2v, %rm2v2
    %19 = fadd <2 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x double> %m3v, %rm2v3
    %21 = fadd <2 x double> %20, %19

    %nm2 = getelementptr inbounds [3 x <2 x double>], [3 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %21, <2 x double>* %nm2

    %22 = load [3 x <2 x double>], [3 x <2 x double>]* %nm

    ret [3 x <2 x double>] %22
}

; GLSL: dmat4x2 = dmat4x2 * dmat4
define spir_func [4 x <2 x double>] @_Z17MatrixTimesMatrixDv4_Dv2_dDv4_Dv4_d(
    [4 x <2 x double>] %m, [4 x <4 x double>] %rm) #0
{
    %nm = alloca [4 x <2 x double>]
    %m0v = extractvalue [4 x <2 x double>] %m, 0
    %m1v = extractvalue [4 x <2 x double>] %m, 1
    %m2v = extractvalue [4 x <2 x double>] %m, 2
    %m3v = extractvalue [4 x <2 x double>] %m, 3

    %rm0v = extractvalue [4 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x double> %m1v, %rm0v1
    %3 = fadd <2 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x double> %m2v, %rm0v2
    %5 = fadd <2 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x double> %m3v, %rm0v3
    %7 = fadd <2 x double> %6, %5

    %nm0 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 0
    store <2 x double> %7, <2 x double>* %nm0

    %rm1v = extractvalue [4 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x double> %m1v, %rm1v1
    %10 = fadd <2 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x double> %m2v, %rm1v2
    %12 = fadd <2 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x double> %m3v, %rm1v3
    %14 = fadd <2 x double> %13, %12

    %nm1 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 1
    store <2 x double> %14, <2 x double>* %nm1

    %rm2v = extractvalue [4 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x double> %m1v, %rm2v1
    %17 = fadd <2 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x double> %m2v, %rm2v2
    %19 = fadd <2 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x double> %m3v, %rm2v3
    %21 = fadd <2 x double> %20, %19

    %nm2 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 2
    store <2 x double> %21, <2 x double>* %nm2

    %rm3v = extractvalue [4 x <4 x double>] %rm, 3
    %rm3v0 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <2 x i32> <i32 0, i32 0>
    %22 = fmul <2 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <2 x i32> <i32 1, i32 1>
    %23 = fmul <2 x double> %m1v, %rm3v1
    %24 = fadd <2 x double> %23, %22

    %rm3v2 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <2 x i32> <i32 2, i32 2>
    %25 = fmul <2 x double> %m2v, %rm3v2
    %26 = fadd <2 x double> %25, %24

    %rm3v3 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <2 x i32> <i32 3, i32 3>
    %27 = fmul <2 x double> %m3v, %rm3v3
    %28 = fadd <2 x double> %27, %26

    %nm3 = getelementptr inbounds [4 x <2 x double>], [4 x <2 x double>]* %nm, i32 0, i32 3
    store <2 x double> %28, <2 x double>* %nm3

    %29 = load [4 x <2 x double>], [4 x <2 x double>]* %nm

    ret [4 x <2 x double>] %29
}

; GLSL: dmat2x4 = dmat2x4 * dmat2
define spir_func [2 x <4 x double>] @_Z17MatrixTimesMatrixDv2_Dv4_dDv2_Dv2_d(
    [2 x <4 x double>] %m, [2 x <2 x double>] %rm) #0
{
    %nm = alloca [2 x <4 x double>]
    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m1v = extractvalue [2 x <4 x double>] %m, 1

    %rm0v = extractvalue [2 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %nm0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %3, <4 x double>* %nm0

    %rm1v = extractvalue [2 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x double> %m1v, %rm1v1
    %6 = fadd <4 x double> %5, %4

    %nm1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %6, <4 x double>* %nm1

    %7 = load [2 x <4 x double>], [2 x <4 x double>]* %nm

    ret [2 x <4 x double>] %7
}

; GLSL: dmat3x4 = dmat2x4 * dmat3x2
define spir_func [3 x <4 x double>] @_Z17MatrixTimesMatrixDv2_Dv4_dDv3_Dv2_d(
    [2 x <4 x double>] %m, [3 x <2 x double>] %rm) #0
{
    %nm = alloca [3 x <4 x double>]
    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m1v = extractvalue [2 x <4 x double>] %m, 1

    %rm0v = extractvalue [3 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %nm0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %3, <4 x double>* %nm0

    %rm1v = extractvalue [3 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x double> %m1v, %rm1v1
    %6 = fadd <4 x double> %5, %4

    %nm1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %6, <4 x double>* %nm1

    %rm2v = extractvalue [3 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x double> %m1v, %rm2v1
    %9 = fadd <4 x double> %8, %7

    %nm2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %9, <4 x double>* %nm2

    %10 = load [3 x <4 x double>], [3 x <4 x double>]* %nm

    ret [3 x <4 x double>] %10
}

; GLSL: dmat4 = dmat2x4 * dmat4x2
define spir_func [4 x <4 x double>] @_Z17MatrixTimesMatrixDv2_Dv4_dDv4_Dv2_d(
    [2 x <4 x double>] %m, [4 x <2 x double>] %rm) #0
{
    %nm = alloca [4 x <4 x double>]
    %m0v = extractvalue [2 x <4 x double>] %m, 0
    %m1v = extractvalue [2 x <4 x double>] %m, 1

    %rm0v = extractvalue [4 x <2 x double>] %rm, 0
    %rm0v0 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x double> %rm0v, <2 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %nm0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %3, <4 x double>* %nm0

    %rm1v = extractvalue [4 x <2 x double>] %rm, 1
    %rm1v0 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x double> %rm1v, <2 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x double> %m1v, %rm1v1
    %6 = fadd <4 x double> %5, %4

    %nm1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %6, <4 x double>* %nm1

    %rm2v = extractvalue [4 x <2 x double>] %rm, 2
    %rm2v0 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x double> %rm2v, <2 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x double> %m1v, %rm2v1
    %9 = fadd <4 x double> %8, %7

    %nm2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %9, <4 x double>* %nm2

    %rm3v = extractvalue [4 x <2 x double>] %rm, 3
    %rm3v0 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %10 = fmul <4 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x double> %rm3v, <2 x double> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %11 = fmul <4 x double> %m1v, %rm3v1
    %12 = fadd <4 x double> %11, %10

    %nm3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 3
    store <4 x double> %12, <4 x double>* %nm3

    %13 = load [4 x <4 x double>], [4 x <4 x double>]* %nm

    ret [4 x <4 x double>] %13
}

; GLSL: dmat2x3 = dmat4x3 * dmat2x4
define spir_func [2 x <3 x double>] @_Z17MatrixTimesMatrixDv4_Dv3_dDv2_Dv4_d(
    [4 x <3 x double>] %m, [2 x <4 x double>] %rm) #0
{
    %nm = alloca [2 x <3 x double>]
    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m3v = extractvalue [4 x <3 x double>] %m, 3

    %rm0v = extractvalue [2 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x double> %m3v, %rm0v3
    %7 = fadd <3 x double> %6, %5

    %nm0 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %7, <3 x double>* %nm0

    %rm1v = extractvalue [2 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x double> %m1v, %rm1v1
    %10 = fadd <3 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x double> %m2v, %rm1v2
    %12 = fadd <3 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x double> %m3v, %rm1v3
    %14 = fadd <3 x double> %13, %12

    %nm1 = getelementptr inbounds [2 x <3 x double>], [2 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %14, <3 x double>* %nm1

    %15 = load [2 x <3 x double>], [2 x <3 x double>]* %nm

    ret [2 x <3 x double>] %15
}

; GLSL: dmat3 = dmat4x3 * dmat3x4
define spir_func [3 x <3 x double>] @_Z17MatrixTimesMatrixDv4_Dv3_dDv3_Dv4_d(
    [4 x <3 x double>] %m, [3 x <4 x double>] %rm) #0
{
    %nm = alloca [3 x <3 x double>]
    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m3v = extractvalue [4 x <3 x double>] %m, 3

    %rm0v = extractvalue [3 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x double> %m3v, %rm0v3
    %7 = fadd <3 x double> %6, %5

    %nm0 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %7, <3 x double>* %nm0

    %rm1v = extractvalue [3 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x double> %m1v, %rm1v1
    %10 = fadd <3 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x double> %m2v, %rm1v2
    %12 = fadd <3 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x double> %m3v, %rm1v3
    %14 = fadd <3 x double> %13, %12

    %nm1 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %14, <3 x double>* %nm1

    %rm2v = extractvalue [3 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x double> %m1v, %rm2v1
    %17 = fadd <3 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x double> %m2v, %rm2v2
    %19 = fadd <3 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x double> %m3v, %rm2v3
    %21 = fadd <3 x double> %20, %19

    %nm2 = getelementptr inbounds [3 x <3 x double>], [3 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %21, <3 x double>* %nm2

    %22 = load [3 x <3 x double>], [3 x <3 x double>]* %nm

    ret [3 x <3 x double>] %22
}

; GLSL: dmat4x3 = dmat4x3 * dmat4
define spir_func [4 x <3 x double>] @_Z17MatrixTimesMatrixDv4_Dv3_dDv4_Dv4_d(
    [4 x <3 x double>] %m, [4 x <4 x double>] %rm) #0
{
    %nm = alloca [4 x <3 x double>]
    %m0v = extractvalue [4 x <3 x double>] %m, 0
    %m1v = extractvalue [4 x <3 x double>] %m, 1
    %m2v = extractvalue [4 x <3 x double>] %m, 2
    %m3v = extractvalue [4 x <3 x double>] %m, 3

    %rm0v = extractvalue [4 x <4 x double>] %rm, 0
    %rm0v0 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x double> %m1v, %rm0v1
    %3 = fadd <3 x double> %2, %1

    %rm0v2 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x double> %m2v, %rm0v2
    %5 = fadd <3 x double> %4, %3

    %rm0v3 = shufflevector <4 x double> %rm0v, <4 x double> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x double> %m3v, %rm0v3
    %7 = fadd <3 x double> %6, %5

    %nm0 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 0
    store <3 x double> %7, <3 x double>* %nm0

    %rm1v = extractvalue [4 x <4 x double>] %rm, 1
    %rm1v0 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x double> %m1v, %rm1v1
    %10 = fadd <3 x double> %9, %8

    %rm1v2 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x double> %m2v, %rm1v2
    %12 = fadd <3 x double> %11, %10

    %rm1v3 = shufflevector <4 x double> %rm1v, <4 x double> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x double> %m3v, %rm1v3
    %14 = fadd <3 x double> %13, %12

    %nm1 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 1
    store <3 x double> %14, <3 x double>* %nm1

    %rm2v = extractvalue [4 x <4 x double>] %rm, 2
    %rm2v0 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x double> %m1v, %rm2v1
    %17 = fadd <3 x double> %16, %15

    %rm2v2 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x double> %m2v, %rm2v2
    %19 = fadd <3 x double> %18, %17

    %rm2v3 = shufflevector <4 x double> %rm2v, <4 x double> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x double> %m3v, %rm2v3
    %21 = fadd <3 x double> %20, %19

    %nm2 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 2
    store <3 x double> %21, <3 x double>* %nm2

    %rm3v = extractvalue [4 x <4 x double>] %rm, 3
    %rm3v0 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %22 = fmul <3 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %23 = fmul <3 x double> %m1v, %rm3v1
    %24 = fadd <3 x double> %23, %22

    %rm3v2 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %25 = fmul <3 x double> %m2v, %rm3v2
    %26 = fadd <3 x double> %25, %24

    %rm3v3 = shufflevector <4 x double> %rm3v, <4 x double> %rm3v, <3 x i32> <i32 3, i32 3, i32 3>
    %27 = fmul <3 x double> %m3v, %rm3v3
    %28 = fadd <3 x double> %27, %26

    %nm3 = getelementptr inbounds [4 x <3 x double>], [4 x <3 x double>]* %nm, i32 0, i32 3
    store <3 x double> %28, <3 x double>* %nm3

    %29 = load [4 x <3 x double>], [4 x <3 x double>]* %nm

    ret [4 x <3 x double>] %29
}

; GLSL: dmat2x4 = dmat3x4 * dmat2x3
define spir_func [2 x <4 x double>] @_Z17MatrixTimesMatrixDv3_Dv4_dDv2_Dv3_d(
    [3 x <4 x double>] %m, [2 x <3 x double>] %rm) #0
{
    %nm = alloca [2 x <4 x double>]
    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m2v = extractvalue [3 x <4 x double>] %m, 2

    %rm0v = extractvalue [2 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %nm0 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %5, <4 x double>* %nm0

    %rm1v = extractvalue [2 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x double> %m1v, %rm1v1
    %8 = fadd <4 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x double> %m2v, %rm1v2
    %10 = fadd <4 x double> %9, %8

    %nm1 = getelementptr inbounds [2 x <4 x double>], [2 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %10, <4 x double>* %nm1

    %11 = load [2 x <4 x double>], [2 x <4 x double>]* %nm

    ret [2 x <4 x double>] %11
}

; GLSL: dmat3x4 = dmat3x4 * dmat3
define spir_func [3 x <4 x double>] @_Z17MatrixTimesMatrixDv3_Dv4_dDv3_Dv3_d(
    [3 x <4 x double>] %m, [3 x <3 x double>] %rm) #0
{
    %nm = alloca [3 x <4 x double>]
    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m2v = extractvalue [3 x <4 x double>] %m, 2

    %rm0v = extractvalue [3 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %nm0 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %5, <4 x double>* %nm0

    %rm1v = extractvalue [3 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x double> %m1v, %rm1v1
    %8 = fadd <4 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x double> %m2v, %rm1v2
    %10 = fadd <4 x double> %9, %8

    %nm1 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %10, <4 x double>* %nm1

    %rm2v = extractvalue [3 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x double> %m1v, %rm2v1
    %13 = fadd <4 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x double> %m2v, %rm2v2
    %15 = fadd <4 x double> %14, %13

    %nm2 = getelementptr inbounds [3 x <4 x double>], [3 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %15, <4 x double>* %nm2

    %16 = load [3 x <4 x double>], [3 x <4 x double>]* %nm

    ret [3 x <4 x double>] %16
}

; GLSL: dmat4 = dmat3x4 * dmat4x3
define spir_func [4 x <4 x double>] @_Z17MatrixTimesMatrixDv3_Dv4_dDv4_Dv3_d(
    [3 x <4 x double>] %m, [4 x <3 x double>] %rm) #0
{
    %nm = alloca [4 x <4 x double>]
    %m0v = extractvalue [3 x <4 x double>] %m, 0
    %m1v = extractvalue [3 x <4 x double>] %m, 1
    %m2v = extractvalue [3 x <4 x double>] %m, 2

    %rm0v = extractvalue [4 x <3 x double>] %rm, 0
    %rm0v0 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x double> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x double> %m1v, %rm0v1
    %3 = fadd <4 x double> %2, %1

    %rm0v2 = shufflevector <3 x double> %rm0v, <3 x double> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x double> %m2v, %rm0v2
    %5 = fadd <4 x double> %4, %3

    %nm0 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 0
    store <4 x double> %5, <4 x double>* %nm0

    %rm1v = extractvalue [4 x <3 x double>] %rm, 1
    %rm1v0 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x double> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x double> %m1v, %rm1v1
    %8 = fadd <4 x double> %7, %6

    %rm1v2 = shufflevector <3 x double> %rm1v, <3 x double> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x double> %m2v, %rm1v2
    %10 = fadd <4 x double> %9, %8

    %nm1 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 1
    store <4 x double> %10, <4 x double>* %nm1

    %rm2v = extractvalue [4 x <3 x double>] %rm, 2
    %rm2v0 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x double> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x double> %m1v, %rm2v1
    %13 = fadd <4 x double> %12, %11

    %rm2v2 = shufflevector <3 x double> %rm2v, <3 x double> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x double> %m2v, %rm2v2
    %15 = fadd <4 x double> %14, %13

    %nm2 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 2
    store <4 x double> %15, <4 x double>* %nm2

    %rm3v = extractvalue [4 x <3 x double>] %rm, 3
    %rm3v0 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %16 = fmul <4 x double> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %17 = fmul <4 x double> %m1v, %rm3v1
    %18 = fadd <4 x double> %17, %16

    %rm3v2 = shufflevector <3 x double> %rm3v, <3 x double> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %19 = fmul <4 x double> %m2v, %rm3v2
    %20 = fadd <4 x double> %19, %18

    %nm3 = getelementptr inbounds [4 x <4 x double>], [4 x <4 x double>]* %nm, i32 0, i32 3
    store <4 x double> %20, <4 x double>* %nm3

    %21 = load [4 x <4 x double>], [4 x <4 x double>]* %nm

    ret [4 x <4 x double>] %21
}

; GLSL helper: double = determinant2(dvec2(double, double), dvec2(double, double))
define spir_func double @llpc.determinant2.f64(
    double %x0, double %y0, double %x1, double %y1)
{
    ; | x0   x1 |
    ; |         | = x0 * y1 - y0 * x1
    ; | y0   y1 |

    %1 = fmul double %x0, %y1
    %2 = fmul double %y0, %x1
    %3 = fsub double %1, %2
    ret double %3
}

; GLSL helper: double = determinant3(dvec3(double, double, double), dvec3(double, double, double))
define spir_func double @llpc.determinant3.f64(
    double %x0, double %y0, double %z0,
    double %x1, double %y1, double %z1,
    double %x2, double %y2, double %z2)
{
    ; | x0   x1   x2 |
    ; |              |        | y1 y2 |        | x1 x2 |        | x1 x2 |
    ; | y0   y1   y2 | = x0 * |       | - y0 * |       | + z0 * |       |
    ; |              |        | z1 z2 |        | z1 z2 |        | y1 y2 |
    ; | z0   z1   z2 |
    ;
    ;                         | y1 y2 |        | z1 z2 |        | x1 x2 |
    ;                  = x0 * |       | + y0 * |       | + z0 * |       |
    ;                         | z1 z2 |        | x1 x2 |        | y1 y2 |

    %1 = call double @llpc.determinant2.f64(double %y1, double %z1, double %y2, double %z2)
    %2 = fmul double %1, %x0
    %3 = call double @llpc.determinant2.f64(double %z1, double %x1, double %z2, double %x2)
    %4 = fmul double %3, %y0
    %5 = fadd double %2, %4
    %6 = call double @llpc.determinant2.f64(double %x1, double %y1, double %x2, double %y2)
    %7 = fmul double %6, %z0
    %8 = fadd double %7, %5
    ret double %8
}

; GLSL helper: double = determinant4(dvec4(double, double, double, double), dvec4(double, double, double, double))
define spir_func double @llpc.determinant4.f64(
    double %x0, double %y0, double %z0, double %w0,
    double %x1, double %y1, double %z1, double %w1,
    double %x2, double %y2, double %z2, double %w2,
    double %x3, double %y3, double %z3, double %w3)

{
    ; | x0   x1   x2   x3 |
    ; |                   |        | y1 y2 y3 |        | x1 x2 x3 |
    ; | y0   y1   y2   y3 |        |          |        |          |
    ; |                   | = x0 * | z1 z2 z3 | - y0 * | z1 z2 z3 | +
    ; | z0   z1   z2   z3 |        |          |        |          |
    ; |                   |        | w1 w2 w3 |        | w1 w2 w3 |
    ; | w0   w1   w2   w3 |
    ;
    ;                              | x1 x2 x3 |        | x1 x2 x3 |
    ;                              |          |        |          |
    ;                         z0 * | y1 y2 y3 | - w0 * | y1 y2 y3 |
    ;                              |          |        |          |
    ;                              | w1 w2 w3 |        | z1 z2 z3 |
    ;
    ;
    ;                              | y1 y2 y3 |        | z1 z2 z3 |
    ;                              |          |        |          |
    ;                       = x0 * | z1 z2 z3 | + y0 * | x1 x2 x3 | +
    ;                              |          |        |          |
    ;                              | w1 w2 w3 |        | w1 w2 w3 |
    ;
    ;                              | x1 x2 x3 |        | y1 y2 y3 |
    ;                              |          |        |          |
    ;                         z0 * | y1 y2 y3 | + w0 * | x1 x2 x3 |
    ;                              |          |        |          |
    ;                              | w1 w2 w3 |        | z1 z2 z3 |

    %1 = call double @llpc.determinant3.f64(double %y1, double %z1, double %w1, double %y2, double %z2, double %w2, double %y3, double %z3, double %w3)
    %2 = fmul double %1, %x0
    %3 = call double @llpc.determinant3.f64(double %z1, double %x1, double %w1, double %z2, double %x2, double %w2, double %z3, double %x3, double %w3)
    %4 = fmul double %3, %y0
    %5 = fadd double %2, %4
    %6 = call double @llpc.determinant3.f64(double %x1, double %y1, double %w1, double %x2, double %y2, double %w2, double %x3, double %y3, double %w3)
    %7 = fmul double %6, %z0
    %8 = fadd double %5, %7
    %9 = call double @llpc.determinant3.f64(double %y1, double %x1, double %z1, double %y2, double %x2, double %z2, double %y3, double %x3, double %z3)
    %10 = fmul double %9, %w0
    %11 = fadd double %8, %10
    ret double %11
}

; GLSL: double = determinant(dmat2)
define spir_func double @_Z11determinantDv2_Dv2_d(
    [2 x <2 x double>] %m) #0
{
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %m0v0 = extractelement <2 x double> %m0v, i32 0
    %m0v1 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x double>] %m, 1
    %m1v0 = extractelement <2 x double> %m1v, i32 0
    %m1v1 = extractelement <2 x double> %m1v, i32 1

    %d = call double @llpc.determinant2.f64(double %m0v0, double %m0v1, double %m1v0, double %m1v1)
    ret double %d
}

; GLSL: double = determinant(dmat3)
define spir_func double @_Z11determinantDv3_Dv3_d(
    [3 x <3 x double>] %m) #0
{
    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %m0v0 = extractelement <3 x double> %m0v, i32 0
    %m0v1 = extractelement <3 x double> %m0v, i32 1
    %m0v2 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %m1v0 = extractelement <3 x double> %m1v, i32 0
    %m1v1 = extractelement <3 x double> %m1v, i32 1
    %m1v2 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x double>] %m, 2
    %m2v0 = extractelement <3 x double> %m2v, i32 0
    %m2v1 = extractelement <3 x double> %m2v, i32 1
    %m2v2 = extractelement <3 x double> %m2v, i32 2

    %d = call double @llpc.determinant3.f64(
        double %m0v0, double %m0v1, double %m0v2,
        double %m1v0, double %m1v1, double %m1v2,
        double %m2v0, double %m2v1, double %m2v2)
    ret double %d
}

; GLSL: double = determinant(dmat4)
define spir_func double @_Z11determinantDv4_Dv4_d(
    [4 x <4 x double>] %m) #0
{
    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %m0v0 = extractelement <4 x double> %m0v, i32 0
    %m0v1 = extractelement <4 x double> %m0v, i32 1
    %m0v2 = extractelement <4 x double> %m0v, i32 2
    %m0v3 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %m1v0 = extractelement <4 x double> %m1v, i32 0
    %m1v1 = extractelement <4 x double> %m1v, i32 1
    %m1v2 = extractelement <4 x double> %m1v, i32 2
    %m1v3 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %m2v0 = extractelement <4 x double> %m2v, i32 0
    %m2v1 = extractelement <4 x double> %m2v, i32 1
    %m2v2 = extractelement <4 x double> %m2v, i32 2
    %m2v3 = extractelement <4 x double> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x double>] %m, 3
    %m3v0 = extractelement <4 x double> %m3v, i32 0
    %m3v1 = extractelement <4 x double> %m3v, i32 1
    %m3v2 = extractelement <4 x double> %m3v, i32 2
    %m3v3 = extractelement <4 x double> %m3v, i32 3

    %d = call double @llpc.determinant4.f64(
        double %m0v0, double %m0v1, double %m0v2, double %m0v3,
        double %m1v0, double %m1v1, double %m1v2, double %m1v3,
        double %m2v0, double %m2v1, double %m2v2, double %m2v3,
        double %m3v0, double %m3v1, double %m3v2, double %m3v3)
    ret double %d
}

; GLSL helper: double = dot3(dvec3(double, double, double), dvec3(double, double, double))
define spir_func double @llpc.dot3.f64(
    double %x0, double %y0, double %z0,
    double %x1, double %y1, double %z1)
{
    %1 = fmul double %x1, %x0
    %2 = fmul double %y1, %y0
    %3 = fadd double %1, %2
    %4 = fmul double %z1, %z0
    %5 = fadd double %3, %4
    ret double %5
}

; GLSL helper: double = dot4(dvec4(double, double, double, double), dvec4(double, double, double, double))
define spir_func double @llpc.dot4.f64(
    double %x0, double %y0, double %z0, double %w0,
    double %x1, double %y1, double %z1, double %w1)
{
    %1 = fmul double %x1, %x0
    %2 = fmul double %y1, %y0
    %3 = fadd double %1, %2
    %4 = fmul double %z1, %z0
    %5 = fadd double %3, %4
    %6 = fmul double %w1, %w0
    %7 = fadd double %5, %6
    ret double %7
}

; GLSL: dmat2 = inverse(dmat2)
define spir_func [2 x <2 x double>] @_Z13matrixInverseDv2_Dv2_d(
    [2 x <2 x double>] %m) #0
{
    ; [ x0   x1 ]                    [  y1 -x1 ]
    ; [         ]  = (1 / det(M))) * [         ]
    ; [ y0   y1 ]                    [ -y0  x0 ]
    %m0v = extractvalue [2 x <2 x double>] %m, 0
    %x0 = extractelement <2 x double> %m0v, i32 0
    %y0 = extractelement <2 x double> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x double>] %m, 1
    %x1 = extractelement <2 x double> %m1v, i32 0
    %y1 = extractelement <2 x double> %m1v, i32 1

    %1 = call double @llpc.determinant2.f64(double %x0, double %y0, double %x1, double %y1)
    %2 = fdiv double 1.0, %1
    %3 = fsub double 0.0, %2
    %4 = fmul double %2, %y1
    %5 = fmul double %3, %y0
    %6 = fmul double %3, %x1
    %7 = fmul double %2, %x0
    %8 = insertelement <2 x double> undef, double %4, i32 0
    %9 = insertelement <2 x double> %8, double %5, i32 1
    %10 = insertvalue [2 x <2 x double>] undef, <2 x double> %9, 0
    %11 = insertelement <2 x double> undef, double %6, i32 0
    %12 = insertelement <2 x double> %11, double %7, i32 1
    %13 = insertvalue [2 x <2 x double>] %10 , <2 x double> %12, 1

    ret [2 x <2 x double>]  %13
}

; GLSL: dmat3 = inverse(dmat3)
define spir_func [3 x <3 x double>] @_Z13matrixInverseDv3_Dv3_d(
    [3 x <3 x double>] %m) #0
{
    ; [ x0   x1   x2 ]                   [ Adj(x0) Adj(x1) Adj(x2) ] T
    ; [              ]                   [                         ]
    ; [ y0   y1   y2 ]  = (1 / det(M)) * [ Adj(y0) Adj(y1) Adj(y2) ]
    ; [              ]                   [                         ]
    ; [ z0   z1   z2 ]                   [ Adj(z0) Adj(z1) Adj(z2) ]
    ;
    ;
    ;                     [ Adj(x0) Adj(y0) Adj(z0) ]
    ;                     [                         ]
    ;  = (1 / det(M)) *   [ Adj(x1) Adj(y1) Adj(y1) ]
    ;                     [                         ]
    ;                     [ Adj(x2) Adj(y2) Adj(z2) ]
    ;

    %m0v = extractvalue [3 x <3 x double>] %m, 0
    %x0 = extractelement <3 x double> %m0v, i32 0
    %y0 = extractelement <3 x double> %m0v, i32 1
    %z0 = extractelement <3 x double> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x double>] %m, 1
    %x1 = extractelement <3 x double> %m1v, i32 0
    %y1 = extractelement <3 x double> %m1v, i32 1
    %z1 = extractelement <3 x double> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x double>] %m, 2
    %x2 = extractelement <3 x double> %m2v, i32 0
    %y2 = extractelement <3 x double> %m2v, i32 1
    %z2 = extractelement <3 x double> %m2v, i32 2

    %adjx0 = call double @llpc.determinant2.f64(double %y1, double %z1, double %y2, double %z2)
    %adjx1 = call double @llpc.determinant2.f64(double %y2, double %z2, double %y0, double %z0)
    %adjx2 = call double @llpc.determinant2.f64(double %y0, double %z0, double %y1, double %z1)

    %det = call double @llpc.dot3.f64(double %x0, double %x1, double %x2,
                    double %adjx0, double %adjx1, double %adjx2)
    %rdet = fdiv double 1.0, %det

    %nx0 = fmul double %rdet, %adjx0
    %nx1 = fmul double %rdet, %adjx1
    %nx2 = fmul double %rdet, %adjx2

    %m00 = insertelement <3 x double> undef, double %nx0, i32 0
    %m01 = insertelement <3 x double> %m00, double %nx1, i32 1
    %m02 = insertelement <3 x double> %m01, double %nx2, i32 2
    %m0 = insertvalue [3 x <3 x double>] undef, <3 x double> %m02, 0

    %adjy0 = call double @llpc.determinant2.f64(double %z1, double %x1, double %z2, double %x2)
    %adjy1 = call double @llpc.determinant2.f64(double %z2, double %x2, double %z0, double %x0)
    %adjy2 = call double @llpc.determinant2.f64(double %z0, double %x0, double %z1, double %x1)


    %ny0 = fmul double %rdet, %adjy0
    %ny1 = fmul double %rdet, %adjy1
    %ny2 = fmul double %rdet, %adjy2

    %m10 = insertelement <3 x double> undef, double %ny0, i32 0
    %m11 = insertelement <3 x double> %m10, double %ny1, i32 1
    %m12 = insertelement <3 x double> %m11, double %ny2, i32 2
    %m1 = insertvalue [3 x <3 x double>] %m0, <3 x double> %m12, 1

    %adjz0 = call double @llpc.determinant2.f64(double %x1, double %y1, double %x2, double %y2)
    %adjz1 = call double @llpc.determinant2.f64(double %x2, double %y2, double %x0, double %y0)
    %adjz2 = call double @llpc.determinant2.f64(double %x0, double %y0, double %x1, double %y1)

    %nz0 = fmul double %rdet, %adjz0
    %nz1 = fmul double %rdet, %adjz1
    %nz2 = fmul double %rdet, %adjz2

    %m20 = insertelement <3 x double> undef, double %nz0, i32 0
    %m21 = insertelement <3 x double> %m20, double %nz1, i32 1
    %m22 = insertelement <3 x double> %m21, double %nz2, i32 2
    %m2 = insertvalue [3 x <3 x double>] %m1, <3 x double> %m22, 2

    ret [3 x <3 x double>] %m2
}

; GLSL: dmat4 = inverse(dmat4)
define spir_func [4 x <4 x double>] @_Z13matrixInverseDv4_Dv4_d(
    [4 x <4 x double>] %m) #0
{
    ; [ x0   x1   x2   x3 ]                   [ Adj(x0) Adj(x1) Adj(x2) Adj(x3) ] T
    ; [                   ]                   [                                 ]
    ; [ y0   y1   y2   y3 ]                   [ Adj(y0) Adj(y1) Adj(y2) Adj(y3) ]
    ; [                   ]  = (1 / det(M)) * [                                 ]
    ; [ z0   z1   z2   z3 ]                   [ Adj(z0) Adj(z1) Adj(z2) Adj(z3) ]
    ; [                   ]                   [                                 ]
    ; [ w0   w1   w2   w3 ]                   [ Adj(w0) Adj(w1) Adj(w2) Adj(w3) ]
    ;
    ;                  [ Adj(x0) Adj(y0) Adj(z0) Adj(w0) ]
    ;                  [                                 ]
    ;                  [ Adj(x1) Adj(y1) Adj(z2) Adj(w1) ]
    ; = (1 / det(M)) * [                                 ]
    ;                  [ Adj(x2) Adj(y2) Adj(z3) Adj(w2) ]
    ;                  [                                 ]
    ;                  [ Adj(x3) Adj(y3) Adj(z4) Adj(w3) ]

    %m0v = extractvalue [4 x <4 x double>] %m, 0
    %x0 = extractelement <4 x double> %m0v, i32 0
    %y0 = extractelement <4 x double> %m0v, i32 1
    %z0 = extractelement <4 x double> %m0v, i32 2
    %w0 = extractelement <4 x double> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x double>] %m, 1
    %x1 = extractelement <4 x double> %m1v, i32 0
    %y1 = extractelement <4 x double> %m1v, i32 1
    %z1 = extractelement <4 x double> %m1v, i32 2
    %w1 = extractelement <4 x double> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x double>] %m, 2
    %x2 = extractelement <4 x double> %m2v, i32 0
    %y2 = extractelement <4 x double> %m2v, i32 1
    %z2 = extractelement <4 x double> %m2v, i32 2
    %w2 = extractelement <4 x double> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x double>] %m, 3
    %x3 = extractelement <4 x double> %m3v, i32 0
    %y3 = extractelement <4 x double> %m3v, i32 1
    %z3 = extractelement <4 x double> %m3v, i32 2
    %w3 = extractelement <4 x double> %m3v, i32 3

    %adjx0 = call double @llpc.determinant3.f64(
            double %y1, double %z1, double %w1,
            double %y2, double %z2, double %w2,
            double %y3, double %z3, double %w3)
    %adjx1 = call double @llpc.determinant3.f64(
            double %y2, double %z2, double %w2,
            double %y0, double %z0, double %w0,
            double %y3, double %z3, double %w3)
    %adjx2 = call double @llpc.determinant3.f64(
            double %y3, double %z3, double %w3,
            double %y0, double %z0, double %w0,
            double %y1, double %z1, double %w1)
    %adjx3 = call double @llpc.determinant3.f64(
            double %y0, double %z0, double %w0,
            double %y2, double %z2, double %w2,
            double %y1, double %z1, double %w1)

    %det = call double @llpc.dot4.f64(double %x0, double %x1, double %x2, double %x3,
            double %adjx0, double %adjx1, double %adjx2, double %adjx3)
    %rdet = fdiv double 1.0, %det

    %nx0 = fmul double %rdet, %adjx0
    %nx1 = fmul double %rdet, %adjx1
    %nx2 = fmul double %rdet, %adjx2
    %nx3 = fmul double %rdet, %adjx3

    %m00 = insertelement <4 x double> undef, double %nx0, i32 0
    %m01 = insertelement <4 x double> %m00, double %nx1, i32 1
    %m02 = insertelement <4 x double> %m01, double %nx2, i32 2
    %m03 = insertelement <4 x double> %m02, double %nx3, i32 3
    %m0 = insertvalue [4 x <4 x double>] undef, <4 x double> %m03, 0

    %adjy0 = call double @llpc.determinant3.f64(
            double %z2, double %w2, double %x2,
            double %z1, double %w1, double %x1,
            double %z3, double %w3, double %x3)
    %adjy1 = call double @llpc.determinant3.f64(
             double %z2, double %w2, double %x2,
             double %z3, double %w3, double %x3,
             double %z0, double %w0, double %x0)
    %adjy2 = call double @llpc.determinant3.f64(
            double %z0, double %w0, double %x0,
            double %z3, double %w3, double %x3,
            double %z1, double %w1, double %x1)
    %adjy3 = call double @llpc.determinant3.f64(
            double %z0, double %w0, double %x0,
            double %z1, double %w1, double %x1,
            double %z2, double %w2, double %x2)

    %ny0 = fmul double %rdet, %adjy0
    %ny1 = fmul double %rdet, %adjy1
    %ny2 = fmul double %rdet, %adjy2
    %ny3 = fmul double %rdet, %adjy3

    %m10 = insertelement <4 x double> undef, double %ny0, i32 0
    %m11 = insertelement <4 x double> %m10, double %ny1, i32 1
    %m12 = insertelement <4 x double> %m11, double %ny2, i32 2
    %m13 = insertelement <4 x double> %m12, double %ny3, i32 3
    %m1 = insertvalue [4 x <4 x double>] %m0, <4 x double> %m13, 1

    %adjz0 = call double @llpc.determinant3.f64(
            double %w1, double %x1, double %y1,
            double %w2, double %x2, double %y2,
            double %w3, double %x3, double %y3)
    %adjz1 = call double @llpc.determinant3.f64(
            double %w3, double %x3, double %y3,
            double %w2, double %x2, double %y2,
            double %w0, double %x0, double %y0)
    %adjz2 = call double @llpc.determinant3.f64(
            double %w3, double %x3, double %y3,
            double %w0, double %x0, double %y0,
            double %w1, double %x1, double %y1)
    %adjz3 = call double @llpc.determinant3.f64(
            double %w1, double %x1, double %y1,
            double %w0, double %x0, double %y0,
            double %w2, double %x2, double %y2)

    %nz0 = fmul double %rdet, %adjz0
    %nz1 = fmul double %rdet, %adjz1
    %nz2 = fmul double %rdet, %adjz2
    %nz3 = fmul double %rdet, %adjz3

    %m20 = insertelement <4 x double> undef, double %nz0, i32 0
    %m21 = insertelement <4 x double> %m20, double %nz1, i32 1
    %m22 = insertelement <4 x double> %m21, double %nz2, i32 2
    %m23 = insertelement <4 x double> %m22, double %nz3, i32 3
    %m2 = insertvalue [4 x <4 x double>] %m1, <4 x double> %m23, 2

    %adjw0 = call double @llpc.determinant3.f64(
            double %x2, double %y2, double %z2,
            double %x1, double %y1, double %z1,
            double %x3, double %y3, double %z3)
    %adjw1 = call double @llpc.determinant3.f64(
            double %x2, double %y2, double %z2,
            double %x3, double %y3, double %z3,
            double %x0, double %y0, double %z0)
    %adjw2 = call double @llpc.determinant3.f64(
            double %x0, double %y0, double %z0,
            double %x3, double %y3, double %z3,
            double %x1, double %y1, double %z1)
    %adjw3 = call double @llpc.determinant3.f64(
            double %x0, double %y0, double %z0,
            double %x1, double %y1, double %z1,
            double %x2, double %y2, double %z2)

    %nw0 = fmul double %rdet, %adjw0
    %nw1 = fmul double %rdet, %adjw1
    %nw2 = fmul double %rdet, %adjw2
    %nw3 = fmul double %rdet, %adjw3

    %m30 = insertelement <4 x double> undef, double %nw0, i32 0
    %m31 = insertelement <4 x double> %m30, double %nw1, i32 1
    %m32 = insertelement <4 x double> %m31, double %nw2, i32 2
    %m33 = insertelement <4 x double> %m32, double %nw3, i32 3
    %m3 = insertvalue [4 x <4 x double>] %m2, <4 x double> %m33, 3

    ret [4 x <4 x double>] %m3
}

declare spir_func double @_Z3dotDv2_dDv2_d(<2 x double> , <2 x double>) #0
declare spir_func double @_Z3dotDv3_dDv3_d(<3 x double> , <3 x double>) #0
declare spir_func double @_Z3dotDv4_dDv4_d(<4 x double> , <4 x double>) #0

attributes #0 = { nounwind }
