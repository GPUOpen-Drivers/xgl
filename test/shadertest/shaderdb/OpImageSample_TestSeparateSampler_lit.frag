#version 450

layout (set = 0, binding = 0) uniform texture2D t0;
layout (set = 0, binding = 1) uniform sampler s1;
layout (set = 0, binding = 1) uniform texture2D t1;
layout (location = 0) in vec4 texcoord;
layout (location = 0) out vec4 uFragColor;
void main() {
   uFragColor = texture(sampler2D(t0, s1), texcoord.xy) + texture(sampler2D(t1, s1), texcoord.xy);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.2d.v4f32.f32
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.2d.v4f32.f32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
