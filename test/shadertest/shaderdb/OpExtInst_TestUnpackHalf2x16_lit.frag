#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 f2 = unpackHalf2x16(u1);

    fragColor = (f2.x != f2.y) ? vec4(f2, 0, 0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <2 x float> @_Z14unpackHalf2x16i(i32 %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = bitcast <2 x i32> %{{.*}} to i64
; SHADERTEST: %{{[0-9]*}} = trunc i32 %{{.*}} to i16
; SHADERTEST: %{{[0-9]*}} = lshr i32 %{{.*}}, 16
; SHADERTEST: %{{[0-9]*}} = trunc i32 %{{.*}} to i16
; SHADERTEST: %{{[0-9]*}} = bitcast i16 %{{.*}} to half
; SHADERTEST: %{{[0-9]*}} = bitcast i16 %{{.*}} to half
; SHADERTEST: %{{[0-9]*}} = fpext half %{{.*}} to float
; SHADERTEST: %{{[0-9]*}} = fpext half %{{.*}} to float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
