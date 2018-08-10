#version 450 core

#extension GL_ARB_shader_group_vote: enable

layout(binding = 0) uniform Uniforms
{
    int i1;
    float f1;
};

layout(location = 0) out vec2 f2;

void main(void)
{
    bool b1 = false;

    switch (i1)
    {
    case 0:
        b1 = anyInvocationARB(f1 > 5.0);
        break;
    case 1:
        b1 = allInvocationsARB(f1 < 4.0);
        break;
    case 2:
        b1 = allInvocationsEqualARB(f1 != 0.0);
        break;
    }

    f2 = b1 ? vec2(1.0) : vec2(0.3);
}