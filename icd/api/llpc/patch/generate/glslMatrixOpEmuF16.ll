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

; GLSL: f16mat2 = outerProduct(f16vec2, f16vec2)
define spir_func [2 x <2 x half>] @_Z12OuterProductDv2_DhDv2_Dh(
    <2 x half> %c, <2 x half> %r) #0
{
    %m = alloca [2 x <2 x half>]
    %m0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 1

    %c0 = extractelement <2 x half> %c, i32 0
    %c1 = extractelement <2 x half> %c, i32 1

    %r0 = extractelement <2 x half> %r, i32 0
    %r1 = extractelement <2 x half> %r, i32 1

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c0, %r1
    store half %3, half* %m10
    %4 = fmul half %c1, %r1
    store half %4, half* %m11
    %5 = load [2 x <2 x half>], [2 x <2 x half>]* %m

    ret [2 x <2 x half>] %5
}

; GLSL: f16mat3 = outerProduct(f16vec3, f16vec3)
define spir_func [3 x <3 x half>] @_Z12OuterProductDv3_DhDv3_Dh(
    <3 x half> %c, <3 x half> %r) #0
{
    %m = alloca [3 x <3 x half>]
    %m0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 2

    %c0 = extractelement <3 x half> %c, i32 0
    %c1 = extractelement <3 x half> %c, i32 1
    %c2 = extractelement <3 x half> %c, i32 2

    %r0 = extractelement <3 x half> %r, i32 0
    %r1 = extractelement <3 x half> %r, i32 1
    %r2 = extractelement <3 x half> %r, i32 2

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c0, %r1
    store half %4, half* %m10
    %5 = fmul half %c1, %r1
    store half %5, half* %m11
    %6 = fmul half %c2, %r1
    store half %6, half* %m12
    %7 = fmul half %c0, %r2
    store half %7, half* %m20
    %8 = fmul half %c1, %r2
    store half %8, half* %m21
    %9 = fmul half %c2, %r2
    store half %9, half* %m22
    %10 = load [3 x <3 x half>], [3 x <3 x half>]* %m

    ret [3 x <3 x half>] %10
}

; GLSL: f16mat4 = outerProduct(f16vec4, f16vec4)
define spir_func [4 x <4 x half>] @_Z12OuterProductDv4_DhDv4_Dh(
    <4 x half> %c, <4 x half> %r) #0
{
    %m = alloca [4 x <4 x half>]
    %m0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 3

    %m3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <4 x half>, <4 x half>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <4 x half>, <4 x half>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <4 x half>, <4 x half>* %m3, i32 0, i32 2
    %m33 = getelementptr inbounds <4 x half>, <4 x half>* %m3, i32 0, i32 3

    %c0 = extractelement <4 x half> %c, i32 0
    %c1 = extractelement <4 x half> %c, i32 1
    %c2 = extractelement <4 x half> %c, i32 2
    %c3 = extractelement <4 x half> %c, i32 3

    %r0 = extractelement <4 x half> %r, i32 0
    %r1 = extractelement <4 x half> %r, i32 1
    %r2 = extractelement <4 x half> %r, i32 2
    %r3 = extractelement <4 x half> %r, i32 3

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c3, %r0
    store half %4, half* %m03
    %5 = fmul half %c0, %r1
    store half %5, half* %m10
    %6 = fmul half %c1, %r1
    store half %6, half* %m11
    %7 = fmul half %c2, %r1
    store half %7, half* %m12
    %8 = fmul half %c3, %r1
    store half %8, half* %m13
    %9 = fmul half %c0, %r2
    store half %9, half* %m20
    %10 = fmul half %c1, %r2
    store half %10, half* %m21
    %11 = fmul half %c2, %r2
    store half %11, half* %m22
    %12 = fmul half %c3, %r2
    store half %12, half* %m23
    %13 = fmul half %c0, %r3
    store half %13, half* %m30
    %14 = fmul half %c1, %r3
    store half %14, half* %m31
    %15 = fmul half %c2, %r3
    store half %15, half* %m32
    %16 = fmul half %c3, %r3
    store half %16, half* %m33
    %17 = load [4 x <4 x half>], [4 x <4 x half>]* %m

    ret [4 x <4 x half>] %17
}

; GLSL: f16mat2x3 = outerProduct(f16vec3, f16vec2)
define spir_func [2 x <3 x half>] @_Z12OuterProductDv3_DhDv2_Dh(
    <3 x half> %c, <2 x half> %r) #0
{
    %m = alloca [2 x <3 x half>]
    %m0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 2

    %c0 = extractelement <3 x half> %c, i32 0
    %c1 = extractelement <3 x half> %c, i32 1
    %c2 = extractelement <3 x half> %c, i32 2

    %r0 = extractelement <2 x half> %r, i32 0
    %r1 = extractelement <2 x half> %r, i32 1

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c0, %r1
    store half %4, half* %m10
    %5 = fmul half %c1, %r1
    store half %5, half* %m11
    %6 = fmul half %c2, %r1
    store half %6, half* %m12
    %7 = load [2 x <3 x half>], [2 x <3 x half>]* %m

    ret [2 x <3 x half>] %7
}

; GLSL: f16mat3x2 = outerProduct(f16vec2, f16vec3)
define spir_func [3 x <2 x half>] @_Z12OuterProductDv2_DhDv3_Dh(
    <2 x half> %c, <3 x half> %r) #0
{
    %m = alloca [3 x <2 x half>]
    %m0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x half>, <2 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x half>, <2 x half>* %m2, i32 0, i32 1

    %c0 = extractelement <2 x half> %c, i32 0
    %c1 = extractelement <2 x half> %c, i32 1

    %r0 = extractelement <3 x half> %r, i32 0
    %r1 = extractelement <3 x half> %r, i32 1
    %r2 = extractelement <3 x half> %r, i32 2

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c0, %r1
    store half %3, half* %m10
    %4 = fmul half %c1, %r1
    store half %4, half* %m11
    %5 = fmul half %c0, %r2
    store half %5, half* %m20
    %6 = fmul half %c1, %r2
    store half %6, half* %m21
    %7 = load [3 x <2 x half>], [3 x <2 x half>]* %m

    ret [3 x <2 x half>] %7
}

; GLSL: f16mat2x4 = outerProduct(f16vec4, f16vec2)
define spir_func [2 x <4 x half>] @_Z12OuterProductDv4_DhDv2_Dh(
    <4 x half> %c, <2 x half> %r) #0
{
    %m = alloca [2 x <4 x half>]
    %m0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 3

    %c0 = extractelement <4 x half> %c, i32 0
    %c1 = extractelement <4 x half> %c, i32 1
    %c2 = extractelement <4 x half> %c, i32 2
    %c3 = extractelement <4 x half> %c, i32 3

    %r0 = extractelement <2 x half> %r, i32 0
    %r1 = extractelement <2 x half> %r, i32 1

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c3, %r0
    store half %4, half* %m03
    %5 = fmul half %c0, %r1
    store half %5, half* %m10
    %6 = fmul half %c1, %r1
    store half %6, half* %m11
    %7 = fmul half %c2, %r1
    store half %7, half* %m12
    %8 = fmul half %c3, %r1
    store half %8, half* %m13
    %9 = load [2 x <4 x half>], [2 x <4 x half>]* %m

    ret [2 x <4 x half>] %9
}

; GLSL: f16mat4x2 = outerProduct(f16vec2, f16vec4)
define spir_func [4 x <2 x half>] @_Z12OuterProductDv2_DhDv4_Dh(
    <2 x half> %c, <4 x half> %r) #0
{
    %m = alloca [4 x <2 x half>]
    %m0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <2 x half>, <2 x half>* %m0, i32 0, i32 1

    %m1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <2 x half>, <2 x half>* %m1, i32 0, i32 1

    %m2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <2 x half>, <2 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <2 x half>, <2 x half>* %m2, i32 0, i32 1

    %m3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <2 x half>, <2 x half>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <2 x half>, <2 x half>* %m3, i32 0, i32 1

    %c0 = extractelement <2 x half> %c, i32 0
    %c1 = extractelement <2 x half> %c, i32 1

    %r0 = extractelement <4 x half> %r, i32 0
    %r1 = extractelement <4 x half> %r, i32 1
    %r2 = extractelement <4 x half> %r, i32 2
    %r3 = extractelement <4 x half> %r, i32 3

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c0, %r1
    store half %3, half* %m10
    %4 = fmul half %c1, %r1
    store half %4, half* %m11
    %5 = fmul half %c0, %r2
    store half %5, half* %m20
    %6 = fmul half %c1, %r2
    store half %6, half* %m21
    %7 = fmul half %c0, %r3
    store half %7, half* %m30
    %8 = fmul half %c1, %r3
    store half %8, half* %m31
    %9 = load [4 x <2 x half>], [4 x <2 x half>]* %m

    ret [4 x <2 x half>] %9
}

; GLSL: f16mat3x4 = outerProduct(f16vec4, f16vec3)
define spir_func [3 x <4 x half>] @_Z12OuterProductDv4_DhDv3_Dh(
    <4 x half> %c, <3 x half> %r) #0
{
    %m = alloca [3 x <4 x half>]
    %m0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 2
    %m03 = getelementptr inbounds <4 x half>, <4 x half>* %m0, i32 0, i32 3

    %m1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 2
    %m13 = getelementptr inbounds <4 x half>, <4 x half>* %m1, i32 0, i32 3

    %m2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 2
    %m23 = getelementptr inbounds <4 x half>, <4 x half>* %m2, i32 0, i32 3

    %c0 = extractelement <4 x half> %c, i32 0
    %c1 = extractelement <4 x half> %c, i32 1
    %c2 = extractelement <4 x half> %c, i32 2
    %c3 = extractelement <4 x half> %c, i32 3

    %r0 = extractelement <3 x half> %r, i32 0
    %r1 = extractelement <3 x half> %r, i32 1
    %r2 = extractelement <3 x half> %r, i32 2

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c3, %r0
    store half %4, half* %m03
    %5 = fmul half %c0, %r1
    store half %5, half* %m10
    %6 = fmul half %c1, %r1
    store half %6, half* %m11
    %7 = fmul half %c2, %r1
    store half %7, half* %m12
    %8 = fmul half %c3, %r1
    store half %8, half* %m13
    %9 = fmul half %c0, %r2
    store half %9, half* %m20
    %10 = fmul half %c1, %r2
    store half %10, half* %m21
    %11 = fmul half %c2, %r2
    store half %11, half* %m22
    %12 = fmul half %c3, %r2
    store half %12, half* %m23
    %13 = load [3 x <4 x half>], [3 x <4 x half>]* %m

    ret [3 x <4 x half>] %13
}

; GLSL: f16mat4x3 = outerProduct(f16vec3, f16vec4)
define spir_func [4 x <3 x half>] @_Z12OuterProductDv3_DhDv4_Dh(
    <3 x half> %c, <4 x half> %r) #0
{
    %m = alloca [4 x <3 x half>]
    %m0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %m, i32 0, i32 0
    %m00 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 0
    %m01 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 1
    %m02 = getelementptr inbounds <3 x half>, <3 x half>* %m0, i32 0, i32 2

    %m1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %m, i32 0, i32 1
    %m10 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 0
    %m11 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 1
    %m12 = getelementptr inbounds <3 x half>, <3 x half>* %m1, i32 0, i32 2

    %m2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %m, i32 0, i32 2
    %m20 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 0
    %m21 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 1
    %m22 = getelementptr inbounds <3 x half>, <3 x half>* %m2, i32 0, i32 2

    %m3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %m, i32 0, i32 3
    %m30 = getelementptr inbounds <3 x half>, <3 x half>* %m3, i32 0, i32 0
    %m31 = getelementptr inbounds <3 x half>, <3 x half>* %m3, i32 0, i32 1
    %m32 = getelementptr inbounds <3 x half>, <3 x half>* %m3, i32 0, i32 2

    %c0 = extractelement <3 x half> %c, i32 0
    %c1 = extractelement <3 x half> %c, i32 1
    %c2 = extractelement <3 x half> %c, i32 2

    %r0 = extractelement <4 x half> %r, i32 0
    %r1 = extractelement <4 x half> %r, i32 1
    %r2 = extractelement <4 x half> %r, i32 2
    %r3 = extractelement <4 x half> %r, i32 3

    %1 = fmul half %c0, %r0
    store half %1, half* %m00
    %2 = fmul half %c1, %r0
    store half %2, half* %m01
    %3 = fmul half %c2, %r0
    store half %3, half* %m02
    %4 = fmul half %c0, %r1
    store half %4, half* %m10
    %5 = fmul half %c1, %r1
    store half %5, half* %m11
    %6 = fmul half %c2, %r1
    store half %6, half* %m12
    %7 = fmul half %c0, %r2
    store half %7, half* %m20
    %8 = fmul half %c1, %r2
    store half %8, half* %m21
    %9 = fmul half %c2, %r2
    store half %9, half* %m22
    %10 = fmul half %c0, %r3
    store half %10, half* %m30
    %11 = fmul half %c1, %r3
    store half %11, half* %m31
    %12 = fmul half %c2, %r3
    store half %12, half* %m32
    %13 = load [4 x <3 x half>], [4 x <3 x half>]* %m

    ret [4 x <3 x half>] %13
}

