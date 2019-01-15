#version 450 core

#extension GL_AMD_gcn_shader: enable

#extension GL_ARB_gpu_shader_int64: enable

layout(location = 0) in vec3 f3;
layout(location = 1) out vec4 f4;

void main()
{
    float f1 = cubeFaceIndexAMD(f3);
    vec2 f2 = cubeFaceCoordAMD(f3);

    uint64_t u64 = timeAMD();

    f4.x = f1;
    f4.yz = f2;
    f4.w = float(u64);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
