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

; GLSL: float dFdx(float)
define float @llpc.dpdx.f32(float %p) #0
{
    ; for quad pixels, quad_perm:[pix0,pix1,pix2,pix3] = [0,1,2,3]
    ; broadcast pix1, [0,1,2,3]->[1,1,1,1], so dpp_ctrl = 85(0b01010101)
    %p.i32 = bitcast float %p to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 85, i32 15, i32 15, i1 true)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float

    ; Broadcast pix0, [0,1,2,3]->[0,0,0,0], so dpp_ctrl = 0(0b00000000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 0, i32 15, i32 15, i1 true)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdx = fsub float %p0, %p1

    ret float %dpdx
}

; GLSL: float dFdy(float)
define float @llpc.dpdy.f32(float %p) #0
{
    ; for quad pixels, quad_perm:[pix0,pix1,pix2,pix3] = [0,1,2,3]
    ; broadcast pix2 [0,1,2,3] -> [2,2,2,2], so dpp_ctrl = 170(0b10101010)
    %p.i32 = bitcast float %p to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 170, i32 15, i32 15, i1 true)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; Broadcast pix0, [0,1,2,3]->[0,0,0,0], so dpp_ctrl = 0(0b00000000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 0, i32 15, i32 15, i1 true)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdy = fsub float %p0, %p1

    ret float %dpdy
}

; GLSL: float dFdxFine(float)
define float @llpc.dpdxFine.f32(float %p) #0
{
    ; for quad pixels, quad_perm:[pix0,pix1,pix2,pix3] = [0,1,2,3]
    ; permute quad, [0,1,2,3]->[1,1,3,3], so dpp_ctrl = 245(0b11110101)
    %p.i32 = bitcast float %p to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 245, i32 15, i32 15, i1 true)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; permute quad, [0,1,2,3]->[0,0,2,2], so dpp_ctrl = 160(0b10100000)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 160, i32 15, i32 15, i1 true)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value p0 - p1
    %dpdx = fsub float %p0, %p1

    ret float %dpdx
}

; GLSL: float dFdyFine(float)
define float @llpc.dpdyFine.f32(float %p) #0
{
    ; for quad pixels, quad_perm:[pix0,pix1,pix2,pix3] = [0,1,2,3]
    ; permute quad, [0,1,2,3]->[2,3,2,3], so dpp_ctrl = 238(0b11101110)
    %p.i32 = bitcast float %p to i32
    %p0.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 238, i32 15, i32 15, i1 true)
    %p0.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p0.dpp)
    %p0 = bitcast i32 %p0.i32 to float
    ; permute quad, [0,1,2,3]->[0,1,0,1], so dpp_ctrl = 68(0b01000100)
    %p1.dpp = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %p.i32, i32 68, i32 15, i32 15, i1 true)
    %p1.i32 = call i32 @llvm.amdgcn.wqm.i32(i32 %p1.dpp)
    %p1 = bitcast i32 %p1.i32 to float

    ; Calculate the delta value
    %dpdy = fsub float %p0, %p1

    ret float %dpdy
}

; =====================================================================================================================
; >>>  Shader Invocation Group Functions
; =====================================================================================================================

; GLSL: int/uint subgroupShuffle(int/uint, uint)
define spir_func i32 @_Z22GroupNonUniformShuffleiii(i32 %scope, i32 %value, i32 %id)
{
    %1 = mul i32 %id, 4
    %2 = call i32 @llvm.amdgcn.ds.bpermute(i32 %1, i32 %value)

    ret i32 %2
}

; GLSL: int/uint subgroupQuadBroadcast(int/uint, uint)
define spir_func i32 @_Z28GroupNonUniformQuadBroadcastiii(i32 %scope, i32 %value, i32 %id)
{
    ; id should be constant of 0 ~ 3
.entry:
    switch i32 %id, label %.end [ i32 0, label %.id0
                                  i32 1, label %.id1
                                  i32 2, label %.id2
                                  i32 3, label %.id3 ]
.id0:
    ; QUAD_PERM 0,0,0,0
    %value.dpp0 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 0, i32 15, i32 15, i1 true)
    br label %.end
