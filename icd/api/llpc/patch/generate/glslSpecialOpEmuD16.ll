;;
 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
 ;
 ;  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
; >>>  Derivative Functions
; =====================================================================================================================

; GLSL: float16_t dFdx(float16_t)
define half @llpc.dpdx.f16(half %p) #0
{
    ; broadcast pix1, [0,1,2,3]->[1,1,1,1], so dpp_ctrl = 85(0b01010101)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 85, i32 15, i32 15, i1 1)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float

    ; Broadcast pix0, [0,1,2,3]->[0,0,0,0], so dpp_ctrl = 0(0b00000000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 0, i32 15, i32 15, i1 1)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdx.f32 = fsub float %p0, %p1
    %dpdx = fptrunc float %dpdx.f32 to half
    ret half %dpdx
}

; GLSL: float16_t dFdy(float16_t)
define half @llpc.dpdy.f16(half %p) #0
{
    ; broadcast pix2 [0,1,2,3] -> [2,2,2,2], so dpp_ctrl = 170(0b10101010)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 170, i32 15, i32 15, i1 1)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; Broadcast pix0, [0,1,2,3]->[0,0,0,0], so dpp_ctrl = 0(0b00000000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 0, i32 15, i32 15, i1 1)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdy.f32 = fsub float %p0, %p1
    %dpdy = fptrunc float %dpdy.f32 to half
    ret half %dpdy
}

; GLSL: float16_t fwidth(float16_t)
define half @llpc.fwidth.f16(half %p) #0
{
    %1 = call half @llpc.dpdx.f16(half %p)
    %2 = call half @llpc.dpdy.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

; GLSL: float16_t dFdxFine(float16_t)
define half @llpc.dpdxFine.f16(half %p) #0
{
    ; permute quad, [0,1,2,3]->[1,1,3,3], so dpp_ctrl = 245(0b11110101)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 245, i32 15, i32 15, i1 1)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; permute quad, [0,1,2,3]->[0,0,2,2], so dpp_ctrl = 160(0b10100000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 160, i32 15, i32 15, i1 1)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdx.f32 = fsub float %p0, %p1
    %dpdx = fptrunc float %dpdx.f32 to half
    ret half %dpdx
}

; GLSL: float16_t dFdyFine(float16_t)
define half @llpc.dpdyFine.f16(half %p) #0
{
    ; permute quad, [0,1,2,3]->[2,3,2,3], so dpp_ctrl = 238(0b11101110)
    %p.i16 = bitcast half %p to i16
    %p.i32 = zext i16 %p.i16 to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 238, i32 15, i32 15, i1 1)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; permute quad, [0,1,2,3]->[0,1,0,1], so dpp_ctrl = 68(0b01000100)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 68, i32 15, i32 15, i1 1)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value
    %dpdy.f32 = fsub float %p0, %p1
    %dpdy = fptrunc float %dpdy.f32 to half
    ret half %dpdy
}

; GLSL: float16_t fwidthFine(float16_t)
define half @llpc.fwidthFine.f16(half %p) #0
{
    %1 = call half @llpc.dpdxFine.f16(half %p)
    %2 = call half @llpc.dpdyFine.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

; GLSL: float16_t dFdxCoarse(float16_t)
define half @llpc.dpdxCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdx.f16(half %p)
    ret half %1
}

; GLSL: float16_t dFdyCoarse(float16_t)
define half @llpc.dpdyCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdy.f16(half %p)
    ret half %1
}

; GLSL: float16_t fwidthCoarse(float16_t)
define half @llpc.fwidthCoarse.f16(half %p) #0
{
    %1 = call half @llpc.dpdxCoarse.f16(half %p)
    %2 = call half @llpc.dpdyCoarse.f16(half %p)
    %3 = call half @llvm.fabs.f16(half %1)
    %4 = call half @llvm.fabs.f16(half %2)
    %5 = fadd half %3, %4
    ret half %5
}

; =====================================================================================================================
; >>>  Shader Invocation Group Functions
; =====================================================================================================================

; GLSL: i16vec/u16vec subgroupAdd(i16vec/u16vec)
;       i16vec/u16vec subgroupInclusiveAdd(i16vec/u16vec)
;       i16vec/u16vec subgroupExclusiveAdd(i16vec/u16vec)

; GLSL: f16vec subgroupAdd(f16vec)
;       f16vec subgroupInclusiveAdd(f16vec)
;       f16vec subgroupExclusiveAdd(f16vec)

; GLSL: i16vec/u16vec subgroupMul(i16vec/u16vec)
;       i16vec/u16vec subgroupInclusiveMul(i16vec/u16vec)
;       i16vec/u16vec subgroupExclusiveMul(i16vec/u16vec)

; GLSL: f16vec subgroupMul(f16vec)
;       f16vec subgroupInclusiveMul(f16vec)
;       f16vec subgroupExclusiveMul(f16vec)

; GLSL: i16vec subgroupMin(i16vec)
;       i16vec subgroupInclusiveMin(i16vec)
;       i16vec subgroupExclusiveMin(i16vec)

