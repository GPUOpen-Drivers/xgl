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

define i32 @llpc.image.querynonlod.sizelod.i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    ret i32 %3
}

define <2 x i32> @llpc.image.querynonlod.sizelod.v2i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
    i32 %resourceBinding, i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1
    ret <2 x i32> %6
}

define <3 x i32> @llpc.image.querynonlod.sizelod.v3i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = insertelement <3 x i32> undef, i32 %3, i32 0
    %7 = insertelement <3 x i32> %6, i32 %4, i32 1
    %8 = insertelement <3 x i32> %7, i32 %5, i32 2
    ret <3 x i32> %8
}

define <2 x i32> @llpc.image.querynonlod.sizelod.array.v2i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
    i32 %resourceBinding, i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 true)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = insertelement <2 x i32> undef, i32 %3, i32 0
    %6 = insertelement <2 x i32> %5, i32 %4, i32 1
    ret <2 x i32> %6
}

define <3 x i32> @llpc.image.querynonlod.sizelod.array.v3i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 true)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = insertelement <3 x i32> undef, i32 %3, i32 0
    %7 = insertelement <3 x i32> %6, i32 %4, i32 1
    %8 = insertelement <3 x i32> %7, i32 %5, i32 2
    ret <3 x i32> %8
}


define <3 x i32> @llpc.image.querynonlod.sizelod.cubearray.v3i32(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 %lod,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 true)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 0
    %4 = extractelement <4 x i32> %2, i32 1
    %5 = extractelement <4 x i32> %2, i32 2
    %6 = sdiv i32 %5, 6
    %7 = insertelement <3 x i32> undef, i32 %3, i32 0
    %8 = insertelement <3 x i32> %7, i32 %4, i32 1
    %9 = insertelement <3 x i32> %8, i32 %6, i32 2
    ret <3 x i32> %9
}

define i32 @llpc.image.querynonlod.sizelod.buffer.i32.gfx6(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <4 x i32> @llpc.descriptor.load.texelbuffer(i32 %resourceDescSet,
                                                                 i32 %resourceBinding,
                                                                 i32 %resourceIdx)

    ; Extract NUM_RECORDS (SQ_BUF_RSRC_WORD2)
    %1 = extractelement <4 x i32> %resource, i32 2

    ret i32 %1
}

define i32 @llpc.image.querynonlod.sizelod.buffer.i32.gfx8(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %lod, i32 %imageCallMeta) #0
{
    %resource = call <4 x i32> @llpc.descriptor.load.texelbuffer(i32 %resourceDescSet,
                                                                 i32 %resourceBinding,
                                                                 i32 %resourceIdx)
    ; Extract NUM_RECORDS (SQ_BUF_RSRC_WORD2)
    %1 = extractelement <4 x i32> %resource, i32 2

    ; Extract STRIDE (SQ_BUF_RSRC_WORD1, [29:16])
    %2 = extractelement <4 x i32> %resource, i32 1
    %3 = call i32 @llvm.amdgcn.ubfe.i32(i32 %2, i32 16, i32 14)

    ; Buffer size = NUM_RECORDS / STRIDE
    %4 = udiv i32 %1, %3

    ret i32 %4
}

define i32 @llpc.image.querynonlod.levels(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = call <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 undef,
                                                                        <8 x i32> %resource,
                                                                        i32 15,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false,
                                                                        i1 false)
    %2 = bitcast <4 x float> %1 to <4 x i32>
    %3 = extractelement <4 x i32> %2, i32 3
    ret i32 %3
}

define i32 @llpc.image.querynonlod.samples(
    i32 %resourceDescSet, i32 %resourceBinding, i32 %resourceIdx, i32 %imageCallMeta) #0
{
    %resource = call <8 x i32> @llpc.descriptor.load.resource(i32 %resourceDescSet,
                                                              i32 %resourceBinding,
                                                              i32 %resourceIdx)
    %1 = extractelement <8 x i32> %resource, i32 3

    ; Extract LAST_LEVEL (SQ_IMG_RSRC_WORD3, [19:16])
    %2 = call i32 @llvm.amdgcn.ubfe.i32(i32 %1, i32 16, i32 4)
    ; Sample numer = 1 << LAST_LEVEL (LAST_LEVEL = log2(sample numer))
    %3 = shl i32 1, %2

    ; Extract TYPE(SQ_IMG_RSRC_WORD3, [31:28])
    %4 = call i32 @llvm.amdgcn.ubfe.i32(i32 %1, i32 28, i32 4)

    ; Check if resource type is 2D MSAA or 2D MSAA array, 14 = SQ_RSRC_IMG_2D_MSAA, 15 = SQ_RSRC_IMG_2D_MSAA_ARRAY
    %5 = icmp eq i32 %4, 14
    %6 = icmp eq i32 %4, 15
    %7 = or i1 %5, %6

    ; Return sample number if resource type is 2D MSAA or 2D MSAA array. Otherwise, return 1.
    %8 = select i1 %7, i32 %3, i32 1

    ret i32 %8
}