; GLSL: f16mat2 = transpose(f16mat2)
define spir_func [2 x <2 x half>] @_Z9TransposeDv2_Dv2_Dh(
    [2 x <2 x half>] %m) #0
{
    %nm = alloca [2 x <2 x half>]
    %nm0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    %nmv = load [2 x <2 x half>], [2 x <2 x half>]* %nm
    ret [2 x <2 x half>] %nmv
}

; GLSL: f16mat3 = transpose(f16mat3)
define spir_func [3 x <3 x half>] @_Z9TransposeDv3_Dv3_Dh(
    [3 x <3 x half>] %m) #0
{
    %nm = alloca [3 x <3 x half>]
    %nm0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x half>] %m, 2
    %m2v0 = extractelement <3 x half> %m2v, i32 0
    %m2v1 = extractelement <3 x half> %m2v, i32 1
    %m2v2 = extractelement <3 x half> %m2v, i32 2

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    store half %m2v2, half* %nm22
    %nmv = load [3 x <3 x half>], [3 x <3 x half>]* %nm
    ret [3 x <3 x half>] %nmv
}

; GLSL: f16mat4 = transpose(f16mat4)
define spir_func [4 x <4 x half>] @_Z9TransposeDv4_Dv4_Dh(
    [4 x <4 x half>] %m) #0
{
    %nm = alloca [4 x <4 x half>]
    %nm0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m2v0 = extractelement <4 x half> %m2v, i32 0
    %m2v1 = extractelement <4 x half> %m2v, i32 1
    %m2v2 = extractelement <4 x half> %m2v, i32 2
    %m2v3 = extractelement <4 x half> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x half>] %m, 3
    %m3v0 = extractelement <4 x half> %m3v, i32 0
    %m3v1 = extractelement <4 x half> %m3v, i32 1
    %m3v2 = extractelement <4 x half> %m3v, i32 2
    %m3v3 = extractelement <4 x half> %m3v, i32 3

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m3v0, half* %nm03
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    store half %m3v1, half* %nm13
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    store half %m2v2, half* %nm22
    store half %m3v2, half* %nm23
    store half %m0v3, half* %nm30
    store half %m1v3, half* %nm31
    store half %m2v3, half* %nm32
    store half %m3v3, half* %nm33
    %nmv = load [4 x <4 x half>], [4 x <4 x half>]* %nm
    ret [4 x <4 x half>] %nmv
}

; GLSL: f16mat2x3 = transpose(f16mat3x2)
define spir_func [2 x <3 x half>] @_Z9TransposeDv3_Dv2_Dh(
    [3 x <2 x half>] %m) #0
{
    %nm = alloca [2 x <3 x half>]
    %nm0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x half>] %m, 2
    %m2v0 = extractelement <2 x half> %m2v, i32 0
    %m2v1 = extractelement <2 x half> %m2v, i32 1

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    %nmv = load [2 x <3 x half>], [2 x <3 x half>]* %nm
    ret [2 x <3 x half>] %nmv
}

; GLSL: f16mat3x2 = transpose(f16mat2x3)
define spir_func [3 x <2 x half>] @_Z9TransposeDv2_Dv3_Dh(
    [2 x <3 x half>] %m) #0
{
    %nm = alloca [3 x <2 x half>]
    %nm0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    %nmv = load [3 x <2 x half>], [3 x <2 x half>]* %nm
    ret [3 x <2 x half>] %nmv
}

; GLSL: f16mat2x4 = transpose(f16mat4x2)
define spir_func [2 x <4 x half>] @_Z9TransposeDv4_Dv2_Dh(
    [4 x <2 x half>] %m) #0
{
    %nm = alloca [2 x <4 x half>]
    %nm0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m2v0 = extractelement <2 x half> %m2v, i32 0
    %m2v1 = extractelement <2 x half> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x half>] %m, 3
    %m3v0 = extractelement <2 x half> %m3v, i32 0
    %m3v1 = extractelement <2 x half> %m3v, i32 1

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m3v0, half* %nm03
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    store half %m3v1, half* %nm13
    %nmv = load [2 x <4 x half>], [2 x <4 x half>]* %nm
    ret [2 x <4 x half>] %nmv
}

; GLSL: f16mat4x2 = transpose(f16mat2x4)
define spir_func [4 x <2 x half>] @_Z9TransposeDv2_Dv4_Dh(
    [2 x <4 x half>] %m) #0
{
    %nm = alloca [4 x <2 x half>]
    %nm0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x half>, <2 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x half>, <2 x half>* %nm3, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    store half %m0v3, half* %nm30
    store half %m1v3, half* %nm31
    %nmv = load [4 x <2 x half>], [4 x <2 x half>]* %nm
    ret [4 x <2 x half>] %nmv
}

; GLSL: f16mat3x4 = transpose(f16mat4x3)
define spir_func [3 x <4 x half>] @_Z9TransposeDv4_Dv3_Dh(
    [4 x <3 x half>] %m) #0
{
    %nm = alloca [3 x <4 x half>]
    %nm0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m2v0 = extractelement <3 x half> %m2v, i32 0
    %m2v1 = extractelement <3 x half> %m2v, i32 1
    %m2v2 = extractelement <3 x half> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x half>] %m, 3
    %m3v0 = extractelement <3 x half> %m3v, i32 0
    %m3v1 = extractelement <3 x half> %m3v, i32 1
    %m3v2 = extractelement <3 x half> %m3v, i32 2

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m3v0, half* %nm03
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    store half %m3v1, half* %nm13
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    store half %m2v2, half* %nm22
    store half %m3v2, half* %nm23
    %nmv = load [3 x <4 x half>], [3 x <4 x half>]* %nm
    ret [3 x <4 x half>] %nmv
}

; GLSL: f16mat4x3 = transpose(f16mat3x4)
define spir_func [4 x <3 x half>] @_Z9TransposeDv3_Dv4_Dh(
    [3 x <4 x half>] %m) #0
{
    %nm = alloca [4 x <3 x half>]
    %nm0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x half>] %m, 2
    %m2v0 = extractelement <4 x half> %m2v, i32 0
    %m2v1 = extractelement <4 x half> %m2v, i32 1
    %m2v2 = extractelement <4 x half> %m2v, i32 2
    %m2v3 = extractelement <4 x half> %m2v, i32 3

    store half %m0v0, half* %nm00
    store half %m1v0, half* %nm01
    store half %m2v0, half* %nm02
    store half %m0v1, half* %nm10
    store half %m1v1, half* %nm11
    store half %m2v1, half* %nm12
    store half %m0v2, half* %nm20
    store half %m1v2, half* %nm21
    store half %m2v2, half* %nm22
    store half %m0v3, half* %nm30
    store half %m1v3, half* %nm31
    store half %m2v3, half* %nm32
    %nmv = load [4 x <3 x half>], [4 x <3 x half>]* %nm
    ret [4 x <3 x half>] %nmv
}

; GLSL: f16mat2 = f16mat2 * float16_t
define spir_func [2 x <2 x half>] @_Z17MatrixTimesScalarDv2_Dv2_DhDh(
    [2 x <2 x half>] %m, half %s) #0
{
    %nm = alloca [2 x <2 x half>]
    %nm0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m1v0, %s
    store half %3, half* %nm10
    %4 = fmul half %m1v1, %s
    store half %4, half* %nm11
    %5 = load [2 x <2 x half>], [2 x <2 x half>]* %nm

    ret [2 x <2 x half>] %5
}

; GLSL: f16mat3 = f16mat3 * float16_t
define spir_func [3 x <3 x half>] @_Z17MatrixTimesScalarDv3_Dv3_DhDh(
    [3 x <3 x half>] %m, half %s) #0
{
    %nm = alloca [3 x <3 x half>]
    %nm0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x half>] %m, 2
    %m2v0 = extractelement <3 x half> %m2v, i32 0
    %m2v1 = extractelement <3 x half> %m2v, i32 1
    %m2v2 = extractelement <3 x half> %m2v, i32 2

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m1v0, %s
    store half %4, half* %nm10
    %5 = fmul half %m1v1, %s
    store half %5, half* %nm11
    %6 = fmul half %m1v2, %s
    store half %6, half* %nm12
    %7 = fmul half %m2v0, %s
    store half %7, half* %nm20
    %8 = fmul half %m2v1, %s
    store half %8, half* %nm21
    %9 = fmul half %m2v2, %s
    store half %9, half* %nm22
    %10 = load [3 x <3 x half>], [3 x <3 x half>]* %nm

    ret [3 x <3 x half>] %10
}

; GLSL: f16mat4 = f16mat4 * float16_t
define spir_func [4 x <4 x half>] @_Z17MatrixTimesScalarDv4_Dv4_DhDh(
    [4 x <4 x half>] %m, half %s) #0
{
    %nm = alloca [4 x <4 x half>]
    %nm0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 3

    %nm3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 2
    %nm33 = getelementptr inbounds <4 x half>, <4 x half>* %nm3, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m2v0 = extractelement <4 x half> %m2v, i32 0
    %m2v1 = extractelement <4 x half> %m2v, i32 1
    %m2v2 = extractelement <4 x half> %m2v, i32 2
    %m2v3 = extractelement <4 x half> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x half>] %m, 3
    %m3v0 = extractelement <4 x half> %m3v, i32 0
    %m3v1 = extractelement <4 x half> %m3v, i32 1
    %m3v2 = extractelement <4 x half> %m3v, i32 2
    %m3v3 = extractelement <4 x half> %m3v, i32 3

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m0v3, %s
    store half %4, half* %nm03
    %5 = fmul half %m1v0, %s
    store half %5, half* %nm10
    %6 = fmul half %m1v1, %s
    store half %6, half* %nm11
    %7 = fmul half %m1v2, %s
    store half %7, half* %nm12
    %8 = fmul half %m1v3, %s
    store half %8, half* %nm13
    %9 = fmul half %m2v0, %s
    store half %9, half* %nm20
    %10 = fmul half %m2v1, %s
    store half %10, half* %nm21
    %11 = fmul half %m2v2, %s
    store half %11, half* %nm22
    %12 = fmul half %m2v3, %s
    store half %12, half* %nm23
    %13 = fmul half %m3v0, %s
    store half %13, half* %nm30
    %14 = fmul half %m3v1, %s
    store half %14, half* %nm31
    %15 = fmul half %m3v2, %s
    store half %15, half* %nm32
    %16 = fmul half %m3v3, %s
    store half %16, half* %nm33
    %17 = load [4 x <4 x half>], [4 x <4 x half>]* %nm

    ret [4 x <4 x half>] %17
}

; GLSL: f16mat3x2 = f16mat3x2 * float16_t
define spir_func [3 x <2 x half>] @_Z17MatrixTimesScalarDv3_Dv2_DhDh(
    [3 x <2 x half>] %m, half %s) #0
{
    %nm = alloca [3 x <2 x half>]
    %nm0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 1

    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %m2v = extractvalue [3 x <2 x half>] %m, 2
    %m2v0 = extractelement <2 x half> %m2v, i32 0
    %m2v1 = extractelement <2 x half> %m2v, i32 1

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m1v0, %s
    store half %3, half* %nm10
    %4 = fmul half %m1v1, %s
    store half %4, half* %nm11
    %5 = fmul half %m2v0, %s
    store half %5, half* %nm20
    %6 = fmul half %m2v1, %s
    store half %6, half* %nm21
    %7 = load [3 x <2 x half>], [3 x <2 x half>]* %nm

    ret [3 x <2 x half>] %7
}

