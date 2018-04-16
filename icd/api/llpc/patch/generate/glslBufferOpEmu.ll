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

; GLSL: load float16/int16/uint16 (word)
define <2 x i8> @llpc.buffer.load.v2i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call float @llvm.amdgcn.buffer.load.ushort(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast float %1 to <4 x i8>
    %3 = shufflevector <4 x i8> %2, <4 x i8> %2, <2 x i32> <i32 0, i32 1>
    ret <2 x i8> %3
}

; GLSL: uniform load float16/int16/uint16 (word)
define <2 x i8> @llpc.buffer.load.uniform.v2i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call float @llvm.amdgcn.buffer.load.ushort(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast float %1 to <4 x i8>
    %3 = shufflevector <4 x i8> %2, <4 x i8> %2, <2 x i32> <i32 0, i32 1>
    ret <2 x i8> %3
}

; GLSL: load f16vec2/i16vec2/u16vec2/float/int/uint (dword)
define <4 x i8> @llpc.buffer.load.v4i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call float @llvm.amdgcn.buffer.load.f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast float %1 to <4 x i8>
    ret <4 x i8> %2
}

; GLSL: uniform load f16vec2/i16vec2/u16vec2/float/int/uint (dword)
define <4 x i8> @llpc.buffer.load.uniform.v4i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast i32 %1 to <4 x i8>
    ret <4 x i8> %2
}

; GLSL: load f16vec3/i16vec3/u16vec3 (wordx3)
define <6 x i8> @llpc.buffer.load.v6i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call float @llvm.amdgcn.buffer.load.f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast float %1 to <4 x i8>
    %3 = add i32 %memberOffset, 4
    %4 = call float @llvm.amdgcn.buffer.load.ushort(<4 x i32> %desc, i32 0, i32 %3, i1 %glc, i1 %slc)
    %5 = bitcast float %4 to <4 x i8>
    %6 = shufflevector <4 x i8> %2, <4 x i8> %5, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    ret <6 x i8> %6
}

; GLSL: uniform load f16vec3/i16vec3/u16vec3 (wordx3)
define <6 x i8> @llpc.buffer.load.uniform.v6i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call float @llvm.amdgcn.buffer.load.f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast float %1 to <4 x i8>
    %3 = add i32 %memberOffset, 4
    %4 = call float @llvm.amdgcn.buffer.load.ushort(<4 x i32> %desc, i32 0, i32 %3, i1 %glc, i1 %slc)
    %5 = bitcast float %4 to <4 x i8>
    %6 = shufflevector <4 x i8> %2, <4 x i8> %5, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5>
    ret <6 x i8> %6
}

; GLSL: load f16vec4/i16vec4/u16vec4/vec2/ivec2/uvec2/double/int64/uint64 (dwordx2)
define <8 x i8> @llpc.buffer.load.v8i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <2 x float> @llvm.amdgcn.buffer.load.v2f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast <2 x float> %1 to <8 x i8>
    ret <8 x i8> %2
}

; GLSL: uniform load f16vec4/i16vec4/u16vec4/vec2/ivec2/uvec2/double/int64/uint64 (dwordx2)
define <8 x i8> @llpc.buffer.load.uniform.v8i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast <2 x i32> %1 to <8 x i8>
    ret <8 x i8> %2
}

; GLSL: load vec3/ivec3/uvec3 (dwordx3)
define <12 x i8> @llpc.buffer.load.v12i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = shufflevector <4 x float> %1, <4 x float> %1, <3 x i32> <i32 0, i32 1, i32 2>
    %3 = bitcast <3 x float> %2 to <12 x i8>
    ret <12 x i8> %3
}

; GLSL: uniform load vec3/ivec3/uvec3 (dwordx3)
define <12 x i8> @llpc.buffer.load.uniform.v12i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = shufflevector <4 x i32> %1, <4 x i32> %1, <3 x i32> <i32 0, i32 1, i32 2>
    %3 = bitcast <3 x i32> %2 to <12 x i8>
    ret <12 x i8> %3
}

; GLSL: load vec4/ivec4/uvec4/dvec2/i64vec2/u64vec2 (dwordx4)
define <16 x i8> @llpc.buffer.load.v16i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = bitcast <4 x float> %1 to <16 x i8>
    ret <16 x i8> %2
}

