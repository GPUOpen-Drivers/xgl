#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 0) out vec4 color;

void main()
{
    color = vec4(length(a0.x) + length(double(a0.x)));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z6lengthf(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z6lengthd(double %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
