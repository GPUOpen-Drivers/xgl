#version 450

struct DATA
{
    vec3  f3[3];
    mat3  m3;
    bool  b1;
    ivec2 i2;
};

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const DATA info = { { vec3(0.1), vec3(0.5), vec3(1.0) }, mat3(1.0), true, ivec2(5) };

    DATA data = info;

    fragColor = data.b1 ? vec4(data.f3[i].y) : vec4(data.m3[i].z);
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: alloca { [3 x <3 x float>], [3 x <3 x float>], i32, <2 x i32> }, addrspace(5)
; SHADERTEST: alloca <4 x float>, addrspace(5)
; SHADERTEST: store { [3 x <3 x float>], [3 x <3 x float>], i32, <2 x i32> } { [3 x <3 x float>] [<3 x float> <float 0x3FB99999A0000000, float 0x3FB99999A0000000, float 0x3FB99999A0000000>, <3 x float> <float 5.000000e-01, float 5.000000e-01, float 5.000000e-01>, <3 x float> <float 1.000000e+00, float 1.000000e+00, float 1.000000e+00>], [3 x <3 x float>] [<3 x float> <float 1.000000e+00, float 0.000000e+00, float 0.000000e+00>, <3 x float> <float 0.000000e+00, float 1.000000e+00, float 0.000000e+00>, <3 x float> <float 0.000000e+00, float 0.000000e+00, float 1.000000e+00>], i32 1, <2 x i32> <i32 5, i32 5> }, { [3 x <3 x float>], [3 x <3 x float>], i32, <2 x i32> } addrspace(5)* %{{[0-9]*}}
; SHADERTEST: load i32, i32 addrspace(5)* %{{[0-9]*}}
; SHADERTEST: trunc i32 %{{[0-9]*}} to i1
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v4f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
