#version 450

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const vec3 f3 = vec3(0.0);

    const mat4 m4 = mat4(0.0);

    const bool b1[3] = { false, false, false };

    fragColor = b1[i] ? vec4(f3[i]) : m4[i];
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: @{{.*}} = {{.*}} addrspace(4) constant [4 x <4 x float>] zeroinitializer
; SHADERTEST: getelementptr [4 x <4 x float>], [4 x <4 x float>] addrspace(4)* @{{.*}}, i64 0, i64 %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
