#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 v3;
    dvec3 d3_0;
};

layout(location = 0) out vec4 fragColor;

void main ()
{
    vec4 color = vec4(0.5);

    dvec3 d3_1 = v3;

    if (d3_0 == d3_1)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST: fpext <3 x float> {{.*}} to <3 x double>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