; GLSL: f16mat2x3 = f16mat2x3 * float16_t
define spir_func [2 x <3 x half>] @_Z17MatrixTimesScalarDv2_Dv3_DhDh(
    [2 x <3 x half>] %m, half %s) #0
{
    %nm = alloca [2 x <3 x half>]
    %nm0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [2 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m1v0, %s
    store half %4, half* %nm10
    %5 = fmul half %m1v1, %s
    store half %5, half* %nm11
    %6 = fmul half %m1v2, %s
    store half %6, half* %nm12
    %7 = load [2 x <3 x half>], [2 x <3 x half>]* %nm

    ret [2 x <3 x half>] %7
}

; GLSL: f16mat4x2 = f16mat4x2 * float16_t
define spir_func [4 x <2 x half>] @_Z17MatrixTimesScalarDv4_Dv2_DhDh(
    [4 x <2 x half>] %m, half %s) #0
{
    %nm = alloca [4 x <2 x half>]
    %nm0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <2 x half>, <2 x half>* %nm0, i32 0, i32 1

    %nm1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <2 x half>, <2 x half>* %nm1, i32 0, i32 1

    %nm2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <2 x half>, <2 x half>* %nm2, i32 0, i32 1

    %nm3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <2 x half>, <2 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <2 x half>, <2 x half>* %nm3, i32 0, i32 1

    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m2v0 = extractelement <2 x half> %m2v, i32 0
    %m2v1 = extractelement <2 x half> %m2v, i32 1

    %m3v = extractvalue [4 x <2 x half>] %m, 3
    %m3v0 = extractelement <2 x half> %m3v, i32 0
    %m3v1 = extractelement <2 x half> %m3v, i32 1

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m1v0, %s
    store half %3, half* %nm10
    %4 = fmul half %m1v1, %s
    store half %4, half* %nm11
    %5 = fmul half %m2v0, %s
    store half %5, half* %nm20
    %6 = fmul half %m2v1, %s
    store half %6, half* %nm21
    %7 = fmul half %m3v0, %s
    store half %7, half* %nm30
    %8 = fmul half %m3v1, %s
    store half %8, half* %nm31
    %9 = load [4 x <2 x half>], [4 x <2 x half>]* %nm

    ret [4 x <2 x half>] %9
}

; GLSL: f16mat2x4 = f16mat2x4 * float16_t
define spir_func [2 x <4 x half>] @_Z17MatrixTimesScalarDv2_Dv4_DhDh(
    [2 x <4 x half>] %m, half %s) #0
{
    %nm = alloca [2 x <4 x half>]
    %nm0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [2 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m0v3, %s
    store half %4, half* %nm03
    %5 = fmul half %m1v0, %s
    store half %5, half* %nm10
    %6 = fmul half %m1v1, %s
    store half %6, half* %nm11
    %7 = fmul half %m1v2, %s
    store half %7, half* %nm12
    %8 = fmul half %m1v3, %s
    store half %8, half* %nm13
    %9 = load [2 x <4 x half>], [2 x <4 x half>]* %nm

    ret [2 x <4 x half>] %9
}

; GLSL: f16mat4x3 = f16mat4x3 * float16_t
define spir_func [4 x <3 x half>] @_Z17MatrixTimesScalarDv4_Dv3_DhDh(
    [4 x <3 x half>] %m, half %s) #0
{
    %nm = alloca [4 x <3 x half>]
    %nm0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <3 x half>, <3 x half>* %nm0, i32 0, i32 2

    %nm1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <3 x half>, <3 x half>* %nm1, i32 0, i32 2

    %nm2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <3 x half>, <3 x half>* %nm2, i32 0, i32 2

    %nm3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 3
    %nm30 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 0
    %nm31 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 1
    %nm32 = getelementptr inbounds <3 x half>, <3 x half>* %nm3, i32 0, i32 2

    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m2v0 = extractelement <3 x half> %m2v, i32 0
    %m2v1 = extractelement <3 x half> %m2v, i32 1
    %m2v2 = extractelement <3 x half> %m2v, i32 2

    %m3v = extractvalue [4 x <3 x half>] %m, 3
    %m3v0 = extractelement <3 x half> %m3v, i32 0
    %m3v1 = extractelement <3 x half> %m3v, i32 1
    %m3v2 = extractelement <3 x half> %m3v, i32 2

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m1v0, %s
    store half %4, half* %nm10
    %5 = fmul half %m1v1, %s
    store half %5, half* %nm11
    %6 = fmul half %m1v2, %s
    store half %6, half* %nm12
    %7 = fmul half %m2v0, %s
    store half %7, half* %nm20
    %8 = fmul half %m2v1, %s
    store half %8, half* %nm21
    %9 = fmul half %m2v2, %s
    store half %9, half* %nm22
    %10 = fmul half %m3v0, %s
    store half %10, half* %nm30
    %11 = fmul half %m3v1, %s
    store half %11, half* %nm31
    %12 = fmul half %m3v2, %s
    store half %12, half* %nm32
    %13 = load [4 x <3 x half>], [4 x <3 x half>]* %nm

    ret [4 x <3 x half>] %13
}

; GLSL: f16mat3x4 = f16mat3x4 * float16_t
define spir_func [3 x <4 x half>] @_Z17MatrixTimesScalarDv3_Dv4_DhDh(
    [3 x <4 x half>] %m, half %s) #0
{
    %nm = alloca [3 x <4 x half>]
    %nm0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 0
    %nm00 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 0
    %nm01 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 1
    %nm02 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 2
    %nm03 = getelementptr inbounds <4 x half>, <4 x half>* %nm0, i32 0, i32 3

    %nm1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 1
    %nm10 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 0
    %nm11 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 1
    %nm12 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 2
    %nm13 = getelementptr inbounds <4 x half>, <4 x half>* %nm1, i32 0, i32 3

    %nm2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 2
    %nm20 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 0
    %nm21 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 1
    %nm22 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 2
    %nm23 = getelementptr inbounds <4 x half>, <4 x half>* %nm2, i32 0, i32 3

    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [3 x <4 x half>] %m, 2
    %m2v0 = extractelement <4 x half> %m2v, i32 0
    %m2v1 = extractelement <4 x half> %m2v, i32 1
    %m2v2 = extractelement <4 x half> %m2v, i32 2
    %m2v3 = extractelement <4 x half> %m2v, i32 3

    %1 = fmul half %m0v0, %s
    store half %1, half* %nm00
    %2 = fmul half %m0v1, %s
    store half %2, half* %nm01
    %3 = fmul half %m0v2, %s
    store half %3, half* %nm02
    %4 = fmul half %m0v3, %s
    store half %4, half* %nm03
    %5 = fmul half %m1v0, %s
    store half %5, half* %nm10
    %6 = fmul half %m1v1, %s
    store half %6, half* %nm11
    %7 = fmul half %m1v2, %s
    store half %7, half* %nm12
    %8 = fmul half %m1v3, %s
    store half %8, half* %nm13
    %9 = fmul half %m2v0, %s
    store half %9, half* %nm20
    %10 = fmul half %m2v1, %s
    store half %10, half* %nm21
    %11 = fmul half %m2v2, %s
    store half %11, half* %nm22
    %12 = fmul half %m2v3, %s
    store half %12, half* %nm23
    %13 = load [3 x <4 x half>], [3 x <4 x half>]* %nm

    ret [3 x <4 x half>] %13
}

; GLSL: f16vec2 = f16vec2 * f16mat2
define spir_func <2 x half> @_Z17VectorTimesMatrixDv2_DhDv2_Dv2_Dh(
    <2 x half> %c, [2 x <2 x half>] %m) #0
{
    %nv = alloca <2 x half>
    %nvp0 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %1 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m0v, <2 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [2 x <2 x half>] %m, 1
    %2 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m1v, <2 x half> %c)
    store half %2, half* %nvp1

    %3 = load <2 x half>, <2 x half>* %nv

    ret <2 x half> %3
}

; GLSL: f16vec3 = f16vec3 * f16mat3
define spir_func <3 x half> @_Z17VectorTimesMatrixDv3_DhDv3_Dv3_Dh(
    <3 x half> %c, [3 x <3 x half>] %m) #0
{
    %nv = alloca <3 x half>
    %nvp0 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %1 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m0v, <3 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %2 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m1v, <3 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [3 x <3 x half>] %m, 2
    %3 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m2v, <3 x half> %c)
    store half %3, half* %nvp2

    %4 = load <3 x half>, <3 x half>* %nv

    ret <3 x half> %4
}

; GLSL: f16vec4 = f16vec4 * f16mat4
define spir_func <4 x half> @_Z17VectorTimesMatrixDv4_DhDv4_Dv4_Dh(
    <4 x half> %c, [4 x <4 x half>] %m) #0
{
    %nv = alloca <4 x half>
    %nvp0 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %1 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m0v, <4 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %2 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m1v, <4 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %3 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m2v, <4 x half> %c)
    store half %3, half* %nvp2
    %m3v = extractvalue [4 x <4 x half>] %m, 3
    %4 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m3v, <4 x half> %c)
    store half %4, half* %nvp3

    %5 = load <4 x half>, <4 x half>* %nv

    ret <4 x half> %5
}

; GLSL: f16vec3 = f16vec2 * f16mat3x2
define spir_func <3 x half> @_Z17VectorTimesMatrixDv2_DhDv3_Dv2_Dh(
    <2 x half> %c, [3 x <2 x half>] %m) #0
{
    %nv = alloca <3 x half>
    %nvp0 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %1 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m0v, <2 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %2 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m1v, <2 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [3 x <2 x half>] %m, 2
    %3 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m2v, <2 x half> %c)
    store half %3, half* %nvp2

    %4 = load <3 x half>, <3 x half>* %nv

    ret <3 x half> %4
}

; GLSL: f16vec4 = f16vec2 * f16mat4x2
define spir_func <4 x half> @_Z17VectorTimesMatrixDv2_DhDv4_Dv2_Dh(
    <2 x half> %c, [4 x <2 x half>] %m) #0
{
    %nv = alloca <4 x half>
    %nvp0 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %1 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m0v, <2 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %2 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m1v, <2 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %3 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m2v, <2 x half> %c)
    store half %3, half* %nvp2
    %m3v = extractvalue [4 x <2 x half>] %m, 3
    %4 = call half @_Z3dotDv2_DhDv2_Dh(<2 x half> %m3v, <2 x half> %c)
    store half %4, half* %nvp3

    %5 = load <4 x half>, <4 x half>* %nv

    ret <4 x half> %5
}

; GLSL: f16vec2 = f16vec3 * f16mat2x3
define spir_func <2 x half> @_Z17VectorTimesMatrixDv3_DhDv2_Dv3_Dh(
    <3 x half> %c, [2 x <3 x half>] %m) #0
{
    %nv = alloca <2 x half>
    %nvp0 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %1 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m0v, <3 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [2 x <3 x half>] %m, 1
    %2 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m1v, <3 x half> %c)
    store half %2, half* %nvp1

    %3 = load <2 x half>, <2 x half>* %nv

    ret <2 x half> %3
}

; GLSL: f16vec4 = f16vec3 * f16mat4x3
define spir_func <4 x half> @_Z17VectorTimesMatrixDv3_DhDv4_Dv3_Dh(
    <3 x half> %c, [4 x <3 x half>] %m) #0
{
    %nv = alloca <4 x half>
    %nvp0 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 2
    %nvp3 = getelementptr inbounds <4 x half>, <4 x half>* %nv, i32 0, i32 3

    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %1 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m0v, <3 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %2 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m1v, <3 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %3 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m2v, <3 x half> %c)
    store half %3, half* %nvp2
    %m3v = extractvalue [4 x <3 x half>] %m, 3
    %4 = call half @_Z3dotDv3_DhDv3_Dh(<3 x half> %m3v, <3 x half> %c)
    store half %4, half* %nvp3

    %5 = load <4 x half>, <4 x half>* %nv

    ret <4 x half> %5
}

; GLSL: f16vec2 = f16vec4 * f16mat2x4
define spir_func <2 x half> @_Z17VectorTimesMatrixDv4_DhDv2_Dv4_Dh(
    <4 x half> %c, [2 x <4 x half>] %m) #0
{
    %nv = alloca <2 x half>
    %nvp0 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <2 x half>, <2 x half>* %nv, i32 0, i32 1

    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %1 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m0v, <4 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [2 x <4 x half>] %m, 1
    %2 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m1v, <4 x half> %c)
    store half %2, half* %nvp1

    %3 = load <2 x half>, <2 x half>* %nv

    ret <2 x half> %3
}

; GLSL: f16vec3 = f16vec4 * f16mat3x4
define spir_func <3 x half> @_Z17VectorTimesMatrixDv4_DhDv3_Dv4_Dh(
    <4 x half> %c, [3 x <4 x half>] %m) #0
{
    %nv = alloca <3 x half>
    %nvp0 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 0
    %nvp1 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 1
    %nvp2 = getelementptr inbounds <3 x half>, <3 x half>* %nv, i32 0, i32 2

    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %1 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m0v, <4 x half> %c)
    store half %1, half* %nvp0
    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %2 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m1v, <4 x half> %c)
    store half %2, half* %nvp1
    %m2v = extractvalue [3 x <4 x half>] %m, 2
    %3 = call half @_Z3dotDv4_DhDv4_Dh(<4 x half> %m2v, <4 x half> %c)
    store half %3, half* %nvp2

    %4 = load <3 x half>, <3 x half>* %nv

    ret <3 x half> %4
}

; GLSL: f16vec2 = f16mat2 * f16vec2
define spir_func <2 x half> @_Z17MatrixTimesVectorDv2_Dv2_DhDv2_Dh(
    [2 x <2 x half>] %m, <2 x half> %r) #0
{
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m1v = extractvalue [2 x <2 x half>] %m, 1

    %r0 = shufflevector <2 x half> %r, <2 x half> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %r0
    %r1 = shufflevector <2 x half> %r, <2 x half> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %r1
    %3 = fadd <2 x half> %2, %1

    ret <2 x half> %3
}

; GLSL: f16vec3 = f16mat3 * f16vec3
define spir_func <3 x half> @_Z17MatrixTimesVectorDv3_Dv3_DhDv3_Dh(
    [3 x <3 x half>] %m, <3 x half> %r) #0
{
    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m2v = extractvalue [3 x <3 x half>] %m, 2

    %r0 = shufflevector <3 x half> %r, <3 x half> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %r0
    %r1 = shufflevector <3 x half> %r, <3 x half> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %r1
    %3 = fadd <3 x half> %2, %1
    %r2 = shufflevector <3 x half> %r, <3 x half> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %r2
    %5 = fadd <3 x half> %4, %3

    ret <3 x half> %5
}

