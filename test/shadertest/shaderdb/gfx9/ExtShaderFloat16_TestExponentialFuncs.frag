#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) buffer Buffers
{
    vec3  fv3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3_1 = f16vec3(fv3);
    f16vec3 f16v3_2 = f16vec3(fv3);
    float16_t f16 = f16v3_1.x;

    f16     = pow(f16, 0.0hf);
    f16     = pow(f16, -2.0hf);
    f16     = pow(f16, 3.0hf);
    f16v3_1 = pow(f16v3_1, f16v3_2);
    f16v3_1 = exp(f16v3_1);
    f16v3_1 = log(f16v3_1);
    f16v3_1 = exp2(f16v3_1);
    f16v3_1 = log2(f16v3_1);
    f16v3_1 = sqrt(f16v3_1);
    f16v3_1 = inversesqrt(f16v3_1);

    fragColor = f16v3_1 + vec3(f16);
}