#version 450 core

layout(location = 0) in flat int i0;
layout(location = 1) in float i1;

layout(location = 0) out int o0;
layout(location = 1) out float o1;

void main()
{
    o0 = i0;
    o1 = i1;
}