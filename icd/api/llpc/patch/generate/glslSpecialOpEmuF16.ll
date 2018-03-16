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

declare half @llvm.fabs.f16(half) #0
declare i32 @llvm.amdgcn.mov.dpp.i32(i32 , i32 , i32 , i32 , i1 ) #2
declare i32 @llvm.amdgcn.wqm.i32(i32 ) #3

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind readnone convergent }
attributes #3 = { nounwind readnone}
