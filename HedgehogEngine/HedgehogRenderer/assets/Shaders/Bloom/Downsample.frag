#version 450

// The bloom chain's downsample (the Bloom pass): 13 taps of the source, one level up, weighted as
// five overlapping 2 x 2 boxes. The first level, from the HDR radiance, takes a Karis average of
// the boxes (each by 1 / (1 + luma)), so a lone bright pixel cannot flicker the bloom, and keeps
// only what is above the threshold, faded in over a soft knee below it.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D source;

layout(push_constant) uniform Constants
{
    vec2  texel;     // the source's texel size
    float threshold;
    float knee;
    uint  prefilter;
} bloom;

layout(location = 0) out vec4 outColor;

float Luma(vec3 c)
{
    return dot(c, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 Tap(vec2 offset)
{
    return max(texture(source, inUv + offset * bloom.texel).rgb, vec3(0.0f));
}

vec3 Threshold(vec3 color)
{
    const float brightness = max(color.r, max(color.g, color.b));
    float       soft       = clamp(brightness - bloom.threshold + bloom.knee, 0.0f, 2.0f * bloom.knee);
    soft                   = soft * soft / (4.0f * bloom.knee + 1e-5f);
    return color * (max(soft, brightness - bloom.threshold) / max(brightness, 1e-5f));
}

void main()
{
    const vec3 a = Tap(vec2(-2.0f, -2.0f));
    const vec3 b = Tap(vec2( 0.0f, -2.0f));
    const vec3 c = Tap(vec2( 2.0f, -2.0f));
    const vec3 d = Tap(vec2(-2.0f,  0.0f));
    const vec3 e = Tap(vec2( 0.0f,  0.0f));
    const vec3 f = Tap(vec2( 2.0f,  0.0f));
    const vec3 g = Tap(vec2(-2.0f,  2.0f));
    const vec3 h = Tap(vec2( 0.0f,  2.0f));
    const vec3 i = Tap(vec2( 2.0f,  2.0f));
    const vec3 j = Tap(vec2(-1.0f, -1.0f));
    const vec3 k = Tap(vec2( 1.0f, -1.0f));
    const vec3 l = Tap(vec2(-1.0f,  1.0f));
    const vec3 m = Tap(vec2( 1.0f,  1.0f));

    // The centre box counts half, the four corner boxes an eighth each.
    vec3 boxes[5] = vec3[](
        (j + k + l + m) * 0.25f,
        (a + b + d + e) * 0.25f,
        (b + c + e + f) * 0.25f,
        (d + e + g + h) * 0.25f,
        (e + f + h + i) * 0.25f);
    const float weights[5] = float[](0.5f, 0.125f, 0.125f, 0.125f, 0.125f);

    vec3  color = vec3(0.0f);
    float total = 0.0f;
    for (int n = 0; n < 5; ++n)
    {
        const float w = bloom.prefilter != 0u ? weights[n] / (1.0f + Luma(boxes[n])) : weights[n];
        color += boxes[n] * w;
        total += w;
    }
    color /= total;
    if (bloom.prefilter != 0u)
        color = Threshold(color);
    outColor = vec4(color, 1.0f);
}
