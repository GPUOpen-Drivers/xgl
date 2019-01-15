#version 450 core

layout(location = 0) in float fIn;
layout(location = 0) out float fOut;

void main()
{
    int exp = 0;
    float f = frexp(fIn, exp);
    f += pow(2.0, exp);
    fOut = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
