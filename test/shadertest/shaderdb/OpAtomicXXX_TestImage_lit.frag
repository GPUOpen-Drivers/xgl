#version 450

layout(set = 0, binding = 0, r32i)  uniform iimage1D        iimg1D;
layout(set = 1, binding = 0, r32i)  uniform iimage2D        iimg2D[4];
layout(set = 0, binding = 1, r32i)  uniform iimage2DMS      iimg2DMS;
layout(set = 0, binding = 2, r32ui) uniform uimageCube      uimgCube;
layout(set = 2, binding = 0, r32ui) uniform uimageBuffer    uimgBuffer[4];
layout(set = 0, binding = 3, r32ui) uniform uimage2DMSArray uimg2DMSArray;
layout(set = 0, binding = 4, r32f)  uniform image2DRect     img2DRect;

layout(set = 3, binding = 0) uniform Uniforms
{
    int   idata;
    uint  udata;
    float fdata;

    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1 = imageAtomicAdd(iimg1D, 1, idata);
    i1 += imageAtomicMin(iimg2D[1], ivec2(2), idata);
    i1 += imageAtomicMax(iimg2D[index], ivec2(2), idata);
    i1 += imageAtomicAnd(iimg2DMS, ivec2(2), 4, idata);
    i1 += imageAtomicOr(iimg1D, 1, idata);
    i1 += imageAtomicXor(iimg1D, 2, idata);
    i1 += imageAtomicExchange(iimg1D, 1, idata);
    i1 += imageAtomicCompSwap(iimg1D, 1, 28, idata);

    uint u1 = imageAtomicAdd(uimgCube, ivec3(1), udata);
    u1 += imageAtomicMin(uimgBuffer[1], 2, udata);
    u1 += imageAtomicMax(uimgBuffer[index], 1, udata);
    u1 += imageAtomicAnd(uimg2DMSArray, ivec3(2), 5, udata);
    u1 += imageAtomicOr(uimgCube, ivec3(1), udata);
    u1 += imageAtomicXor(uimgCube, ivec3(2), udata);
    u1 += imageAtomicExchange(uimgCube, ivec3(1), udata);
    u1 += imageAtomicCompSwap(uimgCube, ivec3(1), 17u, udata);

    float f1 = imageAtomicExchange(img2DRect, ivec2(3), fdata);

    fragColor = vec4(i1, i1, u1, f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.image.atomiciadd.i32.1D.
; SHADERTEST: call i32 @llpc.image.atomicsmin.i32.2D.
; SHADERTEST: call i32 @llpc.image.atomicsmax.i32.2D.
; SHADERTEST: call i32 @llpc.image.atomicand.i32.2D.sample.
; SHADERTEST: call i32 @llpc.image.atomicor.i32.1D.
; SHADERTEST: call i32 @llpc.image.atomicxor.i32.1D.
; SHADERTEST: call i32 @llpc.image.atomicexchange.i32.1D.
; SHADERTEST: call i32 @llpc.image.atomiccompexchange.i32.1D.
; SHADERTEST: call i32 @llpc.image.atomiciadd.u32.Cube.
; SHADERTEST: call i32 @llpc.image.atomicumin.u32.Buffer
; SHADERTEST: call i32 @llpc.image.atomicumax.u32.Buffer
; SHADERTEST: call i32 @llpc.image.atomicand.u32.2DArray.sample.
; SHADERTEST: call i32 @llpc.image.atomicor.u32.Cube.
; SHADERTEST: call i32 @llpc.image.atomicxor.u32.Cube.
; SHADERTEST: call i32 @llpc.image.atomicexchange.u32.Cube.
; SHADERTEST: call i32 @llpc.image.atomiccompexchange.u32.Cube.
; SHADERTEST: call float @llpc.image.atomicexchange.f32.Rect.

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.add.1d.i32.i32(i32 %{{[0-9]*}}, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.smin.2d.i32.i32(i32 %{{[0-9]*}}, i32 2, i32 2, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.smax.2d.i32.i32(i32 %{{[0-9]*}}, i32 2, i32 2, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.and.2dmsaa.i32.i32(i32 %{{[0-9]*}}, i32 2, i32 2, i32 4, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.or.1d.i32.i32(i32 %{{[0-9]*}}, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.xor.1d.i32.i32(i32 %{{[0-9]*}}, i32 2, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.swap.1d.i32.i32(i32 %{{[0-9]*}}, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.cmpswap.1d.i32.i32(i32 %{{[0-9]*}}, i32 28, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.add.cube.i32.i32(i32 %{{[0-9]*}}, i32 1, i32 1, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.struct.buffer.atomic.umin.i32(i32 %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 2, i32 0, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.struct.buffer.atomic.umax.i32(i32 %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 1, i32 0, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.and.2darraymsaa.i32.i32(i32 %{{[0-9]*}}, i32 2, i32 2, i32 2, i32 5, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.or.cube.i32.i32(i32 %{{[0-9]*}}, i32 1, i32 1, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.xor.cube.i32.i32(i32 %{{[0-9]*}}, i32 2, i32 2, i32 2, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.swap.cube.i32.i32(i32 %{{[0-9]*}}, i32 1, i32 1, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.cmpswap.cube.i32.i32(i32 %{{[0-9]*}}, i32 17, i32 1, i32 1, i32 1, <8 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call i32 @llvm.amdgcn.image.atomic.swap.2d.i32.i32(i32 %{{[0-9]*}}, i32 3, i32 3, <8 x i32> %{{[0-9]*}}, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