.id1:
    ; QUAD_PERM 1,1,1,1
    %value.dpp1 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 85, i32 15, i32 15, i1 true)
    br label %.end
.id2:
    ; QUAD_PERM 2,2,2,2
    %value.dpp2 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 170, i32 15, i32 15, i1 true)
    br label %.end
.id3:
    ; QUAD_PERM 3,3,3,3
    %value.dpp3 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 255, i32 15, i32 15, i1 true)
    br label %.end
.end:
    %result = phi i32 [undef, %.entry],[%value.dpp0, %.id0],[%value.dpp1, %.id1], [%value.dpp2, %.id2], [%value.dpp3, %.id3]
    ret i32 %result
}

; GLSL: int/uint subgroupQuadSwapHorizontal(int/uint)
;       int/uint subgroupQuadSwapVertical(int/uint)
;       int/uint subgroupQuadSwapDiagonal(int/uint)
define spir_func i32 @_Z23GroupNonUniformQuadSwapiii(i32 %scope, i32 %value, i32 %direction)
{
    ; direction 0 is Horizontal
    ; direction 1 is Vertical
    ; direction 2 is Diagonal
.entry:
    switch i32 %direction, label %.end [ i32 0, label %.horizonal
                                         i32 1, label %.vertical
                                         i32 2, label %.diagonal ]
.horizonal:
    ; QUAD_PERM [ 0->1, 1->0, 2->3, 3->2], 0b1011,0001
    %value.dir0 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 177, i32 15, i32 15, i1 true)
    br label %.end
.vertical:
    ; QUAD_PERM [ 0->2, 1->3, 2->0, 3->1], 0b0100,1110
    %value.dir1 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 78, i32 15, i32 15, i1 true)
    br label %.end
.diagonal:
    ; QUAD_PERM [ 0->3, 1->2, 2->1, 3->0], 0b0001,1011
    %value.dir2 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %value, i32 27, i32 15, i32 15, i1 true)
    br label %.end
.end:
    %result = phi i32 [undef, %.entry], [%value.dir0, %.horizonal], [%value.dir1, %.vertical], [%value.dir2, %.diagonal]
    ret i32 %result
}

; GLSL: int/uint swizzleInvocations(int/uint, uvec4)
define spir_func i32 @_Z21SwizzleInvocationsAMDiDv4_i(i32 %data, <4 x i32> %offset)
{
    %1 = extractelement <4 x i32> %offset, i32 0
    %2 = extractelement <4 x i32> %offset, i32 1
    %3 = extractelement <4 x i32> %offset, i32 2
    %4 = extractelement <4 x i32> %offset, i32 3

    %5 = and i32 %1, 3
    %6 = and i32 %2, 3
    %7 = and i32 %3, 3
    %8 = and i32 %4, 3

    ; [7:6] = offset[3], [5:4] = offset[2], [3:2] = offset[1], [1:0] = offset[0]
    %9  = shl i32 %6, 2
    %10 = shl i32 %7, 4
    %11 = shl i32 %8, 6

    %12 = or i32 %5,  %9
    %13 = or i32 %12, %10
    %14 = or i32 %13, %11

    ; row_mask = 0xF, bank_mask = 0xF, bound_ctrl = true
    %15 = call i32 @llvm.amdgcn.mov.dpp.i32(i32 %data, i32 %14, i32 15, i32 15, i1 true)

    ret i32 %15
}

declare i32 @llvm.amdgcn.mov.dpp.i32(i32, i32, i32, i32, i1) #2
declare i32 @llvm.amdgcn.wqm.i32(i32) #1
declare i32 @llvm.amdgcn.ds.bpermute(i32, i32) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readnone convergent }
