#version 450 core
layout(set = 0, binding = 0) uniform sampler samp;
layout(set = 0, binding = 1) uniform texture2D tex2D;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = texture(sampler2D(tex2D, samp), vec2(0, 0));
}

