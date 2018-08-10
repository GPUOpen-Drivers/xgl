#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat4x2 m4x2 = mat4x2(0.0);
    m4x2 *= 0.5;

    fragColor = (m4x2[0] != m4x2[1]) ? vec4(1.0) : color;
}