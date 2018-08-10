#version 450

layout(location = 1) smooth                 in float f1;
layout(location = 2) flat                   in float f2;
layout(location = 3) noperspective          in float f3;
layout(location = 4) smooth centroid        in float f4;
layout(location = 5) smooth sample          in float f5;
layout(location = 6) noperspective centroid in float f6;
layout(location = 7) noperspective sample   in float f7;

layout(location = 0) out vec4 fragColor;

void main()
{
    float f = f1;
    f += f2;
    f += f3;
    f += f4;
    f += f5;
    f += f6;
    f += f7;

    fragColor = vec4(f);
}