#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 fIn[];
layout(location = 0, xfb_buffer = 1, xfb_offset = 24, stream = 0) out vec3 fOut1;
layout(location = 2, xfb_buffer = 0, xfb_offset = 16, stream = 1) out vec2 fOut2;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        fOut1 = fIn[i].xyz;
        EmitStreamVertex(0);

        fOut2 = fIn[i].xy;
        EmitStreamVertex(1);
    }

    EndPrimitive();
}
