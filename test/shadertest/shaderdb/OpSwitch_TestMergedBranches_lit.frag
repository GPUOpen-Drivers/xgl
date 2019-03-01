#version 450 core

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 f;

void main()
{
    vec4 f4 = vec4(0.0);

    switch (i)
    {
    case 0:
    case 1:
    default:
        f4.z = 0.7;
        break;
    }

    f = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: switch i32 %{{[0-9]*}}, label %{{[0-9]*}} [
; SHADERTEST:    i32 0, label %{{[0-9]*}}
; SHADERTEST:    i32 1, label %{{[0-9]*}}
; SHADERTEST:  ]
; SHADERTEST:  <label>


; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
