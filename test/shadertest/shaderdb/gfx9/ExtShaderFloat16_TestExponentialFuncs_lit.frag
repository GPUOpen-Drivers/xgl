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
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-3: %{{[0-9]*}} = call {{.*}} half @_Z3powDhDh(half %{{.*}}, half {{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3powDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3expDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3logDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4exp2Dv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4log2Dv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4sqrtDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z11inverseSqrtDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call half @llvm.log2.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llvm.exp2.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llvm.sqrt.f16(half %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
