#version 450

layout (location = 0) in vec4 v0;
layout (location = 0) out vec4 o0;

void main()
{
    bvec4 ret = isnan(v0);
    o0 = vec4((ret.x ? 0 : 1),
              (ret.y ? 0 : 1),
              (ret.z ? 0 : 1),
              (ret.w ? 0 : 1));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x i1> @_Z5isnanDv4_f

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
