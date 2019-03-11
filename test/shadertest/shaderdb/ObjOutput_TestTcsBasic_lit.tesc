#version 450 core

layout(vertices = 3) out;

layout(location = 1) out dvec4 outData1[];
layout(location = 4) patch out float outData2[4];

void main (void)
{
    outData1[gl_InvocationID] = dvec4(gl_in[gl_InvocationID].gl_Position);
    float f[4] = { 1.0, 2.0, 3.0, 4.0 };
    outData2 = f;
    f[0] += float(gl_PrimitiveID);
    outData2[gl_InvocationID] = f[0];

    barrier();

    outData1[gl_InvocationID].x += double(gl_InvocationID);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v4f64
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f64
; SHADERTEST: call double @llpc.output.import.generic.f64{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
