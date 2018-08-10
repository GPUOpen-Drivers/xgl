#version 450 core

#extension GL_ARB_shader_viewport_layer_array: enable

layout(triangles) in;

layout(location = 1) in vec4 inColor[];

void main()
{
    gl_Position = inColor[1];
    gl_PointSize = inColor[2].x;
    gl_ClipDistance[2] = inColor[0].y;
    gl_CullDistance[3] = inColor[0].z;

    gl_Layer = 2;
    gl_ViewportIndex = 1;
}