#version 450

layout (location = 0) out vec4 fragColor;

void main()
{
    vec3 f3 = vec3(0.0);
    f3 = fwidth(f3);
    f3 = fwidthFine(f3);
    f3 = fwidthCoarse(f3);

    fragColor = (f3[0] == f3[1]) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} <3 x float> @_Z6FwidthDv3_f
; SHADERTEST: call {{.*}} <3 x float> @_Z10FwidthFineDv3_f
; SHADERTEST: call {{.*}} <3 x float> @_Z12FwidthCoarseDv3_f
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
