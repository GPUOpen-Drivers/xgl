#version 450 core

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput    subIn1;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInputMS  subIn2;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform isubpassInput   subIn3;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform isubpassInputMS subIn4;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform usubpassInput   subIn5;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform usubpassInputMS subIn6;

layout(location = 0) out vec4  fsOut1;
layout(location = 1) out ivec4 fsOut2;
layout(location = 2) out uvec4 fsOut3;

void main()
{
    fsOut1  = subpassLoad(subIn1);
    fsOut1 += subpassLoad(subIn2, 7);
    fsOut2  = subpassLoad(subIn3);
    fsOut2 += subpassLoad(subIn4, 7);
    fsOut3  = subpassLoad(subIn5);
    fsOut3 += subpassLoad(subIn6, 7);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