; GLSL: uniform load vec4/ivec4/uvec4/dvec2/i64vec2/u64vec2 (dwordx4)
define <16 x i8> @llpc.buffer.load.uniform.v16i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = bitcast <4 x i32> %1 to <16 x i8>
    ret <16 x i8> %2
}

; GLSL: load dvec3/i64vec3/u64vec3 (dwordx6)
define <24 x i8> @llpc.buffer.load.v24i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = add i32 %memberOffset, 16
    %3 = call <2 x float> @llvm.amdgcn.buffer.load.v2f32(<4 x i32> %desc, i32 0, i32 %2, i1 %glc, i1 %slc)
    %4 = shufflevector <2 x float> %3, <2 x float> %3, <4 x i32> <i32 0, i32 1, i32 undef, i32 undef>
    %5 = shufflevector <4 x float> %1, <4 x float> %4, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5> 
    %6 = bitcast <6 x float> %5 to <24 x i8>
    ret <24 x i8> %6
}

; GLSL: uniform load dvec3/i64vec3/u64vec3 (dwordx6)
define <24 x i8> @llpc.buffer.load.uniform.v24i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)

    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = add i32 %memberOffset, 16
    %3 = call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %desc, i32 %2, i1 %glc)
    %4 = shufflevector <2 x i32> %3, <2 x i32> %3, <4 x i32> <i32 0, i32 1, i32 undef, i32 undef>
    %5 = shufflevector <4 x i32> %1, <4 x i32> %4, <6 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5> 
    %6 = bitcast <6 x i32> %5 to <24 x i8>
    ret <24 x i8> %6
}

; GLSL: load dvec4/i64vec4/u64vec4 (dwordx8)
define <32 x i8> @llpc.buffer.load.v32i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %2 = add i32 %memberOffset, 16
    %3 = call <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32> %desc, i32 0, i32 %2, i1 %glc, i1 %slc)
    %4 = shufflevector <4 x float> %1, <4 x float> %3,
                       <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %5 = bitcast <8 x float> %4 to <32 x i8>
    ret <32 x i8> %5
}

; GLSL: uniform load dvec4/i64vec4/u64vec4 (dwordx8)
define <32 x i8> @llpc.buffer.load.uniform.v32i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %readonly, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %memberOffset, i1 %glc)
    %2 = add i32 %memberOffset, 16
    %3 = call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %desc, i32 %2, i1 %glc)
    %4 = shufflevector <4 x i32> %1, <4 x i32> %3,
                       <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
    %5 = bitcast <8 x i32> %4 to <32 x i8>
    ret <32 x i8> %5
}

; GLSL: store float16/int16/uint16 (word)
define void @llpc.buffer.store.v2i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <2 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = shufflevector <2 x i8> %storeData, <2 x i8> <i8 0, i8 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3> 
    %2 = bitcast <4 x i8> %1 to float
    call void @llvm.amdgcn.buffer.store.short(float %2, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    ret void
}

; GLSL: store f16vec2/i16vec2/u16vec2/float/int/uint (dword)
define void @llpc.buffer.store.v4i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <4 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <4 x i8> %storeData to float
    call void @llvm.amdgcn.buffer.store.f32(float %1, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    ret void
}

; GLSL: store f16vec3/i16vec3/u16vec3 (wordx3)
define void @llpc.buffer.store.v6i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <6 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = shufflevector <6 x i8> %storeData, <6 x i8> %storeData, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %2 = bitcast <4 x i8> %1 to float
    call void @llvm.amdgcn.buffer.store.f32(float %2, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %glc, i1 %slc)
    %3 = add i32 %memberOffset, 4
    %4 = shufflevector <6 x i8> %storeData, <6 x i8> %storeData, <2 x i32> <i32 4, i32 5>
    %5 = shufflevector <2 x i8> %4, <2 x i8> <i8 0, i8 0>, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
    %6 = bitcast <4 x i8> %5 to float
    call void @llvm.amdgcn.buffer.store.short(float %6, <4 x i32> %desc, i32 0, i32 %3, i1 %glc, i1 %slc)
    ret void
}

; GLSL: store f16vec4/i61vec4/u16vec4/vec2/ivec2/uvec2/dvec2/int64/uint64 (dwordx2)
define void @llpc.buffer.store.v8i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <8 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <8 x i8> %storeData to <2 x float>
    call void @llvm.amdgcn.buffer.store.v2f32(<2 x float> %1,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %memberOffset,
                                              i1 %glc,
                                              i1 %slc)
    ret void
}

