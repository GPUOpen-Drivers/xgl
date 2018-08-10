#version 450

layout(location = 0) in vec3 data;

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1 = float[6](data[0], data[1], data[2], -data[2], -data[1], -data[0])[3];
    fragColor = vec4(f1);
}