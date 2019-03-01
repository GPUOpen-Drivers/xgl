#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 ua = ivec4(a0);
    color = vec4(sign(ua));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x i32> @_Z5ssignDv4_i(<4 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
