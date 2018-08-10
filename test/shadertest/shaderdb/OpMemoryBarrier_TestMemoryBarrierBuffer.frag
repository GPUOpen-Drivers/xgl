#version 450

layout(binding = 0, std430) buffer Buffers
{
    float f1;
    vec4  f4;
};

void main()
{
    f1 *= 2;
    memoryBarrierBuffer();
    f4 = vec4(f1);
}
