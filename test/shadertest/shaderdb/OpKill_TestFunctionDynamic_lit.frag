#version 310 es

layout(location = 0) in mediump vec4 v_color;
layout(location = 1) in mediump vec4 v_coords;
layout(location = 0) out mediump vec4 o_color;

void myfunc (void)
{
    if (v_coords.x+v_coords.y > 0.0) discard;
}

void main (void)
{
    o_color = v_color;
    myfunc();
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call void{{.*}} @llpc.call.misc.kill

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.kill

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
