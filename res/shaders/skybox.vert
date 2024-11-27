#version 120

varying vec3 LightDir;
varying vec3 ViewDir;

varying vec4 A;
varying vec4 D;

varying mat3 reflMatrix;

uniform mat3 viewMatrix;
uniform bool invertZAxis;

mat3 rotateEnv( mat3 m, float rz )
{
	float	rz_c = cos(rz);
	float	rz_s = -sin(rz);
	float	z = ( !invertZAxis ? 1.0 : -1.0 );
	return mat3(vec3(m[0][0] * rz_c - m[0][1] * rz_s, m[0][0] * rz_s + m[0][1] * rz_c, m[0][2] * z),
				vec3(m[1][0] * rz_c - m[1][1] * rz_s, m[1][0] * rz_s + m[1][1] * rz_c, m[1][2] * z),
				vec3(m[2][0] * rz_c - m[2][1] * rz_s, m[2][0] * rz_s + m[2][1] * rz_c, m[2][2] * z));
}

void main( void )
{
	vec3 v = vec3(gl_ModelViewMatrix * gl_Vertex);

	reflMatrix = rotateEnv( viewMatrix, gl_LightSource[0].position.w * 3.14159265 );

	ViewDir = vec3(-v.xy, 1.0);
	LightDir = gl_LightSource[0].position.xyz;

	A = gl_LightSource[0].ambient;
	D = gl_LightSource[0].diffuse;

	if (gl_ProjectionMatrix[3][3] == 1.0)
		gl_Position = vec4(0.0, 0.0, 2.0, 1.0);	// orthographic view is not supported
	else
		gl_Position = vec4((gl_ModelViewProjectionMatrix * gl_Vertex).xy, 1.0, 1.0);
}
