;**********************************************************************************************************************
;*
;*  Trade secret of Advanced Micro Devices, Inc.
;*  Copyright (c) 2017, Advanced Micro Devices, Inc., (unpublished)
;*
;*  All rights reserved. This notice is intended as a precaution against inadvertent publication and does not imply
;*  publication or any waiver of confidentiality. The year included in the foregoing notice is the year of creation of
;*  the work.
;*
;**********************************************************************************************************************

;**********************************************************************************************************************
;* @file  glslSharedVarOpEmu.ll
;* @brief LLPC LLVM-IR file: contains emulation codes for GLSL shared variable operations.
;**********************************************************************************************************************

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
    %1 = atomicrmw volatile add i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: int atomicMin (inout int mem, int data)
define spir_func i32 @_Z10AtomicSMinPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile min i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicMin (inout uint mem, uint data)
define spir_func i32 @_Z10AtomicUMinPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile umin i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: int atomicMax (inout int mem, int data)
define spir_func i32 @_Z10AtomicSMaxPU3AS3iiii(
    i32 addrspace(3)* %mem, i32 %scope, i32 %semantics, i32 %data)
{
    %1 = atomicrmw volatile max i32 addrspace(3)* %mem, i32 %data seq_cst
    ret i32 %1
}

; GLSL: uint atomicMax (inout uint mem, uint data)
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