#version 450 core

layout(triangles) in;

layout(location = 1) out vec4 outColor;

void main()
{
    vec4 f = vec4(gl_TessCoord, 0.0);

    for (int i = 0; i < 3; ++i)
    {
        f += gl_in[i].gl_Position;
        f.x += gl_in[i].gl_PointSize;
        f.y += gl_in[i].gl_ClipDistance[2];
        f.z += gl_in[i].gl_CullDistance[3];
    }

    f.w += float(gl_PatchVerticesIn + gl_PrimitiveID);
    f.x += gl_TessLevelOuter[2];
    f.y += gl_TessLevelInner[1];

    outColor = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <3 x float> @llpc.input.import.builtin.TessCoord.v3f32{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.builtin.Position.v4f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.PointSize.f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.ClipDistance.f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.CullDistance.f32{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.PatchVertices{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.TessLevelOuter.f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.TessLevelInner.f32{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