; GLSL: store vec3/ivec3/uvec3 (dwordx3)
define void @llpc.buffer.store.v12i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <12 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <12 x i8> %storeData to <3 x float>
    %2 = shufflevector <3 x float> %1, <3 x float> %1, <2 x i32> <i32 0, i32 1>
    call void @llvm.amdgcn.buffer.store.v2f32(<2 x float> %2,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %memberOffset,
                                              i1 %glc,
                                              i1 %slc)
    %3 = extractelement <3 x float> %1, i32 2
    %4 = add i32 %memberOffset, 8
    call void @llvm.amdgcn.buffer.store.f32(float %3, <4 x i32> %desc, i32 0, i32 %4, i1 %glc, i1 %slc)
    ret void
}

; GLSL: store vec4/ivec4/uvec4/dvec2/i64vec2/u64vec2 (dwordx4)
define void @llpc.buffer.store.v16i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <16 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <16 x i8> %storeData to <4 x float>
    call void @llvm.amdgcn.buffer.store.v4f32(<4 x float> %1,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %memberOffset,
                                              i1 %glc,
                                              i1 %slc)
    ret void
}

; GLSL: store dvec3/i64vec3/u64vec3 (dwordx6)
define void @llpc.buffer.store.v24i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <24 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <24 x i8> %storeData to <6 x float>
    %2 = shufflevector <6 x float> %1, <6 x float> %1, <4 x i32> <i32 0, i32 1, i32 2 , i32 3>
    call void @llvm.amdgcn.buffer.store.v4f32(<4 x float> %2,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %memberOffset,
                                              i1 %glc,
                                              i1 %slc)
    %3 = shufflevector <6 x float> %1, <6 x float> %1, <2 x i32> <i32 4, i32 5>
    %4 = add i32 %memberOffset, 16
    call void @llvm.amdgcn.buffer.store.v2f32(<2 x float> %3,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %4,
                                              i1 %glc,
                                              i1 %slc)
    ret void
}

; GLSL: store dvec4/i64vec4/u64vec4 (dwordx8)
define void @llpc.buffer.store.v32i8(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, <32 x i8> %storeData, i1 %glc, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = bitcast <32 x i8> %storeData to <8 x float>
    %2 = shufflevector <8 x float> %1, <8 x float> %1, <4 x i32> <i32 0, i32 1, i32 2 , i32 3>
    call void @llvm.amdgcn.buffer.store.v4f32(<4 x float> %2,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %memberOffset,
                                              i1 %glc,
                                              i1 %slc)
    %3 = shufflevector <8 x float> %1, <8 x float> %1, <4 x i32> <i32 4, i32 5, i32 6, i32 7>
    %4 = add i32 %memberOffset, 16
    call void @llvm.amdgcn.buffer.store.v4f32(<4 x float> %3,
                                              <4 x i32> %desc,
                                              i32 0,
                                              i32 %4,
                                              i1 %glc,
                                              i1 %slc)
    ret void
}

