#version 450 core

#pragma use_storage_buffer

layout(std430, binding = 0) buffer Buffers
{
    vec4  f4;
    float f1[];
};

layout(location = 0) out vec4 f;

void main()
{
    f = f4 + vec4(f1.length());
}