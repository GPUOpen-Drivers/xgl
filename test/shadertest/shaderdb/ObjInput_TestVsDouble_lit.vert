#version 450 core

layout(location = 2) in dvec4 d4;
layout(location = 4) in dvec3 d3[2];
layout(location = 8) in dmat4 dm4;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    dvec4 d = d4;
    d += dvec4(d3[i], 1.0);
    d += dm4[i];

    gl_Position = vec4(d);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-4: call <4 x double> @llpc.input.import.generic.v4f64{{.*}}
; SHADERTEST-COUNT-2: call <3 x double> @llpc.input.import.generic.v3f64{{.*}}
; SHADERTEST-COUNT-1: call <4 x double> @llpc.input.import.generic.v4f64{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-COUNT-3: call <4 x i32> @llvm.amdgcn.struct.tbuffer.load.v4i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
