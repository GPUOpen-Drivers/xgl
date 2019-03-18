#version 450

layout (std140, binding = 1) uniform BB1
{
    vec4 c1;
};

layout (std140, binding = 1) uniform BB2
{
    vec4 c2;
};

layout (binding = 2) uniform sampler2D s1;
layout (binding = 2) uniform sampler2D s2;

layout (rgba8, binding = 3) uniform image2D i1;
layout (rgba8, binding = 3) uniform image2D i2;

layout (std430, binding = 4) buffer SB1
{
    vec4 sb1;
    vec4 sb2;
};

layout (std430, binding = 4) buffer SB2
{
    vec4 sb_1;
    vec4 sb_2;
};

layout (location = 0) out vec4 frag_color;
void main()
{
    frag_color = c1 + c2;
    sb1 = texture(s1, vec2(0)) + texture(s2, vec2(0));
    sb_2 = imageLoad(i1, ivec2(0)) + imageLoad(i2, ivec2(0));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-2: call {{.*}} <4 x float> @spirv.image.sample.f32.2D
; SHADERTEST-COUNT-2: call {{.*}} <4 x float> @spirv.image.read.f32.2D

; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}{{.*}}(i32 0, i32 1,{{.*}}
; SHADERTEST-COUNT-2: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 0,{{.*}}
; SHADERTEST-COUNT-2: call <4 x float> @llpc.image.sample.f32.2D.dimaware
; SHADERTEST-COUNT-2: call <4 x float> @llpc.image.read.f32.2D.dimaware


; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
