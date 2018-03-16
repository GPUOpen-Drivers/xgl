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

; GLSL: mat2 = outerProduct(vec2, vec2)
define spir_func [2 x <2 x float>] @_Z12OuterProductDv2_fDv2_f(
    <2 x float> %c, <2 x float> %r) #0
{
    %m = alloca [2 x <2 x float>]
    %m0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 1

    %c0 = extractelement <2 x float> %c, i32 0
    %c1 = extractelement <2 x float> %c, i32 1

    %r0 = extractelement <2 x float> %r, i32 0
    %r1 = extractelement <2 x float> %r, i32 1

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c0, %r1
    store float %3, float* %m10
    %4 = fmul float %c1, %r1
    store float %4, float* %m11
    %5 = load [2 x <2 x float>], [2 x <2 x float>]* %m

    ret [2 x <2 x float>] %5
}

; GLSL: mat3 = outerProduct(vec3, vec3)
define spir_func [3 x <3 x float>] @_Z12OuterProductDv3_fDv3_f(
    <3 x float> %c, <3 x float> %r) #0
{
    %m = alloca [3 x <3 x float>]
    %m0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 2

    %c0 = extractelement <3 x float> %c, i32 0
    %c1 = extractelement <3 x float> %c, i32 1
    %c2 = extractelement <3 x float> %c, i32 2

    %r0 = extractelement <3 x float> %r, i32 0
    %r1 = extractelement <3 x float> %r, i32 1
    %r2 = extractelement <3 x float> %r, i32 2

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c0, %r1
    store float %4, float* %m10
    %5 = fmul float %c1, %r1
    store float %5, float* %m11
    %6 = fmul float %c2, %r1
    store float %6, float* %m12
    %7 = fmul float %c0, %r2
    store float %7, float* %m20
    %8 = fmul float %c1, %r2
    store float %8, float* %m21
    %9 = fmul float %c2, %r2
    store float %9, float* %m22
    %10 = load [3 x <3 x float>], [3 x <3 x float>]* %m

    ret [3 x <3 x float>] %10
}

; GLSL: mat4 = outerProduct(vec4, vec4)
define spir_func [4 x <4 x float>] @_Z12OuterProductDv4_fDv4_f(
    <4 x float> %c, <4 x float> %r) #0
{
    %m = alloca [4 x <4 x float>]
    %m0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 3

    %m3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <4 x float>, <4 x float>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <4 x float>, <4 x float>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <4 x float>, <4 x float>* %m3, i32 0, i32 2
    %m33 = getelementptr inbounds <4 x float>, <4 x float>* %m3, i32 0, i32 3

    %c0 = extractelement <4 x float> %c, i32 0
    %c1 = extractelement <4 x float> %c, i32 1
    %c2 = extractelement <4 x float> %c, i32 2
    %c3 = extractelement <4 x float> %c, i32 3

    %r0 = extractelement <4 x float> %r, i32 0
    %r1 = extractelement <4 x float> %r, i32 1
    %r2 = extractelement <4 x float> %r, i32 2
    %r3 = extractelement <4 x float> %r, i32 3

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c3, %r0
    store float %4, float* %m03
    %5 = fmul float %c0, %r1
    store float %5, float* %m10
    %6 = fmul float %c1, %r1
    store float %6, float* %m11
    %7 = fmul float %c2, %r1
    store float %7, float* %m12
    %8 = fmul float %c3, %r1
    store float %8, float* %m13
    %9 = fmul float %c0, %r2
    store float %9, float* %m20
    %10 = fmul float %c1, %r2
    store float %10, float* %m21
    %11 = fmul float %c2, %r2
    store float %11, float* %m22
    %12 = fmul float %c3, %r2
    store float %12, float* %m23
    %13 = fmul float %c0, %r3
    store float %13, float* %m30
    %14 = fmul float %c1, %r3
    store float %14, float* %m31
    %15 = fmul float %c2, %r3
    store float %15, float* %m32
    %16 = fmul float %c3, %r3
    store float %16, float* %m33
    %17 = load [4 x <4 x float>], [4 x <4 x float>]* %m

    ret [4 x <4 x float>] %17
}

; GLSL: mat2x3 = outerProduct(vec3, vec2)
define spir_func [2 x <3 x float>] @_Z12OuterProductDv3_fDv2_f(
    <3 x float> %c, <2 x float> %r) #0
{
    %m = alloca [2 x <3 x float>]
    %m0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 2

    %c0 = extractelement <3 x float> %c, i32 0
    %c1 = extractelement <3 x float> %c, i32 1
    %c2 = extractelement <3 x float> %c, i32 2

    %r0 = extractelement <2 x float> %r, i32 0
    %r1 = extractelement <2 x float> %r, i32 1

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c0, %r1
    store float %4, float* %m10
    %5 = fmul float %c1, %r1
    store float %5, float* %m11
    %6 = fmul float %c2, %r1
    store float %6, float* %m12
    %7 = load [2 x <3 x float>], [2 x <3 x float>]* %m

    ret [2 x <3 x float>] %7
}

; GLSL: mat3x2 = outerProduct(vec2, vec3)
define spir_func [3 x <2 x float>] @_Z12OuterProductDv2_fDv3_f(
    <2 x float> %c, <3 x float> %r) #0
{
    %m = alloca [3 x <2 x float>]
    %m0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x float>, <2 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x float>, <2 x float>* %m2, i32 0, i32 1

    %c0 = extractelement <2 x float> %c, i32 0
    %c1 = extractelement <2 x float> %c, i32 1

    %r0 = extractelement <3 x float> %r, i32 0
    %r1 = extractelement <3 x float> %r, i32 1
    %r2 = extractelement <3 x float> %r, i32 2

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c0, %r1
    store float %3, float* %m10
    %4 = fmul float %c1, %r1
    store float %4, float* %m11
    %5 = fmul float %c0, %r2
    store float %5, float* %m20
    %6 = fmul float %c1, %r2
    store float %6, float* %m21
    %7 = load [3 x <2 x float>], [3 x <2 x float>]* %m

    ret [3 x <2 x float>] %7
}

; GLSL: mat2x4 = outerProduct(vec4, vec2)
define spir_func [2 x <4 x float>] @_Z12OuterProductDv4_fDv2_f(
    <4 x float> %c, <2 x float> %r) #0
{
    %m = alloca [2 x <4 x float>]
    %m0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 3

    %c0 = extractelement <4 x float> %c, i32 0
    %c1 = extractelement <4 x float> %c, i32 1
    %c2 = extractelement <4 x float> %c, i32 2
    %c3 = extractelement <4 x float> %c, i32 3

    %r0 = extractelement <2 x float> %r, i32 0
    %r1 = extractelement <2 x float> %r, i32 1

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c3, %r0
    store float %4, float* %m03
    %5 = fmul float %c0, %r1
    store float %5, float* %m10
    %6 = fmul float %c1, %r1
    store float %6, float* %m11
    %7 = fmul float %c2, %r1
    store float %7, float* %m12
    %8 = fmul float %c3, %r1
    store float %8, float* %m13
    %9 = load [2 x <4 x float>], [2 x <4 x float>]* %m

    ret [2 x <4 x float>] %9
}

; GLSL: mat4x2 = outerProduct(vec2, vec4)
define spir_func [4 x <2 x float>] @_Z12OuterProductDv2_fDv4_f(
    <2 x float> %c, <4 x float> %r) #0
{
    %m = alloca [4 x <2 x float>]
    %m0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x float>, <2 x float>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x float>, <2 x float>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x float>, <2 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x float>, <2 x float>* %m2, i32 0, i32 1

    %m3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <2 x float>, <2 x float>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <2 x float>, <2 x float>* %m3, i32 0, i32 1

    %c0 = extractelement <2 x float> %c, i32 0
    %c1 = extractelement <2 x float> %c, i32 1

    %r0 = extractelement <4 x float> %r, i32 0
    %r1 = extractelement <4 x float> %r, i32 1
    %r2 = extractelement <4 x float> %r, i32 2
    %r3 = extractelement <4 x float> %r, i32 3

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c0, %r1
    store float %3, float* %m10
    %4 = fmul float %c1, %r1
    store float %4, float* %m11
    %5 = fmul float %c0, %r2
    store float %5, float* %m20
    %6 = fmul float %c1, %r2
    store float %6, float* %m21
    %7 = fmul float %c0, %r3
    store float %7, float* %m30
    %8 = fmul float %c1, %r3
    store float %8, float* %m31
    %9 = load [4 x <2 x float>], [4 x <2 x float>]* %m

    ret [4 x <2 x float>] %9
}

; GLSL: mat3x4 = outerProduct(vec4, vec3)
define spir_func [3 x <4 x float>] @_Z12OuterProductDv4_fDv3_f(
    <4 x float> %c, <3 x float> %r) #0
{
    %m = alloca [3 x <4 x float>]
    %m0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x float>, <4 x float>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x float>, <4 x float>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x float>, <4 x float>* %m2, i32 0, i32 3

    %c0 = extractelement <4 x float> %c, i32 0
    %c1 = extractelement <4 x float> %c, i32 1
    %c2 = extractelement <4 x float> %c, i32 2
    %c3 = extractelement <4 x float> %c, i32 3

    %r0 = extractelement <3 x float> %r, i32 0
    %r1 = extractelement <3 x float> %r, i32 1
    %r2 = extractelement <3 x float> %r, i32 2

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c3, %r0
    store float %4, float* %m03
    %5 = fmul float %c0, %r1
    store float %5, float* %m10
    %6 = fmul float %c1, %r1
    store float %6, float* %m11
    %7 = fmul float %c2, %r1
    store float %7, float* %m12
    %8 = fmul float %c3, %r1
    store float %8, float* %m13
    %9 = fmul float %c0, %r2
    store float %9, float* %m20
    %10 = fmul float %c1, %r2
    store float %10, float* %m21
    %11 = fmul float %c2, %r2
    store float %11, float* %m22
    %12 = fmul float %c3, %r2
    store float %12, float* %m23
    %13 = load [3 x <4 x float>], [3 x <4 x float>]* %m

    ret [3 x <4 x float>] %13
}

; GLSL: mat4x3 = outerProduct(vec3, vec4)
define spir_func [4 x <3 x float>] @_Z12OuterProductDv3_fDv4_f(
    <3 x float> %c, <4 x float> %r) #0
{
    %m = alloca [4 x <3 x float>]
    %m0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x float>, <3 x float>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x float>, <3 x float>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x float>, <3 x float>* %m2, i32 0, i32 2

    %m3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <3 x float>, <3 x float>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <3 x float>, <3 x float>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <3 x float>, <3 x float>* %m3, i32 0, i32 2

    %c0 = extractelement <3 x float> %c, i32 0
    %c1 = extractelement <3 x float> %c, i32 1
    %c2 = extractelement <3 x float> %c, i32 2

    %r0 = extractelement <4 x float> %r, i32 0
    %r1 = extractelement <4 x float> %r, i32 1
    %r2 = extractelement <4 x float> %r, i32 2
    %r3 = extractelement <4 x float> %r, i32 3

    %1 = fmul float %c0, %r0
    store float %1, float* %m00
    %2 = fmul float %c1, %r0
    store float %2, float* %m01
    %3 = fmul float %c2, %r0
    store float %3, float* %m02
    %4 = fmul float %c0, %r1
    store float %4, float* %m10
    %5 = fmul float %c1, %r1
    store float %5, float* %m11
    %6 = fmul float %c2, %r1
    store float %6, float* %m12
    %7 = fmul float %c0, %r2
    store float %7, float* %m20
    %8 = fmul float %c1, %r2
    store float %8, float* %m21
    %9 = fmul float %c2, %r2
    store float %9, float* %m22
    %10 = fmul float %c0, %r3
    store float %10, float* %m30
    %11 = fmul float %c1, %r3
    store float %11, float* %m31
    %12 = fmul float %c2, %r3
    store float %12, float* %m32
    %13 = load [4 x <3 x float>], [4 x <3 x float>]* %m

    ret [4 x <3 x float>] %13
}

