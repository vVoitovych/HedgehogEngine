#version 450

#include "Common/LightData.glsl"
#include "Common/UniformFrameData.glsl"

layout(location = 0) in vec3 inPosition;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
	gl_Position = ubo.proj * ubo.view * outWorldPosition;
}


