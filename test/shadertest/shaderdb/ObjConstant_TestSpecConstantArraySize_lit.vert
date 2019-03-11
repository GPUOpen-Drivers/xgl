#version 450

layout(constant_id = 1) const int SIZE = 6;

layout(set = 0, binding = 0) uniform UBO
{
    vec4 fa[SIZE];
};

layout(location = 0) in vec4 input0;

void main()
{
    gl_Position = input0 + fa[gl_VertexIndex % fa.length()];
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 @_Z4smodii(i32 %{{[0-9]*}}, i32 6)
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: srem i32 %{{[0-9]*}}, 6
; SHADERTEST: icmp slt i32 %{{[0-9]*}}, 0
; SHADERTEST: icmp ne i32 %{{[0-9]*}}, 0
; SHADERTEST: and i1 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: shl nsw i32 %{{[0-9]*}}, 4
; SHADERTEST: add nsw i32 %{{[0-9]*}}, 96
; SHADERTEST: select i1 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
