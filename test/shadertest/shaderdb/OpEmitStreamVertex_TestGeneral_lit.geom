#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 32, stream = 1) out;

layout(location = 0) in vec4 colorIn[];
layout(location = 6) out vec4 colorOut;

void main ( )
{
    for (int i = 0; i < gl_in.length(); i++)
    {
        colorOut = colorIn[i];
        EmitStreamVertex(1);
    }

    EndStreamPrimitive(1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{[a-zA-Z_]+}} void @_Z16EmitStreamVertexi(i32 1)
; SHADERTEST: call {{[a-zA-Z_]+}} void @_Z18EndStreamPrimitivei(i32 1)
; SHADERTEST-LABEL: {{^// LLPC.*}} patching results
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 290, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 290, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 290, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 274, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 3, i32 %{{[0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
