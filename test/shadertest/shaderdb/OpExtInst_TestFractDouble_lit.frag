#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    dvec3 d3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = fract(d1_1);

    dvec3 d3_0 = fract(d3_1);

    fragColor = ((d1_0 != d3_0.x)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z5fractd(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x double> @_Z5fractDv3_d(<3 x double> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.fract.f64(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call double @llvm.amdgcn.fract.f64(double %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
