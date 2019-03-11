#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_shader_trinary_minmax: enable

layout(binding = 0) buffer Buffers
{
    vec3  fv3[3];
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3_1 = f16vec3(fv3[0]);
    f16vec3 f16v3_2 = f16vec3(fv3[1]);
    f16vec3 f16v3_3 = f16vec3(fv3[2]);

    f16v3_1 = min3(f16v3_1, f16v3_2, f16v3_3);
    f16v3_1 = max3(f16v3_1, f16v3_2, f16v3_3);
    f16v3_1 = mid3(f16v3_1, f16v3_2, f16v3_3);

    fragColor = f16v3_1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <3 x half> @_Z8FMin3AMDDv3_DhDv3_DhDv3_Dh(<3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}})
; SHADERTEST: <3 x half> @_Z8FMax3AMDDv3_DhDv3_DhDv3_Dh(<3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}})
; SHADERTEST: <3 x half> @_Z8FMid3AMDDv3_DhDv3_DhDv3_Dh(<3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}}, <3 x half> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-6: call half @llvm.minnum.f16(half %{{[0-9]*}}, half %{{[0-9]*}})
; SHADERTEST-COUNT-6: call half @llvm.maxnum.f16(half %{{[0-9]*}}, half %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