; GLSL: f16vec4 = f16mat4 * f16vec4
define spir_func <4 x half> @_Z17MatrixTimesVectorDv4_Dv4_DhDv4_Dh(
    [4 x <4 x half>] %m, <4 x half> %r) #0
{
    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m3v = extractvalue [4 x <4 x half>] %m, 3

    %r0 = shufflevector <4 x half> %r, <4 x half> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %r0
    %r1 = shufflevector <4 x half> %r, <4 x half> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %r1
    %3 = fadd <4 x half> %2, %1
    %r2 = shufflevector <4 x half> %r, <4 x half> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %r2
    %5 = fadd <4 x half> %4, %3
    %r3 = shufflevector <4 x half> %r, <4 x half> %r, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x half> %m3v, %r3
    %7 = fadd <4 x half> %6, %5

    ret <4 x half> %7
}

; GLSL: f16vec2 = f16mat3x2 * f16vec3
define spir_func <2 x half> @_Z17MatrixTimesVectorDv3_Dv2_DhDv3_Dh(
    [3 x <2 x half>] %m, <3 x half> %r) #0
{
    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m2v = extractvalue [3 x <2 x half>] %m, 2

    %r0 = shufflevector <3 x half> %r, <3 x half> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %r0
    %r1 = shufflevector <3 x half> %r, <3 x half> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %r1
    %3 = fadd <2 x half> %2, %1
    %r2 = shufflevector <3 x half> %r, <3 x half> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %r2
    %5 = fadd <2 x half> %4, %3

    ret <2 x half> %5
}

; GLSL: f16vec3 = f16mat2x3 * f16vec2
define spir_func <3 x half> @_Z17MatrixTimesVectorDv2_Dv3_DhDv2_Dh(
    [2 x <3 x half>] %m, <2 x half> %r) #0
{
    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m1v = extractvalue [2 x <3 x half>] %m, 1

    %r0 = shufflevector <2 x half> %r, <2 x half> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %r0
    %r1 = shufflevector <2 x half> %r, <2 x half> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %r1
    %3 = fadd <3 x half> %2, %1

    ret <3 x half> %3
}

; GLSL: f16vec2 = f16mat4x2 * f16vec4
define spir_func <2 x half> @_Z17MatrixTimesVectorDv4_Dv2_DhDv4_Dh(
    [4 x <2 x half>] %m, <4 x half> %r) #0
{
    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m3v = extractvalue [4 x <2 x half>] %m, 3

    %r0 = shufflevector <4 x half> %r, <4 x half> %r, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %r0
    %r1 = shufflevector <4 x half> %r, <4 x half> %r, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %r1
    %3 = fadd <2 x half> %2, %1
    %r2 = shufflevector <4 x half> %r, <4 x half> %r, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %r2
    %5 = fadd <2 x half> %4, %3
    %r3 = shufflevector <4 x half> %r, <4 x half> %r, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x half> %m3v, %r3
    %7 = fadd <2 x half> %6, %5

    ret <2 x half> %7
}

; GLSL: f16vec4 = f16mat2x4 * f16vec2
define spir_func <4 x half> @_Z17MatrixTimesVectorDv2_Dv4_DhDv2_Dh(
    [2 x <4 x half>] %m, <2 x half> %r) #0
{
    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m1v = extractvalue [2 x <4 x half>] %m, 1

    %r0 = shufflevector <2 x half> %r, <2 x half> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %r0
    %r1 = shufflevector <2 x half> %r, <2 x half> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %r1
    %3 = fadd <4 x half> %2, %1

    ret <4 x half> %3
}

; GLSL: f16vec3 = f16mat4x3 * f16vec4
define spir_func <3 x half> @_Z17MatrixTimesVectorDv4_Dv3_DhDv4_Dh(
    [4 x <3 x half>] %m, <4 x half> %r) #0
{
    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m3v = extractvalue [4 x <3 x half>] %m, 3

    %r0 = shufflevector <4 x half> %r, <4 x half> %r, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %r0
    %r1 = shufflevector <4 x half> %r, <4 x half> %r, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %r1
    %3 = fadd <3 x half> %2, %1
    %r2 = shufflevector <4 x half> %r, <4 x half> %r, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %r2
    %5 = fadd <3 x half> %4, %3
    %r3 = shufflevector <4 x half> %r, <4 x half> %r, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x half> %m3v, %r3
    %7 = fadd <3 x half> %6, %5

    ret <3 x half> %7
}

; GLSL: f16vec4 = f16mat3x4 * f16vec3
define spir_func <4 x half> @_Z17MatrixTimesVectorDv3_Dv4_DhDv3_Dh(
    [3 x <4 x half>] %m, <3 x half> %r) #0
{
    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m2v = extractvalue [3 x <4 x half>] %m, 2

    %r0 = shufflevector <3 x half> %r, <3 x half> %r, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %r0
    %r1 = shufflevector <3 x half> %r, <3 x half> %r, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %r1
    %3 = fadd <4 x half> %2, %1
    %r2 = shufflevector <3 x half> %r, <3 x half> %r, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %r2
    %5 = fadd <4 x half> %4, %3

    ret <4 x half> %5
}

