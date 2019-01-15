#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 color = vec3(0.5);
    vec4 data  = vec4(0.0);

    data.xw = color.zy;

    fragColor = data;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