; GLSL: u16vec subgroupMin(u16vec)
;       u16vec subgroupInclusiveMin(u16vec)
;       u16vec subgroupExclusiveMin(u16vec)

; GLSL: f16vec subgroupMin(f16vec)
;       f16vec subgroupInclusiveMin(f16vec)
;       f16vec subgroupExclusiveMin(f16vec)

; GLSL: i16vec subgroupMax(i16vec)
;       i16vec subgroupInclusiveMax(i16vec)
;       i16vec subgroupExclusiveMax(i16vec)

; GLSL: u16vec subgroupMax(u16vec)
;       u16vec subgroupInclusiveMax(u16vec)
;       u16vec subgroupExclusiveMax(u16vec)

; GLSL: f16vec subgroupMax(f16vec)
;       f16vec subgroupInclusiveMax(f16vec)
;       f16vec subgroupExclusiveMax(f16vec)

; GLSL: i16vec/u16vec subgroupAnd(i16vec/u16vec)
;       i16vec/u16vec subgroupInclusiveAnd(i16vec/u16vec)
;       i16vec/u16vec subgroupExclusiveAnd(i16vec/u16vec)

; GLSL: i16vec/u16vec subgroupOr(i16vec/u16vec)
;       i16vec/u16vec subgroupInclusiveOr(i16vec/u16vec)
;       i16vec/u16vec subgroupExclusiveOr(i16vec/u16vec)

; GLSL: i16vec/u16vec subgroupXor(i16vec/u16vec)
;       i16vec/u16vec subgroupInclusiveXor(i16vec/u16vec)
;       i16vec/u16vec subgroupExclusiveXor(i16vec/u16vec)

; GLSL: identity (16-bit)
define spir_func i32 @llpc.subgroup.identity.i16(i32 %binaryOp)
{
.entry:
    switch i32 %binaryOp, label %.end [i32 0,  label %.iadd
                                       i32 1,  label %.imul
                                       i32 2,  label %.smin
                                       i32 3,  label %.smax
                                       i32 4,  label %.umin
                                       i32 5,  label %.umax
                                       i32 6,  label %.and
                                       i32 7,  label %.or
                                       i32 8,  label %.xor
                                       i32 9,  label %.fmul
                                       i32 10, label %.fmin
                                       i32 11, label %.fmax
                                       i32 12, label %.fadd ]

.iadd:
    ret i32 0
.imul:
    ret i32 1
.smin:
    ; 0x7FFF
    ret i32 32767
.smax:
    ; 0x8000
    ret i32 32768
.umin:
    ; 0xFFFF FFFF
    ret i32 4294967295
.umax:
    ret i32 0
.and:
    ; 0xFFFF FFFF
    ret i32 4294967295
.or:
    ret i32 0
.xor:
    ret i32 0
.fmul:
    ; 0x3F800000, 1.0
    ret i32 1065353216
.fmin:
    ; 0x7F800000, +1.#INF00E+000
    ret i32 2139095040
.fmax:
    ; 0xFF800000  -1.#INF00E+000
    ret i32 4286578688
.fadd:
    ret i32 0
.end:
    ret i32 0
}



; GLSL: x [binary] y (16-bit)
define spir_func i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %x, i32 %y)
{
.entry:
    switch i32 %binaryOp, label %.end [i32 0,  label %.iadd
                                       i32 1,  label %.imul
                                       i32 2,  label %.smin
                                       i32 3,  label %.smax
                                       i32 4,  label %.umin
                                       i32 5,  label %.umax
                                       i32 6,  label %.and
                                       i32 7,  label %.or
                                       i32 8,  label %.xor
                                       i32 9,  label %.fmul
                                       i32 10, label %.fmin
                                       i32 11, label %.fmax
                                       i32 12, label %.fadd ]
.iadd:
    %0 = add i32 %x, %y
    br label %.end
.imul:
    %1 = mul i32 %x, %y
    br label %.end
.smin:
    %2 = call i32 @llpc.sminnum.i32(i32 %x, i32 %y)
    br label %.end
.smax:
    ; smax must act as 16-bit arithmetic operation
    %s.0 = trunc i32 %x to i16
    %s.1 = trunc i32 %y to i16
    %s.2 = icmp slt i16 %s.0, %s.1
    %s.3 = select i1 %s.2, i16 %s.1, i16 %s.0
    %3 = sext i16 %s.3 to i32
    br label %.end
.umin:
    %4 = call i32 @llpc.uminnum.i32(i32 %x, i32 %y)
    br label %.end
.umax:
    %5 = call i32 @llpc.umaxnum.i32(i32 %x, i32 %y)
    br label %.end
.and:
    %6 = and i32 %x, %y
    br label %.end
.or:
    %7 = or i32 %x, %y
    br label %.end
.xor:
    %8 = xor i32 %x, %y
    br label %.end
.fmul:
    %x.fmul.f32 = bitcast i32 %x to float
    %y.fmul.f32 = bitcast i32 %y to float
    %fmul.f32 = fmul float %x.fmul.f32, %y.fmul.f32
    %9 = bitcast float %fmul.f32 to i32
    br label %.end
.fmin:
    %x.fmin.f32 = bitcast i32 %x to float
    %y.fmin.f32 = bitcast i32 %y to float
    %fmin.f32 = call float @llvm.minnum.f32(float %x.fmin.f32, float %y.fmin.f32)
    %10 = bitcast float %fmin.f32 to i32
    br label %.end
.fmax:
    %x.fmax.f32 = bitcast i32 %x to float
    %y.fmax.f32 = bitcast i32 %y to float
    %fmax.f32 = call float @llvm.maxnum.f32(float %x.fmax.f32, float %y.fmax.f32)
    %11 = bitcast float %fmax.f32 to i32
    br label %.end
.fadd:
    %x.fadd.f32 = bitcast i32 %x to float
    %y.fadd.f32 = bitcast i32 %y to float
    %fadd.f32 = fadd float %x.fadd.f32, %y.fadd.f32
    %12 = bitcast float %fadd.f32 to i32
    br label %.end
.end:
    %result = phi i32 [undef, %.entry], [%0, %.iadd], [%1, %.imul], [%2, %.smin],
              [%3, %.smax], [%4, %.umin], [%5, %.umax], [%6, %.and], [%7, %.or],
              [%8, %.xor], [%9, %.fmul], [%10, %.fmin], [%11, %.fmax], [%12, %.fadd]
    ret i32 %result
}

