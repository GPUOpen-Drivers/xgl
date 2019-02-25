#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = length(f1_1);

    f1_0 += length(f3);

    fragColor = (f1_0 > 0.0) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z6lengthf(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z6lengthDv3_f(<3 x float> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
