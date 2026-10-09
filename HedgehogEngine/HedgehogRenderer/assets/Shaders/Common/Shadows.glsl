#ifndef SHADOWS
#define SHADOWS

// The sun's cascaded shadow, read from the shared phase's atlas (Renderer::ShadowUniform, std140).
// Each cascade is a tile of the atlas holding the depth seen from the light over one slice of the
// shadow view's frustum; a fragment picks its slice by its depth in front of that camera.

layout(set = 3, binding = 0) uniform ShadowData
{
    mat4  viewProj[4];   // world to each cascade's clip space (depth 0 at the light's eye)
    mat4  cameraView;    // the shadow view's camera, whose depth picks the cascade
    vec4  tileRects[4];  // each cascade's tile in atlas UV: x, y, width, height
    vec4  splitDepths;   // where each cascade ends, in front of the camera
    vec4  normalOffsets; // the receiver's push along its normal per cascade, in world units
    float depthBias;
    float slopeBias;
    float cascadeBlend;  // the fraction at the far end of a cascade blended into the next
    float texelSize;     // one atlas texel, in UV
    int   cascadeCount;  // 0: nothing is shadowed
    int   pcfRadius;     // taps on each side: (2r + 1)^2 in all
    int   lightIndex;    // the shadowed light among the scene lights, -1 for none
} shadowData;

// Linear filtering with the comparison: every tap is a 2x2 percentage-closer lookup.
layout(set = 3, binding = 1) uniform sampler2DShadow shadowAtlas;

const int   MAX_PCF_RADIUS = 3;
// Past this slope (about 84 degrees) the slope bias stops growing.
const float MAX_SLOPE      = 10.0f;

// The fraction of the light reaching position through cascade (1 lit, 0 shadowed); outside the
// cascade's box, lit.
float SampleCascade(int cascade, vec3 position, float NdotL)
{
    const vec4 clip = shadowData.viewProj[cascade] * vec4(position, 1.0f);
    const vec3 ndc  = clip.xyz / clip.w;
    const vec2 uv   = ndc.xy * 0.5f + 0.5f;
    if (any(lessThan(uv, vec2(0.0f))) || any(greaterThan(uv, vec2(1.0f))) || ndc.z < 0.0f || ndc.z > 1.0f)
        return 1.0f;

    const float slope     = min(sqrt(max(1.0f - NdotL * NdotL, 0.0f)) / max(NdotL, 1e-3f), MAX_SLOPE);
    const float reference = ndc.z - (shadowData.depthBias + shadowData.slopeBias * slope);

    // Taps stay inside the cascade's own tile, half a texel in from its edges.
    const vec4 tile   = shadowData.tileRects[cascade];
    const vec2 texel  = vec2(shadowData.texelSize);
    const vec2 low    = tile.xy + 0.5f * texel;
    const vec2 high   = tile.xy + tile.zw - 0.5f * texel;
    const vec2 center = tile.xy + uv * tile.zw;

    const int radius = clamp(shadowData.pcfRadius, 0, MAX_PCF_RADIUS);
    float     lit    = 0.0f;
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            const vec2 tap = clamp(center + vec2(x, y) * texel, low, high);
            lit += texture(shadowAtlas, vec3(tap, reference));
        }
    }
    const float taps = float((2 * radius + 1) * (2 * radius + 1));
    return lit / taps;
}

// The fraction of the sun reaching position (geometric unit normal, unit direction towards the
// light): 1 outside every cascade or with no shadow.
float SunShadow(vec3 position, vec3 normal, vec3 toLight)
{
    const int count = min(shadowData.cascadeCount, 4);
    if (count <= 0)
        return 1.0f;

    const float depth   = abs((shadowData.cameraView * vec4(position, 1.0f)).z);
    int         cascade = 0;
    while (cascade < count && depth > shadowData.splitDepths[cascade])
        ++cascade;
    if (cascade >= count)
        return 1.0f;

    const float NdotL = clamp(dot(normal, toLight), 0.0f, 1.0f);
    float shadow = SampleCascade(cascade, position + normal * shadowData.normalOffsets[cascade], NdotL);

    // Across the far end of the cascade, fade into the next one (the last fades to lit).
    const float start = cascade == 0 ? 0.0f : shadowData.splitDepths[cascade - 1];
    const float end   = shadowData.splitDepths[cascade];
    const float band  = shadowData.cascadeBlend * (end - start);
    if (band > 0.0f && depth > end - band)
    {
        const float next = cascade + 1 < count
            ? SampleCascade(cascade + 1, position + normal * shadowData.normalOffsets[cascade + 1], NdotL)
            : 1.0f;
        shadow = mix(shadow, next, smoothstep(end - band, end, depth));
    }
    return shadow;
}

#endif
