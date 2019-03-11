#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0, f3_1;
    vec4 f4_0, f4_1;
    vec2 f2_0, f2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    float f1 = dot(f3_0, f3_1);
    f1 += dot(f4_0, f4_1);
    f1 += dot(f2_0, f2_1);

    fragColor = (f1 > 0.0) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} float @_Z3dotDv3_fDv3_f(<3 x float> %{{[0-9]+}}, <3 x float> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} float @_Z3dotDv4_fDv4_f(<4 x float> %{{[0-9]+}}, <4 x float> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} float @_Z3dotDv2_fDv2_f(<2 x float> %{{[0-9]+}}, <2 x float> %{{[0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
