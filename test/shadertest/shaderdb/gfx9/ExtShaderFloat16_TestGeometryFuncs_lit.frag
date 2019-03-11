#version 450

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0, std430) buffer Buffers
{
    vec3 fv3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3 = f16vec3(fv3);
    f16vec2 f16v2 = f16v3.yz;
    f16vec4 f16v4 = f16vec4(f16v3, f16v2.x);
    float16_t f16 = f16v2.y;

    f16 += length(f16);
    f16 += length(f16v2);
    f16 += length(f16v3);
    f16 += length(f16v4);

    f16 += distance(f16, f16);
    f16 += distance(f16v2, f16v2);
    f16 += distance(f16v3, f16v3);
    f16 += distance(f16v4, f16v4);

    f16 += dot(f16v3, f16v3);
    f16 += dot(f16v4, f16v4);
    f16 += dot(f16v2, f16v2);

    f16v3 = cross(f16v3, f16v3);

    f16   += normalize(f16);
    f16v2 += normalize(f16v2);
    f16v3 += normalize(f16v3);
    f16v4 += normalize(f16v4);

    f16   += faceforward(f16, f16, f16);
    f16v2 += faceforward(f16v2, f16v2, f16v2);
    f16v3 += faceforward(f16v3, f16v3, f16v3);
    f16v4 += faceforward(f16v4, f16v4, f16v4);

    f16   += reflect(f16, f16);
    f16v2 += reflect(f16v2, f16v2);
    f16v3 += reflect(f16v3, f16v3);
    f16v4 += reflect(f16v4, f16v4);

    f16   += refract(f16, f16, f16);
    f16v2 += refract(f16v2, f16v2, f16);
    f16v3 += refract(f16v3, f16v3, f16);
    f16v4 += refract(f16v4, f16v4, f16);

    fragColor = vec3(f16) + vec3(f16v2.x) + vec3(f16v3) + vec3(f16v4.xyz);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z6lengthDh(half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z6lengthDv2_Dh(<2 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z6lengthDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z6lengthDv4_Dh(<4 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z8distanceDhDh(half %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z8distanceDv2_DhDv2_Dh(<2 x half> %{{.*}}, <2 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z8distanceDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z8distanceDv4_DhDv4_Dh(<4 x half> %{{.*}}, <4 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z3dotDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z3dotDv4_DhDv4_Dh(<4 x half> %{{.*}}, <4 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z3dotDv2_DhDv2_Dh(<2 x half> %{{.*}}, <2 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5crossDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z9normalizeDh(half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <2 x half> @_Z9normalizeDv2_Dh(<2 x half>  %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z9normalizeDv3_Dh(<3 x half>  %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x half> @_Z9normalizeDv4_Dh(<4 x half>  %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z11faceForwardDhDhDh(half %{{.*}}, half %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <2 x half> @_Z11faceForwardDv2_DhDv2_DhDv2_Dh(<2 x half> %{{.*}}, <2 x half> %{{.*}}, <2 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z11faceForwardDv3_DhDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x half> @_Z11faceForwardDv4_DhDv4_DhDv4_Dh(<4 x half> %{{.*}}, <4 x half> %{{.*}}, <4 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z7reflectDhDh(half %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <2 x half> @_Z7reflectDv2_DhDv2_Dh(<2 x half> %{{.*}}, <2 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z7reflectDv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x half> @_Z7reflectDv4_DhDv4_Dh(<4 x half> %{{.*}}, <4 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} half @_Z7refractDhDhDh(half %{{.*}}, half %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <2 x half> @_Z7refractDv2_DhDv2_DhDh(<2 x half> %{{.*}}, <2 x half> %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z7refractDv3_DhDv3_DhDh(<3 x half> %{{.*}}, <3 x half> %{{.*}}, half %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x half> @_Z7refractDv4_DhDv4_DhDh(<4 x half> %{{.*}}, <4 x half> %{{.*}}, half %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
