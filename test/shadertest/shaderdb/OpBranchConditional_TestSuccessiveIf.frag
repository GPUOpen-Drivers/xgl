#version 450 core

layout(binding = 0) uniform Uniforms
{
    int i1;
};

layout(location = 0) out vec4 f;

void main()
{
    vec4 f4 = vec4(0.0);

    if (i1 == 1)
    {
        f4 = vec4(0.1);
    }

    if (i1 == 2)
    {
        f4 = vec4(0.2);
    }

    f = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
