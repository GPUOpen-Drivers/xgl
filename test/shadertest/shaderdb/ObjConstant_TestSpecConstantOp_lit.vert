#version 450

layout(constant_id = 200) const float f1  = 3.1415926;
layout(constant_id = 201) const int   i1  = -10;
layout(constant_id = 202) const uint  u1  = 100;

const int i1_1  = i1 + 2;
const uint u1_1 = u1 % 5; 

const ivec4 i4 = ivec4(20, 30, i1_1, i1_1);
const ivec2 i2 = i4.yx;

void main()
{
    vec4 pos = vec4(0.0);
    pos.y += float(i1_1);
    pos.z += float(u1_1);

    pos += vec4(i4);
    pos.xy += vec2(i2);

    gl_Position = pos;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fadd reassoc nnan arcp contract float %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd reassoc nnan arcp contract <4 x float> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd reassoc nnan arcp contract <2 x float> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.builtin.Position{{.*}}v4f32(i32 0, i32 0, <4 x float> <float 5.000000e+01, float 4.200000e+01, float -8.000000e+00, float -8.000000e+00>)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
