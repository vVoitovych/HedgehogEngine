#version 450

// Screen-space ambient occlusion (the Ssao pass): each pixel's view-space position and normal are
// rebuilt from the prepass depth, 16 points in the hemisphere over the normal are projected back
// onto the depth, and every point a nearer surface hides counts as occluded, faded out when the
// hiding surface is farther than the radius (so a wall behind an object does not darken it).
// The hemisphere turns per pixel by a hash, so there is no noise texture; the blur evens it out.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D depthMap;

// SsaoPushConstants: clip.x = A x + G, clip.y = B y + H, clip.z = C z + D, clip.w = E z + F.
layout(push_constant) uniform Constants
{
    vec4 projABCD;
    vec4 projEFGH;
    vec4 settings; // radius, intensity, bias
    vec4 texel;    // the depth's texel size
} ssao;

layout(location = 0) out vec4 outOcclusion;

const int   SAMPLE_COUNT  = 16;
const float GOLDEN_ANGLE  = 2.39996323f;
const float TWO_PI        = 6.28318531f;

float ViewZ(float depth)
{
    return (ssao.projABCD.w - depth * ssao.projEFGH.y) / (depth * ssao.projEFGH.x - ssao.projABCD.z);
}

vec3 ViewPosition(vec2 uv)
{
    const float z   = ViewZ(texture(depthMap, uv).r);
    const float w   = ssao.projEFGH.x * z + ssao.projEFGH.y;
    const vec2  ndc = uv * 2.0f - 1.0f;
    return vec3((ndc.x * w - ssao.projEFGH.z) / ssao.projABCD.x, (ndc.y * w - ssao.projEFGH.w) / ssao.projABCD.y, z);
}

vec2 Project(vec3 position)
{
    const float w   = ssao.projEFGH.x * position.z + ssao.projEFGH.y;
    const vec2  ndc = vec2(ssao.projABCD.x * position.x + ssao.projEFGH.z, ssao.projABCD.y * position.y + ssao.projEFGH.w) / w;
    return ndc * 0.5f + 0.5f;
}

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

void main()
{
    if (texture(depthMap, inUv).r >= 1.0f)
    {
        outOcclusion = vec4(1.0f); // the sky
        return;
    }
    const vec3 position = ViewPosition(inUv);

    // The normal from the neighbours on the nearer side in each direction, so an edge's far side
    // does not bend it.
    const vec2 dx    = vec2(ssao.texel.x, 0.0f);
    const vec2 dy    = vec2(0.0f, ssao.texel.y);
    const vec3 right = ViewPosition(inUv + dx) - position;
    const vec3 left  = position - ViewPosition(inUv - dx);
    const vec3 down  = ViewPosition(inUv + dy) - position;
    const vec3 up    = position - ViewPosition(inUv - dy);
    vec3 normal = normalize(cross(abs(right.z) < abs(left.z) ? right : left, abs(down.z) < abs(up.z) ? down : up));
    // Facing the camera: towards the eye for a perspective view, along +Z for an orthographic one.
    const vec3 toEye = ssao.projEFGH.x != 0.0f ? -position : vec3(0.0f, 0.0f, 1.0f);
    if (dot(normal, toEye) < 0.0f)
        normal = -normal;

    const float angle   = Hash(gl_FragCoord.xy) * TWO_PI;
    vec3        across  = vec3(cos(angle), sin(angle), 0.0f);
    vec3        tangent = across - normal * dot(across, normal);
    if (dot(tangent, tangent) < 1e-6f)
        tangent = vec3(0.0f, -sin(angle), cos(angle)) - normal * dot(vec3(0.0f, -sin(angle), cos(angle)), normal);
    tangent              = normalize(tangent);
    const vec3 bitangent = cross(normal, tangent);

    const float radius    = ssao.settings.x;
    const float intensity = ssao.settings.y;
    const float bias      = ssao.settings.z;
    float       occlusion = 0.0f;
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        // Spread over the hemisphere (a spiral), closer to the pixel for the first samples.
        const float t     = (float(i) + 0.5f) / float(SAMPLE_COUNT);
        const float z     = t;
        const float r     = sqrt(1.0f - z * z);
        const float phi   = float(i) * GOLDEN_ANGLE;
        const float scale = mix(0.1f, 1.0f, t * t);
        const vec3  point = position + (tangent * (r * cos(phi)) + bitangent * (r * sin(phi)) + normal * z) * (radius * scale);

        const vec2 uv = Project(point);
        if (any(lessThan(uv, vec2(0.0f))) || any(greaterThan(uv, vec2(1.0f))))
            continue;
        const float sceneZ = ViewZ(texture(depthMap, uv).r);
        const float range  = smoothstep(0.0f, 1.0f, radius / max(abs(position.z - sceneZ), 1e-4f));
        occlusion += (sceneZ >= point.z + bias ? 1.0f : 0.0f) * range;
    }
    outOcclusion = vec4(clamp(1.0f - intensity * occlusion / float(SAMPLE_COUNT), 0.0f, 1.0f));
}
