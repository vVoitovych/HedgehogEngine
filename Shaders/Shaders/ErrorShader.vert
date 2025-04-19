#version 450

#include "Common/LightData.glsli"
#include "Common/UniformFrameData.glsli"

layout(location = 0) in vec3 inPosition;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
	gl_Position = ubo.proj * ubo.view * outWorldPosition;
}


