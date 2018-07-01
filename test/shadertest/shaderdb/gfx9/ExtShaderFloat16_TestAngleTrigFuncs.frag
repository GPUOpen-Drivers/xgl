#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) buffer Buffers
{
    vec3 fv3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3_1 = f16vec3(fv3);
    f16vec3 f16v3_2 = f16vec3(fv3);

    f16v3_1 = radians(f16v3_1);
    f16v3_1 = degrees(f16v3_1);
    f16v3_1 = sin(f16v3_1);
    f16v3_1 = cos(f16v3_1);
    f16v3_1 = tan(f16v3_1);
    f16v3_1 = asin(f16v3_1);
    f16v3_1 = acos(f16v3_1);
    f16v3_1 = atan(f16v3_1, f16v3_2);
    f16v3_1 = atan(f16v3_1);
    f16v3_1 = sinh(f16v3_1);
    f16v3_1 = cosh(f16v3_1);
    f16v3_1 = tanh(f16v3_1);
    f16v3_1 = asinh(f16v3_1);
    f16v3_1 = acosh(f16v3_1);
    f16v3_1 = atanh(f16v3_1);

    fragColor = f16v3_1;
}