; GLSL: mat2 = transpose(mat2)
define spir_func [2 x <2 x float>] @_Z9TransposeDv2_Dv2_f(
    [2 x <2 x float>] %m) #0
{
    %nm = alloca [2 x <2 x float>]
    %nm0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    %nmv = load [2 x <2 x float>], [2 x <2 x float>]* %nm
    ret [2 x <2 x float>] %nmv
}

; GLSL: mat3 = transpose(mat3)
define spir_func [3 x <3 x float>] @_Z9TransposeDv3_Dv3_f(
    [3 x <3 x float>] %m) #0
{
    %nm = alloca [3 x <3 x float>]
    %nm0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x float>] %m, 2
    %m2v0 = extractelement <3 x float> %m2v, i32 0
    %m2v1 = extractelement <3 x float> %m2v, i32 1
    %m2v2 = extractelement <3 x float> %m2v, i32 2

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    store float %m2v2, float* %nm22
    %nmv = load [3 x <3 x float>], [3 x <3 x float>]* %nm
    ret [3 x <3 x float>] %nmv
}

; GLSL: mat4 = transpose(mat4)
define spir_func [4 x <4 x float>] @_Z9TransposeDv4_Dv4_f(
    [4 x <4 x float>] %m) #0
{
    %nm = alloca [4 x <4 x float>]
    %nm0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m2v0 = extractelement <4 x float> %m2v, i32 0
    %m2v1 = extractelement <4 x float> %m2v, i32 1
    %m2v2 = extractelement <4 x float> %m2v, i32 2
    %m2v3 = extractelement <4 x float> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x float>] %m, 3
    %m3v0 = extractelement <4 x float> %m3v, i32 0
    %m3v1 = extractelement <4 x float> %m3v, i32 1
    %m3v2 = extractelement <4 x float> %m3v, i32 2
    %m3v3 = extractelement <4 x float> %m3v, i32 3

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m3v0, float* %nm03
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    store float %m3v1, float* %nm13
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    store float %m2v2, float* %nm22
    store float %m3v2, float* %nm23
    store float %m0v3, float* %nm30
    store float %m1v3, float* %nm31
    store float %m2v3, float* %nm32
    store float %m3v3, float* %nm33
    %nmv = load [4 x <4 x float>], [4 x <4 x float>]* %nm
    ret [4 x <4 x float>] %nmv
}

; GLSL: mat2x3 = transpose(mat3x2)
define spir_func [2 x <3 x float>] @_Z9TransposeDv3_Dv2_f(
    [3 x <2 x float>] %m) #0
{
    %nm = alloca [2 x <3 x float>]
    %nm0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x float>] %m, 2
    %m2v0 = extractelement <2 x float> %m2v, i32 0
    %m2v1 = extractelement <2 x float> %m2v, i32 1

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    %nmv = load [2 x <3 x float>], [2 x <3 x float>]* %nm
    ret [2 x <3 x float>] %nmv
}

; GLSL: mat3x2 = transpose(mat2x3)
define spir_func [3 x <2 x float>] @_Z9TransposeDv2_Dv3_f(
    [2 x <3 x float>] %m) #0
{
    %nm = alloca [3 x <2 x float>]
    %nm0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    %nmv = load [3 x <2 x float>], [3 x <2 x float>]* %nm
    ret [3 x <2 x float>] %nmv
}

; GLSL: mat2x4 = transpose(mat4x2)
define spir_func [2 x <4 x float>] @_Z9TransposeDv4_Dv2_f(
    [4 x <2 x float>] %m) #0
{
    %nm = alloca [2 x <4 x float>]
    %nm0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m2v0 = extractelement <2 x float> %m2v, i32 0
    %m2v1 = extractelement <2 x float> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x float>] %m, 3
    %m3v0 = extractelement <2 x float> %m3v, i32 0
    %m3v1 = extractelement <2 x float> %m3v, i32 1

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m3v0, float* %nm03
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    store float %m3v1, float* %nm13
    %nmv = load [2 x <4 x float>], [2 x <4 x float>]* %nm
    ret [2 x <4 x float>] %nmv
}

; GLSL: mat4x2 = transpose(mat2x4)
define spir_func [4 x <2 x float>] @_Z9TransposeDv2_Dv4_f(
    [2 x <4 x float>] %m) #0
{
    %nm = alloca [4 x <2 x float>]
    %nm0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x float>, <2 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x float>, <2 x float>* %nm3, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    store float %m0v3, float* %nm30
    store float %m1v3, float* %nm31
    %nmv = load [4 x <2 x float>], [4 x <2 x float>]* %nm
    ret [4 x <2 x float>] %nmv
}

; GLSL: mat3x4 = transpose(mat4x3)
define spir_func [3 x <4 x float>] @_Z9TransposeDv4_Dv3_f(
    [4 x <3 x float>] %m) #0
{
    %nm = alloca [3 x <4 x float>]
    %nm0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m2v0 = extractelement <3 x float> %m2v, i32 0
    %m2v1 = extractelement <3 x float> %m2v, i32 1
    %m2v2 = extractelement <3 x float> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x float>] %m, 3
    %m3v0 = extractelement <3 x float> %m3v, i32 0
    %m3v1 = extractelement <3 x float> %m3v, i32 1
    %m3v2 = extractelement <3 x float> %m3v, i32 2

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m3v0, float* %nm03
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    store float %m3v1, float* %nm13
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    store float %m2v2, float* %nm22
    store float %m3v2, float* %nm23
    %nmv = load [3 x <4 x float>], [3 x <4 x float>]* %nm
    ret [3 x <4 x float>] %nmv
}

; GLSL: mat4x3 = transpose(mat3x4)
define spir_func [4 x <3 x float>] @_Z9TransposeDv3_Dv4_f(
    [3 x <4 x float>] %m) #0
{
    %nm = alloca [4 x <3 x float>]
    %nm0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x float>] %m, 2
    %m2v0 = extractelement <4 x float> %m2v, i32 0
    %m2v1 = extractelement <4 x float> %m2v, i32 1
    %m2v2 = extractelement <4 x float> %m2v, i32 2
    %m2v3 = extractelement <4 x float> %m2v, i32 3

    store float %m0v0, float* %nm00
    store float %m1v0, float* %nm01
    store float %m2v0, float* %nm02
    store float %m0v1, float* %nm10
    store float %m1v1, float* %nm11
    store float %m2v1, float* %nm12
    store float %m0v2, float* %nm20
    store float %m1v2, float* %nm21
    store float %m2v2, float* %nm22
    store float %m0v3, float* %nm30
    store float %m1v3, float* %nm31
    store float %m2v3, float* %nm32
    %nmv = load [4 x <3 x float>], [4 x <3 x float>]* %nm
    ret [4 x <3 x float>] %nmv
}

; GLSL: mat2 = mat2 * float
define spir_func [2 x <2 x float>] @_Z17MatrixTimesScalarDv2_Dv2_ff(
    [2 x <2 x float>] %m, float %s) #0
{
    %nm = alloca [2 x <2 x float>]
    %nm0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m1v0, %s
    store float %3, float* %nm10
    %4 = fmul float %m1v1, %s
    store float %4, float* %nm11
    %5 = load [2 x <2 x float>], [2 x <2 x float>]* %nm

    ret [2 x <2 x float>] %5
}

; GLSL: mat3 = mat3 * float
define spir_func [3 x <3 x float>] @_Z17MatrixTimesScalarDv3_Dv3_ff(
    [3 x <3 x float>] %m, float %s) #0
{
    %nm = alloca [3 x <3 x float>]
    %nm0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x float>] %m, 2
    %m2v0 = extractelement <3 x float> %m2v, i32 0
    %m2v1 = extractelement <3 x float> %m2v, i32 1
    %m2v2 = extractelement <3 x float> %m2v, i32 2

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m1v0, %s
    store float %4, float* %nm10
    %5 = fmul float %m1v1, %s
    store float %5, float* %nm11
    %6 = fmul float %m1v2, %s
    store float %6, float* %nm12
    %7 = fmul float %m2v0, %s
    store float %7, float* %nm20
    %8 = fmul float %m2v1, %s
    store float %8, float* %nm21
    %9 = fmul float %m2v2, %s
    store float %9, float* %nm22
    %10 = load [3 x <3 x float>], [3 x <3 x float>]* %nm

    ret [3 x <3 x float>] %10
}

; GLSL: mat4 = mat4 * float
define spir_func [4 x <4 x float>] @_Z17MatrixTimesScalarDv4_Dv4_ff(
    [4 x <4 x float>] %m, float %s) #0
{
    %nm = alloca [4 x <4 x float>]
    %nm0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x float>, <4 x float>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m2v0 = extractelement <4 x float> %m2v, i32 0
    %m2v1 = extractelement <4 x float> %m2v, i32 1
    %m2v2 = extractelement <4 x float> %m2v, i32 2
    %m2v3 = extractelement <4 x float> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x float>] %m, 3
    %m3v0 = extractelement <4 x float> %m3v, i32 0
    %m3v1 = extractelement <4 x float> %m3v, i32 1
    %m3v2 = extractelement <4 x float> %m3v, i32 2
    %m3v3 = extractelement <4 x float> %m3v, i32 3

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m0v3, %s
    store float %4, float* %nm03
    %5 = fmul float %m1v0, %s
    store float %5, float* %nm10
    %6 = fmul float %m1v1, %s
    store float %6, float* %nm11
    %7 = fmul float %m1v2, %s
    store float %7, float* %nm12
    %8 = fmul float %m1v3, %s
    store float %8, float* %nm13
    %9 = fmul float %m2v0, %s
    store float %9, float* %nm20
    %10 = fmul float %m2v1, %s
    store float %10, float* %nm21
    %11 = fmul float %m2v2, %s
    store float %11, float* %nm22
    %12 = fmul float %m2v3, %s
    store float %12, float* %nm23
    %13 = fmul float %m3v0, %s
    store float %13, float* %nm30
    %14 = fmul float %m3v1, %s
    store float %14, float* %nm31
    %15 = fmul float %m3v2, %s
    store float %15, float* %nm32
    %16 = fmul float %m3v3, %s
    store float %16, float* %nm33
    %17 = load [4 x <4 x float>], [4 x <4 x float>]* %nm

    ret [4 x <4 x float>] %17
}

; GLSL: mat3x2 = mat3x2 * float
define spir_func [3 x <2 x float>] @_Z17MatrixTimesScalarDv3_Dv2_ff(
    [3 x <2 x float>] %m, float %s) #0
{
    %nm = alloca [3 x <2 x float>]
    %nm0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 1

    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x float>] %m, 2
    %m2v0 = extractelement <2 x float> %m2v, i32 0
    %m2v1 = extractelement <2 x float> %m2v, i32 1

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m1v0, %s
    store float %3, float* %nm10
    %4 = fmul float %m1v1, %s
    store float %4, float* %nm11
    %5 = fmul float %m2v0, %s
    store float %5, float* %nm20
    %6 = fmul float %m2v1, %s
    store float %6, float* %nm21
    %7 = load [3 x <2 x float>], [3 x <2 x float>]* %nm

    ret [3 x <2 x float>] %7
}

; GLSL: mat2x3 = mat2x3 * float
define spir_func [2 x <3 x float>] @_Z17MatrixTimesScalarDv2_Dv3_ff(
    [2 x <3 x float>] %m, float %s) #0
{
    %nm = alloca [2 x <3 x float>]
    %nm0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m1v0, %s
    store float %4, float* %nm10
    %5 = fmul float %m1v1, %s
    store float %5, float* %nm11
    %6 = fmul float %m1v2, %s
    store float %6, float* %nm12
    %7 = load [2 x <3 x float>], [2 x <3 x float>]* %nm

    ret [2 x <3 x float>] %7
}

