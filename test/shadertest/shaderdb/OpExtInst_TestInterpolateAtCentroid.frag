#version 450

layout(location = 0) in centroid float f1_1;
layout(location = 1) in vec4 f4_1;

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = interpolateAtCentroid(f1_1);

    vec4 f4_0 = interpolateAtCentroid(f4_1);

    fragColor = (f4_0.y == f1_0) ? vec4(0.0) : vec4(1.0);
}