#version 450 core
layout(set = 0, binding = 0) uniform sampler samp;
layout(binding = 1) uniform texture2D  image;
layout(location = 0) out vec4 oColor;

void main()
{
    ivec2 s = textureSize(sampler2D(image, samp), 0);
    oColor = vec4(s.x, s.y, 0, 1);
}

