#version 450 core

layout(location = 0) in vec4  f4;
layout(location = 1) in int   i1;
layout(location = 2) in uvec2 u2;

void main()
{
    vec4 f = f4;
    f += vec4(i1);
    f += vec4(u2, u2);

    gl_Position = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <2 x i32> @llpc.input.import.generic.v2i32{{.*}}
; SHADERTEST: call i32 @llpc.input.import.generic{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-COUNT-3: call <4 x i32> @llvm.amdgcn.struct.tbuffer.load.v4i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
