#version 450

layout(location = 0) out vec4 fragColor;

bool func(
    int   i1,
    ivec2 i2,
    ivec3 i3,
    ivec4 i4,
    float f1,
    vec2  f2,
    vec3  f3,
    vec4  f4,
    bool  b1,
    bvec2 b2,
    bvec3 b3,
    bvec4 b4)
{
    bool value = false;

    value = any(b4) && all(b3) && any(b2) && b1;

    if (value == true)
    {
        value = ((f4.y + f3.y + f2.x + f1) >= 0.0);
    }
    else
    {
        value = ((i4.x - i3.y + i2.x - i1) == 6);
    }

    return value;
}

void main()
{
    bool value = func(1,
                      ivec2(6),
                      ivec3(7),
                      ivec4(-3),
                      0.5,
                      vec2(1.0),
                      vec3(5.0),
                      vec4(-7.0),
                      false,
                      bvec2(true),
                      bvec3(false),
                      bvec4(true));

    fragColor = (value == false) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i1 @"func
; SHADERTEST: define internal {{.*}} i1 @"func({{.*}}"(i32 {{.*}} %i1, <2 x i32> {{.*}} %i2, <3 x i32> {{.*}} %i3, <4 x i32> {{.*}} %i4, float {{.*}} %f1, <2 x float> {{.*}} %f2, <3 x float> {{.*}} %f3, <4 x float> {{.*}} %f4, i32 {{.*}} %b1, <2 x i32> {{.*}} %b2, <3 x i32> {{.*}} %b3, <4 x i32> {{.*}} %b4)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
