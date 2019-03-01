#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1, d1_2;
    dvec3 d3_1, d3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = min(d1_1, d1_2);

    dvec3 d3_0 = min(d3_1, d3_2);

    fragColor = (d1_0 != d3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z4fmindd(double %{{.*}}, double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x double> @_Z4fminDv3_dDv3_d(<3 x double> %{{.*}}, <3 x double> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call double @llvm.minnum.f64(double %{{.*}}, double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call double @llvm.minnum.f64(double %{{.*}}, double %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
