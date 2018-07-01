#version 450

layout(std430, row_major, set = 0, binding = 0) buffer BufferObject
{
    mat4 m4;
};

layout(location = 0) out vec4 output0;

void main()
{
    m4[0] = vec4(1);
    output0 = m4[0];
}
