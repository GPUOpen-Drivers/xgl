#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0, f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (f3_0 == f3_1) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST: fcmp oeq <3 x float>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST-COUNT-3: fcmp oeq float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
