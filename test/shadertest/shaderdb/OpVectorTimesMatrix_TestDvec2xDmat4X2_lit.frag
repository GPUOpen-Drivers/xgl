#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2;
    dmat4x2 dm4x2;
    dvec4 d4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec4 d4_0 = d2 * dm4x2;

    fragColor = (d4_0 != d4_1) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = call {{.*}} <4 x double> @_Z17VectorTimesMatrixDv2_dDv4_Dv2_d(<2 x double> %{{.*}}, [4 x <2 x double>] %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
