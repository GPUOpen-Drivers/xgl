#version 450 core

layout(binding = 0) uniform Uniforms
{
    int limit;
};

layout(location = 0) out vec4 f;

void main()
{
    vec4 color = vec4(0.0);

    for (uint i = 0; i < limit; (i % 2u == 0) ? i++ : i+=2)
    {
        color += vec4(0.1);

        if (i == limit / 2)
        {
            continue;
        }

        color += vec4(i);
    }

    f = color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
