#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec2 colorIn2;
layout(location = 2) in vec2 colorIn3;

layout(location = 0) out vec4 color;

struct AA
{
   mat4 bb;
};

layout(binding=2) uniform BB
{
  mat4 m1;
  mat4 m2;
};

void main()
{
    mat4 cm = m1*m2;
    color = cm[0] + cm[1] + cm[2] + cm[3];
}