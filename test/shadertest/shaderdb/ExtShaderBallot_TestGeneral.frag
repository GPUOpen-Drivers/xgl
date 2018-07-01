#version 450 core

#extension GL_ARB_shader_ballot: enable
#extension GL_ARB_gpu_shader_int64: enable

layout(location = 0) in flat int i1;
layout(location = 1) in float f1;

layout(location = 0) out float f;

void main(void)
{
    uint64_t u64 = 0;

    u64 += gl_SubGroupInvocationARB;
    u64 += gl_SubGroupSizeARB;
    u64 += gl_SubGroupEqMaskARB;
    u64 += gl_SubGroupGeMaskARB;
    u64 += gl_SubGroupGtMaskARB;
    u64 += gl_SubGroupLeMaskARB;
    u64 += gl_SubGroupLtMaskARB;

    u64 += ballotARB(true);

    f = float(u64);

    f += float(readInvocationARB(i1, 1));
    f += float(readFirstInvocationARB(i1));

    f += readInvocationARB(f1, 2);
    f += readFirstInvocationARB(f1);
}