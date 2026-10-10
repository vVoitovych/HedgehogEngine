#version 450

// The depth of a Cutoff material's skinned instance, passing its UVs on to CutoffSkinned.frag.

#define PALETTE_SET 1
#include "../Common/Skinning.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
} ubo;

layout(location = 0) in vec3  inPosition;
layout(location = 1) in uvec4 inJoints;
layout(location = 2) in vec4  inWeights;
layout(location = 3) in vec2  inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(push_constant) uniform constants
{
    mat4 model;
    uint paletteOffset;
} PushConstants;

void main()
{
    const mat4 skin = SkinMatrix(inJoints, inWeights, PushConstants.paletteOffset);
    fragTexCoord    = inTexCoord;
    gl_Position     = ubo.viewProj * PushConstants.model * skin * vec4(inPosition, 1.0f);
}