; GLSL: mat4x2 = mat4x2 * float
define spir_func [4 x <2 x float>] @_Z17MatrixTimesScalarDv4_Dv2_ff(
    [4 x <2 x float>] %m, float %s) #0
{
    %nm = alloca [4 x <2 x float>]
    %nm0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x float>, <2 x float>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x float>, <2 x float>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x float>, <2 x float>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x float>, <2 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x float>, <2 x float>* %nm3, i32 0, i32 1

    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m2v0 = extractelement <2 x float> %m2v, i32 0
    %m2v1 = extractelement <2 x float> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x float>] %m, 3
    %m3v0 = extractelement <2 x float> %m3v, i32 0
    %m3v1 = extractelement <2 x float> %m3v, i32 1

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m1v0, %s
    store float %3, float* %nm10
    %4 = fmul float %m1v1, %s
    store float %4, float* %nm11
    %5 = fmul float %m2v0, %s
    store float %5, float* %nm20
    %6 = fmul float %m2v1, %s
    store float %6, float* %nm21
    %7 = fmul float %m3v0, %s
    store float %7, float* %nm30
    %8 = fmul float %m3v1, %s
    store float %8, float* %nm31
    %9 = load [4 x <2 x float>], [4 x <2 x float>]* %nm

    ret [4 x <2 x float>] %9
}

; GLSL: mat2x4 = mat2x4 * float
define spir_func [2 x <4 x float>] @_Z17MatrixTimesScalarDv2_Dv4_ff(
    [2 x <4 x float>] %m, float %s) #0
{
    %nm = alloca [2 x <4 x float>]
    %nm0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m0v3, %s
    store float %4, float* %nm03
    %5 = fmul float %m1v0, %s
    store float %5, float* %nm10
    %6 = fmul float %m1v1, %s
    store float %6, float* %nm11
    %7 = fmul float %m1v2, %s
    store float %7, float* %nm12
    %8 = fmul float %m1v3, %s
    store float %8, float* %nm13
    %9 = load [2 x <4 x float>], [2 x <4 x float>]* %nm

    ret [2 x <4 x float>] %9
}

; GLSL: mat4x3 = mat4x3 * float
define spir_func [4 x <3 x float>] @_Z17MatrixTimesScalarDv4_Dv3_ff(
    [4 x <3 x float>] %m, float %s) #0
{
    %nm = alloca [4 x <3 x float>]
    %nm0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x float>, <3 x float>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x float>, <3 x float>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x float>, <3 x float>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x float>, <3 x float>* %nm3, i32 0, i32 2

    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m2v0 = extractelement <3 x float> %m2v, i32 0
    %m2v1 = extractelement <3 x float> %m2v, i32 1
    %m2v2 = extractelement <3 x float> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x float>] %m, 3
    %m3v0 = extractelement <3 x float> %m3v, i32 0
    %m3v1 = extractelement <3 x float> %m3v, i32 1
    %m3v2 = extractelement <3 x float> %m3v, i32 2

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m1v0, %s
    store float %4, float* %nm10
    %5 = fmul float %m1v1, %s
    store float %5, float* %nm11
    %6 = fmul float %m1v2, %s
    store float %6, float* %nm12
    %7 = fmul float %m2v0, %s
    store float %7, float* %nm20
    %8 = fmul float %m2v1, %s
    store float %8, float* %nm21
    %9 = fmul float %m2v2, %s
    store float %9, float* %nm22
    %10 = fmul float %m3v0, %s
    store float %10, float* %nm30
    %11 = fmul float %m3v1, %s
    store float %11, float* %nm31
    %12 = fmul float %m3v2, %s
    store float %12, float* %nm32
    %13 = load [4 x <3 x float>], [4 x <3 x float>]* %nm

    ret [4 x <3 x float>] %13
}

; GLSL: mat3x4 = mat3x4 * float
define spir_func [3 x <4 x float>] @_Z17MatrixTimesScalarDv3_Dv4_ff(
    [3 x <4 x float>] %m, float %s) #0
{
    %nm = alloca [3 x <4 x float>]
    %nm0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x float>, <4 x float>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x float>, <4 x float>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x float>, <4 x float>* %nm2, i32 0, i32 3

    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x float>] %m, 2
    %m2v0 = extractelement <4 x float> %m2v, i32 0
    %m2v1 = extractelement <4 x float> %m2v, i32 1
    %m2v2 = extractelement <4 x float> %m2v, i32 2
    %m2v3 = extractelement <4 x float> %m2v, i32 3

    %1 = fmul float %m0v0, %s
    store float %1, float* %nm00
    %2 = fmul float %m0v1, %s
    store float %2, float* %nm01
    %3 = fmul float %m0v2, %s
    store float %3, float* %nm02
    %4 = fmul float %m0v3, %s
    store float %4, float* %nm03
    %5 = fmul float %m1v0, %s
    store float %5, float* %nm10
    %6 = fmul float %m1v1, %s
    store float %6, float* %nm11
    %7 = fmul float %m1v2, %s
    store float %7, float* %nm12
    %8 = fmul float %m1v3, %s
    store float %8, float* %nm13
    %9 = fmul float %m2v0, %s
    store float %9, float* %nm20
    %10 = fmul float %m2v1, %s
    store float %10, float* %nm21
    %11 = fmul float %m2v2, %s
    store float %11, float* %nm22
    %12 = fmul float %m2v3, %s
    store float %12, float* %nm23
    %13 = load [3 x <4 x float>], [3 x <4 x float>]* %nm

    ret [3 x <4 x float>] %13
}

; GLSL: vec2 = vec2 * mat2
define spir_func <2 x float> @_Z17VectorTimesMatrixDv2_fDv2_Dv2_f(
    <2 x float> %c, [2 x <2 x float>] %m) #0
{
    %nv = alloca <2 x float>
    %nvp0 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %1 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m0v, <2 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [2 x <2 x float>] %m, 1
    %2 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m1v, <2 x float> %c)
    store float %2, float* %nvp1

    %3 = load <2 x float>, <2 x float>* %nv

    ret <2 x float> %3
}

; GLSL: vec3 = vec3 * mat3
define spir_func <3 x float> @_Z17VectorTimesMatrixDv3_fDv3_Dv3_f(
    <3 x float> %c, [3 x <3 x float>] %m) #0
{
    %nv = alloca <3 x float>
    %nvp0 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %1 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m0v, <3 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %2 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m1v, <3 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [3 x <3 x float>] %m, 2
    %3 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m2v, <3 x float> %c)
    store float %3, float* %nvp2

    %4 = load <3 x float>, <3 x float>* %nv

    ret <3 x float> %4
}

; GLSL: vec4 = vec4 * mat4
define spir_func <4 x float> @_Z17VectorTimesMatrixDv4_fDv4_Dv4_f(
    <4 x float> %c, [4 x <4 x float>] %m) #0
{
    %nv = alloca <4 x float>
    %nvp0 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %1 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m0v, <4 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %2 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m1v, <4 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %3 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m2v, <4 x float> %c)
    store float %3, float* %nvp2
    %m3v = extractvalue [4 x <4 x float>] %m, 3
    %4 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m3v, <4 x float> %c)
    store float %4, float* %nvp3

    %5 = load <4 x float>, <4 x float>* %nv

    ret <4 x float> %5
}

; GLSL: vec3 = vec2 * mat3x2
define spir_func <3 x float> @_Z17VectorTimesMatrixDv2_fDv3_Dv2_f(
    <2 x float> %c, [3 x <2 x float>] %m) #0
{
    %nv = alloca <3 x float>
    %nvp0 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %1 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m0v, <2 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %2 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m1v, <2 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [3 x <2 x float>] %m, 2
    %3 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m2v, <2 x float> %c)
    store float %3, float* %nvp2

    %4 = load <3 x float>, <3 x float>* %nv

    ret <3 x float> %4
}

; GLSL: vec4 = vec2 * mat4x2
define spir_func <4 x float> @_Z17VectorTimesMatrixDv2_fDv4_Dv2_f(
    <2 x float> %c, [4 x <2 x float>] %m) #0
{
    %nv = alloca <4 x float>
    %nvp0 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %1 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m0v, <2 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %2 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m1v, <2 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %3 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m2v, <2 x float> %c)
    store float %3, float* %nvp2
    %m3v = extractvalue [4 x <2 x float>] %m, 3
    %4 = call float @_Z3dotDv2_fDv2_f(<2 x float> %m3v, <2 x float> %c)
    store float %4, float* %nvp3

    %5 = load <4 x float>, <4 x float>* %nv

    ret <4 x float> %5
}

; GLSL: vec2 = vec3 * mat2x3
define spir_func <2 x float> @_Z17VectorTimesMatrixDv3_fDv2_Dv3_f(
    <3 x float> %c, [2 x <3 x float>] %m) #0
{
    %nv = alloca <2 x float>
    %nvp0 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %1 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m0v, <3 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [2 x <3 x float>] %m, 1
    %2 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m1v, <3 x float> %c)
    store float %2, float* %nvp1

    %3 = load <2 x float>, <2 x float>* %nv

    ret <2 x float> %3
}

; GLSL: vec4 = vec3 * mat4x3
define spir_func <4 x float> @_Z17VectorTimesMatrixDv3_fDv4_Dv3_f(
    <3 x float> %c, [4 x <3 x float>] %m) #0
{
    %nv = alloca <4 x float>
    %nvp0 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x float>, <4 x float>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %1 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m0v, <3 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %2 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m1v, <3 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %3 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m2v, <3 x float> %c)
    store float %3, float* %nvp2
    %m3v = extractvalue [4 x <3 x float>] %m, 3
    %4 = call float @_Z3dotDv3_fDv3_f(<3 x float> %m3v, <3 x float> %c)
    store float %4, float* %nvp3

    %5 = load <4 x float>, <4 x float>* %nv

    ret <4 x float> %5
}

; GLSL: vec2 = vec4 * mat2x4
define spir_func <2 x float> @_Z17VectorTimesMatrixDv4_fDv2_Dv4_f(
    <4 x float> %c, [2 x <4 x float>] %m) #0
{
    %nv = alloca <2 x float>
    %nvp0 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x float>, <2 x float>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %1 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m0v, <4 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [2 x <4 x float>] %m, 1
    %2 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m1v, <4 x float> %c)
    store float %2, float* %nvp1

    %3 = load <2 x float>, <2 x float>* %nv

    ret <2 x float> %3
}

; GLSL: vec3 = vec4 * mat3x4
define spir_func <3 x float> @_Z17VectorTimesMatrixDv4_fDv3_Dv4_f(
    <4 x float> %c, [3 x <4 x float>] %m) #0
{
    %nv = alloca <3 x float>
    %nvp0 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x float>, <3 x float>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %1 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m0v, <4 x float> %c)
    store float %1, float* %nvp0
    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %2 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m1v, <4 x float> %c)
    store float %2, float* %nvp1
    %m2v = extractvalue [3 x <4 x float>] %m, 2
    %3 = call float @_Z3dotDv4_fDv4_f(<4 x float> %m2v, <4 x float> %c)
    store float %3, float* %nvp2

    %4 = load <3 x float>, <3 x float>* %nv

    ret <3 x float> %4
}

; GLSL: vec2 = mat2 * vec2
define spir_func <2 x float> @_Z17MatrixTimesVectorDv2_Dv2_fDv2_f(
    [2 x <2 x float>] %m, <2 x float> %r) #0
{
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m1v = extractvalue [2 x <2 x float>] %m, 1

    %r0 = shufflevector <2 x float> %r, <2 x float> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %r0
    %r1 = shufflevector <2 x float> %r, <2 x float> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %r1
    %3 = fadd <2 x float> %2, %1

    ret <2 x float> %3
}

; GLSL: vec3 = mat3 * vec3
define spir_func <3 x float> @_Z17MatrixTimesVectorDv3_Dv3_fDv3_f(
    [3 x <3 x float>] %m, <3 x float> %r) #0
{
    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m2v = extractvalue [3 x <3 x float>] %m, 2

    %r0 = shufflevector <3 x float> %r, <3 x float> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %r0
    %r1 = shufflevector <3 x float> %r, <3 x float> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %r1
    %3 = fadd <3 x float> %2, %1
    %r2 = shufflevector <3 x float> %r, <3 x float> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %r2
    %5 = fadd <3 x float> %4, %3

    ret <3 x float> %5
}