; Set values to all inactive lanes (16 bit)
define spir_func i32 @llpc.subgroup.set.inactive.i16(i32 %binaryOp, i32 %value)
{
    ; Get identity value of binary operations
    %identity = call i32 @llpc.subgroup.identity.i16(i32 %binaryOp)
    ; Set identity value for the inactive threads
    %activeValue = call i32 @llvm.amdgcn.set.inactive.i32(i32 %value, i32 %identity)

    ret i32 %activeValue
}

; GLSL: int16_t/uint16_t/float16_t subgroupXXX(int16_t/uint16_t/float16_t)
define spir_func i32 @llpc.subgroup.reduce.i16(i32 %binaryOp, i32 %value)
{
    %1 = call i32 @llpc.subgroup.reduce.i32(i32 %binaryOp, i32 %value)
    ret i32 %1
}

; GLSL: int16_t/uint16_t/float16_t subgroupExclusiveXXX(int16_t/uint16_t/float16_t)
define spir_func i32 @llpc.subgroup.exclusiveScan.i16(i32 %binaryOp, i32 %value)
{
    %tid.lo =  call i32 @llvm.amdgcn.mbcnt.lo(i32 -1, i32 0) #1
    %tid = call i32 @llvm.amdgcn.mbcnt.hi(i32 -1, i32 %tid.lo) #1
    %tid.64 = zext i32 %tid to i64
    %tmask = shl i64 1, %tid.64
    %tidmask = call i64 @llvm.amdgcn.wwm.i64(i64 %tmask)

    ; ds_swizzle work in 32 consecutive lanes/threads BIT modeg
    ; 11 iteration of binary ops needed
    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i1.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %value, i32 1055)
    %i1.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i1.1)
    %i1.3 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %value, i32 %i1.2)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA, update lanes/threads according to mask
    %i1.4 = call i32 @llpc.cndmask.i32(i64 %tidmask , i64 -6148914691236517206, i32 %value, i32 %i1.3)
    %i1.5 = call i32 @llvm.amdgcn.wwm.i32(i32 %i1.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i2.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i1.5, i32 2079)
    %i2.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i2.1)
    %i2.3 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i1.4, i32 %i2.2)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i2.4 = call i32 @llpc.cndmask.i32(i64 %tidmask ,i64 -8608480567731124088, i32 %i1.4, i32 %i2.3)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i3.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i2.4, i32 4127)
    %i3.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i3.1)
    %i3.3 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i2.4, i32 %i3.2)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i3.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9187201950435737472, i32 %i2.4, i32 %i3.3)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i4.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i3.4, i32 8223)
    %i4.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i4.1)
    %i4.3 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i3.4, i32 %i4.2)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i4.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223231297218904064, i32 %i3.4, i32 %i4.3)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i5.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i4.4, i32 16415)
    %i5.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i5.1)
    %i5.3 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i4.4, i32 %i5.2)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i5.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372034707292160, i32 %i4.4, i32 %i5.3)

    ; From now on, scan would be downward
    %identity = call i32 @llpc.subgroup.identity.i16(i32 %binaryOp)
    %i6.1 = call i32 @llvm.amdgcn.readlane(i32 %i5.4, i32 31)
    %i6.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i6.1)
    ; -9223372036854775808 = 0x8000,0000,0000,0000
    %i6.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372036854775808, i32 %i5.4, i32 %i6.2)
    ; 2147483648 = 0x0000,0000,8000,0000
    %i6.4 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 2147483648, i32 %i6.3, i32 %identity)

    ; 16415, bit mode, xor mask = 16 >(SWAP, 16)
    %i7.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i6.4, i32 16415)
    %i7.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i7.1)
    ; 140737488388096 = 0x0000,8000,0000,8000
    %i7.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 140737488388096, i32 %i6.4, i32 %i7.2)
    %i7.4 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i7.2, i32 %i7.3)
    ; -9223372034707292160 = 0x8000,0000,8000,0000
    %i7.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223372034707292160, i32 %i7.3, i32 %i7.4)

    ; 8223, bit mode, xor mask = 8 >(SWAP, 8)
    %i8.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i7.5, i32 8223)
    %i8.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i8.1)
    ; 36029346783166592 = 0x0080,0080,0080,0080
    %i8.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 36029346783166592, i32 %i7.5, i32 %i8.2)
    %i8.4 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i8.2, i32 %i8.3)
    ; -9223231297218904064 = 0x8000,8000,8000,8000
    %i8.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9223231297218904064, i32 %i8.3, i32 %i8.4)

    ; 4127, bit mode, xor mask = 4 ->(SWAP, 4)
    %i9.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i8.5, i32 4127)
    %i9.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i9.1)
    ; 578721382704613384 = 0x0808,0808,0808,0808
    %i9.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 578721382704613384, i32 %i8.5, i32 %i9.2)
    %i9.4 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i9.2, i32 %i9.3)
    ; -9187201950435737472 = 0x8080,8080,8080,8080
    %i9.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -9187201950435737472, i32 %i9.3, i32 %i9.4)

    ; 2079, bit mode, xor mask = 2 ->(SWAP, 2)
    %i10.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i9.5, i32 2079)
    %i10.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i10.1)
    ; 2459565876494606882 = 0x2222,2222,2222,2222
    %i10.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 2459565876494606882, i32 %i9.5, i32 %i10.2)
    %i10.4 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i10.2, i32 %i10.3)
    ; -8608480567731124088 = 0x8888,8888,8888,8888
    %i10.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -8608480567731124088, i32 %i10.3, i32 %i10.4)

    ; 1055, bit mode, xor mask = 1 ->(SWAP, 1)
    %i11.1 = call i32 @llvm.amdgcn.ds.swizzle(i32 %i10.5, i32 1055)
    %i11.2 = call i32 @llvm.amdgcn.wwm.i32(i32 %i11.1)
    ; 6148914691236517205 = 0x5555,5555,5555,5555
    %i11.3 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 6148914691236517205, i32 %i10.5, i32 %i11.2)
    %i11.4 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %i11.2, i32 %i11.3)
    ; -6148914691236517206 = 0xAAAA,AAAA,AAAA,AAAA
    %i11.5 = call i32 @llpc.cndmask.i32(i64 %tidmask, i64 -6148914691236517206, i32 %i11.3, i32 %i11.4)
    %i11.6 = call i32 @llvm.amdgcn.wwm.i32(i32 %i11.5)

    ret i32 %i11.6
}

