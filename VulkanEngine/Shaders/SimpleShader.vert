#version 450

#define MAX_LIGHTS_COUNT 16

#include "LightData.glsli"

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 view;
    mat4 proj;
    vec4 eyePos;
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} ubo;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outWorldPosition;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
    outWorldPosition = PushConstants.model * vec4(inPosition, 1.0);
    outNormal = PushConstants.model * vec4(inNormal, 1.0);
	gl_Position = ubo.proj * ubo.view * outWorldPosition;
    gl_Position /= gl_Position.w;
	fragColor = inColor;
    fragTexCoord = inTexCoord;
}


