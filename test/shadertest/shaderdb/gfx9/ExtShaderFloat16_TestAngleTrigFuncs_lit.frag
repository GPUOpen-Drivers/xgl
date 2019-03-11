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
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z7radiansDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z7degreesDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3sinDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3cosDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z3tanDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4asinDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4acosDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5atan2Dv3_DhDv3_Dh(<3 x half> %{{.*}}, <3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4sinhDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z4coshDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5asinhDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5acoshDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x half> @_Z5atanhDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-3: call half @llvm.sin.f16(half %{{[0-9]*}})
; SHADERTEST-COUNT-3: call half @llvm.cos.f16(half %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
