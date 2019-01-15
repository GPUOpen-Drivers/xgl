#version 450

struct DATA
{
    dmat2x3 dm2x3;
    vec3 f3;
};

layout(location = 0) out OUTPUT
{
    vec3 f3;
    int i1;
    DATA data;
} outBlock;

layout(binding = 0) uniform Uniforms
{
    int index;
};

void main()
{
    outBlock.data.dm2x3[index] = dvec3(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
