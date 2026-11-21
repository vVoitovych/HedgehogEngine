#version 450

// Tone maps the view's HDR radiance: the bloom added (times its intensity, 0 without one), scaled
// by the exposure (2^EV), then Krzysztof Narkowicz's fit of the ACES filmic curve. The result stays
// linear: the swapchain encodes sRGB.
layout(set = 0, binding = 0) uniform sampler2D hdrTexture;
layout(set = 1, binding = 0) uniform sampler2D bloomTexture;
// ColorLut.hpp's strip: 32 tiles of 32 x 32, blue by tile, red along x and green along y, encoded.
layout(set = 2, binding = 0) uniform sampler2D colorLut;

layout(push_constant) uniform ToneMapConstants
{
    float exposureScale;
    float bloomIntensity;
    float lutContribution;
} constants;

const float LUT_SIZE = 32.0f;

vec3 LinearToSrgb(vec3 c)
{
    return mix(c * 12.92f, 1.055f * pow(c, vec3(1.0f / 2.4f)) - 0.055f, step(vec3(0.0031308f), c));
}

vec3 SrgbToLinear(vec3 c)
{
    return mix(c / 12.92f, pow((c + 0.055f) / 1.055f, vec3(2.4f)), step(vec3(0.04045f), c));
}

// The graded colour: the encoded colour looked up between its two nearest blue tiles.
vec3 Grade(vec3 color)
{
    const vec3  encoded = clamp(LinearToSrgb(color), 0.0f, 1.0f);
    const float slice   = encoded.b * (LUT_SIZE - 1.0f);
    const float lower   = floor(slice);
    const float upper   = min(lower + 1.0f, LUT_SIZE - 1.0f);
    const vec2  texel   = vec2(encoded.r, encoded.g) * (LUT_SIZE - 1.0f) + 0.5f;
    const vec2  uvLower = vec2((lower * LUT_SIZE + texel.x) / (LUT_SIZE * LUT_SIZE), texel.y / LUT_SIZE);
    const vec2  uvUpper = vec2((upper * LUT_SIZE + texel.x) / (LUT_SIZE * LUT_SIZE), texel.y / LUT_SIZE);
    const vec3  graded  = mix(textureLod(colorLut, uvLower, 0.0f).rgb, textureLod(colorLut, uvUpper, 0.0f).rgb, slice - lower);
    return SrgbToLinear(graded);
}

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
    vec3 radiance = max(texture(hdrTexture, inUv).rgb, vec3(0.0f));
    if (constants.bloomIntensity > 0.0f)
        radiance += max(texture(bloomTexture, inUv).rgb, vec3(0.0f)) * constants.bloomIntensity;
    radiance *= constants.exposureScale;
    vec3 color = AcesFilmic(radiance);
    if (constants.lutContribution > 0.0f)
        color = mix(color, Grade(color), constants.lutContribution);
    outColor = vec4(color, 1.0f);
}
