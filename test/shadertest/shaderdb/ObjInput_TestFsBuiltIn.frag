#version 450

layout(location = 0) out vec4 f4;
#extension GL_EXT_multiview : enable

void main()
{
    vec4 f = vec4(0.0);

    f.x   += float(gl_SampleMaskIn[0]);
    f     += gl_FragCoord;
    f.x   += float(gl_FrontFacing);
    f.x   += gl_ClipDistance[3];
    f.x   += gl_CullDistance[2];
    f.xy  += gl_PointCoord;
    f.x   += float(gl_PrimitiveID);
    f.x   += float(gl_SampleID);
    f.x   += float(gl_SampleMaskIn[0]);
    f.x   += float(gl_Layer);
    f.x   += float(gl_ViewportIndex);
    f.x   += float(gl_HelperInvocation);
    f.x   += float(gl_ViewIndex);

    f4 = f;
}