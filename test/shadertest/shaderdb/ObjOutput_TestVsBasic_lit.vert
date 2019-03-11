#version 450 core

layout(location = 0) out vec3  f3;
layout(location = 1) out int   i1;
layout(location = 2) out uvec2 u2;

layout(location = 0) in vec4 f4;

void main()
{
    vec4 f = f4;

    f3 = vec3(f.x);
    i1 = int(f.y);
    u2 = uvec2(f.z);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2i32
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
