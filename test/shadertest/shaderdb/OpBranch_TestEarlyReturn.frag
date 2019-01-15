#version 450 core

layout(binding = 0) uniform Uniforms
{
    bool cond;
};

layout(location = 0) out vec4 f;

void main()
{
    if (cond)
    {
        f = vec4(1.0);
        return;
    }

    f = vec4(1.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
