#version 450 core

#extension GL_AMD_shader_ballot: enable

layout(location = 0) out vec4 fv4Out;

layout(location = 0) in flat ivec2 iv2In;
layout(location = 1) in flat uvec3 uv3In;
layout(location = 2) in flat vec4  fv4In;

void main()
{
    vec4 fv4 = vec4(0.0);

    fv4.xy  += swizzleInvocationsAMD(iv2In, uvec4(0, 1, 2, 3));
    fv4.xyz += swizzleInvocationsAMD(uv3In, uvec4(3, 2, 1, 0));
    fv4     += swizzleInvocationsAMD(fv4In, uvec4(1, 0, 3, 2));

    fv4.xy  += swizzleInvocationsMaskedAMD(iv2In, uvec3(16, 2, 8));
    fv4.xyz += swizzleInvocationsMaskedAMD(uv3In, uvec3(14, 28, 7));
    fv4     += swizzleInvocationsMaskedAMD(fv4In, uvec3(3, 15, 6));

    fv4Out = fv4;
}