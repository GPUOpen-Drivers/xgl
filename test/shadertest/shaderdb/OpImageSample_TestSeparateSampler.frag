#version 450

layout (set = 0, binding = 0) uniform texture2D t0;
layout (set = 0, binding = 1) uniform sampler s1;
layout (set = 0, binding = 1) uniform texture2D t1;
layout (location = 0) in vec4 texcoord;
layout (location = 0) out vec4 uFragColor;
void main() {
   uFragColor = texture(sampler2D(t0, s1), texcoord.xy) + texture(sampler2D(t1, s1), texcoord.xy);
}
