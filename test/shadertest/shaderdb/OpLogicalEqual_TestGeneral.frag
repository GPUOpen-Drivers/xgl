#version 450

layout(std430, binding = 0) buffer Buffer
{
    int   i1;
    ivec2 i2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    bool b1_0 = (i1 == i2.x);
    bool b1_1 = (i1 == i2.y);

    bvec3 b3_0 = bvec3(b1_0, b1_1, true);
    bvec3 b3_1 = bvec3(false, false, b1_0);

    b1_0 = (b1_0 == b1_1);
    b3_0 = equal(b3_0, b3_1);

    fragColor = (b1_0 && b3_0.x ? vec4(0.0) : vec4(1.0));
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
