#version 450 core

layout(vertices = 3) out;

layout(location = 0) patch out vec3 outColor;

void PatchOut(float f)
{
    outColor.x = 0.7;
    outColor[1] = f;
    outColor.z = outColor.y;
}

void main(void)
{
    PatchOut(4.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