define <8 x i32> @llpc.patch.image.readwriteatomic.descriptor.cube(
    <8 x i32> %resource) #0
{
    ; Modify DEPTH
    %1 = extractelement <8 x i32> %resource, i32 4
    ; Extract DEPTH in resource descriptor
    %2 = call i32 @llvm.amdgcn.ubfe.i32(i32 %1, i32 0, i32 13)
    %3 = mul i32 %2, 6
    %4 = add i32 %3, 5
    ; -8192 = 0xFFFFE000
    %5 = and i32 %1, -8192
    %6 = or i32 %5, %4

    ; Change resource type to 2D array (0xD)
    %7 = extractelement <8 x i32> %resource, i32 3
    ; 268435455 = 0x0FFFFFFF
    %8 = and i32 %7, 268435455
    ; 3489660928 = 0xD0000000
    %9 = or i32 %8, 3489660928

    ; Insert modified value
    %10 = insertelement <8 x i32> %resource, i32 %6, i32 4
    %11 = insertelement <8 x i32> %10, i32 %9, i32 3

    ret <8 x i32> %11
}

define i1 @llpc.patch.image.gather.check(
    <8 x i32> %resource) #0
{
    ; Check whether we have to do patch operation for image gather by checking the data format
    ; of resource descriptor.
    %1 = extractelement <8 x i32> %resource, i32 1
    ; Extract DATA_FORMAT
    %2 = call i32 @llvm.amdgcn.ubfe.i32(i32 %1, i32 20, i32 6)

    %3 = icmp eq i32 %2, 1
    %4 = icmp eq i32 %2, 3
    %5 = icmp eq i32 %2, 10
    %6 = or i1 %4, %3
    %7 = or i1 %5, %6
    ret i1 %7
}

define <8 x i32> @llpc.patch.image.gather.descriptor.u32(
    <8 x i32> %resource) #0
{
    %1 = extractelement <8 x i32> %resource, i32 1
    %2 = call i1 @llpc.patch.image.gather.check(<8 x i32> %resource)

    ; Change NUM_FORMAT from UINT to USCALE 134217728 = 0x08000000
    %3 = sub i32 %1, 134217728
    %4 = select i1 %2, i32 %3, i32 %1
    %5 = insertelement <8 x i32> %resource, i32 %4, i32 1
    ret <8 x i32> %5
}

define <8 x i32> @llpc.patch.image.gather.descriptor.i32(
   <8 x i32> %resource) #0
{
    %1 = extractelement <8 x i32> %resource, i32 1
    %2 = call i1 @llpc.patch.image.gather.check(<8 x i32> %resource)

    ; Change NUM_FORMAT from SINT to SSCALE 134217728 = 0x08000000
    %3 = sub i32 %1, 134217728
    %4 = select i1 %2, i32 %3, i32 %1
    %5 = insertelement <8 x i32> %resource, i32 %4, i32 1
    ret <8 x i32> %5
}

define <4 x float> @llpc.patch.image.gather.texel.u32(
    <8 x i32> %resource, <4 x float> %result) #0
{
    %1 = extractelement <8 x i32> %resource, i32 1
    %2 = call i1 @llpc.patch.image.gather.check(<8 x i32> %resource)

    %3 = fptoui <4 x float> %result to <4 x i32>
    %4 = bitcast <4 x i32> %3 to <4 x float>
    %5 = select i1 %2, <4 x float> %4, <4 x float> %result
    ret <4 x float> %5
}

define <4 x float> @llpc.patch.image.gather.texel.i32(
    <8 x i32> %resource, <4 x float> %result) #0
{
    %1 = extractelement <8 x i32> %resource, i32 1
    %2 = call i1 @llpc.patch.image.gather.check(<8 x i32> %resource)

    %3 = fptosi <4 x float> %result to <4 x i32>
    %4 = bitcast <4 x i32> %3 to <4 x float>
    %5 = select i1 %2, <4 x float> %4, <4 x float> %result
    ret <4 x float> %5
}

define i1 @llpc.imagesparse.texel.resident(
    i32 %residentCode) #0
{
    %1 = icmp eq i32 %residentCode, 0
    ret i1 %1
}

declare <8 x i32> @llpc.descriptor.load.resource(i32 , i32 , i32) #0

declare <4 x i32> @llpc.descriptor.load.texelbuffer(i32 , i32 , i32) #0

declare <4 x float> @llvm.amdgcn.image.getresinfo.v4f32.i32.v8i32(i32 , <8 x i32> , i32, i1, i1, i1, i1) #0

declare i32 @llvm.amdgcn.ubfe.i32(i32, i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
