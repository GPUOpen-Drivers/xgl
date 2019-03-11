#version 450

layout(location = 2) in vec4 f4;
layout(location = 5) flat in int i1;
layout(location = 7) flat in uvec2 u2;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = f4;
    f += vec4(i1);
    f += vec4(u2, u2);

    fragColor = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <2 x i32> @llpc.input.import.generic.v2i32{{.*}}
; SHADERTEST: call i32 @llpc.input.import.generic{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
