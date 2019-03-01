#version 450

layout(location = 0) in float  inColor;
layout(location = 0) out float outColor;

float func(float a)
{
    for (int i = 0; i < 6; i++)
    {
        a = -a;
        for (int j = 0; j < 4; j++)
        {
            a = -a;
            if (i == 1)
            {
                return a;
            }
        }
        if (i == 4)
        {
            return 1.0;
        }
    }
    return 1.0;
}

void main()
{
    outColor = func(inColor);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: float @"func(f1;"
; SHADERTEST: <label>
; SHADERTEST: ret float %{{[0-9]*}}
; SHADERTEST: <label>
; SHADERTEST: ret float 1.000000e+00

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
