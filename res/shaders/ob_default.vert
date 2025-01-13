#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoord;

out vec4 A;
out vec4 C;
out vec4 D;

out mat3 reflMatrix;

uniform mat3 viewMatrix;
uniform mat3 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 lightSourcePosition[3];	// W0 = environment map rotation (-1.0 to 1.0), W1, W2 = viewport X, Y
uniform vec4 lightSourceDiffuse[3];		// A0 = overall brightness, A1, A2 = viewport width, height
uniform vec4 lightSourceAmbient;		// A = tone mapping control (1.0 = full tone mapping)

uniform vec4 vertexColorOverride;	// components greater than zero replace the vertex color

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec4	vertexColor;
layout ( location = 2 ) in vec3	normalVector;
layout ( location = 3 ) in vec3	tangentVector;
layout ( location = 4 ) in vec3	bitangentVector;
layout ( location = 7 ) in vec2	multiTexCoord0;

#include "bonetransform.glsl"

void main()
{
	vec4	v = vec4( vertexPosition, 1.0 );
	vec3	n = normalVector;
	vec3	t = tangentVector;
	vec3	b = bitangentVector;

	if ( numBones > 0 )
		boneTransform( v, n, t, b );

	v = modelViewMatrix * v;
	gl_Position = projectionMatrix * v;
	texCoord = multiTexCoord0;

	// NOTE: b<->t
	mat3 btnMatrix = mat3( normalize(b * normalMatrix), normalize(t * normalMatrix), normalize(n * normalMatrix) );
	btnMatrix = transpose( btnMatrix );

	reflMatrix = btnMatrix * viewMatrix;

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = btnMatrix * vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = btnMatrix * -v.xyz;
	LightDir = btnMatrix * lightSourcePosition[0].xyz;

	A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, lightSourceAmbient.a );
	C = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
	D = vec4( sqrt(lightSourceDiffuse[0].rgb), lightSourceDiffuse[0].a );
}
