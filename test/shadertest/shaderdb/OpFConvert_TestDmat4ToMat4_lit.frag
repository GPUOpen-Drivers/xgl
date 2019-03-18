#version 450

layout(binding = 0) uniform Uniforms
{
    dmat4 dm4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    mat4 m4 = mat4(dm4);

    fragColor = (m4[0] != m4[1]) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST-COUNT-2: fptrunc <4 x double> {{.*}} to <4 x float>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST-COUNT-8: fptrunc double {{.*}} to float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