; GLSL: uint atomicAdd(inout uint, uint)
;       int  atomicAdd(inout int, int)
define i32 @llpc.buffer.atomic.iadd.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.add(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicSub(inout uint, uint)
;        int atomicSub(inout int, int)
define i32 @llpc.buffer.atomic.isub.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.sub(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicMin(inout uint, uint)
define i32 @llpc.buffer.atomic.umin.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.umin(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: int atomicMin(inout int, int)
define i32 @llpc.buffer.atomic.smin.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.smin(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicMax(inout uint, uint)
define i32 @llpc.buffer.atomic.umax.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.umax(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: int atomicMax(inout int, int)
define i32 @llpc.buffer.atomic.smax.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.smax(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicAnd(inout uint, uint)
;       int  atomicAnd(inout int, int)
define i32 @llpc.buffer.atomic.and.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.and(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicOr(inout uint, uint)
;       int  atomicOr(inout int, int)
define i32 @llpc.buffer.atomic.or.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.or(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicXor(inout uint, uint)
;       int  atomicXor(inout int, int)
define i32 @llpc.buffer.atomic.xor.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.xor(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicExchange(inout uint, uint)
;       int  atomicExchange(inout int, int)
define i32 @llpc.buffer.atomic.exchange.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.swap(i32 %data, <4 x i32> %desc, i32 0, i32 %memberOffset, i1 %slc)
    ret i32 %1
}

; GLSL: float atomicExchange(inout float, float)
define float @llpc.buffer.atomic.exchange.f32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, float %data, i1 %slc) #0
{
    %1 = bitcast float %data to i32
    %2 = call i32 @llpc.buffer.atomic.exchange.i32(i32 %descSet,
                                                   i32 %binding,
                                                   i32 %blockOffset,
                                                   i32 %memberOffset,
                                                   i32 %1,
                                                   i1  %slc)
    %3 = bitcast i32 %2 to float
    ret float %3
}

; GLSL: uint atomicCompSwap(inout uint, uint, uint)
;       int  atomicCompSwap(inout int, int, int)
define i32 @llpc.buffer.atomic.compareexchange.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i32 %compare, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    %1 = call i32 @llvm.amdgcn.buffer.atomic.cmpswap(i32 %data,
                                                     i32 %compare,
                                                     <4 x i32> %desc,
                                                     i32 0,
                                                     i32 %memberOffset,
                                                     i1 %slc)
    ret i32 %1
}

; GLSL: uint atomicIncrement(inout uint)
;       int  atomicIncrement(inout int)
define i32 @llpc.buffer.atomic.iincrement.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i32 @llpc.buffer.atomic.iadd.i32(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i32 1,
                                               i1  %slc)
    ret i32 %1
}

; GLSL: uint atomicDecrement(inout uint)
;       int  atomicDecrement(inout int)
define i32 @llpc.buffer.atomic.idecrement.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i32 @llpc.buffer.atomic.isub.i32(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i32 1,
                                               i1  %slc)
    ret i32 %1
}

; GLSL: uint atomicLoad(inout uint)
;       int  atomicLoad(inout int)
define i32 @llpc.buffer.atomic.load.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i32 @llpc.buffer.atomic.iadd.i32(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i32 0,
                                               i1 %slc)
    ret i32 %1
}

; GLSL: float atomicLoad(inout float)
define float @llpc.buffer.atomic.load.f32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i32 @llpc.buffer.atomic.load.i32(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i1  %slc)
    %2 = bitcast i32 %1 to float
    ret float %2
}

; GLSL: void atomicStore(inout uint, uint)
;       void atomicStore(inout int, int)
define void @llpc.buffer.atomic.store.i32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i32 %data, i1 %slc) #0
{
    %1 = call i32 @llpc.buffer.atomic.exchange.i32(i32 %descSet,
                                                   i32 %binding,
                                                   i32 %blockOffset,
                                                   i32 %memberOffset,
                                                   i32 %data,
                                                   i1  %slc)
    ; We do not care about the result returned by "exchange" operation. We just want to store the specified
    ; data to the targeted buffer.
    ret void
}

; GLSL: void atomicStore(inout float, float)
define void @llpc.buffer.atomic.store.f32(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, float %data, i1 %slc) #0
{
    %1 = bitcast float %data to i32
    %2 = call i32 @llpc.buffer.atomic.exchange.i32(i32 %descSet,
                                                   i32 %binding,
                                                   i32 %blockOffset,
                                                   i32 %memberOffset,
                                                   i32 %1,
                                                   i1  %slc)
    ; We do not care about the result returned by "exchange" operation. We just want to store the specified
    ; data to the targeted buffer.
    ret void
}

; GLSL: uint64_t atomicAdd(inout uint64_t, uint64_t)
;       int64_t  atomicAdd(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.iadd.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicSub(inout uint64_t, uint64_t)
;       int64_t  atomicSub(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.isub.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicMin(inout uint64_t, uint64_t)
define i64 @llpc.buffer.atomic.umin.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: int64_t atomicMin(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.smin.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicMax(inout uint64_t, uint64_t)
define i64 @llpc.buffer.atomic.umax.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: int64_t atomicMax(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.smax.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicAnd(inout uint64_t, uint64_t)
;       int64_t  atomicAnd(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.and.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicOr(inout uint64_t, uint64_t)
;       int64_t  atomicOr(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.or.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicXor(inout uint64_t, uint64_t)
;       int64_t  atomicXor(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.xor.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicExchange(inout uint64_t, uint64_t)
;       int64_t  atomicExchange(inout int64_t, int64_t)
define i64 @llpc.buffer.atomic.exchange.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicCompSwap(inout uint64_t, uint64_t, uint64_t)
;       int64_t  atomicCompSwap(inout int64_t, int64_t, int64_t)
define i64 @llpc.buffer.atomic.compareexchange.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i64 %compare, i1 %slc) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; Support 64-bit buffer atomic operations.
    ret i64 0
}

; GLSL: uint64_t atomicIncrement(inout uint64_t)
;       int64_t  atomicIncrement(inout int64_t)
define i64 @llpc.buffer.atomic.iincrement.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i64 @llpc.buffer.atomic.iadd.i64(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i64 1,
                                               i1  %slc)
    ret i64 %1
}

; GLSL: uint64_t atomicDecrement(inout uint64_t)
;       int64_t  atomicDecrement(inout int64_t)
define i64 @llpc.buffer.atomic.idecrement.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i64 @llpc.buffer.atomic.isub.i64(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i64 1,
                                               i1  %slc)
    ret i64 %1
}

; GLSL: uint64_t atomicLoad(inout uint64_t)
;       int64_t  atomicLoad(inout int64_t)
define i64 @llpc.buffer.atomic.load.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i1 %slc) #0
{
    %1 = call i64 @llpc.buffer.atomic.iadd.i64(i32 %descSet,
                                               i32 %binding,
                                               i32 %blockOffset,
                                               i32 %memberOffset,
                                               i64 0,
                                               i1  %slc)
    ret i64 %1
}

; GLSL: void atomicStore(inout uint64_t, uint64_t)
;       void atomicStore(inout int64_t, int64_t)
define void @llpc.buffer.atomic.store.i64(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %memberOffset, i64 %data, i1 %slc) #0
{
    %1 = call i64 @llpc.buffer.atomic.exchange.i64(i32 %descSet,
                                                   i32 %binding,
                                                   i32 %blockOffset,
                                                   i32 %memberOffset,
                                                   i64 %data,
                                                   i1  %slc)
    ; We do not care about the result returned by "exchange" operation. We just want to store the specified
    ; data to the targeted buffer.
    ret void
}

; GLSL: int array.length()
define i32 @llpc.buffer.arraylength(
    i32 %descSet, i32 %binding, i32 %blockOffset, i32 %arrayOffset, i32 %arrayStride) #0
{
    %desc = call <4 x i32> @llpc.descriptor.load.buffer(i32 %descSet, i32 %binding, i32 %blockOffset)
    ; The third DWORD is the field NUM_RECORDS of SQ_BUF_RSRC_WORD2. For untyped buffer, the buffer stride is
    ; always set to 1. Therefore, this field represents size of the buffer.
    %1 = extractelement <4 x i32> %desc, i32 2
    %2 = sub i32 %1, %arrayOffset
    %3 = udiv i32 %2, %arrayStride
    ret i32 %3
}

declare float @llvm.amdgcn.buffer.load.f32(<4 x i32>, i32, i32, i1, i1) #1
declare <2 x float> @llvm.amdgcn.buffer.load.v2f32(<4 x i32>, i32, i32, i1, i1) #1
declare <4 x float> @llvm.amdgcn.buffer.load.v4f32(<4 x i32>, i32, i32, i1, i1) #1
declare float @llvm.amdgcn.buffer.load.ushort(<4 x i32>, i32, i32, i1, i1) #1

declare i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32>, i32, i1) #1
declare <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32>, i32, i1) #1
declare <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32>, i32, i1) #1

declare void @llvm.amdgcn.buffer.store.f32(float, <4 x i32>, i32, i32, i1, i1) #2
declare void @llvm.amdgcn.buffer.store.v2f32(<2 x float>, <4 x i32>, i32, i32, i1, i1) #2
declare void @llvm.amdgcn.buffer.store.v4f32(<4 x float>, <4 x i32>, i32, i32, i1, i1) #2
declare void @llvm.amdgcn.buffer.store.short(float, <4 x i32>, i32, i32, i1, i1) #2

declare i32 @llvm.amdgcn.buffer.atomic.swap(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.add(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.sub(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.smin(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.umin(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.smax(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.umax(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.and(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.or(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.xor(i32, <4 x i32>, i32, i32, i1) #0
declare i32 @llvm.amdgcn.buffer.atomic.cmpswap(i32, i32, <4 x i32>, i32, i32, i1) #0

declare <4 x i32> @llpc.descriptor.load.buffer(i32 , i32 , i32) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind writeonly }

!0 = !{}
