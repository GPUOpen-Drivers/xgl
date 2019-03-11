#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices=32) out;

layout(location = 0) in vec4 colorIn[];
layout(location = 6) out vec4 colorOut;

void main ( )
{
    for (int i = 0; i < gl_in.length(); i++)
    {
        colorOut = colorIn[i];
        EmitVertex();
    }

    EndPrimitive();
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{[a-zA-Z_]+}} void @_Z10EmitVertexv()
; SHADERTEST: call {{[a-zA-Z_]+}} void @_Z12EndPrimitivev()
; SHADERTEST-LABEL: {{^// LLPC.*}} patching results
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 34, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 34, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 34, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 18, i32 %{{[0-9]+}})
; SHADERTEST: call void @llvm.amdgcn.s.sendmsg(i32 3, i32 %{{[0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
