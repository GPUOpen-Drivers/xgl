#version 450

struct PosAttrib
{
    vec4 position;
    int dummy[4];
    vec4 attrib;
};

layout(std140, binding = 0) buffer Buffer
{
    mat4      mvp;
    PosAttrib vertData;
} buf;

void main()
{
    PosAttrib pa = buf.vertData;
    gl_Position = pa.position;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: getelementptr { [4 x <4 x float>], { <4 x float>, [4 x i32], <4 x float> } }, { [4 x <4 x float>], { <4 x float>, [4 x i32], <4 x float> } } addrspace({{.*}})* @{{.*}}, i32 0, i32 1

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 64
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 80
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 96
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 112
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 128
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 144

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
