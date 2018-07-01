#version 450

layout(set = 0, binding = 0, row_major) uniform DATA0
{
    dvec3   d3;
    dmat2x3 dm2x3;
} data0;

layout(set = 1, binding = 1, row_major) buffer DATA1
{
    vec4  f4;
    mat4  m4;
} data1;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

void main()
{
    double d1 = data0.d3[1];
    d1 += data0.d3[index];
    d1 += data0.dm2x3[1][1];
    d1 += data0.dm2x3[1][index];
    d1 += data0.dm2x3[index][1];
    d1 += data0.dm2x3[index + 1][index];

    float f1 = data1.f4[index];
    f1 += data1.f4[2];
    f1 += data1.m4[2][3];
    f1 += data1.m4[index][2];
    f1 += data1.m4[3][index];
    f1 += data1.m4[index][index + 1];

    fragColor = vec4(float(d1), f1, f1, f1);
}