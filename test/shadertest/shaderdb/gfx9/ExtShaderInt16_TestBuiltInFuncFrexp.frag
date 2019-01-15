#version 450

#extension GL_AMD_gpu_shader_half_float : enable
#extension GL_AMD_gpu_shader_int16 : enable

layout(location = 0) in vec2 fvIn;

layout(location = 0) out vec2 fvOut;

void main()
{
    f16vec2 fv16 = f16vec2(fvIn);

    i16vec2 iv16 = i16vec2(0s);

    fv16 = frexp(fv16, iv16);

    fvOut = fv16;
    fvOut += iv16;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
