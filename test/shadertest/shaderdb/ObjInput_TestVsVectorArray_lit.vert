#version 450 core

layout(location = 2) in vec4  f4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    gl_Position = f4[i];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-2: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.struct.tbuffer.load.v4i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
