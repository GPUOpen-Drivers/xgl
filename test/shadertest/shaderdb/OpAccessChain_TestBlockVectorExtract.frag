#version 450

layout(set = 0, binding = 0) uniform DATA0
{
    vec3   f3;
    mat2x3 m2x3;
} data0;

layout(set = 1, binding = 1) buffer DATA1
{
    dvec4  d4;
    dmat4  dm4;
} data1;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

void main()
{
    float f1 = data0.f3[1];
    f1 += data0.m2x3[1][index];
    f1 += data0.m2x3[index][1];

    double d1 = data1.d4[index];
    d1 += data1.dm4[2][3];
    d1 += data1.dm4[index][index + 1];

    fragColor = vec4(float(d1), f1, f1, f1);
}