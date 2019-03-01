#version 450

layout(binding = 0) uniform Uniforms
{
    uvec2 u2;
    double d1_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = packDouble2x32(u2);

    fragColor = (d1_0 != d1_1) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z14packDouble2x32Dv2_i(<2 x i32> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
