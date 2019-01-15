#version 450 core

layout(binding = 0) buffer Buffers
{
    vec4 f4;
};

layout(location = 0) out vec4 f;

void main()
{
    if (f4.x > 0.0)
    {
        f4.y = 0.5;
    }
    else
    {
        f4.z = 1.0;
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
