#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) buffer Buffers
{
    vec3  fv3;
    bvec3 bv3;
    ivec3 iv3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3_1 = f16vec3(fv3);
    f16vec3 f16v3_2 = f16vec3(fv3);
    float16_t f16 = float16_t(fv3.x);

    bvec2 bv2 = bvec2(false);

    f16v3_1 = abs(f16v3_1);
    f16v3_1 = sign(f16v3_1);
    f16v3_1 = floor(f16v3_1);
    f16v3_1 = trunc(f16v3_1);
    f16v3_1 = round(f16v3_1);
    f16v3_1 = roundEven(f16v3_1);
    f16v3_1 = ceil(f16v3_1);
    f16v3_1 = fract(f16v3_1);
    f16v3_1 = mod(f16v3_1, f16v3_2);
    f16v3_1 = modf(f16v3_1, f16v3_2);
    f16v3_1 = min(f16v3_1, f16v3_2);
    f16v3_1 = clamp(f16v3_1, f16v3_2.x, f16v3_2.y);
    f16v3_1 = mix(f16v3_1, f16v3_2, f16);
    f16v3_1 = mix(f16v3_1, f16v3_2, bv3);
    f16v3_1 = step(f16v3_1, f16v3_2);
    f16v3_1 = smoothstep(f16v3_2.x, f16v3_2.y, f16v3_1);
    bv2.x   = isnan(f16v3_1).x;
    bv2.y   = isinf(f16v3_2).x;
    f16v3_1 = fma(f16v3_1, f16v3_2, f16vec3(f16));
    f16v3_1 = frexp(f16v3_1, iv3);
    f16v3_1 = ldexp(f16v3_1, iv3);
    f16v3_1 = max(f16v3_1, f16v3_2);

    fragColor = any(bv2) ? f16v3_1 : f16v3_2;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{.*}} = call half @llvm.fabs.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call half @llvm.floor.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call half @llvm.trunc.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call half @llvm.rint.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call half @llvm.ceil.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call {{.*}} <3 x half> @_Z5fractDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{.*}} = call half @_Z4fdivDhDh(half {{.*}}, half %{{.*}})
; SHADERTEST: %{{.*}} = call half @llvm.floor.f16(half %{{[0-9]*}})
; SHADERTEST: %{{.*}} = call half @llvm.minnum.f16(half %{{[0-9]*}}, half %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z10smoothStepDv3_DhDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{.*}} = call {{.*}} <3 x i1> @_Z5isnanDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{.*}} = call {{.*}} <3 x i1> @_Z5isinfDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST-COUNT-3: %{{.*}} = call half @llvm.fmuladd.f16(half %{{[0-9]*}}, half %{{[0-9]*}}, half %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call { <3 x half>, <3 x i16> } @_Z11frexpStructDv3_DhDv3_s(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5ldexpDv3_DhDv3_i(<3 x half> %{{.*}}, <3 x i32> %{{.*}})
; SHADERTEST: %{{.*}} = call half @llvm.maxnum.f16(half %{{[0-9]*}}, half %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
