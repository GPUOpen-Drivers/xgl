#version 450 core

#extension GL_ARB_shader_stencil_export: enable

layout(location = 0) in vec4 f4;

void main()
{
    gl_FragDepth = f4.x;
    gl_SampleMask[0] = 27;
    gl_FragStencilRefARB = 32;
}