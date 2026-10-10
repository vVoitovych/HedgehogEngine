#version 450

// The depth of a Cutoff material's rigid instance, passing its UVs on to Cutoff.frag.

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(push_constant) uniform constants
{
    mat4 model;
} PushConstants;

void main()
{
    fragTexCoord = inTexCoord;
    gl_Position  = ubo.viewProj * PushConstants.model * vec4(inPosition, 1.0f);
}
