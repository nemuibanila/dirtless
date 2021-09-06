#version 460

layout (set = 0,binding = 0) uniform sampler2D inputTexture;



layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


void main() 
{
	vec4 tex_color = texture(inputTexture, inUV);
	outFragColor = vec4(tex_color.rgb, 1.0f);
}