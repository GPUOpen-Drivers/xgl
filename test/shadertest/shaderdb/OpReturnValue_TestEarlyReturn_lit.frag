#version 450

layout(binding = 0) uniform Uniforms
{
    bool cond;
};

layout(location = 0) out vec4 fragColor;

vec4 func()
{
    vec4 f4 = vec4(0.0);

    if (cond)
    {
        return f4;
    }

    f4 = vec4(2.0);
    f4 += vec4(0.35);

    return f4;
}

void main()
{
    fragColor = func();
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @"func("
; SHADERTEST: <label>
; SHADERTEST:  ret <4 x float>
; SHADERTEST: <label>
; SHADERTEST:  ret <4 x float>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