; GLSL: f16mat2 = f16mat2 * f16mat2
define spir_func [2 x <2 x half>] @_Z17MatrixTimesMatrixDv2_Dv2_DhDv2_Dv2_Dh(
    [2 x <2 x half>] %m, [2 x <2 x half>] %rm) #0
{
    %nm = alloca [2 x <2 x half>]
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m1v = extractvalue [2 x <2 x half>] %m, 1

    %rm0v = extractvalue [2 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %nm0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %3, <2 x half>* %nm0

    %rm1v = extractvalue [2 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x half> %m1v, %rm1v1
    %6 = fadd <2 x half> %5, %4

    %nm1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %6, <2 x half>* %nm1

    %7 = load [2 x <2 x half>], [2 x <2 x half>]* %nm

    ret [2 x <2 x half>] %7
}

; GLSL: f16mat3x2 = f16mat2 * f16mat3x2
define spir_func [3 x <2 x half>] @_Z17MatrixTimesMatrixDv2_Dv2_DhDv3_Dv2_Dh(
    [2 x <2 x half>] %m, [3 x <2 x half>] %rm) #0
{
    %nm = alloca [3 x <2 x half>]
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m1v = extractvalue [2 x <2 x half>] %m, 1

    %rm0v = extractvalue [3 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %nm0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %3, <2 x half>* %nm0

    %rm1v = extractvalue [3 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x half> %m1v, %rm1v1
    %6 = fadd <2 x half> %5, %4

    %nm1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %6, <2 x half>* %nm1

    %rm2v = extractvalue [3 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x half> %m1v, %rm2v1
    %9 = fadd <2 x half> %8, %7

    %nm2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %9, <2 x half>* %nm2

    %10 = load [3 x <2 x half>], [3 x <2 x half>]* %nm

    ret [3 x <2 x half>] %10
}

; GLSL: f16mat4x2 = f16mat2 * f16mat4x2
define spir_func [4 x <2 x half>] @_Z17MatrixTimesMatrixDv2_Dv2_DhDv4_Dv2_Dh(
    [2 x <2 x half>] %m, [4 x <2 x half>] %rm) #0
{
    %nm = alloca [4 x <2 x half>]
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m1v = extractvalue [2 x <2 x half>] %m, 1

    %rm0v = extractvalue [4 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %nm0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %3, <2 x half>* %nm0

    %rm1v = extractvalue [4 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %4 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %5 = fmul <2 x half> %m1v, %rm1v1
    %6 = fadd <2 x half> %5, %4

    %nm1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %6, <2 x half>* %nm1

    %rm2v = extractvalue [4 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %7 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %8 = fmul <2 x half> %m1v, %rm2v1
    %9 = fadd <2 x half> %8, %7

    %nm2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %9, <2 x half>* %nm2

    %rm3v = extractvalue [4 x <2 x half>] %rm, 3
    %rm3v0 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <2 x i32> <i32 0, i32 0>
    %10 = fmul <2 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <2 x i32> <i32 1, i32 1>
    %11 = fmul <2 x half> %m1v, %rm3v1
    %12 = fadd <2 x half> %11, %10

    %nm3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 3
    store <2 x half> %12, <2 x half>* %nm3

    %13 = load [4 x <2 x half>], [4 x <2 x half>]* %nm

    ret [4 x <2 x half>] %13
}

; GLSL: f16mat3 = f16mat3 * f16mat3
define spir_func [3 x <3 x half>] @_Z17MatrixTimesMatrixDv3_Dv3_DhDv3_Dv3_Dh(
    [3 x <3 x half>] %m, [3 x <3 x half>] %rm) #0
{
    %nm = alloca [3 x <3 x half>]
    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m2v = extractvalue [3 x <3 x half>] %m, 2

    %rm0v = extractvalue [3 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %nm0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %5, <3 x half>* %nm0

    %rm1v = extractvalue [3 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x half> %m1v, %rm1v1
    %8 = fadd <3 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x half> %m2v, %rm1v2
    %10 = fadd <3 x half> %9, %8

    %nm1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %10, <3 x half>* %nm1

    %rm2v = extractvalue [3 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x half> %m1v, %rm2v1
    %13 = fadd <3 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x half> %m2v, %rm2v2
    %15 = fadd <3 x half> %14, %13

    %nm2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %15, <3 x half>* %nm2

    %16 = load [3 x <3 x half>], [3 x <3 x half>]* %nm

    ret [3 x <3 x half>] %16
}

; GLSL: f16mat2x3 = f16mat3 * f16mat2x3
define spir_func [2 x <3 x half>] @_Z17MatrixTimesMatrixDv3_Dv3_DhDv2_Dv3_Dh(
    [3 x <3 x half>] %m, [2 x <3 x half>] %rm) #0
{
    %nm = alloca [2 x <3 x half>]
    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m2v = extractvalue [3 x <3 x half>] %m, 2

    %rm0v = extractvalue [2 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %nm0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %5, <3 x half>* %nm0

    %rm1v = extractvalue [2 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x half> %m1v, %rm1v1
    %8 = fadd <3 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x half> %m2v, %rm1v2
    %10 = fadd <3 x half> %9, %8

    %nm1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %10, <3 x half>* %nm1

    %11 = load [2 x <3 x half>], [2 x <3 x half>]* %nm

    ret [2 x <3 x half>] %11
}

; GLSL: f16mat4x3 = f16mat3 * f16mat4x3
define spir_func [4 x <3 x half>] @_Z17MatrixTimesMatrixDv3_Dv3_DhDv4_Dv3_Dh(
    [3 x <3 x half>] %m, [4 x <3 x half>] %rm) #0
{
    %nm = alloca [4 x <3 x half>]
    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m2v = extractvalue [3 x <3 x half>] %m, 2

    %rm0v = extractvalue [4 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %nm0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %5, <3 x half>* %nm0

    %rm1v = extractvalue [4 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %6 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %7 = fmul <3 x half> %m1v, %rm1v1
    %8 = fadd <3 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %9 = fmul <3 x half> %m2v, %rm1v2
    %10 = fadd <3 x half> %9, %8

    %nm1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %10, <3 x half>* %nm1

    %rm2v = extractvalue [4 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %11 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %12 = fmul <3 x half> %m1v, %rm2v1
    %13 = fadd <3 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %14 = fmul <3 x half> %m2v, %rm2v2
    %15 = fadd <3 x half> %14, %13

    %nm2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %15, <3 x half>* %nm2

    %rm3v = extractvalue [4 x <3 x half>] %rm, 3
    %rm3v0 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %16 = fmul <3 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %17 = fmul <3 x half> %m1v, %rm3v1
    %18 = fadd <3 x half> %17, %16

    %rm3v2 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %19 = fmul <3 x half> %m2v, %rm3v2
    %20 = fadd <3 x half> %19, %18

    %nm3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 3
    store <3 x half> %20, <3 x half>* %nm3

    %21 = load [4 x <3 x half>], [4 x <3 x half>]* %nm

    ret [4 x <3 x half>] %21
}

; GLSL: f16mat4 = f16mat4 * f16mat4
define spir_func [4 x <4 x half>] @_Z17MatrixTimesMatrixDv4_Dv4_DhDv4_Dv4_Dh(
    [4 x <4 x half>] %m, [4 x <4 x half>] %rm) #0
{
    %nm = alloca [4 x <4 x half>]
    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m3v = extractvalue [4 x <4 x half>] %m, 3

    %rm0v = extractvalue [4 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x half> %m3v, %rm0v3
    %7 = fadd <4 x half> %6, %5

    %nm0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %7, <4 x half>* %nm0

    %rm1v = extractvalue [4 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x half> %m1v, %rm1v1
    %10 = fadd <4 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x half> %m2v, %rm1v2
    %12 = fadd <4 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x half> %m3v, %rm1v3
    %14 = fadd <4 x half> %13, %12

    %nm1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %14, <4 x half>* %nm1

    %rm2v = extractvalue [4 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x half> %m1v, %rm2v1
    %17 = fadd <4 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x half> %m2v, %rm2v2
    %19 = fadd <4 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x half> %m3v, %rm2v3
    %21 = fadd <4 x half> %20, %19

    %nm2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %21, <4 x half>* %nm2

    %rm3v = extractvalue [4 x <4 x half>] %rm, 3
    %rm3v0 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %22 = fmul <4 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %23 = fmul <4 x half> %m1v, %rm3v1
    %24 = fadd <4 x half> %23, %22

    %rm3v2 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %25 = fmul <4 x half> %m2v, %rm3v2
    %26 = fadd <4 x half> %25, %24

    %rm3v3 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %27 = fmul <4 x half> %m3v, %rm3v3
    %28 = fadd <4 x half> %27, %26

    %nm3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 3
    store <4 x half> %28, <4 x half>* %nm3

    %29 = load [4 x <4 x half>], [4 x <4 x half>]* %nm

    ret [4 x <4 x half>] %29
}

; GLSL: f16mat2x4 = f16mat4 * f16mat2x4
define spir_func [2 x <4 x half>] @_Z17MatrixTimesMatrixDv4_Dv4_DhDv2_Dv4_Dh(
    [4 x <4 x half>] %m, [2 x <4 x half>] %rm) #0
{
    %nm = alloca [2 x <4 x half>]
    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m3v = extractvalue [4 x <4 x half>] %m, 3

    %rm0v = extractvalue [2 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x half> %m3v, %rm0v3
    %7 = fadd <4 x half> %6, %5

    %nm0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %7, <4 x half>* %nm0

    %rm1v = extractvalue [2 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x half> %m1v, %rm1v1
    %10 = fadd <4 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x half> %m2v, %rm1v2
    %12 = fadd <4 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x half> %m3v, %rm1v3
    %14 = fadd <4 x half> %13, %12

    %nm1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %14, <4 x half>* %nm1

    %15 = load [2 x <4 x half>], [2 x <4 x half>]* %nm

    ret [2 x <4 x half>] %15
}

; GLSL: f16mat3x4 = f16mat4 * f16mat3x4
define spir_func [3 x <4 x half>] @_Z17MatrixTimesMatrixDv4_Dv4_DhDv3_Dv4_Dh(
    [4 x <4 x half>] %m, [3 x <4 x half>] %rm) #0
{
    %nm = alloca [3 x <4 x half>]
    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m3v = extractvalue [4 x <4 x half>] %m, 3

    %rm0v = extractvalue [3 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %6 = fmul <4 x half> %m3v, %rm0v3
    %7 = fadd <4 x half> %6, %5

    %nm0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %7, <4 x half>* %nm0

    %rm1v = extractvalue [3 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %8 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %9 = fmul <4 x half> %m1v, %rm1v1
    %10 = fadd <4 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %11 = fmul <4 x half> %m2v, %rm1v2
    %12 = fadd <4 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %13 = fmul <4 x half> %m3v, %rm1v3
    %14 = fadd <4 x half> %13, %12

    %nm1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %14, <4 x half>* %nm1

    %rm2v = extractvalue [3 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %15 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %16 = fmul <4 x half> %m1v, %rm2v1
    %17 = fadd <4 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %18 = fmul <4 x half> %m2v, %rm2v2
    %19 = fadd <4 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <4 x i32> <i32 3, i32 3, i32 3, i32 3>
    %20 = fmul <4 x half> %m3v, %rm2v3
    %21 = fadd <4 x half> %20, %19

    %nm2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %21, <4 x half>* %nm2

    %22 = load [3 x <4 x half>], [3 x <4 x half>]* %nm

    ret [3 x <4 x half>] %22
}

; GLSL: f16mat2 = f16mat3x2 * f16mat2x3
define spir_func [2 x <2 x half>] @_Z17MatrixTimesMatrixDv3_Dv2_DhDv2_Dv3_Dh(
    [3 x <2 x half>] %m, [2 x <3 x half>] %rm) #0
{
    %nm = alloca [2 x <2 x half>]
    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m2v = extractvalue [3 x <2 x half>] %m, 2

    %rm0v = extractvalue [2 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %nm0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %5, <2 x half>* %nm0

    %rm1v = extractvalue [2 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x half> %m1v, %rm1v1
    %8 = fadd <2 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x half> %m2v, %rm1v2
    %10 = fadd <2 x half> %9, %8

    %nm1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %10, <2 x half>* %nm1

    %11 = load [2 x <2 x half>], [2 x <2 x half>]* %nm

    ret [2 x <2 x half>] %11
}

; GLSL: f16mat3x2 = f16mat3x2 * f16mat3
define spir_func [3 x <2 x half>] @_Z17MatrixTimesMatrixDv3_Dv2_DhDv3_Dv3_Dh(
    [3 x <2 x half>] %m, [3 x <3 x half>] %rm) #0
{
    %nm = alloca [3 x <2 x half>]
    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m2v = extractvalue [3 x <2 x half>] %m, 2

    %rm0v = extractvalue [3 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %nm0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %5, <2 x half>* %nm0

    %rm1v = extractvalue [3 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x half> %m1v, %rm1v1
    %8 = fadd <2 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x half> %m2v, %rm1v2
    %10 = fadd <2 x half> %9, %8

    %nm1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %10, <2 x half>* %nm1

    %rm2v = extractvalue [3 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x half> %m1v, %rm2v1
    %13 = fadd <2 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x half> %m2v, %rm2v2
    %15 = fadd <2 x half> %14, %13

    %nm2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %15, <2 x half>* %nm2

    %16 = load [3 x <2 x half>], [3 x <2 x half>]* %nm

    ret [3 x <2 x half>] %16
}

; GLSL: f16mat4x2 = f16mat3x2 * f16mat4x3
define spir_func [4 x <2 x half>] @_Z17MatrixTimesMatrixDv3_Dv2_DhDv4_Dv3_Dh(
    [3 x <2 x half>] %m, [4 x <3 x half>] %rm) #0
{
    %nm = alloca [4 x <2 x half>]
    %m0v = extractvalue [3 x <2 x half>] %m, 0
    %m1v = extractvalue [3 x <2 x half>] %m, 1
    %m2v = extractvalue [3 x <2 x half>] %m, 2

    %rm0v = extractvalue [4 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %nm0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %5, <2 x half>* %nm0

    %rm1v = extractvalue [4 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %6 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %7 = fmul <2 x half> %m1v, %rm1v1
    %8 = fadd <2 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %9 = fmul <2 x half> %m2v, %rm1v2
    %10 = fadd <2 x half> %9, %8

    %nm1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %10, <2 x half>* %nm1

    %rm2v = extractvalue [4 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %11 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %12 = fmul <2 x half> %m1v, %rm2v1
    %13 = fadd <2 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <2 x i32> <i32 2, i32 2>
    %14 = fmul <2 x half> %m2v, %rm2v2
    %15 = fadd <2 x half> %14, %13

    %nm2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %15, <2 x half>* %nm2

    %rm3v = extractvalue [4 x <3 x half>] %rm, 3
    %rm3v0 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <2 x i32> <i32 0, i32 0>
    %16 = fmul <2 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <2 x i32> <i32 1, i32 1>
    %17 = fmul <2 x half> %m1v, %rm3v1
    %18 = fadd <2 x half> %17, %16

    %rm3v2 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <2 x i32> <i32 2, i32 2>
    %19 = fmul <2 x half> %m2v, %rm3v2
    %20 = fadd <2 x half> %19, %18

    %nm3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 3
    store <2 x half> %20, <2 x half>* %nm3

    %21 = load [4 x <2 x half>], [4 x <2 x half>]* %nm

    ret [4 x <2 x half>] %21
}

; GLSL: f16mat2x3 = f16mat2x3 * f16mat2
define spir_func [2 x <3 x half>] @_Z17MatrixTimesMatrixDv2_Dv3_DhDv2_Dv2_Dh(
    [2 x <3 x half>] %m, [2 x <2 x half>] %rm) #0
{
    %nm = alloca [2 x <3 x half>]
    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m1v = extractvalue [2 x <3 x half>] %m, 1

    %rm0v = extractvalue [2 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %nm0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %3, <3 x half>* %nm0

    %rm1v = extractvalue [2 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x half> %m1v, %rm1v1
    %6 = fadd <3 x half> %5, %4

    %nm1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %6, <3 x half>* %nm1

    %7 = load [2 x <3 x half>], [2 x <3 x half>]* %nm

    ret [2 x <3 x half>] %7
}

; GLSL: f16mat3 = f16mat2x3 * f16mat3x2
define spir_func [3 x <3 x half>] @_Z17MatrixTimesMatrixDv2_Dv3_DhDv3_Dv2_Dh(
    [2 x <3 x half>] %m, [3 x <2 x half>] %rm) #0
{
    %nm = alloca [3 x <3 x half>]
    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m1v = extractvalue [2 x <3 x half>] %m, 1

    %rm0v = extractvalue [3 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %nm0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %3, <3 x half>* %nm0

    %rm1v = extractvalue [3 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x half> %m1v, %rm1v1
    %6 = fadd <3 x half> %5, %4

    %nm1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %6, <3 x half>* %nm1

    %rm2v = extractvalue [3 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x half> %m1v, %rm2v1
    %9 = fadd <3 x half> %8, %7

    %nm2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %9, <3 x half>* %nm2

    %10 = load [3 x <3 x half>], [3 x <3 x half>]* %nm

    ret [3 x <3 x half>] %10
}

; GLSL: f16mat4x3 = f16mat2x3 * f16mat4x2
define spir_func [4 x <3 x half>] @_Z17MatrixTimesMatrixDv2_Dv3_DhDv4_Dv2_Dh(
    [2 x <3 x half>] %m, [4 x <2 x half>] %rm) #0
{
    %nm = alloca [4 x <3 x half>]
    %m0v = extractvalue [2 x <3 x half>] %m, 0
    %m1v = extractvalue [2 x <3 x half>] %m, 1

    %rm0v = extractvalue [4 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %nm0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %3, <3 x half>* %nm0

    %rm1v = extractvalue [4 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %4 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %5 = fmul <3 x half> %m1v, %rm1v1
    %6 = fadd <3 x half> %5, %4

    %nm1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %6, <3 x half>* %nm1

    %rm2v = extractvalue [4 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %7 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %8 = fmul <3 x half> %m1v, %rm2v1
    %9 = fadd <3 x half> %8, %7

    %nm2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %9, <3 x half>* %nm2

    %rm3v = extractvalue [4 x <2 x half>] %rm, 3
    %rm3v0 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %10 = fmul <3 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %11 = fmul <3 x half> %m1v, %rm3v1
    %12 = fadd <3 x half> %11, %10

    %nm3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 3
    store <3 x half> %12, <3 x half>* %nm3

    %13 = load [4 x <3 x half>], [4 x <3 x half>]* %nm

    ret [4 x <3 x half>] %13
}

; GLSL: f16mat2 = f16mat4x2 * f16mat2x4
define spir_func [2 x <2 x half>] @_Z17MatrixTimesMatrixDv4_Dv2_DhDv2_Dv4_Dh(
    [4 x <2 x half>] %m, [2 x <4 x half>] %rm) #0
{
    %nm = alloca [2 x <2 x half>]
    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m3v = extractvalue [4 x <2 x half>] %m, 3

    %rm0v = extractvalue [2 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x half> %m3v, %rm0v3
    %7 = fadd <2 x half> %6, %5

    %nm0 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %7, <2 x half>* %nm0

    %rm1v = extractvalue [2 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x half> %m1v, %rm1v1
    %10 = fadd <2 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x half> %m2v, %rm1v2
    %12 = fadd <2 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x half> %m3v, %rm1v3
    %14 = fadd <2 x half> %13, %12

    %nm1 = getelementptr inbounds [2 x <2 x half>], [2 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %14, <2 x half>* %nm1

    %15 = load [2 x <2 x half>], [2 x <2 x half>]* %nm

    ret [2 x <2 x half>] %15
}

; GLSL: f16mat3x2 = f16mat4x2 * f16mat3x4
define spir_func [3 x <2 x half>] @_Z17MatrixTimesMatrixDv4_Dv2_DhDv3_Dv4_Dh(
    [4 x <2 x half>] %m, [3 x <4 x half>] %rm) #0
{
    %nm = alloca [3 x <2 x half>]
    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m3v = extractvalue [4 x <2 x half>] %m, 3

    %rm0v = extractvalue [3 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x half> %m3v, %rm0v3
    %7 = fadd <2 x half> %6, %5

    %nm0 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %7, <2 x half>* %nm0

    %rm1v = extractvalue [3 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x half> %m1v, %rm1v1
    %10 = fadd <2 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x half> %m2v, %rm1v2
    %12 = fadd <2 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x half> %m3v, %rm1v3
    %14 = fadd <2 x half> %13, %12

    %nm1 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %14, <2 x half>* %nm1

    %rm2v = extractvalue [3 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x half> %m1v, %rm2v1
    %17 = fadd <2 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x half> %m2v, %rm2v2
    %19 = fadd <2 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x half> %m3v, %rm2v3
    %21 = fadd <2 x half> %20, %19

    %nm2 = getelementptr inbounds [3 x <2 x half>], [3 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %21, <2 x half>* %nm2

    %22 = load [3 x <2 x half>], [3 x <2 x half>]* %nm

    ret [3 x <2 x half>] %22
}

; GLSL: f16mat4x2 = f16mat4x2 * f16mat4
define spir_func [4 x <2 x half>] @_Z17MatrixTimesMatrixDv4_Dv2_DhDv4_Dv4_Dh(
    [4 x <2 x half>] %m, [4 x <4 x half>] %rm) #0
{
    %nm = alloca [4 x <2 x half>]
    %m0v = extractvalue [4 x <2 x half>] %m, 0
    %m1v = extractvalue [4 x <2 x half>] %m, 1
    %m2v = extractvalue [4 x <2 x half>] %m, 2
    %m3v = extractvalue [4 x <2 x half>] %m, 3

    %rm0v = extractvalue [4 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 0, i32 0>
    %1 = fmul <2 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 1, i32 1>
    %2 = fmul <2 x half> %m1v, %rm0v1
    %3 = fadd <2 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 2, i32 2>
    %4 = fmul <2 x half> %m2v, %rm0v2
    %5 = fadd <2 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <2 x i32> <i32 3, i32 3>
    %6 = fmul <2 x half> %m3v, %rm0v3
    %7 = fadd <2 x half> %6, %5

    %nm0 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 0
    store <2 x half> %7, <2 x half>* %nm0

    %rm1v = extractvalue [4 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 0, i32 0>
    %8 = fmul <2 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 1, i32 1>
    %9 = fmul <2 x half> %m1v, %rm1v1
    %10 = fadd <2 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 2, i32 2>
    %11 = fmul <2 x half> %m2v, %rm1v2
    %12 = fadd <2 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <2 x i32> <i32 3, i32 3>
    %13 = fmul <2 x half> %m3v, %rm1v3
    %14 = fadd <2 x half> %13, %12

    %nm1 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 1
    store <2 x half> %14, <2 x half>* %nm1

    %rm2v = extractvalue [4 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 0, i32 0>
    %15 = fmul <2 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 1, i32 1>
    %16 = fmul <2 x half> %m1v, %rm2v1
    %17 = fadd <2 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 2, i32 2>
    %18 = fmul <2 x half> %m2v, %rm2v2
    %19 = fadd <2 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <2 x i32> <i32 3, i32 3>
    %20 = fmul <2 x half> %m3v, %rm2v3
    %21 = fadd <2 x half> %20, %19

    %nm2 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 2
    store <2 x half> %21, <2 x half>* %nm2

    %rm3v = extractvalue [4 x <4 x half>] %rm, 3
    %rm3v0 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <2 x i32> <i32 0, i32 0>
    %22 = fmul <2 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <2 x i32> <i32 1, i32 1>
    %23 = fmul <2 x half> %m1v, %rm3v1
    %24 = fadd <2 x half> %23, %22

    %rm3v2 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <2 x i32> <i32 2, i32 2>
    %25 = fmul <2 x half> %m2v, %rm3v2
    %26 = fadd <2 x half> %25, %24

    %rm3v3 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <2 x i32> <i32 3, i32 3>
    %27 = fmul <2 x half> %m3v, %rm3v3
    %28 = fadd <2 x half> %27, %26

    %nm3 = getelementptr inbounds [4 x <2 x half>], [4 x <2 x half>]* %nm, i32 0, i32 3
    store <2 x half> %28, <2 x half>* %nm3

    %29 = load [4 x <2 x half>], [4 x <2 x half>]* %nm

    ret [4 x <2 x half>] %29
}

; GLSL: f16mat2x4 = f16mat2x4 * f16mat2
define spir_func [2 x <4 x half>] @_Z17MatrixTimesMatrixDv2_Dv4_DhDv2_Dv2_Dh(
    [2 x <4 x half>] %m, [2 x <2 x half>] %rm) #0
{
    %nm = alloca [2 x <4 x half>]
    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m1v = extractvalue [2 x <4 x half>] %m, 1

    %rm0v = extractvalue [2 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %nm0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %3, <4 x half>* %nm0

    %rm1v = extractvalue [2 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x half> %m1v, %rm1v1
    %6 = fadd <4 x half> %5, %4

    %nm1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %6, <4 x half>* %nm1

    %7 = load [2 x <4 x half>], [2 x <4 x half>]* %nm

    ret [2 x <4 x half>] %7
}

; GLSL: f16mat3x4 = f16mat2x4 * f16mat3x2
define spir_func [3 x <4 x half>] @_Z17MatrixTimesMatrixDv2_Dv4_DhDv3_Dv2_Dh(
    [2 x <4 x half>] %m, [3 x <2 x half>] %rm) #0
{
    %nm = alloca [3 x <4 x half>]
    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m1v = extractvalue [2 x <4 x half>] %m, 1

    %rm0v = extractvalue [3 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %nm0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %3, <4 x half>* %nm0

    %rm1v = extractvalue [3 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x half> %m1v, %rm1v1
    %6 = fadd <4 x half> %5, %4

    %nm1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %6, <4 x half>* %nm1

    %rm2v = extractvalue [3 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x half> %m1v, %rm2v1
    %9 = fadd <4 x half> %8, %7

    %nm2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %9, <4 x half>* %nm2

    %10 = load [3 x <4 x half>], [3 x <4 x half>]* %nm

    ret [3 x <4 x half>] %10
}

; GLSL: f16mat4 = f16mat2x4 * f16mat4x2
define spir_func [4 x <4 x half>] @_Z17MatrixTimesMatrixDv2_Dv4_DhDv4_Dv2_Dh(
    [2 x <4 x half>] %m, [4 x <2 x half>] %rm) #0
{
    %nm = alloca [4 x <4 x half>]
    %m0v = extractvalue [2 x <4 x half>] %m, 0
    %m1v = extractvalue [2 x <4 x half>] %m, 1

    %rm0v = extractvalue [4 x <2 x half>] %rm, 0
    %rm0v0 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <2 x half> %rm0v, <2 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %nm0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %3, <4 x half>* %nm0

    %rm1v = extractvalue [4 x <2 x half>] %rm, 1
    %rm1v0 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %4 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <2 x half> %rm1v, <2 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %5 = fmul <4 x half> %m1v, %rm1v1
    %6 = fadd <4 x half> %5, %4

    %nm1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %6, <4 x half>* %nm1

    %rm2v = extractvalue [4 x <2 x half>] %rm, 2
    %rm2v0 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %7 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <2 x half> %rm2v, <2 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %8 = fmul <4 x half> %m1v, %rm2v1
    %9 = fadd <4 x half> %8, %7

    %nm2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %9, <4 x half>* %nm2

    %rm3v = extractvalue [4 x <2 x half>] %rm, 3
    %rm3v0 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %10 = fmul <4 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <2 x half> %rm3v, <2 x half> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %11 = fmul <4 x half> %m1v, %rm3v1
    %12 = fadd <4 x half> %11, %10

    %nm3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 3
    store <4 x half> %12, <4 x half>* %nm3

    %13 = load [4 x <4 x half>], [4 x <4 x half>]* %nm

    ret [4 x <4 x half>] %13
}

; GLSL: f16mat2x3 = f16mat4x3 * f16mat2x4
define spir_func [2 x <3 x half>] @_Z17MatrixTimesMatrixDv4_Dv3_DhDv2_Dv4_Dh(
    [4 x <3 x half>] %m, [2 x <4 x half>] %rm) #0
{
    %nm = alloca [2 x <3 x half>]
    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m3v = extractvalue [4 x <3 x half>] %m, 3

    %rm0v = extractvalue [2 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x half> %m3v, %rm0v3
    %7 = fadd <3 x half> %6, %5

    %nm0 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %7, <3 x half>* %nm0

    %rm1v = extractvalue [2 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x half> %m1v, %rm1v1
    %10 = fadd <3 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x half> %m2v, %rm1v2
    %12 = fadd <3 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x half> %m3v, %rm1v3
    %14 = fadd <3 x half> %13, %12

    %nm1 = getelementptr inbounds [2 x <3 x half>], [2 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %14, <3 x half>* %nm1

    %15 = load [2 x <3 x half>], [2 x <3 x half>]* %nm

    ret [2 x <3 x half>] %15
}

; GLSL: f16mat3 = f16mat4x3 * f16mat3x4
define spir_func [3 x <3 x half>] @_Z17MatrixTimesMatrixDv4_Dv3_DhDv3_Dv4_Dh(
    [4 x <3 x half>] %m, [3 x <4 x half>] %rm) #0
{
    %nm = alloca [3 x <3 x half>]
    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m3v = extractvalue [4 x <3 x half>] %m, 3

    %rm0v = extractvalue [3 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x half> %m3v, %rm0v3
    %7 = fadd <3 x half> %6, %5

    %nm0 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %7, <3 x half>* %nm0

    %rm1v = extractvalue [3 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x half> %m1v, %rm1v1
    %10 = fadd <3 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x half> %m2v, %rm1v2
    %12 = fadd <3 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x half> %m3v, %rm1v3
    %14 = fadd <3 x half> %13, %12

    %nm1 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %14, <3 x half>* %nm1

    %rm2v = extractvalue [3 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x half> %m1v, %rm2v1
    %17 = fadd <3 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x half> %m2v, %rm2v2
    %19 = fadd <3 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x half> %m3v, %rm2v3
    %21 = fadd <3 x half> %20, %19

    %nm2 = getelementptr inbounds [3 x <3 x half>], [3 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %21, <3 x half>* %nm2

    %22 = load [3 x <3 x half>], [3 x <3 x half>]* %nm

    ret [3 x <3 x half>] %22
}

; GLSL: f16mat4x3 = f16mat4x3 * f16mat4
define spir_func [4 x <3 x half>] @_Z17MatrixTimesMatrixDv4_Dv3_DhDv4_Dv4_Dh(
    [4 x <3 x half>] %m, [4 x <4 x half>] %rm) #0
{
    %nm = alloca [4 x <3 x half>]
    %m0v = extractvalue [4 x <3 x half>] %m, 0
    %m1v = extractvalue [4 x <3 x half>] %m, 1
    %m2v = extractvalue [4 x <3 x half>] %m, 2
    %m3v = extractvalue [4 x <3 x half>] %m, 3

    %rm0v = extractvalue [4 x <4 x half>] %rm, 0
    %rm0v0 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 0, i32 0, i32 0>
    %1 = fmul <3 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 1, i32 1, i32 1>
    %2 = fmul <3 x half> %m1v, %rm0v1
    %3 = fadd <3 x half> %2, %1

    %rm0v2 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 2, i32 2, i32 2>
    %4 = fmul <3 x half> %m2v, %rm0v2
    %5 = fadd <3 x half> %4, %3

    %rm0v3 = shufflevector <4 x half> %rm0v, <4 x half> %rm0v, <3 x i32> <i32 3, i32 3, i32 3>
    %6 = fmul <3 x half> %m3v, %rm0v3
    %7 = fadd <3 x half> %6, %5

    %nm0 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 0
    store <3 x half> %7, <3 x half>* %nm0

    %rm1v = extractvalue [4 x <4 x half>] %rm, 1
    %rm1v0 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 0, i32 0, i32 0>
    %8 = fmul <3 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 1, i32 1, i32 1>
    %9 = fmul <3 x half> %m1v, %rm1v1
    %10 = fadd <3 x half> %9, %8

    %rm1v2 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 2, i32 2, i32 2>
    %11 = fmul <3 x half> %m2v, %rm1v2
    %12 = fadd <3 x half> %11, %10

    %rm1v3 = shufflevector <4 x half> %rm1v, <4 x half> %rm1v, <3 x i32> <i32 3, i32 3, i32 3>
    %13 = fmul <3 x half> %m3v, %rm1v3
    %14 = fadd <3 x half> %13, %12

    %nm1 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 1
    store <3 x half> %14, <3 x half>* %nm1

    %rm2v = extractvalue [4 x <4 x half>] %rm, 2
    %rm2v0 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 0, i32 0, i32 0>
    %15 = fmul <3 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 1, i32 1, i32 1>
    %16 = fmul <3 x half> %m1v, %rm2v1
    %17 = fadd <3 x half> %16, %15

    %rm2v2 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 2, i32 2, i32 2>
    %18 = fmul <3 x half> %m2v, %rm2v2
    %19 = fadd <3 x half> %18, %17

    %rm2v3 = shufflevector <4 x half> %rm2v, <4 x half> %rm2v, <3 x i32> <i32 3, i32 3, i32 3>
    %20 = fmul <3 x half> %m3v, %rm2v3
    %21 = fadd <3 x half> %20, %19

    %nm2 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 2
    store <3 x half> %21, <3 x half>* %nm2

    %rm3v = extractvalue [4 x <4 x half>] %rm, 3
    %rm3v0 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <3 x i32> <i32 0, i32 0, i32 0>
    %22 = fmul <3 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <3 x i32> <i32 1, i32 1, i32 1>
    %23 = fmul <3 x half> %m1v, %rm3v1
    %24 = fadd <3 x half> %23, %22

    %rm3v2 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <3 x i32> <i32 2, i32 2, i32 2>
    %25 = fmul <3 x half> %m2v, %rm3v2
    %26 = fadd <3 x half> %25, %24

    %rm3v3 = shufflevector <4 x half> %rm3v, <4 x half> %rm3v, <3 x i32> <i32 3, i32 3, i32 3>
    %27 = fmul <3 x half> %m3v, %rm3v3
    %28 = fadd <3 x half> %27, %26

    %nm3 = getelementptr inbounds [4 x <3 x half>], [4 x <3 x half>]* %nm, i32 0, i32 3
    store <3 x half> %28, <3 x half>* %nm3

    %29 = load [4 x <3 x half>], [4 x <3 x half>]* %nm

    ret [4 x <3 x half>] %29
}

; GLSL: f16mat2x4 = f16mat3x4 * f16mat2x3
define spir_func [2 x <4 x half>] @_Z17MatrixTimesMatrixDv3_Dv4_DhDv2_Dv3_Dh(
    [3 x <4 x half>] %m, [2 x <3 x half>] %rm) #0
{
    %nm = alloca [2 x <4 x half>]
    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m2v = extractvalue [3 x <4 x half>] %m, 2

    %rm0v = extractvalue [2 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %nm0 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %5, <4 x half>* %nm0

    %rm1v = extractvalue [2 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x half> %m1v, %rm1v1
    %8 = fadd <4 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x half> %m2v, %rm1v2
    %10 = fadd <4 x half> %9, %8

    %nm1 = getelementptr inbounds [2 x <4 x half>], [2 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %10, <4 x half>* %nm1

    %11 = load [2 x <4 x half>], [2 x <4 x half>]* %nm

    ret [2 x <4 x half>] %11
}

; GLSL: f16mat3x4 = f16mat3x4 * f16mat3
define spir_func [3 x <4 x half>] @_Z17MatrixTimesMatrixDv3_Dv4_DhDv3_Dv3_Dh(
    [3 x <4 x half>] %m, [3 x <3 x half>] %rm) #0
{
    %nm = alloca [3 x <4 x half>]
    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m2v = extractvalue [3 x <4 x half>] %m, 2

    %rm0v = extractvalue [3 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %nm0 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %5, <4 x half>* %nm0

    %rm1v = extractvalue [3 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x half> %m1v, %rm1v1
    %8 = fadd <4 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x half> %m2v, %rm1v2
    %10 = fadd <4 x half> %9, %8

    %nm1 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %10, <4 x half>* %nm1

    %rm2v = extractvalue [3 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x half> %m1v, %rm2v1
    %13 = fadd <4 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x half> %m2v, %rm2v2
    %15 = fadd <4 x half> %14, %13

    %nm2 = getelementptr inbounds [3 x <4 x half>], [3 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %15, <4 x half>* %nm2

    %16 = load [3 x <4 x half>], [3 x <4 x half>]* %nm

    ret [3 x <4 x half>] %16
}

; GLSL: f16mat4 = f16mat3x4 * f16mat4x3
define spir_func [4 x <4 x half>] @_Z17MatrixTimesMatrixDv3_Dv4_DhDv4_Dv3_Dh(
    [3 x <4 x half>] %m, [4 x <3 x half>] %rm) #0
{
    %nm = alloca [4 x <4 x half>]
    %m0v = extractvalue [3 x <4 x half>] %m, 0
    %m1v = extractvalue [3 x <4 x half>] %m, 1
    %m2v = extractvalue [3 x <4 x half>] %m, 2

    %rm0v = extractvalue [4 x <3 x half>] %rm, 0
    %rm0v0 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %1 = fmul <4 x half> %m0v, %rm0v0

    %rm0v1 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %2 = fmul <4 x half> %m1v, %rm0v1
    %3 = fadd <4 x half> %2, %1

    %rm0v2 = shufflevector <3 x half> %rm0v, <3 x half> %rm0v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %4 = fmul <4 x half> %m2v, %rm0v2
    %5 = fadd <4 x half> %4, %3

    %nm0 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 0
    store <4 x half> %5, <4 x half>* %nm0

    %rm1v = extractvalue [4 x <3 x half>] %rm, 1
    %rm1v0 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %6 = fmul <4 x half> %m0v, %rm1v0

    %rm1v1 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %7 = fmul <4 x half> %m1v, %rm1v1
    %8 = fadd <4 x half> %7, %6

    %rm1v2 = shufflevector <3 x half> %rm1v, <3 x half> %rm1v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %9 = fmul <4 x half> %m2v, %rm1v2
    %10 = fadd <4 x half> %9, %8

    %nm1 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 1
    store <4 x half> %10, <4 x half>* %nm1

    %rm2v = extractvalue [4 x <3 x half>] %rm, 2
    %rm2v0 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %11 = fmul <4 x half> %m0v, %rm2v0

    %rm2v1 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %12 = fmul <4 x half> %m1v, %rm2v1
    %13 = fadd <4 x half> %12, %11

    %rm2v2 = shufflevector <3 x half> %rm2v, <3 x half> %rm2v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %14 = fmul <4 x half> %m2v, %rm2v2
    %15 = fadd <4 x half> %14, %13

    %nm2 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 2
    store <4 x half> %15, <4 x half>* %nm2

    %rm3v = extractvalue [4 x <3 x half>] %rm, 3
    %rm3v0 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
    %16 = fmul <4 x half> %m0v, %rm3v0

    %rm3v1 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
    %17 = fmul <4 x half> %m1v, %rm3v1
    %18 = fadd <4 x half> %17, %16

    %rm3v2 = shufflevector <3 x half> %rm3v, <3 x half> %rm3v, <4 x i32> <i32 2, i32 2, i32 2, i32 2>
    %19 = fmul <4 x half> %m2v, %rm3v2
    %20 = fadd <4 x half> %19, %18

    %nm3 = getelementptr inbounds [4 x <4 x half>], [4 x <4 x half>]* %nm, i32 0, i32 3
    store <4 x half> %20, <4 x half>* %nm3

    %21 = load [4 x <4 x half>], [4 x <4 x half>]* %nm

    ret [4 x <4 x half>] %21
}

; GLSL helper: float16_t = determinant2(f16vec2(float16_t, float16_t), f16vec2(float16_t, float16_t))
define spir_func half @llpc.determinant2(
    half %x0, half %y0, half %x1, half %y1)
{
    ; | x0   x1 |
    ; |         | = x0 * y1 - y0 * x1
    ; | y0   y1 |

    %1 = fmul half %x0, %y1
    %2 = fmul half %y0, %x1
    %3 = fsub half %1, %2
    ret half %3
}

; GLSL helper: float16_t = determinant3(f16vec3(float16_t, float16_t, float16_t), f16vec3(float16_t, float16_t, float16_t))
define spir_func half @llpc.determinant3.f16(
    half %x0, half %y0, half %z0,
    half %x1, half %y1, half %z1,
    half %x2, half %y2, half %z2)
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

    %1 = call half @llpc.determinant2(half %y1, half %z1, half %y2, half %z2)
    %2 = fmul half %1, %x0
    %3 = call half @llpc.determinant2(half %z1, half %x1, half %z2, half %x2)
    %4 = fmul half %3, %y0
    %5 = fadd half %2, %4
    %6 = call half @llpc.determinant2(half %x1, half %y1, half %x2, half %y2)
    %7 = fmul half %6, %z0
    %8 = fadd half %7, %5
    ret half %8
}

; GLSL helper: float16_t = determinant4(f16vec4(float16_t, float16_t, float16_t, float16_t), f16vec4(float16_t, float16_t, float16_t, float16_t))
define spir_func half @llpc.determinant4.f16(
    half %x0, half %y0, half %z0, half %w0,
    half %x1, half %y1, half %z1, half %w1,
    half %x2, half %y2, half %z2, half %w2,
    half %x3, half %y3, half %z3, half %w3)

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

    %1 = call half @llpc.determinant3.f16(half %y1, half %z1, half %w1, half %y2, half %z2, half %w2, half %y3, half %z3, half %w3)
    %2 = fmul half %1, %x0
    %3 = call half @llpc.determinant3.f16(half %z1, half %x1, half %w1, half %z2, half %x2, half %w2, half %z3, half %x3, half %w3)
    %4 = fmul half %3, %y0
    %5 = fadd half %2, %4
    %6 = call half @llpc.determinant3.f16(half %x1, half %y1, half %w1, half %x2, half %y2, half %w2, half %x3, half %y3, half %w3)
    %7 = fmul half %6, %z0
    %8 = fadd half %5, %7
    %9 = call half @llpc.determinant3.f16(half %y1, half %x1, half %z1, half %y2, half %x2, half %z2, half %y3, half %x3, half %z3)
    %10 = fmul half %9, %w0
    %11 = fadd half %8, %10
    ret half %11
}

; GLSL: half = determinant(f16mat2)
define spir_func half @_Z11determinantDv2_Dv2_Dh(
    [2 x <2 x half>] %m) #0
{
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %m0v0 = extractelement <2 x half> %m0v, i32 0
    %m0v1 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x half>] %m, 1
    %m1v0 = extractelement <2 x half> %m1v, i32 0
    %m1v1 = extractelement <2 x half> %m1v, i32 1

    %d = call half @llpc.determinant2(half %m0v0, half %m0v1, half %m1v0, half %m1v1)
    ret half %d
}

; GLSL: half = determinant(f16mat3)
define spir_func half @_Z11determinantDv3_Dv3_Dh(
    [3 x <3 x half>] %m) #0
{
    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %m0v0 = extractelement <3 x half> %m0v, i32 0
    %m0v1 = extractelement <3 x half> %m0v, i32 1
    %m0v2 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %m1v0 = extractelement <3 x half> %m1v, i32 0
    %m1v1 = extractelement <3 x half> %m1v, i32 1
    %m1v2 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x half>] %m, 2
    %m2v0 = extractelement <3 x half> %m2v, i32 0
    %m2v1 = extractelement <3 x half> %m2v, i32 1
    %m2v2 = extractelement <3 x half> %m2v, i32 2

    %d = call half @llpc.determinant3.f16(
        half %m0v0, half %m0v1, half %m0v2,
        half %m1v0, half %m1v1, half %m1v2,
        half %m2v0, half %m2v1, half %m2v2)
    ret half %d
}

; GLSL: half = determinant(f16mat4)
define spir_func half @_Z11determinantDv4_Dv4_Dh(
    [4 x <4 x half>] %m) #0
{
    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %m0v0 = extractelement <4 x half> %m0v, i32 0
    %m0v1 = extractelement <4 x half> %m0v, i32 1
    %m0v2 = extractelement <4 x half> %m0v, i32 2
    %m0v3 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %m1v0 = extractelement <4 x half> %m1v, i32 0
    %m1v1 = extractelement <4 x half> %m1v, i32 1
    %m1v2 = extractelement <4 x half> %m1v, i32 2
    %m1v3 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %m2v0 = extractelement <4 x half> %m2v, i32 0
    %m2v1 = extractelement <4 x half> %m2v, i32 1
    %m2v2 = extractelement <4 x half> %m2v, i32 2
    %m2v3 = extractelement <4 x half> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x half>] %m, 3
    %m3v0 = extractelement <4 x half> %m3v, i32 0
    %m3v1 = extractelement <4 x half> %m3v, i32 1
    %m3v2 = extractelement <4 x half> %m3v, i32 2
    %m3v3 = extractelement <4 x half> %m3v, i32 3

    %d = call half @llpc.determinant4.f16(
        half %m0v0, half %m0v1, half %m0v2, half %m0v3,
        half %m1v0, half %m1v1, half %m1v2, half %m1v3,
        half %m2v0, half %m2v1, half %m2v2, half %m2v3,
        half %m3v0, half %m3v1, half %m3v2, half %m3v3)
    ret half %d
}

; GLSL helper: float16_t = dot3(f16vec3(float16_t, float16_t, float16_t), f16vec3(float16_t, float16_t, float16_t))
define spir_func half @llpc.dot3.f16(
    half %x0, half %y0, half %z0,
    half %x1, half %y1, half %z1)
{
    %1 = fmul half %x1, %x0
    %2 = fmul half %y1, %y0
    %3 = fadd half %1, %2
    %4 = fmul half %z1, %z0
    %5 = fadd half %3, %4
    ret half %5
}

; GLSL helper: float16_t = dot4(f16vec4(float16_t, float16_t, float16_t, float16_t), f16vec4(float16_t, float16_t, float16_t, float16_t))
define spir_func half @llpc.dot4.f16(
    half %x0, half %y0, half %z0, half %w0,
    half %x1, half %y1, half %z1, half %w1)
{
    %1 = fmul half %x1, %x0
    %2 = fmul half %y1, %y0
    %3 = fadd half %1, %2
    %4 = fmul half %z1, %z0
    %5 = fadd half %3, %4
    %6 = fmul half %w1, %w0
    %7 = fadd half %5, %6
    ret half %7
}

; GLSL: f16mat2 = inverse(f16mat2)
define spir_func [2 x <2 x half>] @_Z13matrixInverseDv2_Dv2_Dh(
    [2 x <2 x half>] %m) #0
{
    ; [ x0   x1 ]                    [  y1 -x1 ]
    ; [         ]  = (1 / det(M))) * [         ]
    ; [ y0   y1 ]                    [ -y0  x0 ]
    %m0v = extractvalue [2 x <2 x half>] %m, 0
    %x0 = extractelement <2 x half> %m0v, i32 0
    %y0 = extractelement <2 x half> %m0v, i32 1

    %m1v = extractvalue [2 x <2 x half>] %m, 1
    %x1 = extractelement <2 x half> %m1v, i32 0
    %y1 = extractelement <2 x half> %m1v, i32 1

    %1 = call half @llpc.determinant2(half %x0, half %y0, half %x1, half %y1)
    %2 = fdiv half 1.0, %1
    %3 = fsub half 0.0, %2
    %4 = fmul half %2, %y1
    %5 = fmul half %3, %y0
    %6 = fmul half %3, %x1
    %7 = fmul half %2, %x0
    %8 = insertelement <2 x half> undef, half %4, i32 0
    %9 = insertelement <2 x half> %8, half %5, i32 1
    %10 = insertvalue [2 x <2 x half>] undef, <2 x half> %9, 0
    %11 = insertelement <2 x half> undef, half %6, i32 0
    %12 = insertelement <2 x half> %11, half %7, i32 1
    %13 = insertvalue [2 x <2 x half>] %10 , <2 x half> %12, 1

    ret [2 x <2 x half>]  %13
}

; GLSL: f16mat3 = inverse(f16mat3)
define spir_func [3 x <3 x half>] @_Z13matrixInverseDv3_Dv3_Dh(
    [3 x <3 x half>] %m) #0
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

    %m0v = extractvalue [3 x <3 x half>] %m, 0
    %x0 = extractelement <3 x half> %m0v, i32 0
    %y0 = extractelement <3 x half> %m0v, i32 1
    %z0 = extractelement <3 x half> %m0v, i32 2

    %m1v = extractvalue [3 x <3 x half>] %m, 1
    %x1 = extractelement <3 x half> %m1v, i32 0
    %y1 = extractelement <3 x half> %m1v, i32 1
    %z1 = extractelement <3 x half> %m1v, i32 2

    %m2v = extractvalue [3 x <3 x half>] %m, 2
    %x2 = extractelement <3 x half> %m2v, i32 0
    %y2 = extractelement <3 x half> %m2v, i32 1
    %z2 = extractelement <3 x half> %m2v, i32 2

    %adjx0 = call half @llpc.determinant2(half %y1, half %z1, half %y2, half %z2)
    %adjx1 = call half @llpc.determinant2(half %y2, half %z2, half %y0, half %z0)
    %adjx2 = call half @llpc.determinant2(half %y0, half %z0, half %y1, half %z1)

    %det = call half @llpc.dot3.f16(half %x0, half %x1, half %x2,
                    half %adjx0, half %adjx1, half %adjx2)
    %rdet = fdiv half 1.0, %det

    %nx0 = fmul half %rdet, %adjx0
    %nx1 = fmul half %rdet, %adjx1
    %nx2 = fmul half %rdet, %adjx2

    %m00 = insertelement <3 x half> undef, half %nx0, i32 0
    %m01 = insertelement <3 x half> %m00, half %nx1, i32 1
    %m02 = insertelement <3 x half> %m01, half %nx2, i32 2
    %m0 = insertvalue [3 x <3 x half>] undef, <3 x half> %m02, 0

    %adjy0 = call half @llpc.determinant2(half %z1, half %x1, half %z2, half %x2)
    %adjy1 = call half @llpc.determinant2(half %z2, half %x2, half %z0, half %x0)
    %adjy2 = call half @llpc.determinant2(half %z0, half %x0, half %z1, half %x1)


    %ny0 = fmul half %rdet, %adjy0
    %ny1 = fmul half %rdet, %adjy1
    %ny2 = fmul half %rdet, %adjy2

    %m10 = insertelement <3 x half> undef, half %ny0, i32 0
    %m11 = insertelement <3 x half> %m10, half %ny1, i32 1
    %m12 = insertelement <3 x half> %m11, half %ny2, i32 2
    %m1 = insertvalue [3 x <3 x half>] %m0, <3 x half> %m12, 1

    %adjz0 = call half @llpc.determinant2(half %x1, half %y1, half %x2, half %y2)
    %adjz1 = call half @llpc.determinant2(half %x2, half %y2, half %x0, half %y0)
    %adjz2 = call half @llpc.determinant2(half %x0, half %y0, half %x1, half %y1)

    %nz0 = fmul half %rdet, %adjz0
    %nz1 = fmul half %rdet, %adjz1
    %nz2 = fmul half %rdet, %adjz2

    %m20 = insertelement <3 x half> undef, half %nz0, i32 0
    %m21 = insertelement <3 x half> %m20, half %nz1, i32 1
    %m22 = insertelement <3 x half> %m21, half %nz2, i32 2
    %m2 = insertvalue [3 x <3 x half>] %m1, <3 x half> %m22, 2

    ret [3 x <3 x half>] %m2
}

; GLSL: f16mat4 = inverse(f16mat4)
define spir_func [4 x <4 x half>] @_Z13matrixInverseDv4_Dv4_Dh(
    [4 x <4 x half>] %m) #0
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

    %m0v = extractvalue [4 x <4 x half>] %m, 0
    %x0 = extractelement <4 x half> %m0v, i32 0
    %y0 = extractelement <4 x half> %m0v, i32 1
    %z0 = extractelement <4 x half> %m0v, i32 2
    %w0 = extractelement <4 x half> %m0v, i32 3

    %m1v = extractvalue [4 x <4 x half>] %m, 1
    %x1 = extractelement <4 x half> %m1v, i32 0
    %y1 = extractelement <4 x half> %m1v, i32 1
    %z1 = extractelement <4 x half> %m1v, i32 2
    %w1 = extractelement <4 x half> %m1v, i32 3

    %m2v = extractvalue [4 x <4 x half>] %m, 2
    %x2 = extractelement <4 x half> %m2v, i32 0
    %y2 = extractelement <4 x half> %m2v, i32 1
    %z2 = extractelement <4 x half> %m2v, i32 2
    %w2 = extractelement <4 x half> %m2v, i32 3

    %m3v = extractvalue [4 x <4 x half>] %m, 3
    %x3 = extractelement <4 x half> %m3v, i32 0
    %y3 = extractelement <4 x half> %m3v, i32 1
    %z3 = extractelement <4 x half> %m3v, i32 2
    %w3 = extractelement <4 x half> %m3v, i32 3

    %adjx0 = call half @llpc.determinant3.f16(
            half %y1, half %z1, half %w1,
            half %y2, half %z2, half %w2,
            half %y3, half %z3, half %w3)
    %adjx1 = call half @llpc.determinant3.f16(
            half %y2, half %z2, half %w2,
            half %y0, half %z0, half %w0,
            half %y3, half %z3, half %w3)
    %adjx2 = call half @llpc.determinant3.f16(
            half %y3, half %z3, half %w3,
            half %y0, half %z0, half %w0,
            half %y1, half %z1, half %w1)
    %adjx3 = call half @llpc.determinant3.f16(
            half %y0, half %z0, half %w0,
            half %y2, half %z2, half %w2,
            half %y1, half %z1, half %w1)

    %det = call half @llpc.dot4.f16(half %x0, half %x1, half %x2, half %x3,
            half %adjx0, half %adjx1, half %adjx2, half %adjx3)
    %rdet = fdiv half 1.0, %det

    %nx0 = fmul half %rdet, %adjx0
    %nx1 = fmul half %rdet, %adjx1
    %nx2 = fmul half %rdet, %adjx2
    %nx3 = fmul half %rdet, %adjx3

    %m00 = insertelement <4 x half> undef, half %nx0, i32 0
    %m01 = insertelement <4 x half> %m00, half %nx1, i32 1
    %m02 = insertelement <4 x half> %m01, half %nx2, i32 2
    %m03 = insertelement <4 x half> %m02, half %nx3, i32 3
    %m0 = insertvalue [4 x <4 x half>] undef, <4 x half> %m03, 0

    %adjy0 = call half @llpc.determinant3.f16(
            half %z2, half %w2, half %x2,
            half %z1, half %w1, half %x1,
            half %z3, half %w3, half %x3)
    %adjy1 = call half @llpc.determinant3.f16(
             half %z2, half %w2, half %x2,
             half %z3, half %w3, half %x3,
             half %z0, half %w0, half %x0)
    %adjy2 = call half @llpc.determinant3.f16(
            half %z0, half %w0, half %x0,
            half %z3, half %w3, half %x3,
            half %z1, half %w1, half %x1)
    %adjy3 = call half @llpc.determinant3.f16(
            half %z0, half %w0, half %x0,
            half %z1, half %w1, half %x1,
            half %z2, half %w2, half %x2)

    %ny0 = fmul half %rdet, %adjy0
    %ny1 = fmul half %rdet, %adjy1
    %ny2 = fmul half %rdet, %adjy2
    %ny3 = fmul half %rdet, %adjy3

    %m10 = insertelement <4 x half> undef, half %ny0, i32 0
    %m11 = insertelement <4 x half> %m10, half %ny1, i32 1
    %m12 = insertelement <4 x half> %m11, half %ny2, i32 2
    %m13 = insertelement <4 x half> %m12, half %ny3, i32 3
    %m1 = insertvalue [4 x <4 x half>] %m0, <4 x half> %m13, 1

    %adjz0 = call half @llpc.determinant3.f16(
            half %w1, half %x1, half %y1,
            half %w2, half %x2, half %y2,
            half %w3, half %x3, half %y3)
    %adjz1 = call half @llpc.determinant3.f16(
            half %w3, half %x3, half %y3,
            half %w2, half %x2, half %y2,
            half %w0, half %x0, half %y0)
    %adjz2 = call half @llpc.determinant3.f16(
            half %w3, half %x3, half %y3,
            half %w0, half %x0, half %y0,
            half %w1, half %x1, half %y1)
    %adjz3 = call half @llpc.determinant3.f16(
            half %w1, half %x1, half %y1,
            half %w0, half %x0, half %y0,
            half %w2, half %x2, half %y2)

    %nz0 = fmul half %rdet, %adjz0
    %nz1 = fmul half %rdet, %adjz1
    %nz2 = fmul half %rdet, %adjz2
    %nz3 = fmul half %rdet, %adjz3

    %m20 = insertelement <4 x half> undef, half %nz0, i32 0
    %m21 = insertelement <4 x half> %m20, half %nz1, i32 1
    %m22 = insertelement <4 x half> %m21, half %nz2, i32 2
    %m23 = insertelement <4 x half> %m22, half %nz3, i32 3
    %m2 = insertvalue [4 x <4 x half>] %m1, <4 x half> %m23, 2

    %adjw0 = call half @llpc.determinant3.f16(
            half %x2, half %y2, half %z2,
            half %x1, half %y1, half %z1,
            half %x3, half %y3, half %z3)
    %adjw1 = call half @llpc.determinant3.f16(
            half %x2, half %y2, half %z2,
            half %x3, half %y3, half %z3,
            half %x0, half %y0, half %z0)
    %adjw2 = call half @llpc.determinant3.f16(
            half %x0, half %y0, half %z0,
            half %x3, half %y3, half %z3,
            half %x1, half %y1, half %z1)
    %adjw3 = call half @llpc.determinant3.f16(
            half %x0, half %y0, half %z0,
            half %x1, half %y1, half %z1,
            half %x2, half %y2, half %z2)

    %nw0 = fmul half %rdet, %adjw0
    %nw1 = fmul half %rdet, %adjw1
    %nw2 = fmul half %rdet, %adjw2
    %nw3 = fmul half %rdet, %adjw3

    %m30 = insertelement <4 x half> undef, half %nw0, i32 0
    %m31 = insertelement <4 x half> %m30, half %nw1, i32 1
    %m32 = insertelement <4 x half> %m31, half %nw2, i32 2
    %m33 = insertelement <4 x half> %m32, half %nw3, i32 3
    %m3 = insertvalue [4 x <4 x half>] %m2, <4 x half> %m33, 3

    ret [4 x <4 x half>] %m3
}

declare spir_func half @_Z3dotDv2_DhDv2_Dh(<2 x half> , <2 x half>) #0
declare spir_func half @_Z3dotDv3_DhDv3_Dh(<3 x half> , <3 x half>) #0
declare spir_func half @_Z3dotDv4_DhDv4_Dh(<4 x half> , <4 x half>) #0

attributes #0 = { nounwind }
