#version 450

layout(location = 0) flat in vec4 color[4];
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform Uniforms
{
    int index;
};

void main()
{
    float f1 = color[index][index + 1];
    fragColor[index] = f1;
    fragColor[1] = 0.4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: add i32 %{{[0-9]*}}, 1
; SHADERTEST: getelementptr [4 x <4 x float>], [4 x <4 x float>] addrspace({{.*}})* %{{.*}}, i32 0, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}
; SHADERTEST: load i32, i32 addrspace({{.*}})* %{{[0-9]*}}

; SHADERTEST: icmp eq i32 %{{[0-9]*}}, 1
; SHADERTEST: select i1 %{{[0-9]*}}, float addrspace({{.*}})* %{{.*}}, float addrspace({{.*}})* %{{.*}}
; SHADERTEST: icmp eq i32 %{{[0-9]*}}, 2
; SHADERTEST: select i1 %{{[0-9]*}}, float addrspace({{.*}})* %{{.*}}, float addrspace({{.*}})* %{{[0-9]*}}
; SHADERTEST: icmp eq i32 %{{[0-9]*}}, 3
; SHADERTEST: select i1 %{{[0-9]*}}, float addrspace({{.*}})* %{{.*}}, float addrspace({{.*}})* %{{[0-9]*}}
; SHADERTEST: store i32 %{{[0-9]*}}, i32 addrspace({{.*}})* %{{[0-9]*}}
; SHADERTEST: store float 0x3FD99999A0000000, float addrspace({{.*}})* %{{.*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
