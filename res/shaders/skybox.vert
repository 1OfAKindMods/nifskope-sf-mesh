#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec4 A;
out vec4 D;

out mat3 reflMatrix;

uniform bool invertZAxis;

uniform mat3 viewMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 lightSourcePosition[3];	// W0 = environment map rotation (-1.0 to 1.0), W1, W2 = viewport X, Y
uniform vec4 lightSourceDiffuse[3];		// A0 = overall brightness, A1, A2 = viewport width, height
uniform vec4 lightSourceAmbient;		// A = tone mapping control (1.0 = full tone mapping)

layout ( location = 0 ) in vec3 vertexPosition;

mat3 rotateEnv( mat3 m, float rz )
{
	float	rz_c = cos(rz);
	float	rz_s = -sin(rz);
	float	z = ( !invertZAxis ? 1.0 : -1.0 );
	return mat3(vec3(m[0][0] * rz_c - m[0][1] * rz_s, m[0][0] * rz_s + m[0][1] * rz_c, m[0][2] * z),
				vec3(m[1][0] * rz_c - m[1][1] * rz_s, m[1][0] * rz_s + m[1][1] * rz_c, m[1][2] * z),
				vec3(m[2][0] * rz_c - m[2][1] * rz_s, m[2][0] * rz_s + m[2][1] * rz_c, m[2][2] * z));
}

void main()
{
	vec4 v = modelViewMatrix * vec4( vertexPosition, 1.0 );

	reflMatrix = rotateEnv( viewMatrix, lightSourcePosition[0].w * 3.14159265 );

	ViewDir = vec3(-v.xy, 1.0);
	LightDir = lightSourcePosition[0].xyz;

	A = lightSourceAmbient;
	D = lightSourceDiffuse[0];

	if ( projectionMatrix[3][3] == 1.0 )
		gl_Position = vec4(0.0, 0.0, 2.0, 1.0);	// orthographic view is not supported
	else
		gl_Position = vec4( ( projectionMatrix * v ).xy, 1.0, 1.0 );
}
