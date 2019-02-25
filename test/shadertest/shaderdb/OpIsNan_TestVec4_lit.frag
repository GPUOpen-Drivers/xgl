#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4;
};

layout(location = 0) out vec4 f;

void main()
{
    bvec4 b4 = isnan(f4);

    f = (b4.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x i1> @_Z5isnanDv4_f

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
