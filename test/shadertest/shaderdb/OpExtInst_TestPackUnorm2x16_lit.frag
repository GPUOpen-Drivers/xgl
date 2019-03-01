#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1 = packUnorm2x16(f2);

    fragColor = (u1 != 5) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} i32 @_Z13packUnorm2x16Dv2_f(<2 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 6.553500e+04
; SHADERTEST: %{{[0-9]*}} = fptoui float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = shl i32 %{{.*}}, 16
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 6.553500e+04
; SHADERTEST: %{{[0-9]*}} = fptoui float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = or i32 %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
