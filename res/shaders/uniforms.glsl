layout ( std140 ) uniform globalUniforms
{
	mat3	viewMatrix;					// rotation part of view transform
	mat3	envMapRotation;				// view space to environment map
	mat4	projectionMatrix;
	vec4	lightSourcePosition[3];
	vec4	lightSourceDiffuse[3];
	vec4	lightSourceAmbient;
	vec4	lightingControls;			// tone mapping control (1.0 = full tone mapping), overall and glow brightness
	ivec4	viewportDimensions;			// X, Y, width, height
	// skinning enabled, scene flags, cube map background mip level, Starfield POM steps
	ivec4	renderOptions1;
	vec4	renderOptions2;				// Starfield POM scale, offset
	mat3x4	boneTransforms[256];		// bone transforms in row-major order
};
