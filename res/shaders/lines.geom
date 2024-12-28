#version 410 core

layout ( lines ) in;
layout ( triangle_strip, max_vertices = 6 ) out;

uniform mat4 projectionMatrix;

in vec4 vsColor[];
out vec4 C;

#include "drawline.glsl"

void main()
{
	C = vsColor[0];

	vec4	p0 = projectionMatrix * gl_in[0].gl_Position;
	vec4	p1 = projectionMatrix * gl_in[1].gl_Position;

	drawLine( p0, p1 );
}
