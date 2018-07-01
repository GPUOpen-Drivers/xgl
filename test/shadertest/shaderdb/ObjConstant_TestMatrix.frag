#version 450

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const vec3 f3 = vec3(0.0);

    const mat4 m4 = mat4(0.0);

    const bool b1[3] = { false, false, false };

    fragColor = b1[i] ? vec4(f3[i]) : m4[i];
}

