#version 450

// The forward pass's surfaces, rigid and skinned: glTF metallic-roughness PBR under the scene's
// lights, the sun shadowed through the cascaded atlas, and the scene's environment as ambient light,
// written as HDR radiance (the ToneMap pass maps it to the view's colour). Without an environment
// there is no ambient term, so a surface no light reaches is black.

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inWorldPosition;
layout(location = 3) in vec4 inTangent; // xyz world tangent, w handedness

#define MAX_LIGHTS_COUNT 16

#include "Common/Pbr.glsl"
#include "Common/Shadows.glsl"

layout(set = 0, binding = 0) uniform ViewData
{
    mat4 view;
    mat4 viewProj;
    vec4 eyePos;
} viewData;

// ResourceRegistry's MaterialUniform (std140); a slot without a map samples its neutral default.
layout(set = 1, binding = 0) uniform MaterialData
{
    vec4  baseColorFactor;
    vec4  emissiveFactor;
    float metallic;
    float roughness;
    float normalScale;
    float occlusionStrength;
    float transparency;
    uint  textureFlags;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D baseColorMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 4) uniform sampler2D occlusionMap;
layout(set = 1, binding = 5) uniform sampler2D emissiveMap;

// MaterialTextureBinding's bits in textureFlags.
const uint NORMAL_MAP_FLAG = 1u << 1;

layout(set = 2, binding = 0) uniform SceneLights
{
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} sceneLights;

layout(location = 0) out vec4 outColor;

// The interpolated normal, bent by the normal map through the tangent basis when the material
// has one (its xy scaled by normalScale, as glTF defines it).
vec3 SurfaceNormal()
{
    const vec3 normal = normalize(inNormal.xyz);
    if ((materialData.textureFlags & NORMAL_MAP_FLAG) == 0u)
        return normal;

    const vec3 tangent   = normalize(inTangent.xyz - normal * dot(normal, inTangent.xyz));
    const vec3 bitangent = cross(normal, tangent) * (inTangent.w < 0.0f ? -1.0f : 1.0f);
    vec3 sampled = texture(normalMap, fragTexCoord).xyz * 2.0f - 1.0f;
    sampled.xy  *= materialData.normalScale;
    return normalize(mat3(tangent, bitangent, normal) * sampled);
}

void main()
{
    const vec4 baseColor         = texture(baseColorMap, fragTexCoord) * materialData.baseColorFactor;
    const vec4 metallicRoughness = texture(metallicRoughnessMap, fragTexCoord);

    const vec3 geometricNormal = normalize(inNormal.xyz);

    PbrSurface surface;
    surface.position  = inWorldPosition.xyz;
    surface.normal    = SurfaceNormal();
    surface.view      = normalize(viewData.eyePos.xyz - inWorldPosition.xyz);
    surface.baseColor = baseColor.rgb;
    surface.metallic  = clamp(materialData.metallic * metallicRoughness.b, 0.0f, 1.0f);
    surface.roughness = clamp(materialData.roughness * metallicRoughness.g, 0.0f, 1.0f);

    vec3 radiance = vec3(0.0f);
    for (int i = 0; i < sceneLights.lightCount; ++i)
    {
        vec3 light = ShadeLight(sceneLights.lights[i], surface);
        if (i == shadowData.lightIndex)
            light *= SunShadow(surface.position, geometricNormal, normalize(sceneLights.lights[i].direction));
        radiance += light;
    }

    // The occlusion map darkens ambient light only, as glTF defines it.
    const float occlusion = 1.0f + materialData.occlusionStrength * (texture(occlusionMap, fragTexCoord).r - 1.0f);
    radiance += AmbientLight(surface, occlusion);

    radiance += materialData.emissiveFactor.rgb * texture(emissiveMap, fragTexCoord).rgb;
    outColor = vec4(radiance, 1.0f);
}
