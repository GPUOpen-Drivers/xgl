#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    int i1;

    dvec3 d3_1;
    ivec3 i3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = ldexp(d1_1, i1);

    dvec3 d3_0 = ldexp(d3_1, i3);

    fragColor = ((d3_0.x != d1_0) || (i3.x != i1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z5ldexpdi(double %{{.*}}, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x double> @_Z5ldexpDv3_dDv3_i(<3 x double> %{{.*}}, <3 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.ldexp.f64(double %{{.*}}, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.ldexp.f64(double %{{.*}}, i32 %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
