#version 450

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0, std430) buffer Buffers
{
    vec4 fv4;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec2 f16v2_0 = f16vec2(fv4.xy);
    f16vec2 f16v2_1 = f16vec2(fv4.zw);

    bool b = false;
    bvec2 bv2 = equal(f16v2_0, f16v2_1);
    b = b || bv2.x;

    bv2 = notEqual(f16v2_0, f16v2_1);
    b = b || bv2.x;

    bv2 = lessThan(f16v2_0, f16v2_1);
    b = b && bv2.x;

    bv2 = greaterThan(f16v2_0, f16v2_1);
    b = b || bv2.x;

    bv2 = lessThanEqual(f16v2_0, f16v2_1);
    b = b && bv2.x;

    bv2 = greaterThanEqual(f16v2_0, f16v2_1);
    b = b || bv2.x;

    fragColor = b ? vec3(1.0) : vec3(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fcmp oeq <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: %{{[0-9]*}} = fcmp one <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: %{{[0-9]*}} = fcmp olt <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: %{{[0-9]*}} = fcmp ogt <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: %{{[0-9]*}} = fcmp ole <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: %{{[0-9]*}} = fcmp oge <2 x half> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
