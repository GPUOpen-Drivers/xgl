#version 450 core

layout(binding = 0) buffer Buffers
{
    int i;
};

layout(location = 0) out vec4 f;

void main()
{
    do
    {
        if (i > 6)
        {
            i += 2;
            continue;
        }

        i++;
    } while (i < 19);

    f = vec4(i);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
