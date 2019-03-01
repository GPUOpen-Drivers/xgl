#version 450 core

#extension GL_ARB_gpu_shader_int64: enable
#extension GL_AMD_shader_ballot: enable

layout(location = 0) out vec4 fv4Out;

layout(location = 0) in flat ivec2 iv2In;
layout(location = 1) in flat uvec3 uv3In;
layout(location = 2) in flat vec4  fv4In;
layout(location = 3) in flat dvec2 dv2In;

layout(location = 4) in flat i64vec2 i64v2In;
layout(location = 5) in flat u64vec2 u64v2In;

void main()
{
    vec4 fv4 = vec4(0.0);

    fv4.xy  += addInvocationsAMD(iv2In);
    fv4.xyz += addInvocationsAMD(uv3In);
    fv4     += addInvocationsAMD(fv4In);

    fv4.xy  += vec2(addInvocationsAMD(dv2In));

    fv4.xy  += addInvocationsNonUniformAMD(iv2In);
    fv4.xyz += addInvocationsNonUniformAMD(uv3In);
    fv4     += addInvocationsNonUniformAMD(fv4In);

    fv4.xy  += vec2(addInvocationsNonUniformAMD(dv2In));

    fv4.xy  += minInvocationsAMD(iv2In);
    fv4.xyz += minInvocationsAMD(uv3In);
    fv4     += minInvocationsAMD(fv4In);

    fv4.xy  += vec2(minInvocationsAMD(dv2In));

    fv4.xy  += minInvocationsNonUniformAMD(iv2In);
    fv4.xyz += minInvocationsNonUniformAMD(uv3In);
    fv4     += minInvocationsNonUniformAMD(fv4In);

    fv4.xy  += vec2(minInvocationsNonUniformAMD(dv2In));

    fv4.xy  += maxInvocationsAMD(iv2In);
    fv4.xyz += maxInvocationsAMD(uv3In);
    fv4     += maxInvocationsAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsAMD(dv2In));

    fv4.xy  += maxInvocationsNonUniformAMD(iv2In);
    fv4.xyz += maxInvocationsNonUniformAMD(uv3In);
    fv4     += maxInvocationsNonUniformAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsNonUniformAMD(dv2In));

    fv4.xy  += addInvocationsInclusiveScanAMD(iv2In);
    fv4.xyz += addInvocationsInclusiveScanAMD(uv3In);
    fv4     += addInvocationsInclusiveScanAMD(fv4In);

    fv4.xy  += vec2(addInvocationsInclusiveScanAMD(dv2In));

    fv4.xy  += addInvocationsInclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += addInvocationsInclusiveScanNonUniformAMD(uv3In);
    fv4     += addInvocationsInclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(addInvocationsInclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += minInvocationsInclusiveScanAMD(iv2In);
    fv4.xyz += minInvocationsInclusiveScanAMD(uv3In);
    fv4     += minInvocationsInclusiveScanAMD(fv4In);

    fv4.xy  += vec2(minInvocationsInclusiveScanAMD(dv2In));

    fv4.xy  += minInvocationsInclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += minInvocationsInclusiveScanNonUniformAMD(uv3In);
    fv4     += minInvocationsInclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(minInvocationsInclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += maxInvocationsInclusiveScanAMD(iv2In);
    fv4.xyz += maxInvocationsInclusiveScanAMD(uv3In);
    fv4     += maxInvocationsInclusiveScanAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsInclusiveScanAMD(dv2In));

    fv4.xy  += maxInvocationsInclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += maxInvocationsInclusiveScanNonUniformAMD(uv3In);
    fv4     += maxInvocationsInclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsInclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += addInvocationsExclusiveScanAMD(iv2In);
    fv4.xyz += addInvocationsExclusiveScanAMD(uv3In);
    fv4     += addInvocationsExclusiveScanAMD(fv4In);

    fv4.xy  += vec2(addInvocationsExclusiveScanAMD(dv2In));

    fv4.xy  += addInvocationsExclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += addInvocationsExclusiveScanNonUniformAMD(uv3In);
    fv4     += addInvocationsExclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(addInvocationsExclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += minInvocationsExclusiveScanAMD(iv2In);
    fv4.xyz += minInvocationsExclusiveScanAMD(uv3In);
    fv4     += minInvocationsExclusiveScanAMD(fv4In);

    fv4.xy  += vec2(minInvocationsExclusiveScanAMD(dv2In));

    fv4.xy  += minInvocationsExclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += minInvocationsExclusiveScanNonUniformAMD(uv3In);
    fv4     += minInvocationsExclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(minInvocationsExclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += maxInvocationsExclusiveScanAMD(iv2In);
    fv4.xyz += maxInvocationsExclusiveScanAMD(uv3In);
    fv4     += maxInvocationsExclusiveScanAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsExclusiveScanAMD(dv2In));

    fv4.xy  += maxInvocationsExclusiveScanNonUniformAMD(iv2In);
    fv4.xyz += maxInvocationsExclusiveScanNonUniformAMD(uv3In);
    fv4     += maxInvocationsExclusiveScanNonUniformAMD(fv4In);

    fv4.xy  += vec2(maxInvocationsExclusiveScanNonUniformAMD(dv2In));

    fv4.xy  += vec2(addInvocationsAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsAMD(u64v2In));

    fv4.xy  += vec2(addInvocationsNonUniformAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsNonUniformAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsNonUniformAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsNonUniformAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsNonUniformAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsNonUniformAMD(u64v2In));

    fv4.xy  += vec2(addInvocationsInclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsInclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(addInvocationsInclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsInclusiveScanNonUniformAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsInclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsInclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsInclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsInclusiveScanNonUniformAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsInclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsInclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsInclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsInclusiveScanNonUniformAMD(u64v2In));

    fv4.xy  += vec2(addInvocationsExclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsExclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(addInvocationsExclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(addInvocationsExclusiveScanNonUniformAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsExclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsExclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(minInvocationsExclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(minInvocationsExclusiveScanNonUniformAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsExclusiveScanAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsExclusiveScanAMD(u64v2In));

    fv4.xy  += vec2(maxInvocationsExclusiveScanNonUniformAMD(i64v2In));
    fv4.xy  += vec2(maxInvocationsExclusiveScanNonUniformAMD(u64v2In));

    fv4Out = fv4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call {{.*}} i32 @_Z38sub_group_reduce_add_nonuniform_waveSzi
; SHADERTEST: call {{.*}} float @_Z38sub_group_reduce_add_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z38sub_group_reduce_add_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z38sub_group_reduce_min_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z38sub_group_reduce_min_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z38sub_group_reduce_min_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z38sub_group_reduce_min_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z38sub_group_reduce_max_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z38sub_group_reduce_max_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z38sub_group_reduce_max_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z38sub_group_reduce_max_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_inclusive_add_nonuniform_waveSzi
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_inclusive_add_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_inclusive_add_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_exclusive_add_nonuniform_waveSzi
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_exclusive_add_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_exclusive_add_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzi
; SHADERTEST: call {{.*}} i32 @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzj
; SHADERTEST: call {{.*}} float @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzf
; SHADERTEST: call {{.*}} double @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzd
; SHADERTEST: call {{.*}} i64 @_Z38sub_group_reduce_add_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z38sub_group_reduce_min_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z38sub_group_reduce_min_nonuniform_waveSzm
; SHADERTEST: call {{.*}} i64 @_Z38sub_group_reduce_max_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z38sub_group_reduce_max_nonuniform_waveSzm
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_inclusive_add_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_inclusive_min_nonuniform_waveSzm
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_inclusive_max_nonuniform_waveSzm
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_exclusive_add_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_exclusive_min_nonuniform_waveSzm
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzl
; SHADERTEST: call {{.*}} i64 @_Z46sub_group_scan_exclusive_max_nonuniform_waveSzm
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