; GLSL: int16_t/uint16_t/float16_t subgroupInclusiveXXX(int16_t/uint16_t/float16_t)
define spir_func i32 @llpc.subgroup.inclusiveScan.i16(i32 %binaryOp, i32 %value)
{
    %1 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 %binaryOp, i32 %value)
    %2 = call i32 @llpc.subgroup.arithmetic.i16(i32 %binaryOp, i32 %1, i32 %value)

    ret i32 %2
}

; GLSL: int16_t/uint16_t subgroupAdd(int16_t/uint16_t)
define spir_func i16 @_Z31sub_group_reduce_add_nonuniforms(i16 %value)
{
    ; 0 = arithmetic iadd
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i16(i32 0, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2/u16vec2 subgroupAdd(i16vec2/u16vec2)
define spir_func <2 x i16> @_Z31sub_group_reduce_add_nonuniformDv2_s(<2 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3/u16vec3 subgroupAdd(i16vec3/u16vec3)
define spir_func <3 x i16> @_Z31sub_group_reduce_add_nonuniformDv3_s(<3 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4/u16vec4 subgroupAdd(i16vec4/u16vec4)
define spir_func <4 x i16> @_Z31sub_group_reduce_add_nonuniformDv4_s(<4 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 0 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t/uint16_t subgroupInclusiveAdd(int16_t/uint16_t)
define spir_func i16 @_Z39sub_group_scan_inclusive_add_nonuniforms(i16 %value)
{
    ; 0 = arithmetic iadd
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2/u16vec2 subgroupInclusiveAdd(i16vec2/u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_s(<2 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3/u16vec3 subgroupInclusiveAdd(i16vec3/u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_s(<3 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4/u16vec4 subgroupInclusiveAdd(i16vec4/u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_s(<4 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 0 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t/uint16_t subgroupExclusiveAdd(int16_t/uint16_t)
define spir_func i16 @_Z39sub_group_scan_exclusive_add_nonuniforms(i16 %value)
{
    ; 0 = arithmetic iadd
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2/u16vec2 subgroupExclusiveAdd(i16vec2/u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_s(<2 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3/u16vec3 subgroupExclusiveAdd(i16vec3/u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_s(<3 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4/u16vec4 subgroupExclusiveAdd(i16vec4/u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_s(<4 x i16> %value)
{
    ; 0 = arithmetic iadd
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 0, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 0 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: float16_t subgroupAdd(float16_t)
define spir_func half @_Z31sub_group_reduce_add_nonuniformDh(half %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %2)
    %4 = call i32 @llpc.subgroup.reduce.i16(i32 12, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupAdd(f16vec2)
define spir_func <2 x half> @_Z31sub_group_reduce_add_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)

    %7 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %5)
    %8 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupAdd(f16vec3)
define spir_func <3 x half> @_Z31sub_group_reduce_add_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)

    %9 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %7)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupAdd(f16vec4)
define spir_func <4 x half> @_Z31sub_group_reduce_add_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %6)

    %11 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %9)
    %14 = call i32 @llpc.subgroup.reduce.i16(i32 12 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupInclusiveAdd(float16_t)
define spir_func half @_Z39sub_group_scan_inclusive_add_nonuniformDh(half %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %2)
    %4 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupInclusiveAdd(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_inclusive_add_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %5)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupInclusiveAdd(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_inclusive_add_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %7)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupInclusiveAdd(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_inclusive_add_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %6)

    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %9)
    %14 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 12 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupExclusiveAdd(float16_t)
define spir_func half @_Z39sub_group_scan_exclusive_add_nonuniformDh(half %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %2)
    %4 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupExclusiveAdd(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_exclusive_add_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %5)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupExclusiveAdd(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_exclusive_add_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %7)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupExclusiveAdd(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_exclusive_add_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 12 = arithmetic fadd
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 12, i32 %6)

    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %9)
    %14 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 12 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: int16_t subgroupMin(int16_t)
define spir_func i16 @_Z31sub_group_reduce_min_nonuniforms(i16 %value)
{
    ; 2 = arithmetic smin
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i16(i32 2, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupMin(i16vec2)
define spir_func <2 x i16> @_Z31sub_group_reduce_min_nonuniformDv2_s(<2 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupMin(i16vec3)
define spir_func <3 x i16> @_Z31sub_group_reduce_min_nonuniformDv3_s(<3 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupMin(i16vec4)
define spir_func <4 x i16> @_Z31sub_group_reduce_min_nonuniformDv4_s(<4 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 2 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t subgroupInclusiveMin(int16_t)
define spir_func i16 @_Z39sub_group_scan_inclusive_min_nonuniforms(i16 %value)
{
    ; 2 = arithmetic smin
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupInclusiveMin(i16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_s(<2 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupInclusiveMin(i16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_s(<3 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupInclusiveMin(i16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_s(<4 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 2 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t subgroupExclusiveMin(int16_t)
define spir_func i16 @_Z39sub_group_scan_exclusive_min_nonuniforms(i16 %value)
{
    ; 2 = arithmetic smin
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupExclusiveMin(i16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_s(<2 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupExclusiveMin(i16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_s(<3 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupExclusiveMin(i16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_s(<4 x i16> %value)
{
    ; 2 = arithmetic smin
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 2, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 2 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupMin(uint16_t)
define spir_func i16 @_Z31sub_group_reduce_min_nonuniformt(i16 %value)
{
    ; 4 = arithmetic umin
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i16(i32 4, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupMin(u16vec2)
define spir_func <2 x i16> @_Z31sub_group_reduce_min_nonuniformDv2_t(<2 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupMin(u16vec3)
define spir_func <3 x i16> @_Z31sub_group_reduce_min_nonuniformDv3_t(<3 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupMin(u16vec4)
define spir_func <4 x i16> @_Z31sub_group_reduce_min_nonuniformDv4_t(<4 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 4 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupInclusiveMin(uint16_t)
define spir_func i16 @_Z39sub_group_scan_inclusive_min_nonuniformt(i16 %value)
{
    ; 4 = arithmetic umin
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupInclusiveMin(u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_t(<2 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupInclusiveMin(u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_t(<3 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupInclusiveMin(u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_t(<4 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 4 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupExclusiveMin(uint16_t)
define spir_func i16 @_Z39sub_group_scan_exclusive_min_nonuniformt(i16 %value)
{
    ; 4 = arithmetic umin
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupExclusiveMin(u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_t(<2 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupExclusiveMin(u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_t(<3 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupExclusiveMin(u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_t(<4 x i16> %value)
{
    ; 4 = arithmetic umin
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 4, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 4 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: float16_t subgroupMin(float16_t)
define spir_func half @_Z31sub_group_reduce_min_nonuniformDh(half %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %2)
    %4 = call i32 @llpc.subgroup.reduce.i16(i32 10, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupMin(f16vec2)
define spir_func <2 x half> @_Z31sub_group_reduce_min_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)

    %7 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %5)
    %8 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupMin(f16vec3)
define spir_func <3 x half> @_Z31sub_group_reduce_min_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)

    %9 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %7)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupMin(f16vec4)
define spir_func <4 x half> @_Z31sub_group_reduce_min_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %6)

    %11 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %9)
    %14 = call i32 @llpc.subgroup.reduce.i16(i32 10 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupInclusiveMin(float16_t)
define spir_func half @_Z39sub_group_scan_inclusive_min_nonuniformDh(half %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %2)
    %4 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupInclusiveMin(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_inclusive_min_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %5)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupInclusiveMin(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_inclusive_min_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %7)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupInclusiveMin(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_inclusive_min_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %6)

    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %9)
    %14 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 10 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupExclusiveMin(float16_t)
define spir_func half @_Z39sub_group_scan_exclusive_min_nonuniformDh(half %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %2)
    %4 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupExclusiveMin(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_exclusive_min_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %5)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupExclusiveMin(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_exclusive_min_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %7)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupExclusiveMin(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_exclusive_min_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 10 = arithmetic fmin
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 10, i32 %6)

    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %9)
    %14 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 10 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: int16_t subgroupMax(int16_t)
define spir_func i16 @_Z31sub_group_reduce_max_nonuniforms(i16 %value)
{
    ; 3 = arithmetic smax
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i16(i32 3, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupMax(i16vec2)
define spir_func <2 x i16> @_Z31sub_group_reduce_max_nonuniformDv2_s(<2 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupMax(i16vec3)
define spir_func <3 x i16> @_Z31sub_group_reduce_max_nonuniformDv3_s(<3 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupMax(i16vec4)
define spir_func <4 x i16> @_Z31sub_group_reduce_max_nonuniformDv4_s(<4 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 3 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t subgroupInclusiveMax(int16_t)
define spir_func i16 @_Z39sub_group_scan_inclusive_max_nonuniforms(i16 %value)
{
    ; 3 = arithmetic smax
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupInclusiveMax(i16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_s(<2 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupInclusiveMax(i16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_s(<3 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupInclusiveMax(i16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_s(<4 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 3 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: int16_t subgroupExclusiveMax(int16_t)
define spir_func i16 @_Z39sub_group_scan_exclusive_max_nonuniforms(i16 %value)
{
    ; 3 = arithmetic smax
    %1 = sext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: i16vec2 subgroupExclusiveMax(i16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_s(<2 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: i16vec3 subgroupExclusiveMax(i16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_s(<3 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: i16vec4 subgroupExclusiveMax(i16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_s(<4 x i16> %value)
{
    ; 3 = arithmetic smax
    %1 = sext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 3, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 3 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupMax(uint16_t)
define spir_func i16 @_Z31sub_group_reduce_max_nonuniformt(i16 %value)
{
    ; 5 = arithmetic umax
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %1)
    %3 = call i32 @llpc.subgroup.reduce.i16(i32 5, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupMax(u16vec2)
define spir_func <2 x i16> @_Z31sub_group_reduce_max_nonuniformDv2_t(<2 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)

    %6 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %4)
    %7 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupMax(u16vec3)
define spir_func <3 x i16> @_Z31sub_group_reduce_max_nonuniformDv3_t(<3 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)

    %8 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupMax(u16vec4)
define spir_func <4 x i16> @_Z31sub_group_reduce_max_nonuniformDv4_t(<4 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %5)

    %10 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 5 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupInclusiveMax(uint16_t)
define spir_func i16 @_Z39sub_group_scan_inclusive_max_nonuniformt(i16 %value)
{
    ; 5 = arithmetic umax
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %1)
    %3 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupInclusiveMax(u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_t(<2 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)

    %6 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %4)
    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupInclusiveMax(u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_t(<3 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)

    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupInclusiveMax(u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_t(<4 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %5)

    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 5 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: uint16_t subgroupExclusiveMax(uint16_t)
define spir_func i16 @_Z39sub_group_scan_exclusive_max_nonuniformt(i16 %value)
{
    ; 5 = arithmetic umax
    %1 = zext i16 %value to i32
    %2 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %1)
    %3 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5, i32 %2)
    %4 = trunc i32 %3 to i16

    ret i16 %4
}

; GLSL: u16vec2 subgroupExclusiveMax(u16vec2)
define spir_func <2 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_t(<2 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <2 x i16> %value to <2 x i32>

    %2 = extractelement <2 x i32> %1, i32 0
    %3 = extractelement <2 x i32> %1, i32 1

    %4 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)

    %6 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %4)
    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %5)

    %8 = insertelement <2 x i32> undef, i32 %6, i32 0
    %9 = insertelement <2 x i32> %8, i32 %7, i32 1
    %10 = trunc <2 x i32> %9 to <2 x i16>

    ret <2 x i16> %10
}

; GLSL: u16vec3 subgroupExclusiveMax(u16vec3)
define spir_func <3 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_t(<3 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <3 x i16> %value to <3 x i32>

    %2 = extractelement <3 x i32> %1, i32 0
    %3 = extractelement <3 x i32> %1, i32 1
    %4 = extractelement <3 x i32> %1, i32 2

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)

    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %5)
    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %7)

    %11 = insertelement <3 x i32> undef, i32 %8, i32 0
    %12 = insertelement <3 x i32> %11, i32 %9, i32 1
    %13 = insertelement <3 x i32> %12, i32 %10, i32 2
    %14 = trunc <3 x i32> %13 to <3 x i16>

    ret <3 x i16> %14
}

; GLSL: u16vec4 subgroupExclusiveMax(u16vec4)
define spir_func <4 x i16> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_t(<4 x i16> %value)
{
    ; 5 = arithmetic umax
    %1 = zext <4 x i16> %value to <4 x i32>

    %2 = extractelement <4 x i32> %1, i32 0
    %3 = extractelement <4 x i32> %1, i32 1
    %4 = extractelement <4 x i32> %1, i32 2
    %5 = extractelement <4 x i32> %1, i32 3

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %2)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 5, i32 %5)

    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %6)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 5 ,i32 %9)

    %14 = insertelement <4 x i32> undef, i32 %10, i32 0
    %15 = insertelement <4 x i32> %14, i32 %11, i32 1
    %16 = insertelement <4 x i32> %15, i32 %12, i32 2
    %17 = insertelement <4 x i32> %16, i32 %13, i32 3
    %18 = trunc <4 x i32> %17 to <4 x i16>

    ret <4 x i16> %18
}

; GLSL: float16_t subgroupMax(float16_t)
define spir_func half @_Z31sub_group_reduce_max_nonuniformDh(half %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %2)
    %4 = call i32 @llpc.subgroup.reduce.i16(i32 11, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupMax(f16vec2)
define spir_func <2 x half> @_Z31sub_group_reduce_max_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)

    %7 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %5)
    %8 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupMax(f16vec3)
define spir_func <3 x half> @_Z31sub_group_reduce_max_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)

    %9 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %7)
    %11 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupMax(f16vec4)
define spir_func <4 x half> @_Z31sub_group_reduce_max_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %6)

    %11 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %9)
    %14 = call i32 @llpc.subgroup.reduce.i16(i32 11 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupInclusiveMax(float16_t)
define spir_func half @_Z39sub_group_scan_inclusive_max_nonuniformDh(half %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %2)
    %4 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupInclusiveMax(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_inclusive_max_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)

    %7 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %5)
    %8 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupInclusiveMax(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_inclusive_max_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)

    %9 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %7)
    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupInclusiveMax(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_inclusive_max_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %6)

    %11 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %9)
    %14 = call i32 @llpc.subgroup.inclusiveScan.i16(i32 11 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; GLSL: float16_t subgroupExclusiveMax(float16_t)
define spir_func half @_Z39sub_group_scan_exclusive_max_nonuniformDh(half %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext half %value to float
    %2 = bitcast float %1 to i32
    %3 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %2)
    %4 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11, i32 %3)
    %5 = bitcast i32 %4 to float
    %6 = fptrunc float %5 to half

    ret half %6
}

; GLSL: f16vec2 subgroupExclusiveMax(f16vec2)
define spir_func <2 x half> @_Z39sub_group_scan_exclusive_max_nonuniformDv2_Dh(<2 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <2 x half> %value to <2 x float>
    %2 = bitcast <2 x float> %1 to <2 x i32>

    %3 = extractelement <2 x i32> %2, i32 0
    %4 = extractelement <2 x i32> %2, i32 1

    %5 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)

    %7 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %5)
    %8 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %6)

    %9 = insertelement <2 x i32> undef, i32 %7, i32 0
    %10 = insertelement <2 x i32> %9, i32 %8, i32 1
    %11 = bitcast <2 x i32> %10 to <2 x float>
    %12 = fptrunc <2 x float> %11 to <2 x half>

    ret <2 x half> %12
}

; GLSL: f16vec3 subgroupExclusiveMax(f16vec3)
define spir_func <3 x half> @_Z39sub_group_scan_exclusive_max_nonuniformDv3_Dh(<3 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <3 x half> %value to <3 x float>
    %2 = bitcast <3 x float> %1 to <3 x i32>

    %3 = extractelement <3 x i32> %2, i32 0
    %4 = extractelement <3 x i32> %2, i32 1
    %5 = extractelement <3 x i32> %2, i32 2

    %6 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)

    %9 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %6)
    %10 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %7)
    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %8)

    %12 = insertelement <3 x i32> undef, i32 %9, i32 0
    %13 = insertelement <3 x i32> %12, i32 %10, i32 1
    %14 = insertelement <3 x i32> %13, i32 %11, i32 2
    %15 = bitcast <3 x i32> %14 to <3 x float>
    %16 = fptrunc <3 x float> %15 to <3 x half>

    ret <3 x half> %16
}

; GLSL: f16vec4 subgroupExclusiveMax(f16vec4)
define spir_func <4 x half> @_Z39sub_group_scan_exclusive_max_nonuniformDv4_Dh(<4 x half> %value)
{
    ; 11 = arithmetic fmax
    %1 = fpext <4 x half> %value to <4 x float>
    %2 = bitcast <4 x float> %1 to <4 x i32>

    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = extractelement <4 x i32> %2, i32 3

    %7 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %3)
    %8 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %4)
    %9 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %5)
    %10 = call i32 @llpc.subgroup.set.inactive.i16(i32 11, i32 %6)

    %11 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %7)
    %12 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %8)
    %13 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %9)
    %14 = call i32 @llpc.subgroup.exclusiveScan.i16(i32 11 ,i32 %10)

    %15 = insertelement <4 x i32> undef, i32 %11, i32 0
    %16 = insertelement <4 x i32> %15, i32 %12, i32 1
    %17 = insertelement <4 x i32> %16, i32 %13, i32 2
    %18 = insertelement <4 x i32> %17, i32 %14, i32 3
    %19 = bitcast <4 x i32> %18 to <4 x float>
    %20 = fptrunc <4 x float> %19 to <4 x half>

    ret <4 x half> %20
}

; =====================================================================================================================
; >>>  Interpolation Functions
; =====================================================================================================================

; Adjust interpolation I/J according to specified offsets X/Y
define float @llpc.input.interpolate.adjustij.f16(float %ij, half %offsetX, half %offsetY)
{
    ; Calculate DpDx, DpDy for %ij
    %1 = call float @llpc.dpdxFine.f32(float %ij)
    %2 = call float @llpc.dpdyFine.f32(float %ij)

    ; Adjust %ij by offset
    %3 = fpext half %offsetX to float
    %4 = fpext half %offsetY to float

    %5 = fmul float %3, %1
    %6 = fadd float %ij, %5
    %7 = fmul float %4, %2
    %8 = fadd float %6, %7

    ret float %8
}

; Evaluate interpolation I/J for GLSL function interpolateAtOffset()
define <2 x float> @llpc.input.interpolate.evalij.offset.v2f16(<2 x half> %offset) #0
{
    ; BuiltInInterpPullMode 268435459 = 0x10000003
    %1 = call <3 x float> @llpc.input.import.builtin.InterpPullMode(i32 268435459)
    ; Extract Pull Model I/W, J/W, 1/W
    %2 = extractelement <3 x float> %1, i32 0
    %3 = extractelement <3 x float> %1, i32 1
    %4 = extractelement <3 x float> %1, i32 2

    ; Extract offset to scalar
    %5 = extractelement <2 x half> %offset, i32 0
    %6 = extractelement <2 x half> %offset, i32 1

    ; Adjust each coefficient by offset
    %7 = call float @llpc.input.interpolate.adjustij.f16(float %2, half %5, half %6)
    %8 = call float @llpc.input.interpolate.adjustij.f16(float %3, half %5, half %6)
    %9 = call float @llpc.input.interpolate.adjustij.f16(float %4, half %5, half %6)

    ; Get final I, J
    %10 = fmul float %7, %9
    %11 = fmul float %8, %9

    %12 = insertelement <2 x float> undef, float %10, i32 0
    %13 = insertelement <2 x float> %12, float %11, i32 1

    ret <2 x float> %13
}

; Evaluate interpolation I/J for GLSL function interpolateAtOffset() with "noperspective" qualifier specified
; on interpolant
define <2 x float> @llpc.input.interpolate.evalij.offset.noperspective.v2f16(<2 x half> %offset) #0
{
    ; BuiltInInterpLinearCenter 268435461 = 0x10000005
    %1 = call <2 x float> @llpc.input.import.builtin.InterpLinearCenter(i32 268435461)
    ; Extract I, J
    %2 = extractelement <2 x float> %1, i32 0
    %3 = extractelement <2 x float> %1, i32 1

    ; Extract offset to scalar
    %4 = extractelement <2 x half> %offset, i32 0
    %5 = extractelement <2 x half> %offset, i32 1

    ; Adjust I,J by offset
    %6 = call float @llpc.input.interpolate.adjustij.f16(float %2, half %4, half %5)
    %7 = call float @llpc.input.interpolate.adjustij.f16(float %3, half %4, half %5)

    %8 = insertelement <2 x float> undef, float %6, i32 0
    %9 = insertelement <2 x float> %8, float %7, i32 1

    ret <2 x float> %9
}

declare half @llvm.fabs.f16(half) #0
declare i32 @llvm.amdgcn.mov.dpp.i32(i32, i32, i32, i32, i1 ) #2
declare i32 @llvm.amdgcn.wqm.i32(i32) #1
declare i32 @llvm.amdgcn.ds.swizzle(i32, i32) #2
declare i32 @llvm.amdgcn.readlane(i32, i32) #2
declare i32 @llvm.amdgcn.mbcnt.lo(i32, i32) #1
declare i32 @llvm.amdgcn.mbcnt.hi(i32, i32) #1
declare i32 @llvm.amdgcn.set.inactive.i32(i32, i32) #2
declare i64 @llvm.amdgcn.wwm.i64(i64) #1
declare i32 @llvm.amdgcn.wwm.i32(i32) #1
declare float @llpc.dpdxFine.f32(float) #0
declare float @llpc.dpdyFine.f32(float) #0
declare <3 x float> @llpc.input.import.builtin.InterpPullMode(i32) #0
declare <2 x float> @llpc.input.import.builtin.InterpLinearCenter(i32) #0
declare i32 @llpc.subgroup.reduce.i32(i32 %binaryOp, i32 %value) #0
declare i32 @llpc.cndmask.i32(i64, i64, i32, i32)
declare i32 @llpc.sminnum.i32(i32, i32) #0
declare i32 @llpc.uminnum.i32(i32, i32) #0
declare i32 @llpc.umaxnum.i32(i32, i32) #0
declare float @llvm.minnum.f32(float, float) #0
declare float @llvm.maxnum.f32(float, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone}
attributes #2 = { nounwind readnone convergent }
attributes #3 = { convergent nounwind }
attributes #4 = { nounwind readonly }
