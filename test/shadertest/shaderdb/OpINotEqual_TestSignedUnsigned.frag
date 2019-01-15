#version 450

layout(set = 0, binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 color;

void main()
{
    if (bool(i))
    {
        color = vec4(1, 1, 1, 1);
    }
    else
    {
        color = vec4(0, 0, 0, 0);
    }
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
