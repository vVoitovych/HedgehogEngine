#version 450

// Tone maps the view's HDR radiance: scaled by the exposure (2^EV), then Krzysztof Narkowicz's fit
// of the ACES filmic curve. The result stays linear: the swapchain encodes sRGB.
layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

layout(push_constant) uniform ToneMapConstants
{
    float exposureScale;
} constants;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 AcesFilmic(vec3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

void main()
{
    const vec3 radiance = max(texture(hdrTexture, inUv).rgb, vec3(0.0f)) * constants.exposureScale;
    outColor = vec4(AcesFilmic(radiance), 1.0f);
}
