#version 450 core

layout(vertices = 3) out;

layout(location = 0) patch out vec3 outColor;

void PatchOut(float f)
{
    outColor.x = 0.7;
    outColor[1] = f;
    outColor.z = outColor.y;
}

void main(void)
{
    PatchOut(4.5);
}