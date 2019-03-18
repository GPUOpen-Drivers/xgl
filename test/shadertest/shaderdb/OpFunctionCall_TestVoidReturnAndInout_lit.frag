#version 450

layout(location = 0) out vec4 fragColor;

void func(void);

void main()
{
    func();
}

void func(void)
{
    fragColor = vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: define internal {{.*}} void @"func("()
; SHADERTEST: ret void
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