; GLSL: vec4 = mat4 * vec4
define spir_func <4 x float> @_Z17MatrixTimesVectorDv4_Dv4_fDv4_f(
    [4 x <4 x float>] %m, <4 x float> %r) #0
{
    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m3v = extractvalue [4 x <4 x float>] %m, 3

    %r0 = shufflevector <4 x float> %r, <4 x float> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %r0
    %r1 = shufflevector <4 x float> %r, <4 x float> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %r1
    %3 = fadd <4 x float> %2, %1
    %r2 = shufflevector <4 x float> %r, <4 x float> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %r2
    %5 = fadd <4 x float> %4, %3
    %r3 = shufflevector <4 x float> %r, <4 x float> %r, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x float> %m3v, %r3
    %7 = fadd <4 x float> %6, %5

    ret <4 x float> %7
}

; GLSL: vec2 = mat3x2 * vec3
define spir_func <2 x float> @_Z17MatrixTimesVectorDv3_Dv2_fDv3_f(
    [3 x <2 x float>] %m, <3 x float> %r) #0
{
    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m2v = extractvalue [3 x <2 x float>] %m, 2

    %r0 = shufflevector <3 x float> %r, <3 x float> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %r0
    %r1 = shufflevector <3 x float> %r, <3 x float> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %r1
    %3 = fadd <2 x float> %2, %1
    %r2 = shufflevector <3 x float> %r, <3 x float> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %r2
    %5 = fadd <2 x float> %4, %3

    ret <2 x float> %5
}

; GLSL: vec3 = mat2x3 * vec2
define spir_func <3 x float> @_Z17MatrixTimesVectorDv2_Dv3_fDv2_f(
    [2 x <3 x float>] %m, <2 x float> %r) #0
{
    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m1v = extractvalue [2 x <3 x float>] %m, 1

    %r0 = shufflevector <2 x float> %r, <2 x float> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %r0
    %r1 = shufflevector <2 x float> %r, <2 x float> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %r1
    %3 = fadd <3 x float> %2, %1

    ret <3 x float> %3
}

; GLSL: vec2 = mat4x2 * vec4
define spir_func <2 x float> @_Z17MatrixTimesVectorDv4_Dv2_fDv4_f(
    [4 x <2 x float>] %m, <4 x float> %r) #0
{
    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m3v = extractvalue [4 x <2 x float>] %m, 3

    %r0 = shufflevector <4 x float> %r, <4 x float> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %r0
    %r1 = shufflevector <4 x float> %r, <4 x float> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %r1
    %3 = fadd <2 x float> %2, %1
    %r2 = shufflevector <4 x float> %r, <4 x float> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %r2
    %5 = fadd <2 x float> %4, %3
    %r3 = shufflevector <4 x float> %r, <4 x float> %r, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x float> %m3v, %r3
    %7 = fadd <2 x float> %6, %5

    ret <2 x float> %7
}

; GLSL: vec4 = mat2x4 * vec2
define spir_func <4 x float> @_Z17MatrixTimesVectorDv2_Dv4_fDv2_f(
    [2 x <4 x float>] %m, <2 x float> %r) #0
{
    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m1v = extractvalue [2 x <4 x float>] %m, 1

    %r0 = shufflevector <2 x float> %r, <2 x float> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %r0
    %r1 = shufflevector <2 x float> %r, <2 x float> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %r1
    %3 = fadd <4 x float> %2, %1

    ret <4 x float> %3
}

; GLSL: vec3 = mat4x3 * vec4
define spir_func <3 x float> @_Z17MatrixTimesVectorDv4_Dv3_fDv4_f(
    [4 x <3 x float>] %m, <4 x float> %r) #0
{
    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m3v = extractvalue [4 x <3 x float>] %m, 3

    %r0 = shufflevector <4 x float> %r, <4 x float> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %r0
    %r1 = shufflevector <4 x float> %r, <4 x float> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %r1
    %3 = fadd <3 x float> %2, %1
    %r2 = shufflevector <4 x float> %r, <4 x float> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %r2
    %5 = fadd <3 x float> %4, %3
    %r3 = shufflevector <4 x float> %r, <4 x float> %r, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x float> %m3v, %r3
    %7 = fadd <3 x float> %6, %5

    ret <3 x float> %7
}

; GLSL: vec4 = mat3x4 * vec3
define spir_func <4 x float> @_Z17MatrixTimesVectorDv3_Dv4_fDv3_f(
    [3 x <4 x float>] %m, <3 x float> %r) #0
{
    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m2v = extractvalue [3 x <4 x float>] %m, 2

    %r0 = shufflevector <3 x float> %r, <3 x float> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %r0
    %r1 = shufflevector <3 x float> %r, <3 x float> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %r1
    %3 = fadd <4 x float> %2, %1
    %r2 = shufflevector <3 x float> %r, <3 x float> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %r2
    %5 = fadd <4 x float> %4, %3

    ret <4 x float> %5
}

