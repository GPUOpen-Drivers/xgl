#version 450
layout(location = 0) in mat2 m0;
layout(location = 0) out vec4 o_color;

void main (void)
{
	o_color = vec4(determinant(m0));
}