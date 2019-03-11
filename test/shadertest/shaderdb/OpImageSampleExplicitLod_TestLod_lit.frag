#version 450 core

layout(set = 0, binding = 0) uniform sampler2D samp;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = textureLod(samp, vec2(0, 0), 1);
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D.lod({{.*}}, <2 x float> zeroinitializer, float 1.000000e+00, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D.lod{{.*}}({{.*}}, <2 x float> zeroinitializer, float 1.000000e+00, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.l.2d.v4f32.f32({{.*}}, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00, {{.*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
