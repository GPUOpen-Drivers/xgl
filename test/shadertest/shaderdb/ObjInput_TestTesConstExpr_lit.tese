#version 450 core

layout(triangles) in;

layout(location = 0) out float outColor;

float TessCoord()
{
    float f = gl_TessCoord.x;
    f += gl_TessCoord[1] * 0.5;
    f -= gl_TessCoord.z;
    return f;
}

void main(void)
{
    outColor = TessCoord();
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-3: call float @llpc.input.import.builtin.TessCoord.f32{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
