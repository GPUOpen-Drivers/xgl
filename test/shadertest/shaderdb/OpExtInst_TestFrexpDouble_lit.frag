#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    dvec3 d3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1;
    double d1_0 = frexp(d1_1, i1);

    ivec3 i3;
    dvec3 d3_0 = frexp(d3_1, i3);

    fragColor = ((d3_0.x != d1_0) || (i3.x != i1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { double, i32 } @_Z11frexpStructd(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { <3 x double>, <3 x i32> } @_Z11frexpStructDv3_d(<3 x double> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.frexp.mant.f64(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.frexp.mant.f64(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f64(double %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
