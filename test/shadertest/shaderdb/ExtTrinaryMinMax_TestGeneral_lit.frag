#version 450 core

#extension GL_AMD_shader_trinary_minmax: enable

layout(location = 0) in flat ivec2 ivIn0;
layout(location = 1) in flat ivec2 ivIn1;

layout(location = 2) in flat uvec3 uvIn0;
layout(location = 3) in flat uvec3 uvIn1;

layout(location = 4) in flat vec3 fvIn0;
layout(location = 5) in flat vec3 fvIn1;

layout(location = 1) out vec4 f4;

void main()
{
    ivec2 iv = ivec2(0);

    iv += min3(ivIn0, ivIn1, ivec2(1));
    iv += max3(ivIn0, ivIn1, ivec2(2));
    iv += mid3(ivIn0, ivIn1, ivec2(3));

    f4.xy += iv;

    uvec3 uv = uvec3(0);

    uv += min3(uvIn0, uvIn1, uvec3(4));
    uv += max3(uvIn0, uvIn1, uvec3(5));
    uv += mid3(uvIn0, uvIn1, uvec3(6));

    f4.xyz += uv;

    vec3 fv = vec3(0.0);

    fv += min3(fvIn0, fvIn1, vec3(7.0));
    fv += max3(fvIn0, fvIn1, vec3(8.0));
    fv += mid3(fvIn0, fvIn1, vec3(9.0));

    f4.xyz += fv;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v4f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
