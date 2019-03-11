#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{

    bvec4 bd = bvec4(colorIn1);
    bvec4 bc = bvec4(true, false, true, false);
    color = vec4(any(bd) || any(bc));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: zext <4 x i1> <i1 true, i1 false, i1 true, i1 false> to <4 x i32>
; SHADERTEST-COUNT-2: call {{.*}} i32 @{{.*}}any{{.*}}(<4 x i32> %{{[0-9]*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
