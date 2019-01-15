#version 450

layout(set = 0, binding = 0) buffer INPUT
{
    int   imem;
    uint  umem[4];
};

layout(set = 0, binding = 1) buffer OUTPUT
{
    uvec4 u4;
};

layout(set = 1, binding = 0) uniform Uniforms
{
    int   idata;
    uint  udata;

    int index;
};

void main()
{
    int i1 = atomicAdd(imem, idata);
    i1 += atomicMin(imem, idata);
    i1 += atomicMax(imem, idata);
    i1 += atomicAnd(imem, idata);
    i1 += atomicOr(imem, idata);
    i1 += atomicXor(imem, idata);
    i1 += atomicExchange(imem, idata);
    i1 += atomicCompSwap(imem, 28, idata);

    uint u1 = atomicAdd(umem[0], udata);
    u1 += atomicMin(umem[1], udata);
    u1 += atomicMax(umem[2], udata);
    u1 += atomicAnd(umem[3], udata);
    u1 += atomicOr(umem[index], udata);
    u1 += atomicXor(umem[index], udata);
    u1 += atomicExchange(umem[index], udata);
    u1 += atomicCompSwap(umem[index], 16u, udata);

    u4[i1 % 4] = u1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
