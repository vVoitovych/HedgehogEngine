#version 450

#include "Common/LightData.glsl"
#include "Common/UniformFrameData.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
//layout(location = 4) in vec3 inTangent;
//layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPosition;

layout( push_constant ) uniform constants
{
	mat4 model;
} PushConstants;

void main()
{
    outWorldPosition = PushConstants.model * vec4(inPosition, 1.0);
    outNormal = PushConstants.model * vec4(inNormal, 0.0);
	gl_Position = ubo.proj * ubo.view * outWorldPosition;
    fragTexCoord = inTexCoord;
}


