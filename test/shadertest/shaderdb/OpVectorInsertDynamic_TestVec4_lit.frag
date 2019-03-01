#version 450

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);
    color[i] = 0.4;

    fragColor = color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = getelementptr <4 x float>, <4 x float> {{.*}}* %{{.*}}, i32 0, i32 %{{.*}},
; SHADERTEST: store float 0x3FD99999A0000000, float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
