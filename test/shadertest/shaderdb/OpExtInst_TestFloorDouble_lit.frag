#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    dvec3 d3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = floor(d1_1);

    dvec3 d3_0 = floor(d3_1);

    fragColor = ((d1_0 != d3_0.x)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call spir_func double @_Z5floord(double %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call spir_func <3 x double> @_Z5floorDv3_d(<3 x double> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call double @llvm.floor.f64(double %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call double @llvm.floor.f64(double %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = tail call double @llvm.floor.f64(double %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = tail call double @llvm.floor.f64(double %.i{{[0-9]*}}.upto1.extract)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
