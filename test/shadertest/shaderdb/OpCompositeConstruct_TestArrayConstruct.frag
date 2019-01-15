#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_0, d1_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_2[4] = double[4](d1_0, d1_1, d1_1, d1_0);

    vec3 f3[5] = vec3[5](vec3(0.5), vec3(1.0), vec3(0.5), vec3(1.0), vec3(1.5));

    fragColor = (d1_2[1] == d1_2[2]) ? vec4(1.0) : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
