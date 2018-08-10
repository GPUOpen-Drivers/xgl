#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main ()
{
    vec4 color = vec4(0.5);

    float  f1 = float(d1);

    if (f1 > 0.0f)
    {
        color = vec4(1.0);
    }

    fragColor = vec4(1.0);
}