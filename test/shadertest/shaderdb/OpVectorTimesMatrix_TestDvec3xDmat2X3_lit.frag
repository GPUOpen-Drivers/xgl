#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3;
    dmat2x3 dm2x3;
    dvec2 d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_0 = d3 * dm2x3;

    fragColor = (d2_0 != d2_1) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = call {{.*}} <2 x double> @_Z17VectorTimesMatrixDv3_dDv2_Dv3_d(<3 x double> %{{.*}}, [2 x <3 x double>] %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
