#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3 = uvec3(d3);

    fragColor = (u3.x == u3.y) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fptoui <3 x double> {{.*}} to <3 x i32>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
