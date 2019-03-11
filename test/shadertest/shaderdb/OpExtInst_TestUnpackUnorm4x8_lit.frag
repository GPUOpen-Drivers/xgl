#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = unpackUnorm4x8(u1);

    fragColor = (f4.x != f4.y) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z14unpackUnorm4x8i(i32 %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = and i32 %{{.*}}, 255
; SHADERTEST: %{{[0-9]*}} = uitofp i32 %{{.*}} to float
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 0x3F70101020000000
; SHADERTEST: %{{[0-9]*}} = lshr i32 %{{.*}}, 8
; SHADERTEST: %{{[0-9]*}} = and i32 %{{.*}}, 255
; SHADERTEST: %{{[0-9]*}} = uitofp i32 %{{.*}} to float
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 0x3F70101020000000
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
