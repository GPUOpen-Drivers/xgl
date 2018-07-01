#version 450 core

layout(location = 0) out int i1;

void main()
{
    i1 = gl_VertexIndex + gl_InstanceIndex;
}