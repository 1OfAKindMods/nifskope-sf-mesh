layout ( location = 5 ) in vec4	boneWeights0;
layout ( location = 6 ) in vec4	boneWeights1;

uniform int numBones;
uniform mat3x4 boneTransforms[MAX_NUM_BONES];

#ifdef BT_POSITION_ONLY
void boneTransform( inout vec4 v )
#elif defined( BT_NO_TANGENTS )
void boneTransform( inout vec4 v, inout vec3 n )
#else
void boneTransform( inout vec4 v, inout vec3 n, inout vec3 t, inout vec3 b )
#endif
{
	vec3	vTmp = vec3( 0.0 );
#ifndef BT_POSITION_ONLY
	vec3	nTmp = vec3( 0.0 );
#ifndef BT_NO_TANGENTS
	vec3	tTmp = vec3( 0.0 );
	vec3	bTmp = vec3( 0.0 );
#endif
#endif
	float	wSum = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		float	bw;
		if ( i < 4 )
			bw = boneWeights0[i];
		else
			bw = boneWeights1[i & 3];
		if ( !( bw > 0.0 ) )
			break;
		int	bone = int( bw );
		if ( bone >= numBones )
			continue;
		float	w = fract( bw );
		mat3x4	m = boneTransforms[bone];
		vTmp += v * m * w;
#ifndef BT_POSITION_ONLY
		mat3	r = mat3( m );
		nTmp += n * r * w;
#ifndef BT_NO_TANGENTS
		tTmp += t * r * w;
		bTmp += b * r * w;
#endif
#endif
		wSum += w;
	}
	if ( wSum > 0.0 ) {
		v = vec4( vTmp / wSum, 1.0 );
#ifndef BT_POSITION_ONLY
		n = nTmp;
#ifndef BT_NO_TANGENTS
		t = tTmp;
		b = bTmp;
#endif
#endif
	}
}
