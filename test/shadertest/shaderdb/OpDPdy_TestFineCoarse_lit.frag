#version 450

layout (location = 0) out vec4 fragColor;
layout (location = 1) in vec3 fsIn;

void main()
{
    vec3 f3 = fsIn;
    f3 = dFdy(f3);
    f3 = dFdyFine(f3);
    f3 = dFdyCoarse(f3);

    fragColor = (f3[0] == f3[1]) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} <3 x float> @_Z4DPdyDv3_f(<3 x float> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} <3 x float> @_Z8DPdyFineDv3_f(<3 x float> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} <3 x float> @_Z10DPdyCoarseDv3_f(<3 x float> %{{[0-9]+}})
; SHADERTEST-LABEL: {{^// LLPC.*}} patching results
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32(i32 {{[%0-9]+}}, i32 170, i32 15, i32 15, i1 true)
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32(i32 {{[%0-9]+}})
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32(i32 {{[%0-9]+}}, i32 0, i32 15, i32 15, i1 true)
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32(i32 {{[%0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
