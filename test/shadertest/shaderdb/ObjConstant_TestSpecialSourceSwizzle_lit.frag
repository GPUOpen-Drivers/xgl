#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    const float f1 = 0.2;
    const vec4 f4 = vec4(0.1, 0.9, 0.2, 1.0);
    fragColor[0] = f1;
    fragColor += f4;

    const double d1 = 0.25;
    const dvec4 d4_0 = dvec4(0.1, 0.8, 0.25, 0.2);
    dvec4 d4_1 = d4_0;
    d4_1[1] = d1;
    fragColor += vec4(d4_1);
}
 
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic.i32.i32.v4f32(i32 0, i32 0, <4 x float> <float 0x3FD99999A0000000, float 0x7FF8000000000000, float 0x7FF8000000000000, float 0x7FF8000000000000>)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
