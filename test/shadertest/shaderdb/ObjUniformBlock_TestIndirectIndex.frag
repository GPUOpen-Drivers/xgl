#version 450

layout(set = 0, binding = 1) uniform BB
{
   vec4 m1;
   vec4 m2[10];
} b[4];

layout(binding = 0) uniform Uniforms
{
    int i;
    int j;
};

layout(location = 0) out vec4 o1;

void main()
{
    o1 = b[0].m1 + b[1].m2[i] + b[j].m2[3]+ b[i].m2[j];
}