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

; GLSL: int64_t = int64_t % int64_t
define i64 @llpc.mod.i64(i64 %x, i64 %y) #0
{
    %1 = srem i64 %x, %y
    %2 = add i64 %1, %y
    ; Check if the signedness of x and y are the same.
    %3 = xor i64 %x, %y
    ; if negative, slt signed less than
    %4 = icmp slt i64 %3, 0
    ; Check if the remainder is not 0.
    %5 = icmp ne i64 %1, 0
    %6 = and i1 %4, %5
    %7 = select i1 %6, i64 %2, i64 %1
    ret i64 %7
}

; =====================================================================================================================
; >>>  Common Functions
; =====================================================================================================================

; GLSL: int64_t abs(int64_t)
define i64 @llpc.sabs.i64(i64 %x) #0
{
    %nx = sub i64 0, %x
    %con = icmp sgt i64 %x, %nx
    %val = select i1 %con, i64 %x, i64 %nx
    ret i64 %val
}

; GLSL: int64_t sign(int64_t)
define i64 @llpc.ssign.i64(i64 %x) #0
{
    %con1 = icmp sgt i64 %x, 0
    %ret1 = select i1 %con1, i64 1, i64 %x
    %con2 = icmp sge i64 %ret1, 0
    %ret2 = select i1 %con2, i64 %ret1, i64 -1
    ret i64 %ret2
}

; GLSL: int64_t min(int64_t, int64_t)
define i64 @llpc.sminnum.i64(i64 %x, i64 %y) #0
{
    %1 = icmp slt i64 %y, %x
    %2 = select i1 %1, i64 %y, i64 %x
    ret i64 %2
}

; GLSL: uint64_t min(uint64_t, uint64_t)
define i64 @llpc.uminnum.i64(i64 %x, i64 %y) #0
{
    %1 = icmp ult i64 %y, %x
    %2 = select i1 %1, i64 %y, i64 %x
    ret i64 %2
}

; GLSL: int64_t max(int64_t, int64_t)
define i64 @llpc.smaxnum.i64(i64 %x, i64 %y) #0
{
    %1 = icmp slt i64 %x, %y
    %2 = select i1 %1, i64 %y, i64 %x
    ret i64 %2
}

; GLSL: uint64_t max(uint64_t, uint64_t)
define i64 @llpc.umaxnum.i64(i64 %x, i64 %y) #0
{
    %1 = icmp ult i64 %x, %y
    %2 = select i1 %1, i64 %y, i64 %x
    ret i64 %2
}

; GLSL: int64_t clamp(int64_t, int64_t, int64_t)
define i64 @llpc.sclamp.i64(i64 %x, i64 %minVal, i64 %maxVal) #0
{
    %1 = call i64 @llpc.smaxnum.i64(i64 %x, i64 %minVal)
    %2 = call i64 @llpc.sminnum.i64(i64 %1, i64 %maxVal)
    ret i64 %2
}

; GLSL: uint64_t clamp(uint64_t, uint64_t, uint64_t)
define i64 @llpc.uclamp.i64(i64 %x, i64 %minVal, i64 %maxVal) #0
{
    %1 = call i64 @llpc.umaxnum.i64(i64 %x, i64 %minVal)
    %2 = call i64 @llpc.uminnum.i64(i64 %1, i64 %maxVal)
    ret i64 %2
}

attributes #0 = { nounwind }