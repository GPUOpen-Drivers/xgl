#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    color = vec4(bitfieldInsert(bd.x, bd.y, bd.z, bd.w));
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldInsert{{.*}}(i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
