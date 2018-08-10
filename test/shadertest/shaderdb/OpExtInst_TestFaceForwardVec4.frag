#version 450 core


layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 2) in vec4 c0;
layout(location = 0) out vec4 color;

void main()
{

    color = vec4(faceforward(a0, b0, c0));
}

