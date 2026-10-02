#version 450

#define PALETTE_SET 3
#include "../Common/Skinning.glsl"

layout(set = 0, binding = 0) uniform ViewData
{
    mat4 view;
    mat4 viewProj;
    vec4 eyePos;
} viewData;

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec2  inTexCoord;
layout(location = 2) in vec3  inNormal;
layout(location = 3) in uvec4 inJoints;
layout(location = 4) in vec4  inWeights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPosition;

layout(push_constant) uniform constants
{
    mat4 model;
    uint paletteOffset;
} PushConstants;

// Base.vert with each vertex first moved by its joints: the same outputs, so Base.frag shades it.
void main()
{
    const mat4 skinnedModel = PushConstants.model * SkinMatrix(inJoints, inWeights, PushConstants.paletteOffset);
    outWorldPosition = skinnedModel * vec4(inPosition, 1.0f);
    outNormal        = skinnedModel * vec4(inNormal, 0.0f);
    gl_Position      = viewData.viewProj * outWorldPosition;
    fragTexCoord     = inTexCoord;
}
