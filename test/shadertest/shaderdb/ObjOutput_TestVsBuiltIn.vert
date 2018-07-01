#version 450 core

#extension GL_ARB_shader_viewport_layer_array: enable

void main()
{
    gl_Position = vec4(1.0);
    gl_PointSize = 5.0;
    gl_ClipDistance[3] = 1.5;
    gl_CullDistance[1] = 2.0;

    gl_Layer = 2;
    gl_ViewportIndex = 1;
}