#version 120

varying vec3 LightDir;
varying vec3 ViewDir;

varying mat3 btnMatrix;

varying vec4 A;
varying vec4 C;
varying vec4 D;

varying mat3 reflMatrix;

uniform mat3 viewMatrix;

// FIXME: these uniforms are never set
uniform bool isGPUSkinned;
uniform mat4 boneTransforms[100];

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

	vec3 v;
	if ( isGPUSkinned ) {
		mat4 bt = boneTransforms[int(gl_MultiTexCoord3[0])] * gl_MultiTexCoord4[0];
		bt += boneTransforms[int(gl_MultiTexCoord3[1])] * gl_MultiTexCoord4[1];
		bt += boneTransforms[int(gl_MultiTexCoord3[2])] * gl_MultiTexCoord4[2];
		bt += boneTransforms[int(gl_MultiTexCoord3[3])] * gl_MultiTexCoord4[3];

		vec4	V = bt * gl_Vertex;
		vec3	n = vec3( bt * vec4(gl_Normal, 0.0) );
		vec3	t = vec3( bt * vec4(gl_MultiTexCoord1.xyz, 0.0) );
		vec3	b = vec3( bt * vec4(gl_MultiTexCoord2.xyz, 0.0) );

		gl_Position = gl_ModelViewProjectionMatrix * V;
		btnMatrix[2] = normalize( gl_NormalMatrix * n );
		btnMatrix[1] = normalize( gl_NormalMatrix * t );
		btnMatrix[0] = normalize( gl_NormalMatrix * b );
		v = vec3( gl_ModelViewMatrix * V );
	} else {
		btnMatrix[2] = normalize( gl_NormalMatrix * gl_Normal );
		btnMatrix[1] = normalize( gl_NormalMatrix * gl_MultiTexCoord1.xyz );
		btnMatrix[0] = normalize( gl_NormalMatrix * gl_MultiTexCoord2.xyz );
		v = vec3( gl_ModelViewMatrix * gl_Vertex );
	}

	reflMatrix = rotateEnv( viewMatrix, gl_LightSource[0].position.w * 3.14159265 );

	if (gl_ProjectionMatrix[3][3] == 1.0)
		v = vec3(0.0, 0.0, -1.0);	// orthographic view
	ViewDir = -v.xyz;
	LightDir = gl_LightSource[0].position.xyz;

	A = gl_LightSource[0].ambient;
	C = gl_Color;
	D = gl_LightSource[0].diffuse;
}
