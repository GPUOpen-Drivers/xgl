#version 450

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0, std430) buffer Buffers
{
    vec3 fv3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec4 f16v4 = f16vec4(fv3, fv3.x);
    f16vec3 f16v3 = f16vec3(fv3);
    f16vec2 f16v2 = f16v3.yz;

    f16mat2x3 f16m2x3 = outerProduct(f16v3, f16v2);
    f16m2x3 = matrixCompMult(f16m2x3, f16m2x3);
    f16mat3x2 f16m3x2 = transpose(f16m2x3);

    f16mat2 f16m2 = f16mat2(f16v2, f16v2);
    f16mat3 f16m3 = f16mat3(f16v3, f16v3, f16v3);
    f16mat4 f16m4 = f16mat4(f16v4, f16v4, f16v4, f16v4);

    float16_t f16 = determinant(f16m2);
    f16 += determinant(f16m3);
    f16 += determinant(f16m4);

    f16m2 = inverse(f16m2);
    f16m3 = inverse(f16m3);
    f16m4 = inverse(f16m4);

    f16 += f16m3x2[1][1] + f16m2[1][1] + f16m3[1][1] + f16m4[1][1];

    fragColor = vec3(f16);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [2 x <3 x half>] @_Z12OuterProductDv3_DhDv2_Dh(<3 x half> %{{[0-9]*}}, <2 x half> %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [3 x <2 x half>] @_Z9TransposeDv2_Dv3_Dh([2 x <3 x half>] %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z11determinantDv2_Dv2_Dh([2 x <2 x half>] %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z11determinantDv3_Dv3_Dh([3 x <3 x half>] %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z11determinantDv4_Dv4_Dh([4 x <4 x half>] %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [2 x <2 x half>] @_Z13matrixInverseDv2_Dv2_Dh([2 x <2 x half>] %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [3 x <3 x half>] @_Z13matrixInverseDv3_Dv3_Dh([3 x <3 x half>] %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [4 x <4 x half>] @_Z13matrixInverseDv4_Dv4_Dh([4 x <4 x half>] %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
