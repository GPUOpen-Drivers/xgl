#version 450 core

#define ITER 5
layout(set=0, binding=0) uniform UniformBuffer
{
    vec4 c[4];
    ivec4 ci[4];
    double cd[4];
};

struct TSTR {
    vec4 member;
};

layout(location = 0) out vec4 frag_color;
layout(location = 0) in TSTR interp[ITER];

void main()
{
    int i;
    vec4 s = vec4(0.0f);
    vec2 offset = vec2(0.00001);
    for (i =0; i < ITER; i++)
    {
        offset = vec2(float(i)/float(ITER));
        s += interpolateAtOffset(interp[i].member, offset);
    }
    frag_color.x = float(s.x - interp[0].member.x);
    frag_color.y = float(s.y + interp[0].member.y);
    frag_color.z = 0.0f;
    frag_color.w = 1.0f;
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-5: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST: call <2 x float> @llpc.input.interpolate.evalij.offset.v2f32
; SHADERTEST: call <4 x float> @llpc.input.import.interpolant.v4f32{{.*}}v2f32
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-COUNT-2: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call i32 @llvm.amdgcn.mov.dpp.i32
; SHADERTEST: call i32 @llvm.amdgcn.wqm.i32
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST-COUNT-3: call float @llvm.amdgcn.interp.p2
; SHADERTEST-LABEL: {{^// LLPC}} final pipeline module info
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
