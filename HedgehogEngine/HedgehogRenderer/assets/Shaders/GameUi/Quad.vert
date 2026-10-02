#version 450

// The GameUi pass: HX::UiVertex positions are pixels of the UI's target, (0, 0) its top-left and
// y down, which a scale and an offset map onto clip space. Colours are picked in sRGB, as the
// textures they tint are stored, so they are decoded to linear here.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform constants
{
    vec2 scale;
    vec2 offset;
} PushConstants;

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;

void main()
{
    outUv       = inUv;
    outColor    = vec4(pow(inColor.rgb, vec3(2.2f)), inColor.a);
    gl_Position = vec4(inPosition * PushConstants.scale + PushConstants.offset, 0.0f, 1.0f);
}
