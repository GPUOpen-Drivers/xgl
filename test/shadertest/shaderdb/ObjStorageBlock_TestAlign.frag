#version 450

struct str
{
    float f;
};

layout(set = 0, binding = 0, std430) buffer BB
{
   layout(align = 32) vec4 m1;
   layout(align = 64) str m2;
   layout(align = 256) vec2 m3;
};

layout(set = 1, binding = 0) uniform Uniforms
{
    vec4 u1;
    float u2;
    vec2 u3;
};

layout(location = 0) out vec4 o1;
layout(location = 1) out float o2;
layout(location = 2) out vec2 o3;

void main()
{
    m1 = u1;
    m2.f = u2;
    m3 = u3;

    o1 = m1;
    o2 = m2.f;
    o3 = m3;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
