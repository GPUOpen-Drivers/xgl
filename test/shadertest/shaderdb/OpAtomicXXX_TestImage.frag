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