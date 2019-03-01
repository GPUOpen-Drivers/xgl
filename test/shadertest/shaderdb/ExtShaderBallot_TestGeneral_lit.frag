#version 450 core

#extension GL_ARB_shader_ballot: enable
#extension GL_ARB_gpu_shader_int64: enable

layout(location = 0) in flat int i1;
layout(location = 1) in float f1;

layout(location = 0) out float f;

void main(void)
{
    uint64_t u64 = 0;

    u64 += gl_SubGroupInvocationARB;
    u64 += gl_SubGroupSizeARB;
    u64 += gl_SubGroupEqMaskARB;
    u64 += gl_SubGroupGeMaskARB;
    u64 += gl_SubGroupGtMaskARB;
    u64 += gl_SubGroupLeMaskARB;
    u64 += gl_SubGroupLtMaskARB;

    u64 += ballotARB(true);

    f = float(u64);

    f += float(readInvocationARB(i1, 1));
    f += float(readFirstInvocationARB(i1));

    f += readInvocationARB(f1, 2);
    f += readFirstInvocationARB(f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i64 @llpc.input.import.builtin.SubgroupLtMaskKHR.i64.i32
; SHADERTEST: call i64 @llpc.input.import.builtin.SubgroupLeMaskKHR.i64.i32
; SHADERTEST: call i64 @llpc.input.import.builtin.SubgroupGtMaskKHR.i64.i32
; SHADERTEST: call i64 @llpc.input.import.builtin.SubgroupGeMaskKHR.i64.i32
; SHADERTEST: call i64 @llpc.input.import.builtin.SubgroupEqMaskKHR.i64.i32
; SHADERTEST: call i32 @llpc.input.import.builtin.SubgroupSize.i32.i32
; SHADERTEST: call i32 @llpc.input.import.builtin.SubgroupLocalInvocationId.i32.i32
; SHADERTEST: call i64 @llpc.subgroup.ballot
; SHADERTEST: call i32 @_Z25SubgroupReadInvocationKHRii
; SHADERTEST: call i32 @_Z26SubgroupFirstInvocationKHRi
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: tail call i32 asm sideeffect "; %{{[0-9]*}}", "=v,0"(i32 1)
; SHADERTEST: tail call i64 @llvm.amdgcn.icmp.i32
; SHADERTEST: tail call i32 @llvm.amdgcn.readlane
; SHADERTEST: tail call i32 @llvm.amdgcn.readfirstlane
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
