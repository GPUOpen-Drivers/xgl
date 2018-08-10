#version 450

const bool g_envCond = bool(false);

vec2 foo(vec2 coord, const bool cond)
{
    if (cond)
    {
        return vec2(coord.x, 1.0 - coord.y);
    }
    else
    {
        return coord;
    }
}

layout(location = 0) in vec2 envCoord;
layout(location = 0) out vec2 color;

void main()
{
    color = foo(envCoord, g_envCond);
}
