#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1 = packSnorm4x8(f4);

    fragColor = (u1 != 5) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} i32 @_Z12packSnorm4x8Dv4_f(<4 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 1.270000e+02
; SHADERTEST: %{{[0-9]*}} = call float @llvm.rint.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fptosi float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = and i32 %{{.*}}, 255
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 1.270000e+02
; SHADERTEST: %{{[0-9]*}} = call float @llvm.rint.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fptosi float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = shl i32 %{{.*}}, 8
; SHADERTEST: %{{[0-9]*}} = and i32 %{{.*}}, 65280
; SHADERTEST: %{{[0-9]*}} = or i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 1.270000e+02
; SHADERTEST: %{{[0-9]*}} = call float @llvm.rint.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fptosi float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = shl i32 %{{.*}}, 16
; SHADERTEST: %{{[0-9]*}} = and i32 %{{.*}}, 16711680
; SHADERTEST: %{{[0-9]*}} = or i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 1.270000e+02
; SHADERTEST: %{{[0-9]*}} = call float @llvm.rint.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fptosi float %{{.*}} to i32
; SHADERTEST: %{{[0-9]*}} = shl i32 %{{.*}}, 24
; SHADERTEST: %{{[0-9]*}} = or i32 %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
