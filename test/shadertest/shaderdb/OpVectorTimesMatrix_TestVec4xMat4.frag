#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;

struct AA
{
   mat4 bb;
};

layout(binding=2) uniform BB
{

 mat3x4 m2;

};

void main()
{
    vec3 newm =  colorIn1 * m2;
    color.xyz = newm;
}