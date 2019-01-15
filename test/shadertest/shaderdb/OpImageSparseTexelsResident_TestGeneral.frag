#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2D  samp2D;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    int resident = sparseTextureARB(samp2D, vec2(0.1), texel);

    fragColor = sparseTexelsResidentARB(resident) ? texel : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
