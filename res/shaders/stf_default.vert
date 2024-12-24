#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec4 texCoord;

out mat3 btnMatrix;

out vec4 A;
out vec4 C;
out vec4 D;

out mat3 reflMatrix;

uniform mat3 viewMatrix;
uniform mat3 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 lightSourcePosition0;	// W = environment map rotation (-1.0 to 1.0)
uniform vec4 lightSourceDiffuse0;	// A = overall brightness
uniform vec4 lightSourceAmbient;	// A = tone mapping control (1.0 = full tone mapping)

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 multiTexCoord0;
layout ( location = 2 ) in vec4 vertexColor;
layout ( location = 3 ) in vec3 normalVector;
layout ( location = 4 ) in vec3 tangentVector;
layout ( location = 5 ) in vec3 bitangentVector;

mat3 rotateEnv( mat3 m, float rz )
{
	float	rz_c = cos(rz);
	float	rz_s = -sin(rz);
	return mat3(vec3(m[0][0] * rz_c - m[0][1] * rz_s, m[0][0] * rz_s + m[0][1] * rz_c, m[0][2]),
				vec3(m[1][0] * rz_c - m[1][1] * rz_s, m[1][0] * rz_s + m[1][1] * rz_c, m[1][2]),
				vec3(m[2][0] * rz_c - m[2][1] * rz_s, m[2][0] * rz_s + m[2][1] * rz_c, m[2][2]));
}

void main( void )
{
	vec4 v = modelViewMatrix * vec4( vertexPosition, 1.0 );
	gl_Position = projectionMatrix * v;
	texCoord = multiTexCoord0;

	btnMatrix[2] = normalize( normalVector * normalMatrix );
	btnMatrix[1] = normalize( tangentVector * normalMatrix );
	btnMatrix[0] = normalize( bitangentVector * normalMatrix );

	reflMatrix = rotateEnv( viewMatrix, lightSourcePosition0.w * 3.14159265 );

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition0.xyz;

	A = lightSourceAmbient;
	C = vertexColor;
	D = lightSourceDiffuse0;
}
