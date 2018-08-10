#version 450

struct str
{
    float f;
};

layout(set = 0, binding = 0, std140) uniform BB
{
   layout(offset = 128) vec4 m1;
   layout(offset = 256) str m2;
   layout(offset = 512) vec2 m3;
};

layout(location = 0) out vec4 o1;
layout(location = 1) out float o2;
layout(location = 2) out vec2 o3;

void main()
{
    o1 = m1;
    o2 = m2.f;
    o3 = m3;
}
