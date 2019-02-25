#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 f;

void main()
{
    f = (isinf(d1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: i1 @_Z5isinfd

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i1 @llvm.amdgcn.class.f64(double %{{[0-9]*}}, i32 516)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
