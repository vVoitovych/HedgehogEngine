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
layout(location = 3) in vec4  inTangent; // xyz along increasing u, w handedness
layout(location = 4) in uvec4 inJoints;
layout(location = 5) in vec4  inWeights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPosition;
layout(location = 3) out vec4 outTangent;

layout(push_constant) uniform constants
{
    mat4 model;
    uint paletteOffset;
} PushConstants;

// Base.vert with each vertex first moved by its joints (its normal and tangent turned with it): the
// same outputs, so Base.frag shades it.
void main()
{
    const mat4 skinnedModel = PushConstants.model * SkinMatrix(inJoints, inWeights, PushConstants.paletteOffset);
    outWorldPosition = skinnedModel * vec4(inPosition, 1.0f);
    outNormal        = skinnedModel * vec4(inNormal, 0.0f);
    outTangent       = vec4((skinnedModel * vec4(inTangent.xyz, 0.0f)).xyz, inTangent.w);
    gl_Position      = viewData.viewProj * outWorldPosition;
    fragTexCoord     = inTexCoord;
}
