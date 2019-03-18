#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main ()
{
    vec4 color = vec4(0.5);

    float  f1 = float(d1);

    if (f1 > 0.0f)
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
; SHADERTEST fptrunc double {{.*}} to float
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST fptrunc double {{.*}} to float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
