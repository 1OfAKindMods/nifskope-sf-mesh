#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec4 vsAmbient;
out vec4 vsColor;
out vec4 vsDiffuse;

out float vsParticleSize;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 lightSourcePosition[3];	// W0 = environment map rotation (-1.0 to 1.0), W1, W2 = viewport X, Y
uniform vec4 lightSourceDiffuse[3];		// A0 = overall brightness, A1, A2 = viewport width, height
uniform vec4 lightSourceAmbient;		// A = tone mapping control (1.0 = full tone mapping)

uniform vec4 vertexColorOverride;	// components greater than zero replace the vertex color

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;
// location 4 (bitangent) is used for sizes because the default is set to vec3(1, 0, 0)
layout ( location = 4 ) in float particleSize;

void main()
{
	vec4	v = modelViewMatrix * vec4( vertexPosition, 1.0 );

	gl_Position = v;

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	vsAmbient = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, lightSourceAmbient.a );
	vsColor = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
	vsDiffuse = vec4( sqrt(lightSourceDiffuse[0].rgb), lightSourceDiffuse[0].a );

	vsParticleSize = particleSize;
}
