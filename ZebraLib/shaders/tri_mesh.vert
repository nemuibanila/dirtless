#version 460
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform  SceneData{   
    vec4 fogColor; // w is for exponent
	vec4 fogDistances; //x for min, y for max, zw unused.
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} sceneData;

struct ObjectData{
	mat4 model;
	vec4 color;
};

//all object matrices
layout(set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

//push constants block
layout( push_constant ) uniform constants
{
 vec4 data;
 mat4 render_matrix;
} PushConstants;

void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
	mat4 transformMatrix = (sceneData.viewproj * modelMatrix);
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	outColor = mix(vColor, objectBuffer.objects[gl_InstanceIndex].color.rgb, 0.5);
	
	texCoord = vTexCoord;
}
