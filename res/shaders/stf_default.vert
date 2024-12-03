#version 400 compatibility

out vec3 LightDir;
out vec3 ViewDir;

out mat3 btnMatrix;

out vec4 A;
out vec4 C;
out vec4 D;

out mat3 reflMatrix;

uniform mat3 viewMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

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
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;

	btnMatrix[2] = normalize( gl_NormalMatrix * gl_Normal );
	btnMatrix[1] = normalize( gl_NormalMatrix * gl_MultiTexCoord1.xyz );
	btnMatrix[0] = normalize( gl_NormalMatrix * gl_MultiTexCoord2.xyz );
	vec3 v = vec3( gl_ModelViewMatrix * gl_Vertex );

	reflMatrix = rotateEnv( viewMatrix, gl_LightSource[0].position.w * 3.14159265 );

	if (gl_ProjectionMatrix[3][3] == 1.0)
		v = vec3(0.0, 0.0, -1.0);	// orthographic view
	ViewDir = -v.xyz;
	LightDir = gl_LightSource[0].position.xyz;

	A = gl_LightSource[0].ambient;
	C = gl_Color;
	D = gl_LightSource[0].diffuse;
}
