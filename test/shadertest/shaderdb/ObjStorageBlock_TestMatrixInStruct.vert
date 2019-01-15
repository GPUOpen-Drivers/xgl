#version 450

struct PosAttrib
{
    vec4 position;
    int dummy[4];
    vec4 attrib;
};

layout(std140, binding = 0) buffer Buffer
{
    mat4      mvp;
    PosAttrib vertData;
} buf;

void main()
{
    PosAttrib pa = buf.vertData;
    gl_Position = pa.position;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
