#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_1;
    ivec3 i3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_0 = sign(i1_1);

    ivec3 i3_0 = sign(i3_1);

    fragColor = ((i1_0 != i3_0.x)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} i32 @_Z5ssigni(i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x i32> @_Z5ssignDv3_i(<3 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, 1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: %{{[0-9]*}} = icmp sgt i32 %{{.*}}, -1
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 -1
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
