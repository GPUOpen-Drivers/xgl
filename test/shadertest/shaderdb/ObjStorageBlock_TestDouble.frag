#version 450

layout(std430, column_major, set = 0, binding = 0) buffer BufferObject
{
    uint ui;
    double d;
    dvec4 dv4;
    dmat2x4 dm2x4;
};

layout(location = 0) out vec4 output0;

void main()
{
    dvec4 dv4temp = dv4;
    dv4temp.x = d;
    d = dv4temp.y;
    output0 = vec4(dv4temp);
    dv4temp = dm2x4[0];
    dm2x4[1] = dv4temp;
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
