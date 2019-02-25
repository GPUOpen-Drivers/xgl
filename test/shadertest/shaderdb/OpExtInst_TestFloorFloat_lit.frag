#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = floor(f1_1);

    vec3 f3_0 = floor(f3_1);

    fragColor = ((f1_0 != f3_0.x)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call spir_func float @_Z5floorf(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call spir_func <3 x float> @_Z5floorDv3_f(<3 x float> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = tail call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = tail call float @llvm.floor.f32(float %.bitcast.i{{[0-9]*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