; GLSL: mat2 = mat2 * mat2
define spir_func [2 x <2 x float>] @_Z17MatrixTimesMatrixDv2_Dv2_fDv2_Dv2_f(
    [2 x <2 x float>] %m, [2 x <2 x float>] %rm) #0
{
    %nm = alloca [2 x <2 x float>]
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m1v = extractvalue [2 x <2 x float>] %m, 1

    %rm0v = extractvalue [2 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %nm0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %3, <2 x float>* %nm0

    %rm1v = extractvalue [2 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x float> %m1v, %rm1v1
    %6 = fadd <2 x float> %5, %4

    %nm1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %6, <2 x float>* %nm1

    %7 = load [2 x <2 x float>], [2 x <2 x float>]* %nm

    ret [2 x <2 x float>] %7
}

; GLSL: mat3x2 = mat2 * mat3x2
define spir_func [3 x <2 x float>] @_Z17MatrixTimesMatrixDv2_Dv2_fDv3_Dv2_f(
    [2 x <2 x float>] %m, [3 x <2 x float>] %rm) #0
{
    %nm = alloca [3 x <2 x float>]
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m1v = extractvalue [2 x <2 x float>] %m, 1

    %rm0v = extractvalue [3 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %nm0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %3, <2 x float>* %nm0

    %rm1v = extractvalue [3 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x float> %m1v, %rm1v1
    %6 = fadd <2 x float> %5, %4

    %nm1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %6, <2 x float>* %nm1

    %rm2v = extractvalue [3 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x float> %m1v, %rm2v1
    %9 = fadd <2 x float> %8, %7

    %nm2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %9, <2 x float>* %nm2

    %10 = load [3 x <2 x float>], [3 x <2 x float>]* %nm

    ret [3 x <2 x float>] %10
}

; GLSL: mat4x2 = mat2 * mat4x2
define spir_func [4 x <2 x float>] @_Z17MatrixTimesMatrixDv2_Dv2_fDv4_Dv2_f(
    [2 x <2 x float>] %m, [4 x <2 x float>] %rm) #0
{
    %nm = alloca [4 x <2 x float>]
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m1v = extractvalue [2 x <2 x float>] %m, 1

    %rm0v = extractvalue [4 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %nm0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %3, <2 x float>* %nm0

    %rm1v = extractvalue [4 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x float> %m1v, %rm1v1
    %6 = fadd <2 x float> %5, %4

    %nm1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %6, <2 x float>* %nm1

    %rm2v = extractvalue [4 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x float> %m1v, %rm2v1
    %9 = fadd <2 x float> %8, %7

    %nm2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %9, <2 x float>* %nm2

    %rm3v = extractvalue [4 x <2 x float>] %rm, 3
    %rm3v0 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <2 x i32> <i32 0, i32 0>
    %10 = fmul <2 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <2 x i32> <i32 1, i32 1>
    %11 = fmul <2 x float> %m1v, %rm3v1
    %12 = fadd <2 x float> %11, %10

    %nm3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 3
    store <2 x float> %12, <2 x float>* %nm3

    %13 = load [4 x <2 x float>], [4 x <2 x float>]* %nm

    ret [4 x <2 x float>] %13
}

; GLSL: mat3 = mat3 * mat3
define spir_func [3 x <3 x float>] @_Z17MatrixTimesMatrixDv3_Dv3_fDv3_Dv3_f(
    [3 x <3 x float>] %m, [3 x <3 x float>] %rm) #0
{
    %nm = alloca [3 x <3 x float>]
    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m2v = extractvalue [3 x <3 x float>] %m, 2

    %rm0v = extractvalue [3 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %nm0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %5, <3 x float>* %nm0

    %rm1v = extractvalue [3 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x float> %m1v, %rm1v1
    %8 = fadd <3 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x float> %m2v, %rm1v2
    %10 = fadd <3 x float> %9, %8

    %nm1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %10, <3 x float>* %nm1

    %rm2v = extractvalue [3 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x float> %m1v, %rm2v1
    %13 = fadd <3 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x float> %m2v, %rm2v2
    %15 = fadd <3 x float> %14, %13

    %nm2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %15, <3 x float>* %nm2

    %16 = load [3 x <3 x float>], [3 x <3 x float>]* %nm

    ret [3 x <3 x float>] %16
}

; GLSL: mat2x3 = mat3 * mat2x3
define spir_func [2 x <3 x float>] @_Z17MatrixTimesMatrixDv3_Dv3_fDv2_Dv3_f(
    [3 x <3 x float>] %m, [2 x <3 x float>] %rm) #0
{
    %nm = alloca [2 x <3 x float>]
    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m2v = extractvalue [3 x <3 x float>] %m, 2

    %rm0v = extractvalue [2 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %nm0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %5, <3 x float>* %nm0

    %rm1v = extractvalue [2 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x float> %m1v, %rm1v1
    %8 = fadd <3 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x float> %m2v, %rm1v2
    %10 = fadd <3 x float> %9, %8

    %nm1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %10, <3 x float>* %nm1

    %11 = load [2 x <3 x float>], [2 x <3 x float>]* %nm

    ret [2 x <3 x float>] %11
}

; GLSL: mat4x3 = mat3 * mat4x3
define spir_func [4 x <3 x float>] @_Z17MatrixTimesMatrixDv3_Dv3_fDv4_Dv3_f(
    [3 x <3 x float>] %m, [4 x <3 x float>] %rm) #0
{
    %nm = alloca [4 x <3 x float>]
    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m2v = extractvalue [3 x <3 x float>] %m, 2

    %rm0v = extractvalue [4 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %nm0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %5, <3 x float>* %nm0

    %rm1v = extractvalue [4 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x float> %m1v, %rm1v1
    %8 = fadd <3 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x float> %m2v, %rm1v2
    %10 = fadd <3 x float> %9, %8

    %nm1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %10, <3 x float>* %nm1

    %rm2v = extractvalue [4 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x float> %m1v, %rm2v1
    %13 = fadd <3 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x float> %m2v, %rm2v2
    %15 = fadd <3 x float> %14, %13

    %nm2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %15, <3 x float>* %nm2

    %rm3v = extractvalue [4 x <3 x float>] %rm, 3
    %rm3v0 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %16 = fmul <3 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %17 = fmul <3 x float> %m1v, %rm3v1
    %18 = fadd <3 x float> %17, %16

    %rm3v2 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %19 = fmul <3 x float> %m2v, %rm3v2
    %20 = fadd <3 x float> %19, %18

    %nm3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 3
    store <3 x float> %20, <3 x float>* %nm3

    %21 = load [4 x <3 x float>], [4 x <3 x float>]* %nm

    ret [4 x <3 x float>] %21
}

; GLSL: mat4 = mat4 * mat4
define spir_func [4 x <4 x float>] @_Z17MatrixTimesMatrixDv4_Dv4_fDv4_Dv4_f(
    [4 x <4 x float>] %m, [4 x <4 x float>] %rm) #0
{
    %nm = alloca [4 x <4 x float>]
    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m3v = extractvalue [4 x <4 x float>] %m, 3

    %rm0v = extractvalue [4 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x float> %m3v, %rm0v3
    %7 = fadd <4 x float> %6, %5

    %nm0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %7, <4 x float>* %nm0

    %rm1v = extractvalue [4 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x float> %m1v, %rm1v1
    %10 = fadd <4 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x float> %m2v, %rm1v2
    %12 = fadd <4 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x float> %m3v, %rm1v3
    %14 = fadd <4 x float> %13, %12

    %nm1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %14, <4 x float>* %nm1

    %rm2v = extractvalue [4 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x float> %m1v, %rm2v1
    %17 = fadd <4 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x float> %m2v, %rm2v2
    %19 = fadd <4 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x float> %m3v, %rm2v3
    %21 = fadd <4 x float> %20, %19

    %nm2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %21, <4 x float>* %nm2

    %rm3v = extractvalue [4 x <4 x float>] %rm, 3
    %rm3v0 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %22 = fmul <4 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %23 = fmul <4 x float> %m1v, %rm3v1
    %24 = fadd <4 x float> %23, %22

    %rm3v2 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %25 = fmul <4 x float> %m2v, %rm3v2
    %26 = fadd <4 x float> %25, %24

    %rm3v3 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %27 = fmul <4 x float> %m3v, %rm3v3
    %28 = fadd <4 x float> %27, %26

    %nm3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 3
    store <4 x float> %28, <4 x float>* %nm3

    %29 = load [4 x <4 x float>], [4 x <4 x float>]* %nm

    ret [4 x <4 x float>] %29
}

; GLSL: mat2x4 = mat4 * mat2x4
define spir_func [2 x <4 x float>] @_Z17MatrixTimesMatrixDv4_Dv4_fDv2_Dv4_f(
    [4 x <4 x float>] %m, [2 x <4 x float>] %rm) #0
{
    %nm = alloca [2 x <4 x float>]
    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m3v = extractvalue [4 x <4 x float>] %m, 3

    %rm0v = extractvalue [2 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x float> %m3v, %rm0v3
    %7 = fadd <4 x float> %6, %5

    %nm0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %7, <4 x float>* %nm0

    %rm1v = extractvalue [2 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x float> %m1v, %rm1v1
    %10 = fadd <4 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x float> %m2v, %rm1v2
    %12 = fadd <4 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x float> %m3v, %rm1v3
    %14 = fadd <4 x float> %13, %12

    %nm1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %14, <4 x float>* %nm1

    %15 = load [2 x <4 x float>], [2 x <4 x float>]* %nm

    ret [2 x <4 x float>] %15
}

; GLSL: mat3x4 = mat4 * mat3x4
define spir_func [3 x <4 x float>] @_Z17MatrixTimesMatrixDv4_Dv4_fDv3_Dv4_f(
    [4 x <4 x float>] %m, [3 x <4 x float>] %rm) #0
{
    %nm = alloca [3 x <4 x float>]
    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m3v = extractvalue [4 x <4 x float>] %m, 3

    %rm0v = extractvalue [3 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x float> %m3v, %rm0v3
    %7 = fadd <4 x float> %6, %5

    %nm0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %7, <4 x float>* %nm0

    %rm1v = extractvalue [3 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x float> %m1v, %rm1v1
    %10 = fadd <4 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x float> %m2v, %rm1v2
    %12 = fadd <4 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x float> %m3v, %rm1v3
    %14 = fadd <4 x float> %13, %12

    %nm1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %14, <4 x float>* %nm1

    %rm2v = extractvalue [3 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x float> %m1v, %rm2v1
    %17 = fadd <4 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x float> %m2v, %rm2v2
    %19 = fadd <4 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x float> %m3v, %rm2v3
    %21 = fadd <4 x float> %20, %19

    %nm2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %21, <4 x float>* %nm2

    %22 = load [3 x <4 x float>], [3 x <4 x float>]* %nm

    ret [3 x <4 x float>] %22
}

; GLSL: mat2 = mat3x2 * mat2x3
define spir_func [2 x <2 x float>] @_Z17MatrixTimesMatrixDv3_Dv2_fDv2_Dv3_f(
    [3 x <2 x float>] %m, [2 x <3 x float>] %rm) #0
{
    %nm = alloca [2 x <2 x float>]
    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m2v = extractvalue [3 x <2 x float>] %m, 2

    %rm0v = extractvalue [2 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %nm0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %5, <2 x float>* %nm0

    %rm1v = extractvalue [2 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x float> %m1v, %rm1v1
    %8 = fadd <2 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x float> %m2v, %rm1v2
    %10 = fadd <2 x float> %9, %8

    %nm1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %10, <2 x float>* %nm1

    %11 = load [2 x <2 x float>], [2 x <2 x float>]* %nm

    ret [2 x <2 x float>] %11
}

; GLSL: mat3x2 = mat3x2 * mat3
define spir_func [3 x <2 x float>] @_Z17MatrixTimesMatrixDv3_Dv2_fDv3_Dv3_f(
    [3 x <2 x float>] %m, [3 x <3 x float>] %rm) #0
{
    %nm = alloca [3 x <2 x float>]
    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m2v = extractvalue [3 x <2 x float>] %m, 2

    %rm0v = extractvalue [3 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %nm0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %5, <2 x float>* %nm0

    %rm1v = extractvalue [3 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x float> %m1v, %rm1v1
    %8 = fadd <2 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x float> %m2v, %rm1v2
    %10 = fadd <2 x float> %9, %8

    %nm1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %10, <2 x float>* %nm1

    %rm2v = extractvalue [3 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x float> %m1v, %rm2v1
    %13 = fadd <2 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x float> %m2v, %rm2v2
    %15 = fadd <2 x float> %14, %13

    %nm2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %15, <2 x float>* %nm2

    %16 = load [3 x <2 x float>], [3 x <2 x float>]* %nm

    ret [3 x <2 x float>] %16
}

; GLSL: mat4x2 = mat3x2 * mat4x3
define spir_func [4 x <2 x float>] @_Z17MatrixTimesMatrixDv3_Dv2_fDv4_Dv3_f(
    [3 x <2 x float>] %m, [4 x <3 x float>] %rm) #0
{
    %nm = alloca [4 x <2 x float>]
    %m0v = extractvalue [3 x <2 x float>] %m, 0
    %m1v = extractvalue [3 x <2 x float>] %m, 1
    %m2v = extractvalue [3 x <2 x float>] %m, 2

    %rm0v = extractvalue [4 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %nm0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %5, <2 x float>* %nm0

    %rm1v = extractvalue [4 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x float> %m1v, %rm1v1
    %8 = fadd <2 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x float> %m2v, %rm1v2
    %10 = fadd <2 x float> %9, %8

    %nm1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %10, <2 x float>* %nm1

    %rm2v = extractvalue [4 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x float> %m1v, %rm2v1
    %13 = fadd <2 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x float> %m2v, %rm2v2
    %15 = fadd <2 x float> %14, %13

    %nm2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %15, <2 x float>* %nm2

    %rm3v = extractvalue [4 x <3 x float>] %rm, 3
    %rm3v0 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <2 x i32> <i32 0, i32 0>
    %16 = fmul <2 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <2 x i32> <i32 1, i32 1>
    %17 = fmul <2 x float> %m1v, %rm3v1
    %18 = fadd <2 x float> %17, %16

    %rm3v2 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <2 x i32> <i32 2, i32 2>
    %19 = fmul <2 x float> %m2v, %rm3v2
    %20 = fadd <2 x float> %19, %18

    %nm3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 3
    store <2 x float> %20, <2 x float>* %nm3

    %21 = load [4 x <2 x float>], [4 x <2 x float>]* %nm

    ret [4 x <2 x float>] %21
}

; GLSL: mat2x3 = mat2x3 * mat2
define spir_func [2 x <3 x float>] @_Z17MatrixTimesMatrixDv2_Dv3_fDv2_Dv2_f(
    [2 x <3 x float>] %m, [2 x <2 x float>] %rm) #0
{
    %nm = alloca [2 x <3 x float>]
    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m1v = extractvalue [2 x <3 x float>] %m, 1

    %rm0v = extractvalue [2 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %nm0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %3, <3 x float>* %nm0

    %rm1v = extractvalue [2 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x float> %m1v, %rm1v1
    %6 = fadd <3 x float> %5, %4

    %nm1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %6, <3 x float>* %nm1

    %7 = load [2 x <3 x float>], [2 x <3 x float>]* %nm

    ret [2 x <3 x float>] %7
}

; GLSL: mat3 = mat2x3 * mat3x2
define spir_func [3 x <3 x float>] @_Z17MatrixTimesMatrixDv2_Dv3_fDv3_Dv2_f(
    [2 x <3 x float>] %m, [3 x <2 x float>] %rm) #0
{
    %nm = alloca [3 x <3 x float>]
    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m1v = extractvalue [2 x <3 x float>] %m, 1

    %rm0v = extractvalue [3 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %nm0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %3, <3 x float>* %nm0

    %rm1v = extractvalue [3 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x float> %m1v, %rm1v1
    %6 = fadd <3 x float> %5, %4

    %nm1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %6, <3 x float>* %nm1

    %rm2v = extractvalue [3 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x float> %m1v, %rm2v1
    %9 = fadd <3 x float> %8, %7

    %nm2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %9, <3 x float>* %nm2

    %10 = load [3 x <3 x float>], [3 x <3 x float>]* %nm

    ret [3 x <3 x float>] %10
}

; GLSL: mat4x3 = mat2x3 * mat4x2
define spir_func [4 x <3 x float>] @_Z17MatrixTimesMatrixDv2_Dv3_fDv4_Dv2_f(
    [2 x <3 x float>] %m, [4 x <2 x float>] %rm) #0
{
    %nm = alloca [4 x <3 x float>]
    %m0v = extractvalue [2 x <3 x float>] %m, 0
    %m1v = extractvalue [2 x <3 x float>] %m, 1

    %rm0v = extractvalue [4 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %nm0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %3, <3 x float>* %nm0

    %rm1v = extractvalue [4 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x float> %m1v, %rm1v1
    %6 = fadd <3 x float> %5, %4

    %nm1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %6, <3 x float>* %nm1

    %rm2v = extractvalue [4 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x float> %m1v, %rm2v1
    %9 = fadd <3 x float> %8, %7

    %nm2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %9, <3 x float>* %nm2

    %rm3v = extractvalue [4 x <2 x float>] %rm, 3
    %rm3v0 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %10 = fmul <3 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %11 = fmul <3 x float> %m1v, %rm3v1
    %12 = fadd <3 x float> %11, %10

    %nm3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 3
    store <3 x float> %12, <3 x float>* %nm3

    %13 = load [4 x <3 x float>], [4 x <3 x float>]* %nm

    ret [4 x <3 x float>] %13
}

; GLSL: mat2 = mat4x2 * mat2x4
define spir_func [2 x <2 x float>] @_Z17MatrixTimesMatrixDv4_Dv2_fDv2_Dv4_f(
    [4 x <2 x float>] %m, [2 x <4 x float>] %rm) #0
{
    %nm = alloca [2 x <2 x float>]
    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m3v = extractvalue [4 x <2 x float>] %m, 3

    %rm0v = extractvalue [2 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x float> %m3v, %rm0v3
    %7 = fadd <2 x float> %6, %5

    %nm0 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %7, <2 x float>* %nm0

    %rm1v = extractvalue [2 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x float> %m1v, %rm1v1
    %10 = fadd <2 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x float> %m2v, %rm1v2
    %12 = fadd <2 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x float> %m3v, %rm1v3
    %14 = fadd <2 x float> %13, %12

    %nm1 = getelementptr inbounds [2 x <2 x float>], [2 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %14, <2 x float>* %nm1

    %15 = load [2 x <2 x float>], [2 x <2 x float>]* %nm

    ret [2 x <2 x float>] %15
}

; GLSL: mat3x2 = mat4x2 * mat3x4
define spir_func [3 x <2 x float>] @_Z17MatrixTimesMatrixDv4_Dv2_fDv3_Dv4_f(
    [4 x <2 x float>] %m, [3 x <4 x float>] %rm) #0
{
    %nm = alloca [3 x <2 x float>]
    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m3v = extractvalue [4 x <2 x float>] %m, 3

    %rm0v = extractvalue [3 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x float> %m3v, %rm0v3
    %7 = fadd <2 x float> %6, %5

    %nm0 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %7, <2 x float>* %nm0

    %rm1v = extractvalue [3 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x float> %m1v, %rm1v1
    %10 = fadd <2 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x float> %m2v, %rm1v2
    %12 = fadd <2 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x float> %m3v, %rm1v3
    %14 = fadd <2 x float> %13, %12

    %nm1 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %14, <2 x float>* %nm1

    %rm2v = extractvalue [3 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x float> %m1v, %rm2v1
    %17 = fadd <2 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x float> %m2v, %rm2v2
    %19 = fadd <2 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x float> %m3v, %rm2v3
    %21 = fadd <2 x float> %20, %19

    %nm2 = getelementptr inbounds [3 x <2 x float>], [3 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %21, <2 x float>* %nm2

    %22 = load [3 x <2 x float>], [3 x <2 x float>]* %nm

    ret [3 x <2 x float>] %22
}

; GLSL: mat4x2 = mat4x2 * mat4
define spir_func [4 x <2 x float>] @_Z17MatrixTimesMatrixDv4_Dv2_fDv4_Dv4_f(
    [4 x <2 x float>] %m, [4 x <4 x float>] %rm) #0
{
    %nm = alloca [4 x <2 x float>]
    %m0v = extractvalue [4 x <2 x float>] %m, 0
    %m1v = extractvalue [4 x <2 x float>] %m, 1
    %m2v = extractvalue [4 x <2 x float>] %m, 2
    %m3v = extractvalue [4 x <2 x float>] %m, 3

    %rm0v = extractvalue [4 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x float> %m1v, %rm0v1
    %3 = fadd <2 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x float> %m2v, %rm0v2
    %5 = fadd <2 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x float> %m3v, %rm0v3
    %7 = fadd <2 x float> %6, %5

    %nm0 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 0
    store <2 x float> %7, <2 x float>* %nm0

    %rm1v = extractvalue [4 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x float> %m1v, %rm1v1
    %10 = fadd <2 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x float> %m2v, %rm1v2
    %12 = fadd <2 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x float> %m3v, %rm1v3
    %14 = fadd <2 x float> %13, %12

    %nm1 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 1
    store <2 x float> %14, <2 x float>* %nm1

    %rm2v = extractvalue [4 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x float> %m1v, %rm2v1
    %17 = fadd <2 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x float> %m2v, %rm2v2
    %19 = fadd <2 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x float> %m3v, %rm2v3
    %21 = fadd <2 x float> %20, %19

    %nm2 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 2
    store <2 x float> %21, <2 x float>* %nm2

    %rm3v = extractvalue [4 x <4 x float>] %rm, 3
    %rm3v0 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <2 x i32> <i32 0, i32 0>
    %22 = fmul <2 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <2 x i32> <i32 1, i32 1>
    %23 = fmul <2 x float> %m1v, %rm3v1
    %24 = fadd <2 x float> %23, %22

    %rm3v2 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <2 x i32> <i32 2, i32 2>
    %25 = fmul <2 x float> %m2v, %rm3v2
    %26 = fadd <2 x float> %25, %24

    %rm3v3 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <2 x i32> <i32 3, i32 3>
    %27 = fmul <2 x float> %m3v, %rm3v3
    %28 = fadd <2 x float> %27, %26

    %nm3 = getelementptr inbounds [4 x <2 x float>], [4 x <2 x float>]* %nm, i32 0, i32 3
    store <2 x float> %28, <2 x float>* %nm3

    %29 = load [4 x <2 x float>], [4 x <2 x float>]* %nm

    ret [4 x <2 x float>] %29
}

; GLSL: mat2x4 = mat2x4 * mat2
define spir_func [2 x <4 x float>] @_Z17MatrixTimesMatrixDv2_Dv4_fDv2_Dv2_f(
    [2 x <4 x float>] %m, [2 x <2 x float>] %rm) #0
{
    %nm = alloca [2 x <4 x float>]
    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m1v = extractvalue [2 x <4 x float>] %m, 1

    %rm0v = extractvalue [2 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %nm0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %3, <4 x float>* %nm0

    %rm1v = extractvalue [2 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x float> %m1v, %rm1v1
    %6 = fadd <4 x float> %5, %4

    %nm1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %6, <4 x float>* %nm1

    %7 = load [2 x <4 x float>], [2 x <4 x float>]* %nm

    ret [2 x <4 x float>] %7
}

; GLSL: mat3x4 = mat2x4 * mat3x2
define spir_func [3 x <4 x float>] @_Z17MatrixTimesMatrixDv2_Dv4_fDv3_Dv2_f(
    [2 x <4 x float>] %m, [3 x <2 x float>] %rm) #0
{
    %nm = alloca [3 x <4 x float>]
    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m1v = extractvalue [2 x <4 x float>] %m, 1

    %rm0v = extractvalue [3 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %nm0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %3, <4 x float>* %nm0

    %rm1v = extractvalue [3 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x float> %m1v, %rm1v1
    %6 = fadd <4 x float> %5, %4

    %nm1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %6, <4 x float>* %nm1

    %rm2v = extractvalue [3 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x float> %m1v, %rm2v1
    %9 = fadd <4 x float> %8, %7

    %nm2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %9, <4 x float>* %nm2

    %10 = load [3 x <4 x float>], [3 x <4 x float>]* %nm

    ret [3 x <4 x float>] %10
}

; GLSL: mat4 = mat2x4 * mat4x2
define spir_func [4 x <4 x float>] @_Z17MatrixTimesMatrixDv2_Dv4_fDv4_Dv2_f(
    [2 x <4 x float>] %m, [4 x <2 x float>] %rm) #0
{
    %nm = alloca [4 x <4 x float>]
    %m0v = extractvalue [2 x <4 x float>] %m, 0
    %m1v = extractvalue [2 x <4 x float>] %m, 1

    %rm0v = extractvalue [4 x <2 x float>] %rm, 0
    %rm0v0 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x float> %rm0v, <2 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %nm0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %3, <4 x float>* %nm0

    %rm1v = extractvalue [4 x <2 x float>] %rm, 1
    %rm1v0 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x float> %rm1v, <2 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x float> %m1v, %rm1v1
    %6 = fadd <4 x float> %5, %4

    %nm1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %6, <4 x float>* %nm1

    %rm2v = extractvalue [4 x <2 x float>] %rm, 2
    %rm2v0 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x float> %rm2v, <2 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x float> %m1v, %rm2v1
    %9 = fadd <4 x float> %8, %7

    %nm2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %9, <4 x float>* %nm2

    %rm3v = extractvalue [4 x <2 x float>] %rm, 3
    %rm3v0 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %10 = fmul <4 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x float> %rm3v, <2 x float> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %11 = fmul <4 x float> %m1v, %rm3v1
    %12 = fadd <4 x float> %11, %10

    %nm3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 3
    store <4 x float> %12, <4 x float>* %nm3

    %13 = load [4 x <4 x float>], [4 x <4 x float>]* %nm

    ret [4 x <4 x float>] %13
}

; GLSL: mat2x3 = mat4x3 * mat2x4
define spir_func [2 x <3 x float>] @_Z17MatrixTimesMatrixDv4_Dv3_fDv2_Dv4_f(
    [4 x <3 x float>] %m, [2 x <4 x float>] %rm) #0
{
    %nm = alloca [2 x <3 x float>]
    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m3v = extractvalue [4 x <3 x float>] %m, 3

    %rm0v = extractvalue [2 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x float> %m3v, %rm0v3
    %7 = fadd <3 x float> %6, %5

    %nm0 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %7, <3 x float>* %nm0

    %rm1v = extractvalue [2 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x float> %m1v, %rm1v1
    %10 = fadd <3 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x float> %m2v, %rm1v2
    %12 = fadd <3 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x float> %m3v, %rm1v3
    %14 = fadd <3 x float> %13, %12

    %nm1 = getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %14, <3 x float>* %nm1

    %15 = load [2 x <3 x float>], [2 x <3 x float>]* %nm

    ret [2 x <3 x float>] %15
}

; GLSL: mat3 = mat4x3 * mat3x4
define spir_func [3 x <3 x float>] @_Z17MatrixTimesMatrixDv4_Dv3_fDv3_Dv4_f(
    [4 x <3 x float>] %m, [3 x <4 x float>] %rm) #0
{
    %nm = alloca [3 x <3 x float>]
    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m3v = extractvalue [4 x <3 x float>] %m, 3

    %rm0v = extractvalue [3 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x float> %m3v, %rm0v3
    %7 = fadd <3 x float> %6, %5

    %nm0 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %7, <3 x float>* %nm0

    %rm1v = extractvalue [3 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x float> %m1v, %rm1v1
    %10 = fadd <3 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x float> %m2v, %rm1v2
    %12 = fadd <3 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x float> %m3v, %rm1v3
    %14 = fadd <3 x float> %13, %12

    %nm1 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %14, <3 x float>* %nm1

    %rm2v = extractvalue [3 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x float> %m1v, %rm2v1
    %17 = fadd <3 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x float> %m2v, %rm2v2
    %19 = fadd <3 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x float> %m3v, %rm2v3
    %21 = fadd <3 x float> %20, %19

    %nm2 = getelementptr inbounds [3 x <3 x float>], [3 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %21, <3 x float>* %nm2

    %22 = load [3 x <3 x float>], [3 x <3 x float>]* %nm

    ret [3 x <3 x float>] %22
}

; GLSL: mat4x3 = mat4x3 * mat4
define spir_func [4 x <3 x float>] @_Z17MatrixTimesMatrixDv4_Dv3_fDv4_Dv4_f(
    [4 x <3 x float>] %m, [4 x <4 x float>] %rm) #0
{
    %nm = alloca [4 x <3 x float>]
    %m0v = extractvalue [4 x <3 x float>] %m, 0
    %m1v = extractvalue [4 x <3 x float>] %m, 1
    %m2v = extractvalue [4 x <3 x float>] %m, 2
    %m3v = extractvalue [4 x <3 x float>] %m, 3

    %rm0v = extractvalue [4 x <4 x float>] %rm, 0
    %rm0v0 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x float> %m1v, %rm0v1
    %3 = fadd <3 x float> %2, %1

    %rm0v2 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x float> %m2v, %rm0v2
    %5 = fadd <3 x float> %4, %3

    %rm0v3 = shufflevector <4 x float> %rm0v, <4 x float> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x float> %m3v, %rm0v3
    %7 = fadd <3 x float> %6, %5

    %nm0 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 0
    store <3 x float> %7, <3 x float>* %nm0

    %rm1v = extractvalue [4 x <4 x float>] %rm, 1
    %rm1v0 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x float> %m1v, %rm1v1
    %10 = fadd <3 x float> %9, %8

    %rm1v2 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x float> %m2v, %rm1v2
    %12 = fadd <3 x float> %11, %10

    %rm1v3 = shufflevector <4 x float> %rm1v, <4 x float> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x float> %m3v, %rm1v3
    %14 = fadd <3 x float> %13, %12

    %nm1 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 1
    store <3 x float> %14, <3 x float>* %nm1

    %rm2v = extractvalue [4 x <4 x float>] %rm, 2
    %rm2v0 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x float> %m1v, %rm2v1
    %17 = fadd <3 x float> %16, %15

    %rm2v2 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x float> %m2v, %rm2v2
    %19 = fadd <3 x float> %18, %17

    %rm2v3 = shufflevector <4 x float> %rm2v, <4 x float> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x float> %m3v, %rm2v3
    %21 = fadd <3 x float> %20, %19

    %nm2 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 2
    store <3 x float> %21, <3 x float>* %nm2

    %rm3v = extractvalue [4 x <4 x float>] %rm, 3
    %rm3v0 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %22 = fmul <3 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %23 = fmul <3 x float> %m1v, %rm3v1
    %24 = fadd <3 x float> %23, %22

    %rm3v2 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %25 = fmul <3 x float> %m2v, %rm3v2
    %26 = fadd <3 x float> %25, %24

    %rm3v3 = shufflevector <4 x float> %rm3v, <4 x float> %rm3v, <3 x i32> <i32 3, i32 3, i32 3>
    %27 = fmul <3 x float> %m3v, %rm3v3
    %28 = fadd <3 x float> %27, %26

    %nm3 = getelementptr inbounds [4 x <3 x float>], [4 x <3 x float>]* %nm, i32 0, i32 3
    store <3 x float> %28, <3 x float>* %nm3

    %29 = load [4 x <3 x float>], [4 x <3 x float>]* %nm

    ret [4 x <3 x float>] %29
}

; GLSL: mat2x4 = mat3x4 * mat2x3
define spir_func [2 x <4 x float>] @_Z17MatrixTimesMatrixDv3_Dv4_fDv2_Dv3_f(
    [3 x <4 x float>] %m, [2 x <3 x float>] %rm) #0
{
    %nm = alloca [2 x <4 x float>]
    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m2v = extractvalue [3 x <4 x float>] %m, 2

    %rm0v = extractvalue [2 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %nm0 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %5, <4 x float>* %nm0

    %rm1v = extractvalue [2 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x float> %m1v, %rm1v1
    %8 = fadd <4 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x float> %m2v, %rm1v2
    %10 = fadd <4 x float> %9, %8

    %nm1 = getelementptr inbounds [2 x <4 x float>], [2 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %10, <4 x float>* %nm1

    %11 = load [2 x <4 x float>], [2 x <4 x float>]* %nm

    ret [2 x <4 x float>] %11
}

; GLSL: mat3x4 = mat3x4 * mat3
define spir_func [3 x <4 x float>] @_Z17MatrixTimesMatrixDv3_Dv4_fDv3_Dv3_f(
    [3 x <4 x float>] %m, [3 x <3 x float>] %rm) #0
{
    %nm = alloca [3 x <4 x float>]
    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m2v = extractvalue [3 x <4 x float>] %m, 2

    %rm0v = extractvalue [3 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %nm0 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %5, <4 x float>* %nm0

    %rm1v = extractvalue [3 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x float> %m1v, %rm1v1
    %8 = fadd <4 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x float> %m2v, %rm1v2
    %10 = fadd <4 x float> %9, %8

    %nm1 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %10, <4 x float>* %nm1

    %rm2v = extractvalue [3 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x float> %m1v, %rm2v1
    %13 = fadd <4 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x float> %m2v, %rm2v2
    %15 = fadd <4 x float> %14, %13

    %nm2 = getelementptr inbounds [3 x <4 x float>], [3 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %15, <4 x float>* %nm2

    %16 = load [3 x <4 x float>], [3 x <4 x float>]* %nm

    ret [3 x <4 x float>] %16
}

; GLSL: mat4 = mat3x4 * mat4x3
define spir_func [4 x <4 x float>] @_Z17MatrixTimesMatrixDv3_Dv4_fDv4_Dv3_f(
    [3 x <4 x float>] %m, [4 x <3 x float>] %rm) #0
{
    %nm = alloca [4 x <4 x float>]
    %m0v = extractvalue [3 x <4 x float>] %m, 0
    %m1v = extractvalue [3 x <4 x float>] %m, 1
    %m2v = extractvalue [3 x <4 x float>] %m, 2

    %rm0v = extractvalue [4 x <3 x float>] %rm, 0
    %rm0v0 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x float> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x float> %m1v, %rm0v1
    %3 = fadd <4 x float> %2, %1

    %rm0v2 = shufflevector <3 x float> %rm0v, <3 x float> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x float> %m2v, %rm0v2
    %5 = fadd <4 x float> %4, %3

    %nm0 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 0
    store <4 x float> %5, <4 x float>* %nm0

    %rm1v = extractvalue [4 x <3 x float>] %rm, 1
    %rm1v0 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x float> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x float> %m1v, %rm1v1
    %8 = fadd <4 x float> %7, %6

    %rm1v2 = shufflevector <3 x float> %rm1v, <3 x float> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x float> %m2v, %rm1v2
    %10 = fadd <4 x float> %9, %8

    %nm1 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 1
    store <4 x float> %10, <4 x float>* %nm1

    %rm2v = extractvalue [4 x <3 x float>] %rm, 2
    %rm2v0 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x float> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x float> %m1v, %rm2v1
    %13 = fadd <4 x float> %12, %11

    %rm2v2 = shufflevector <3 x float> %rm2v, <3 x float> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x float> %m2v, %rm2v2
    %15 = fadd <4 x float> %14, %13

    %nm2 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 2
    store <4 x float> %15, <4 x float>* %nm2

    %rm3v = extractvalue [4 x <3 x float>] %rm, 3
    %rm3v0 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %16 = fmul <4 x float> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %17 = fmul <4 x float> %m1v, %rm3v1
    %18 = fadd <4 x float> %17, %16

    %rm3v2 = shufflevector <3 x float> %rm3v, <3 x float> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %19 = fmul <4 x float> %m2v, %rm3v2
    %20 = fadd <4 x float> %19, %18

    %nm3 = getelementptr inbounds [4 x <4 x float>], [4 x <4 x float>]* %nm, i32 0, i32 3
    store <4 x float> %20, <4 x float>* %nm3

    %21 = load [4 x <4 x float>], [4 x <4 x float>]* %nm

    ret [4 x <4 x float>] %21
}

; GLSL helper: float = determinant2(vec2(float, float), vec2(float, float))
define spir_func float @llpc.determinant2.f32(
    float %x0, float %y0, float %x1, float %y1)
{
    ; | x0   x1 |
    ; |         | = x0 * y1 - y0 * x1
    ; | y0   y1 |

    %1 = fmul float %x0, %y1
    %2 = fmul float %y0, %x1
    %3 = fsub float %1, %2
    ret float %3
}

; GLSL helper: float = determinant3(vec3(float, float, float), vec3(float, float, float))
define spir_func float @llpc.determinant3.f32(
    float %x0, float %y0, float %z0,
    float %x1, float %y1, float %z1,
    float %x2, float %y2, float %z2)
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

    %1 = call float @llpc.determinant2.f32(float %y1, float %z1, float %y2, float %z2)
    %2 = fmul float %1, %x0
    %3 = call float @llpc.determinant2.f32(float %z1, float %x1, float %z2, float %x2)
    %4 = fmul float %3, %y0
    %5 = fadd float %2, %4
    %6 = call float @llpc.determinant2.f32(float %x1, float %y1, float %x2, float %y2)
    %7 = fmul float %6, %z0
    %8 = fadd float %7, %5
    ret float %8
}

; GLSL helper: float = determinant4(vec4(float, float, float, float), vec4(float, float, float, float))
define spir_func float @llpc.determinant4.f32(
    float %x0, float %y0, float %z0, float %w0,
    float %x1, float %y1, float %z1, float %w1,
    float %x2, float %y2, float %z2, float %w2,
    float %x3, float %y3, float %z3, float %w3)

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

    %1 = call float @llpc.determinant3.f32(float %y1, float %z1, float %w1, float %y2, float %z2, float %w2, float %y3, float %z3, float %w3)
    %2 = fmul float %1, %x0
    %3 = call float @llpc.determinant3.f32(float %z1, float %x1, float %w1, float %z2, float %x2, float %w2, float %z3, float %x3, float %w3)
    %4 = fmul float %3, %y0
    %5 = fadd float %2, %4
    %6 = call float @llpc.determinant3.f32(float %x1, float %y1, float %w1, float %x2, float %y2, float %w2, float %x3, float %y3, float %w3)
    %7 = fmul float %6, %z0
    %8 = fadd float %5, %7
    %9 = call float @llpc.determinant3.f32(float %y1, float %x1, float %z1, float %y2, float %x2, float %z2, float %y3, float %x3, float %z3)
    %10 = fmul float %9, %w0
    %11 = fadd float %8, %10
    ret float %11
}

; GLSL: float = determinant(mat2)
define spir_func float @_Z11determinantDv2_Dv2_f(
    [2 x <2 x float>] %m) #0
{
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %m0v0 = extractelement <2 x float> %m0v, i32 0
    %m0v1 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x float>] %m, 1
    %m1v0 = extractelement <2 x float> %m1v, i32 0
    %m1v1 = extractelement <2 x float> %m1v, i32 1

    %d = call float @llpc.determinant2.f32(float %m0v0, float %m0v1, float %m1v0, float %m1v1)
    ret float %d
}

; GLSL: float = determinant(mat3)
define spir_func float @_Z11determinantDv3_Dv3_f(
    [3 x <3 x float>] %m) #0
{
    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %m0v0 = extractelement <3 x float> %m0v, i32 0
    %m0v1 = extractelement <3 x float> %m0v, i32 1
    %m0v2 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %m1v0 = extractelement <3 x float> %m1v, i32 0
    %m1v1 = extractelement <3 x float> %m1v, i32 1
    %m1v2 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x float>] %m, 2
    %m2v0 = extractelement <3 x float> %m2v, i32 0
    %m2v1 = extractelement <3 x float> %m2v, i32 1
    %m2v2 = extractelement <3 x float> %m2v, i32 2

    %d = call float @llpc.determinant3.f32(
        float %m0v0, float %m0v1, float %m0v2,
        float %m1v0, float %m1v1, float %m1v2,
        float %m2v0, float %m2v1, float %m2v2)
    ret float %d
}

; GLSL: float = determinant(mat4)
define spir_func float @_Z11determinantDv4_Dv4_f(
    [4 x <4 x float>] %m) #0
{
    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %m0v0 = extractelement <4 x float> %m0v, i32 0
    %m0v1 = extractelement <4 x float> %m0v, i32 1
    %m0v2 = extractelement <4 x float> %m0v, i32 2
    %m0v3 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %m1v0 = extractelement <4 x float> %m1v, i32 0
    %m1v1 = extractelement <4 x float> %m1v, i32 1
    %m1v2 = extractelement <4 x float> %m1v, i32 2
    %m1v3 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %m2v0 = extractelement <4 x float> %m2v, i32 0
    %m2v1 = extractelement <4 x float> %m2v, i32 1
    %m2v2 = extractelement <4 x float> %m2v, i32 2
    %m2v3 = extractelement <4 x float> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x float>] %m, 3
    %m3v0 = extractelement <4 x float> %m3v, i32 0
    %m3v1 = extractelement <4 x float> %m3v, i32 1
    %m3v2 = extractelement <4 x float> %m3v, i32 2
    %m3v3 = extractelement <4 x float> %m3v, i32 3

    %d = call float @llpc.determinant4.f32(
        float %m0v0, float %m0v1, float %m0v2, float %m0v3,
        float %m1v0, float %m1v1, float %m1v2, float %m1v3,
        float %m2v0, float %m2v1, float %m2v2, float %m2v3,
        float %m3v0, float %m3v1, float %m3v2, float %m3v3)
    ret float %d
}

; GLSL helper: float = dot3(vec3(float, float, float), vec3(float, float, float))
define spir_func float @llpc.dot3.f32(
    float %x0, float %y0, float %z0,
    float %x1, float %y1, float %z1)
{
    %1 = fmul float %x1, %x0
    %2 = fmul float %y1, %y0
    %3 = fadd float %1, %2
    %4 = fmul float %z1, %z0
    %5 = fadd float %3, %4
    ret float %5
}

; GLSL helper: float = dot4(vec4(float, float, float, float), vec4(float, float, float, float))
define spir_func float @llpc.dot4.f32(
    float %x0, float %y0, float %z0, float %w0,
    float %x1, float %y1, float %z1, float %w1)
{
    %1 = fmul float %x1, %x0
    %2 = fmul float %y1, %y0
    %3 = fadd float %1, %2
    %4 = fmul float %z1, %z0
    %5 = fadd float %3, %4
    %6 = fmul float %w1, %w0
    %7 = fadd float %5, %6
    ret float %7
}

; GLSL: mat2 = inverse(mat2)
define spir_func [2 x <2 x float>] @_Z13matrixInverseDv2_Dv2_f(
    [2 x <2 x float>] %m) #0
{
    ; [ x0   x1 ]                    [  y1 -x1 ]
    ; [         ]  = (1 / det(M))) * [         ]
    ; [ y0   y1 ]                    [ -y0  x0 ]
    %m0v = extractvalue [2 x <2 x float>] %m, 0
    %x0 = extractelement <2 x float> %m0v, i32 0
    %y0 = extractelement <2 x float> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x float>] %m, 1
    %x1 = extractelement <2 x float> %m1v, i32 0
    %y1 = extractelement <2 x float> %m1v, i32 1

    %1 = call float @llpc.determinant2.f32(float %x0, float %y0, float %x1, float %y1)
    %2 = fdiv float 1.0, %1
    %3 = fsub float 0.0, %2
    %4 = fmul float %2, %y1
    %5 = fmul float %3, %y0
    %6 = fmul float %3, %x1
    %7 = fmul float %2, %x0
    %8 = insertelement <2 x float> undef, float %4, i32 0
    %9 = insertelement <2 x float> %8, float %5, i32 1
    %10 = insertvalue [2 x <2 x float>] undef, <2 x float> %9, 0
    %11 = insertelement <2 x float> undef, float %6, i32 0
    %12 = insertelement <2 x float> %11, float %7, i32 1
    %13 = insertvalue [2 x <2 x float>] %10 , <2 x float> %12, 1

    ret [2 x <2 x float>]  %13
}

; GLSL: mat3 = inverse(mat3)
define spir_func [3 x <3 x float>] @_Z13matrixInverseDv3_Dv3_f(
    [3 x <3 x float>] %m) #0
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

    %m0v = extractvalue [3 x <3 x float>] %m, 0
    %x0 = extractelement <3 x float> %m0v, i32 0
    %y0 = extractelement <3 x float> %m0v, i32 1
    %z0 = extractelement <3 x float> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x float>] %m, 1
    %x1 = extractelement <3 x float> %m1v, i32 0
    %y1 = extractelement <3 x float> %m1v, i32 1
    %z1 = extractelement <3 x float> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x float>] %m, 2
    %x2 = extractelement <3 x float> %m2v, i32 0
    %y2 = extractelement <3 x float> %m2v, i32 1
    %z2 = extractelement <3 x float> %m2v, i32 2

    %adjx0 = call float @llpc.determinant2.f32(float %y1, float %z1, float %y2, float %z2)
    %adjx1 = call float @llpc.determinant2.f32(float %y2, float %z2, float %y0, float %z0)
    %adjx2 = call float @llpc.determinant2.f32(float %y0, float %z0, float %y1, float %z1)

    %det = call float @llpc.dot3.f32(float %x0, float %x1, float %x2,
                    float %adjx0, float %adjx1, float %adjx2)
    %rdet = fdiv float 1.0, %det

    %nx0 = fmul float %rdet, %adjx0
    %nx1 = fmul float %rdet, %adjx1
    %nx2 = fmul float %rdet, %adjx2

    %m00 = insertelement <3 x float> undef, float %nx0, i32 0
    %m01 = insertelement <3 x float> %m00, float %nx1, i32 1
    %m02 = insertelement <3 x float> %m01, float %nx2, i32 2
    %m0 = insertvalue [3 x <3 x float>] undef, <3 x float> %m02, 0

    %adjy0 = call float @llpc.determinant2.f32(float %z1, float %x1, float %z2, float %x2)
    %adjy1 = call float @llpc.determinant2.f32(float %z2, float %x2, float %z0, float %x0)
    %adjy2 = call float @llpc.determinant2.f32(float %z0, float %x0, float %z1, float %x1)


    %ny0 = fmul float %rdet, %adjy0
    %ny1 = fmul float %rdet, %adjy1
    %ny2 = fmul float %rdet, %adjy2

    %m10 = insertelement <3 x float> undef, float %ny0, i32 0
    %m11 = insertelement <3 x float> %m10, float %ny1, i32 1
    %m12 = insertelement <3 x float> %m11, float %ny2, i32 2
    %m1 = insertvalue [3 x <3 x float>] %m0, <3 x float> %m12, 1

    %adjz0 = call float @llpc.determinant2.f32(float %x1, float %y1, float %x2, float %y2)
    %adjz1 = call float @llpc.determinant2.f32(float %x2, float %y2, float %x0, float %y0)
    %adjz2 = call float @llpc.determinant2.f32(float %x0, float %y0, float %x1, float %y1)

    %nz0 = fmul float %rdet, %adjz0
    %nz1 = fmul float %rdet, %adjz1
    %nz2 = fmul float %rdet, %adjz2

    %m20 = insertelement <3 x float> undef, float %nz0, i32 0
    %m21 = insertelement <3 x float> %m20, float %nz1, i32 1
    %m22 = insertelement <3 x float> %m21, float %nz2, i32 2
    %m2 = insertvalue [3 x <3 x float>] %m1, <3 x float> %m22, 2

    ret [3 x <3 x float>] %m2
}

; GLSL: mat4 = inverse(mat4)
define spir_func [4 x <4 x float>] @_Z13matrixInverseDv4_Dv4_f(
    [4 x <4 x float>] %m) #0
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

    %m0v = extractvalue [4 x <4 x float>] %m, 0
    %x0 = extractelement <4 x float> %m0v, i32 0
    %y0 = extractelement <4 x float> %m0v, i32 1
    %z0 = extractelement <4 x float> %m0v, i32 2
    %w0 = extractelement <4 x float> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x float>] %m, 1
    %x1 = extractelement <4 x float> %m1v, i32 0
    %y1 = extractelement <4 x float> %m1v, i32 1
    %z1 = extractelement <4 x float> %m1v, i32 2
    %w1 = extractelement <4 x float> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x float>] %m, 2
    %x2 = extractelement <4 x float> %m2v, i32 0
    %y2 = extractelement <4 x float> %m2v, i32 1
    %z2 = extractelement <4 x float> %m2v, i32 2
    %w2 = extractelement <4 x float> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x float>] %m, 3
    %x3 = extractelement <4 x float> %m3v, i32 0
    %y3 = extractelement <4 x float> %m3v, i32 1
    %z3 = extractelement <4 x float> %m3v, i32 2
    %w3 = extractelement <4 x float> %m3v, i32 3

    %adjx0 = call float @llpc.determinant3.f32(
            float %y1, float %z1, float %w1,
            float %y2, float %z2, float %w2,
            float %y3, float %z3, float %w3)
    %adjx1 = call float @llpc.determinant3.f32(
            float %y2, float %z2, float %w2,
            float %y0, float %z0, float %w0,
            float %y3, float %z3, float %w3)
    %adjx2 = call float @llpc.determinant3.f32(
            float %y3, float %z3, float %w3,
            float %y0, float %z0, float %w0,
            float %y1, float %z1, float %w1)
    %adjx3 = call float @llpc.determinant3.f32(
            float %y0, float %z0, float %w0,
            float %y2, float %z2, float %w2,
            float %y1, float %z1, float %w1)

    %det = call float @llpc.dot4.f32(float %x0, float %x1, float %x2, float %x3,
            float %adjx0, float %adjx1, float %adjx2, float %adjx3)
    %rdet = fdiv float 1.0, %det

    %nx0 = fmul float %rdet, %adjx0
    %nx1 = fmul float %rdet, %adjx1
    %nx2 = fmul float %rdet, %adjx2
    %nx3 = fmul float %rdet, %adjx3

    %m00 = insertelement <4 x float> undef, float %nx0, i32 0
    %m01 = insertelement <4 x float> %m00, float %nx1, i32 1
    %m02 = insertelement <4 x float> %m01, float %nx2, i32 2
    %m03 = insertelement <4 x float> %m02, float %nx3, i32 3
    %m0 = insertvalue [4 x <4 x float>] undef, <4 x float> %m03, 0

    %adjy0 = call float @llpc.determinant3.f32(
            float %z2, float %w2, float %x2,
            float %z1, float %w1, float %x1,
            float %z3, float %w3, float %x3)
    %adjy1 = call float @llpc.determinant3.f32(
             float %z2, float %w2, float %x2,
             float %z3, float %w3, float %x3,
             float %z0, float %w0, float %x0)
    %adjy2 = call float @llpc.determinant3.f32(
            float %z0, float %w0, float %x0,
            float %z3, float %w3, float %x3,
            float %z1, float %w1, float %x1)
    %adjy3 = call float @llpc.determinant3.f32(
            float %z0, float %w0, float %x0,
            float %z1, float %w1, float %x1,
            float %z2, float %w2, float %x2)

    %ny0 = fmul float %rdet, %adjy0
    %ny1 = fmul float %rdet, %adjy1
    %ny2 = fmul float %rdet, %adjy2
    %ny3 = fmul float %rdet, %adjy3

    %m10 = insertelement <4 x float> undef, float %ny0, i32 0
    %m11 = insertelement <4 x float> %m10, float %ny1, i32 1
    %m12 = insertelement <4 x float> %m11, float %ny2, i32 2
    %m13 = insertelement <4 x float> %m12, float %ny3, i32 3
    %m1 = insertvalue [4 x <4 x float>] %m0, <4 x float> %m13, 1

    %adjz0 = call float @llpc.determinant3.f32(
            float %w1, float %x1, float %y1,
            float %w2, float %x2, float %y2,
            float %w3, float %x3, float %y3)
    %adjz1 = call float @llpc.determinant3.f32(
            float %w3, float %x3, float %y3,
            float %w2, float %x2, float %y2,
            float %w0, float %x0, float %y0)
    %adjz2 = call float @llpc.determinant3.f32(
            float %w3, float %x3, float %y3,
            float %w0, float %x0, float %y0,
            float %w1, float %x1, float %y1)
    %adjz3 = call float @llpc.determinant3.f32(
            float %w1, float %x1, float %y1,
            float %w0, float %x0, float %y0,
            float %w2, float %x2, float %y2)

    %nz0 = fmul float %rdet, %adjz0
    %nz1 = fmul float %rdet, %adjz1
    %nz2 = fmul float %rdet, %adjz2
    %nz3 = fmul float %rdet, %adjz3

    %m20 = insertelement <4 x float> undef, float %nz0, i32 0
    %m21 = insertelement <4 x float> %m20, float %nz1, i32 1
    %m22 = insertelement <4 x float> %m21, float %nz2, i32 2
    %m23 = insertelement <4 x float> %m22, float %nz3, i32 3
    %m2 = insertvalue [4 x <4 x float>] %m1, <4 x float> %m23, 2

    %adjw0 = call float @llpc.determinant3.f32(
            float %x2, float %y2, float %z2,
            float %x1, float %y1, float %z1,
            float %x3, float %y3, float %z3)
    %adjw1 = call float @llpc.determinant3.f32(
            float %x2, float %y2, float %z2,
            float %x3, float %y3, float %z3,
            float %x0, float %y0, float %z0)
    %adjw2 = call float @llpc.determinant3.f32(
            float %x0, float %y0, float %z0,
            float %x3, float %y3, float %z3,
            float %x1, float %y1, float %z1)
    %adjw3 = call float @llpc.determinant3.f32(
            float %x0, float %y0, float %z0,
            float %x1, float %y1, float %z1,
            float %x2, float %y2, float %z2)

    %nw0 = fmul float %rdet, %adjw0
    %nw1 = fmul float %rdet, %adjw1
    %nw2 = fmul float %rdet, %adjw2
    %nw3 = fmul float %rdet, %adjw3

    %m30 = insertelement <4 x float> undef, float %nw0, i32 0
    %m31 = insertelement <4 x float> %m30, float %nw1, i32 1
    %m32 = insertelement <4 x float> %m31, float %nw2, i32 2
    %m33 = insertelement <4 x float> %m32, float %nw3, i32 3
    %m3 = insertvalue [4 x <4 x float>] %m2, <4 x float> %m33, 3

    ret [4 x <4 x float>] %m3
}

declare spir_func float @_Z3dotDv2_fDv2_f(<2 x float> , <2 x float>) #0
declare spir_func float @_Z3dotDv3_fDv3_f(<3 x float> , <3 x float>) #0
declare spir_func float @_Z3dotDv4_fDv4_f(<4 x float> , <4 x float>) #0

attributes #0 = { nounwind }
