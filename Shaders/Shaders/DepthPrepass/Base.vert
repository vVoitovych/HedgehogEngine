#version 450

#define MAX_LIGHTS_COUNT 16

#include "Common/LightData.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 view;
    mat4 proj;
    vec4 eyePos;
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} ubo;


layout(location = 0) in vec3 inPosition;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
	gl_Position = ubo.proj * ubo.view * PushConstants.model * vec4(inPosition, 1.0f);
}


