#version 450 core

layout(set = 0, binding = 0) uniform sampler2DShadow samp;
layout(location = 0) in vec3 inUV;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(textureGradOffset(samp, inUV, vec2(0.1, 0.2), vec2(0.3, 0.4), ivec2(2, 3)));
}
