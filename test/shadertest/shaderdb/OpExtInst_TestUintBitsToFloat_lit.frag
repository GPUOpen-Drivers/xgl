#version 450

layout(binding = 0) uniform Uniforms
{
    uvec3 u3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3 = uintBitsToFloat(u3);

    fragColor = (f3.x != f3.y) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = bitcast <3 x i32> %{{.*}} to <3 x float>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
