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

; GLSL: uint atomicAdd(inout uint, uint)
;       int  atomicAdd(inout int, int)
define spir_func i32 @_Z10AtomicIAddPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile add i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicSub(inout uint, uint)
;        int atomicSub(inout int, int)
define spir_func i32 @_Z10AtomicISubPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile sub i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: int atomicMin(inout int, int)
define spir_func i32 @_Z10AtomicSMinPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile min i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicMin(inout uint, uint)
define spir_func i32 @_Z10AtomicUMinPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile umin i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: int atomicMax(inout int, int)
define spir_func i32 @_Z10AtomicSMaxPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile max i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicMax(inout uint, uint)
define spir_func i32 @_Z10AtomicUMaxPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile umax i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicAnd(inout uint, uint)
;       int  atomicAnd(inout int, int)
define spir_func i32 @_Z9AtomicAndPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile and i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicOr(inout uint, uint)
;       int  atomicOr(inout int, int)
define spir_func i32 @_Z8AtomicOrPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile or i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicXor(inout uint, uint)
;       int  atomicXor(inout int, int)
define spir_func i32 @_Z9AtomicXorPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile xor i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicExchange(inout uint, uint)
;       int  atomicExchange(inout int, int)
define spir_func i32 @_Z14AtomicExchangePU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile xchg i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicCompSwap(inout uint, uint, uint)
;       int  atomicCompSwap(inout int, int, int)
define spir_func i32 @_Z21AtomicCompareExchangePU3AS3iiiiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics_equ,
    i32 %semantics_unequ, i32 %data, i32 %compare)
{
    %1 = cmpxchg i32 addrspace(3)* %mem, i32 %compare, i32 %data seq_cst monotonic
    %2 = extractvalue { i32, i1 } %1, 0
    ret i32 %2
}

; GLSL: uint64_t atomicAdd(inout uint64_t, uint64_t)
;       int64_t  atomicAdd(inout int64_t, int64_t)
define spir_func i64 @_Z10AtomicIAddPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile add i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicSub(inout uint64_t, uint64_t)
;       int64_t  atomicSub(inout int64_t, int64_t)
define spir_func i64 @_Z10AtomicISubPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile sub i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: int64_t atomicMin(inout int64_t, int64_t)
define spir_func i64 @_Z10AtomicSMinPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile min i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicMin(inout uint64_t, uint64_t)
define spir_func i64 @_Z10AtomicUMinPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile umin i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: int64_t atomicMax(inout int64_t, int64_t)
define spir_func i64 @_Z10AtomicSMaxPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile max i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicMax(inout uint64_t, uint64_t)
define spir_func i64 @_Z10AtomicUMaxPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile umax i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicAnd(inout uint64_t, uint64_t)
;       int64_t  atomicAnd(inout int64_t, int64_t)
define spir_func i64 @_Z9AtomicAndPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile and i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicOr(inout uint64_t, uint64_t)
;       int64_t  atomicOr(inout int64_t, int64_t)
define spir_func i64 @_Z8AtomicOrPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile or i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicXor(inout uint64_t, uint64_t)
;       int64_t  atomicXor(inout int64_t, int64_t)
define spir_func i64 @_Z9AtomicXorPU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile xor i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicExchange(inout uint64_t, uint64_t)
;       int64_t  atomicExchange(inout int64_t, int64_t)
define spir_func i64 @_Z14AtomicExchangePU3AS3liil(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics, i64 %data)
{
    %1 = atomicrmw volatile xchg i64 addrspace(3)* %mem, i64 %data seq_cst
    ret i64 %1
}

; GLSL: uint64_t atomicCompSwap(inout uint64_t, uint64_t, uint64_t)
;       int64_t  atomicCompSwap(inout int64_t, int64_t, int64_t)
define spir_func i64 @_Z21AtomicCompareExchangePU3AS3liiill(
    i64 addrspace(3)* %mem, i32 %scope, i32 %semantics_equ,
    i32 %semantics_unequ, i64 %data, i64 %compare)
{
    %1 = cmpxchg i64 addrspace(3)* %mem, i64 %compare, i64 %data seq_cst monotonic
    %2 = extractvalue { i64, i1 } %1, 0
    ret i64 %2
}
