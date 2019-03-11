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

    f16v3 = dFdx(f16v3);
    f16v3 = dFdy(f16v3);
    f16v3 = dFdxFine(f16v3);
    f16v3 = dFdyFine(f16v3);
    f16v3 = dFdxCoarse(f16v3);
    f16v3 = dFdyCoarse(f16v3);
    f16v3 = fwidth(f16v3);
    f16v3 = fwidthFine(f16v3);
    f16v3 = fwidthCoarse(f16v3);

    fragColor = f16v3;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call {{.*}} <3 x half> @_Z4DPdxDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: call {{.*}} <3 x half> @_Z4DPdyDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: call {{.*}} <3 x half> @_Z8DPdxFineDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: call {{.*}} <3 x half> @_Z8DPdyFineDv3_Dh(<3 x half> %{{.*}})
; SHADERTEST: call half @llpc.dpdxFine.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llpc.dpdyFine.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llpc.dpdx.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llpc.dpdy.f16(half %{{[0-9]*}})
; SHADERTEST: call half @llvm.fabs.f16(half %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
