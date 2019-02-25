#version 450

layout(binding = 0) uniform Uniforms
{
    uvec3 u3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3 = u3;

    fragColor = (d3.x > d3.z) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: uitofp <3 x i32> {{.*}} to <3 x double>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
