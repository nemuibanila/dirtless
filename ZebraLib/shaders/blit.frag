#version 460

layout (set = 0,binding = 0) uniform sampler2D inputTexture;



layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


void main() 
{
	float depth_value = 1.0f - texture(inputTexture, inUV).x;
	outFragColor = vec4(
		depth_value,
		depth_value,
		depth_value, 1.0f);	
}