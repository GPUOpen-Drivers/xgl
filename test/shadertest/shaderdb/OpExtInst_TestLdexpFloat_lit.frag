#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    int i1;

    vec3 f3_1;
    ivec3 i3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = ldexp(f1_1, i1);

    vec3 f3_0 = ldexp(f3_1, i3);

    fragColor = ((f3_0.x != f1_0) || (i3.x != i1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z5ldexpfi(float %{{.*}}, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z5ldexpDv3_fDv3_i(<3 x float> %{{.*}}, <3 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.ldexp.f32(float %{{.*}}, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.ldexp.f32(float %{{.*}}, i32 %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
