#version 450


layout(xfb_buffer = 0, xfb_offset = 16, xfb_stride = 32, location = 0) out vec4 output1;

void main()
{
    gl_Position = vec4(1);
    output1 = vec4(2);
}
