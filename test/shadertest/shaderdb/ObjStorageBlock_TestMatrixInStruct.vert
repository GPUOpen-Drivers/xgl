#version 450

struct PosAttrib
{
    vec4 position;
    int dummy[4];
    vec4 attrib;
};

layout(std140, binding = 0) buffer Buffer
{
    mat4      mvp;
    PosAttrib vertData;
} buf;

void main()
{
    PosAttrib pa = buf.vertData;
    gl_Position = pa.position;
}