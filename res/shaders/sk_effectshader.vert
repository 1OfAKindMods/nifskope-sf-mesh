#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoord;

out vec3 N;

out vec4 C;

uniform mat3 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 lightSourcePosition[3];	// W0 = environment map rotation (-1.0 to 1.0), W1, W2 = viewport X, Y
uniform vec4 lightSourceDiffuse[3];		// A0 = overall brightness, A1, A2 = viewport width, height
uniform vec4 lightSourceAmbient;		// A = tone mapping control (1.0 = full tone mapping)

uniform vec4 vertexColorOverride;	// components greater than zero replace the vertex color

uniform int numBones;
uniform mat4x3 boneTransforms[100];

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec4	vertexColor;
layout ( location = 2 ) in vec3	normalVector;
layout ( location = 5 ) in vec4	boneWeights0;
layout ( location = 6 ) in vec4	boneWeights1;
layout ( location = 7 ) in vec2	multiTexCoord0;

void main()
{
	vec4	v = vec4( vertexPosition, 1.0 );
	vec3	n = normalVector;

	if ( numBones > 0 ) {
		vec3	vTmp = vec3( 0.0 );
		vec3	nTmp = vec3( 0.0 );
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
				wSum += w;
			}
		}
		if ( wSum > 0.0 ) {
			v = vec4( vTmp / wSum, 1.0 );
			n = nTmp;
		}
	}

	v = modelViewMatrix * v;
	gl_Position = projectionMatrix * v;
	texCoord = multiTexCoord0;

	N = normalize( n * normalMatrix );

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	C = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
}
