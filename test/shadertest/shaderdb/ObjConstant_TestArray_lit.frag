#version 450

layout(location= 0) in vec4 input1;

layout(location = 0) out vec4 output1;

const float carry[4] = {1, 2, 3, 4};

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    output1 = input1 + vec4(carry[3], 5, carry[i], float[4](7, 8, 9, 0)[i + 1]);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: @{{.*}} = {{.*}} addrspace(4) constant [4 x float] [float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00]
; SHADERTEST: @{{.*}} = {{.*}} addrspace(4) constant [4 x float] [float 7.000000e+00, float 8.000000e+00, float 9.000000e+00, float 0.000000e+00]
; SHADERTEST: getelementptr [4 x float], [4 x float] addrspace(4)* @{{.*}}, i64 0, i64 %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
