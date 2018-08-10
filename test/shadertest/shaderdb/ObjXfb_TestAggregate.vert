#version 450

layout(xfb_buffer = 0, xfb_offset = 0) out gl_PerVertex
{
    vec4 gl_Position;
};

layout(xfb_buffer = 1) out;
layout(location = 0) out OB
{
    layout(xfb_buffer = 1, xfb_offset = 16, xfb_stride = 96) out vec4 output1;
    layout(xfb_buffer = 1, xfb_offset = 32) out dvec3 output2;
};

layout(location = 0) in dvec3 input0;

void main()
{
    gl_Position = vec4(1);
    output1 = vec4(2);
    output2 = input0;
}
