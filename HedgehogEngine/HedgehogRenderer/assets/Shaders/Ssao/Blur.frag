#version 450

// The Ssao pass's blur: a 4 x 4 average of the raw occlusion, each tap weighted down by how far its
// depth lies from the centre's, so occlusion does not bleed across an object's edge.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D rawOcclusion;
layout(set = 1, binding = 0) uniform sampler2D depthMap;

// SsaoBlurPushConstants: the depth's linearization (clip.z = C z + D, clip.w = E z + F), then the
// raw occlusion's texel size.
layout(push_constant) uniform Constants
{
    vec4 projCDEF;
    vec2 texel;
} blur;

layout(location = 0) out vec4 outOcclusion;

float ViewZ(vec2 uv)
{
    const float depth = texture(depthMap, uv).r;
    return (blur.projCDEF.y - depth * blur.projCDEF.w) / (depth * blur.projCDEF.z - blur.projCDEF.x);
}

void main()
{
    const float centerZ = ViewZ(inUv);
    float       sum     = 0.0f;
    float       weights = 0.0f;
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const vec2  uv     = inUv + (vec2(x, y) - 1.5f) * blur.texel;
            const float weight = 1.0f / (1.0f + 40.0f * abs(ViewZ(uv) - centerZ) / max(abs(centerZ), 1e-3f));
            sum += texture(rawOcclusion, uv).r * weight;
            weights += weight;
        }
    }
    outOcclusion = vec4(sum / weights);
}
