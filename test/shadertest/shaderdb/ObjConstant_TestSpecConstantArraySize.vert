#version 450

layout(constant_id = 1) const int SIZE = 6;

layout(set = 0, binding = 0) uniform UBO
{
    vec4 fa[SIZE];
};

layout(location = 0) in vec4 input0;

void main()
{
    gl_Position = input0 + fa[gl_VertexIndex % fa.length()];
}
