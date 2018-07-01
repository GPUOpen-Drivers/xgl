#version 450 core

#extension GL_ARB_shader_draw_parameters: enable

layout(binding = 0) uniform Block
{
    vec4 pos[2][4];
} block;

void main()
{
    if ((gl_BaseVertexARB > 0) || (gl_BaseInstanceARB > 0))
        gl_Position = block.pos[gl_VertexIndex % 2][gl_DrawIDARB % 4];
    else
        gl_Position = block.pos[gl_InstanceIndex % 2][gl_DrawIDARB % 4];
}