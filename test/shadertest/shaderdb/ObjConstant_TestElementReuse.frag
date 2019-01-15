#version 450

struct Struct
{
    float   f1_0;
    float   f1_1;
    vec3    f3;
    mat2x3  m2x3;
};

layout(binding = 0) uniform Uniforms
{
    int i;
    int j;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const Struct constData =
    {
        0.0,
        1.0,
        vec3(1.0, 1.0, 0.0),
        mat2x3(vec3(1.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0))
    };

    Struct data = constData;
    fragColor = vec4(data.f1_0 + data.f1_1) + vec4(data.f3[i]) + vec4(data.m2x3[i][j]);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
