#version 450

#define MAX_LIGHTS_COUNT 16

struct Light
{
	vec3 mPosition;
	vec3 mDirection;
	vec3 mColor;
	vec4 mData;		// type, intencity, radius, coneAngle
};

layout(binding = 0) uniform UniformBufferObject 
{
    mat4 view;
    mat4 proj;
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec2 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
	gl_Position = ubo.proj * ubo.view * PushConstants.model * vec4(inPosition, 1.0);
    gl_Position /= gl_Position.w;
	fragColor = inColor;
    fragTexCoord = inTexCoord;
}


