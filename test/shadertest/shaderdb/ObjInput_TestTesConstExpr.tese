#version 450 core

layout(triangles) in;

layout(location = 0) out float outColor;

float TessCoord()
{
    float f = gl_TessCoord.x;
    f += gl_TessCoord[1] * 0.5;
    f -= gl_TessCoord.z;
    return f;
}

void main(void)
{
    outColor = TessCoord();
}

