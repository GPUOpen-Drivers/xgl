#version 450

layout(location= 0) in vec4 input1;

layout(location = 0) out vec4 output1;

const float carry[4] = {1, 2, 3, 4};

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    output1 = input1 + vec4(carry[3], 5, carry[i], float[4](7, 8, 9, 0)[i + 1]);
}
