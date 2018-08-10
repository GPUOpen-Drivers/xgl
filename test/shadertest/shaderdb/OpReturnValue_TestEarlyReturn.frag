#version 450

layout(binding = 0) uniform Uniforms
{
    bool cond;
};

layout(location = 0) out vec4 fragColor;

vec4 func()
{
    vec4 f4 = vec4(0.0);

    if (cond)
    {
        return f4;
    }

    f4 = vec4(2.0);
    f4 += vec4(0.35);

    return f4;
}

void main()
{
    fragColor = func();
}