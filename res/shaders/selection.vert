#version 410 core

out vec4 C;

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

uniform vec4 vertexColorOverride;	// components greater than zero replace the vertex color
uniform vec4 highlightColor;

// bit 0 = Scene::selecting
// bit 1 = vertex mode (drawing points instead of triangles)
// bit 2 = triangle selection mode (primitive ID << 15 is added to the color)
// bits 8 to 15 = point size * 8 (0: do not draw smooth points)
uniform int selectionFlags;
// if Scene::selecting == false: vertex selected (-1: none)
// if Scene::selecting == true: value to add to color key (e.g. shapeID << 16)
uniform int selectionParam;

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec4	vertexColor;

#define BT_POSITION_ONLY 1
#include "bonetransform.glsl"

void main()
{
	vec4	v = vec4( vertexPosition, 1.0 );

	if ( boneWeights[0] > 0.0 && renderOptions1.x != 0 )
		boneTransform( v );

	v = projectionMatrix * ( modelViewMatrix * v );

	if ( ( selectionFlags & 1 ) != 0 ) {
		int	colorKey = selectionParam + 1;
		if ( ( selectionFlags & 2 ) != 0 )
			colorKey = colorKey + gl_VertexID;
		C = unpackUnorm4x8( uint( colorKey ) ) + vec4( 0.0005 );
	} else if ( !( gl_VertexID == selectionParam && ( selectionFlags & 2 ) != 0 ) ) {
		C = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
	} else {
		C = highlightColor;
	}

	gl_Position = v;
}
