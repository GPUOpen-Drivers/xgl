#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    dvec3 d3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = length(d1_1);

    d1_0 += length(d3);

    fragColor = (d1_0 != d1_1) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z6lengthd(double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z6lengthDv3_d(<3 x double> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
