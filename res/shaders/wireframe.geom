#version 410 core

layout ( triangles ) in;
layout ( triangle_strip, max_vertices = 12 ) out;

uniform mat4 projectionMatrix;

in vec4 vsColor[];
out vec4 C;

#include "drawline.glsl"

void main()
{
	C = vec4( vsColor[0].rgb, vsColor[0].a * min( lineWidth, 1.0 ) );

	vec4	p0 = projectionMatrix * gl_in[0].gl_Position;
	vec4	p1 = projectionMatrix * gl_in[1].gl_Position;
	vec4	p2 = projectionMatrix * gl_in[2].gl_Position;

	drawLine( p0, p1 );
	drawLine( p1, p2 );
	drawLine( p2, p0 );
}
