#version 130

uniform sampler2D BaseMap;
uniform sampler2D NormalMap;
uniform sampler2D GlowMap;

in vec3 LightDir;
in vec3 HalfVector;
in vec3 ViewDir;

in vec4 ColorEA;
in vec4 ColorD;
in float toneMapScale;

void main( void )
{
	float offset = 0.015 - texture2D( BaseMap, gl_TexCoord[0].st ).a * 0.03;
	vec2 texco = gl_TexCoord[0].st + normalize( ViewDir ).xy * offset;

	vec4 color = ColorEA;
	color += texture2D( GlowMap, texco );

	vec4 normal = texture2D( NormalMap, texco );
	normal.rgb = normal.rgb * 2.0 - 1.0;
	if ( !gl_FrontFacing )
		normal *= -1.0;

	float NdotL = max( dot( normal.rgb, normalize( LightDir ) ), 0.0 );

	if ( NdotL > 0.0 )
	{
		color += ColorD * NdotL;
		float NdotHV = max( dot( normal.rgb, normalize( HalfVector ) ), 0.0 );
		color += normal.a * gl_FrontMaterial.specular * gl_LightSource[0].specular * pow( NdotHV, gl_FrontMaterial.shininess );
	}

	color = min( color, 1.0 );
	color *= texture2D( BaseMap, texco );

	gl_FragColor = color;
}
