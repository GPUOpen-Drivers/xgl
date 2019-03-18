#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4_1;
    float f1;
    float f2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4_0 = vec4(f2);
    f4_0 = mod(f4_0, f4_1);

    f4_0 += mod(f4_0, f1);

    fragColor = (f4_0.y > 0.0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-2: call spir_func <4 x float> @_Z4fmodDv4_fDv4_f
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST: fdiv float
; SHADERTEST: call float @llvm.floor.f32
; SHADERTEST: fsub float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
