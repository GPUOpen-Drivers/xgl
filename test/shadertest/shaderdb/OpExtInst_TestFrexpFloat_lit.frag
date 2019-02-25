#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1;
    float f1_0 = frexp(f1_1, i1);

    ivec3 i3;
    vec3 f3_0 = frexp(f3_1, i3);

    fragColor = ((f3_0.x != f1_0) || (i3.x != i1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { float, i32 } @_Z11frexpStructf(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { <3 x float>, <3 x i32> } @_Z11frexpStructDv3_f(<3 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
