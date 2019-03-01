#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    dvec4 d4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = normalize(d1_1);

    dvec4 d4_0 = normalize(d4_1);

    fragColor = (d1_1 != d4_1.x) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z9normalized(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x double> @_Z9normalizeDv4_d(<4 x double> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
