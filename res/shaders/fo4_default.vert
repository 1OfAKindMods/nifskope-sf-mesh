#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoord;

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

uniform bool noVertexAlpha;

uniform int numBones;
uniform mat4x3 boneTransforms[100];

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec2	multiTexCoord0;
layout ( location = 2 ) in vec4	vertexColor;
layout ( location = 3 ) in vec3	normalVector;
layout ( location = 4 ) in vec3	tangentVector;
layout ( location = 5 ) in vec3	bitangentVector;
layout ( location = 6 ) in vec4	boneWeights0;
layout ( location = 7 ) in vec4	boneWeights1;

void main( void )
{
	vec4	v = vec4( vertexPosition, 1.0 );
	vec3	n = normalVector;
	vec3	t = tangentVector;
	vec3	b = bitangentVector;

	if ( numBones > 0 ) {
		vec3	vTmp = vec3( 0.0 );
		vec3	nTmp = vec3( 0.0 );
		vec3	tTmp = vec3( 0.0 );
		vec3	bTmp = vec3( 0.0 );
		float	wSum = 0.0;
		for ( int i = 0; i < 8; i++ ) {
			float	bw;
			if ( i < 4 )
				bw = boneWeights0[i];
			else
				bw = boneWeights1[i & 3];
			if ( bw > 0.0 ) {
				int	bone = int( bw );
				if ( bone >= numBones )
					continue;
				float	w = fract( bw );
				mat4x3	m = boneTransforms[bone];
				mat3	r = mat3( m );
				vTmp += m * v * w;
				nTmp += r * n * w;
				tTmp += r * t * w;
				bTmp += r * b * w;
				wSum += w;
			}
		}
		if ( wSum > 0.0 ) {
			v = vec4( vTmp / wSum, 1.0 );
			n = nTmp;
			t = tTmp;
			b = bTmp;
		}
	}

	v = modelViewMatrix * v;
	gl_Position = projectionMatrix * v;
	texCoord = multiTexCoord0;

	btnMatrix[2] = normalize( n * normalMatrix );
	btnMatrix[1] = normalize( t * normalMatrix );
	btnMatrix[0] = normalize( b * normalMatrix );

	reflMatrix = mat3( vec3(viewMatrix[0][0], viewMatrix[0][1], viewMatrix[0][2] * -1.0),
						vec3(viewMatrix[1][0], viewMatrix[1][1], viewMatrix[1][2] * -1.0),
						vec3(viewMatrix[2][0], viewMatrix[2][1], viewMatrix[2][2] * -1.0) );

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition0.xyz;

	A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, lightSourceAmbient.a );
	C = vec4( vertexColor.rgb, ( !noVertexAlpha ? vertexColor.a : 1.0 ) );
	D = sqrt( lightSourceDiffuse0 );
}
