#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

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

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(texSampler, fragTexCoord);
}

