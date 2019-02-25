#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1 = int(d1);

    fragColor = (i1 == 5) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: fptosi double {{.*}} to i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
