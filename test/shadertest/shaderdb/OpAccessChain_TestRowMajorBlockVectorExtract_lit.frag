#version 450

layout(set = 0, binding = 0, row_major) uniform DATA0
{
    dvec3   d3;
    dmat2x3 dm2x3;
} data0;

layout(set = 1, binding = 1, row_major) buffer DATA1
{
    vec4  f4;
    mat4  m4;
} data1;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

void main()
{
    double d1 = data0.d3[1];
    d1 += data0.d3[index];
    d1 += data0.dm2x3[1][1];
    d1 += data0.dm2x3[1][index];
    d1 += data0.dm2x3[index][1];
    d1 += data0.dm2x3[index + 1][index];

    float f1 = data1.f4[index];
    f1 += data1.f4[2];
    f1 += data1.m4[2][3];
    f1 += data1.m4[index][2];
    f1 += data1.m4[3][index];
    f1 += data1.m4[index][index + 1];

    fragColor = vec4(float(d1), f1, f1, f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: getelementptr { <3 x double>, [2 x <3 x double>] }, { <3 x double>, [2 x <3 x double>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 0, i32 %{{[0-9]*}}
; SHADERTEST: getelementptr { <3 x double>, [2 x <3 x double>] }, { <3 x double>, [2 x <3 x double>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 1, i32 %{{[0-9]*}}
; SHADERTEST: getelementptr { <3 x double>, [2 x <3 x double>] }, { <3 x double>, [2 x <3 x double>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 %{{[0-9]*}}, i32 1
; SHADERTEST: getelementptr { <3 x double>, [2 x <3 x double>] }, { <3 x double>, [2 x <3 x double>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}
; SHADERTEST: getelementptr { <4 x float>, [4 x <4 x float>] }, { <4 x float>, [4 x <4 x float>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 0, i32 %{{[0-9]*}}
; SHADERTEST: getelementptr { <4 x float>, [4 x <4 x float>] }, { <4 x float>, [4 x <4 x float>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 %{{[0-9]*}}, i32 2
; SHADERTEST: getelementptr { <4 x float>, [4 x <4 x float>] }, { <4 x float>, [4 x <4 x float>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 3, i32 %{{[0-9]*}}
; SHADERTEST: getelementptr { <4 x float>, [4 x <4 x float>] }, { <4 x float>, [4 x <4 x float>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 false, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 false, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 false, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 false, i32 0, i1 false)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
