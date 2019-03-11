#version 450

struct S
{
    int  i1;
    vec3 f3;
    mat4 m4;
};

layout(location = 4) flat in S s;

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = vec4(s.i1);
    f += vec4(s.f3, 1.0);
    f += s.m4[i];

    fragColor = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-1: call i32 @llpc.input.import.generic{{.*}}
; SHADERTEST-COUNT-1: call <3 x float> @llpc.input.import.generic.v3f32{{.*}}
; SHADERTEST-COUNT-4: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-COUNT-20: call float @llvm.amdgcn.interp.mov
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
