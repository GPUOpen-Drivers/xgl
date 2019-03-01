#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2_1;
    double d1;

    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_0 = dvec2(0.0);
    d2_0[i] = d1;

    if (d2_0 == d2_1)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = getelementptr <2 x double>, <2 x double> {{.*}}* %{{.*}}, i32 0, i32 %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
