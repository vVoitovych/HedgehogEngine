#version 450

// A terrain tile's surface: four material layers blended by its splat map (one RGBA weight per
// layer, across the tile's UVs 0 to 1), each layer's maps repeated Tiling times across the tile, lit
// as Base.frag lights a material: glTF metallic-roughness PBR under the scene's lights, the sun
// shadowed through the cascaded atlas, the environment as ambient light, written as HDR radiance.

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inWorldPosition;
layout(location = 3) in vec4 inTangent; // xyz world tangent, w handedness

#define MAX_LIGHTS_COUNT 16
#define TERRAIN_LAYER_COUNT 4

#include "Common/Pbr.glsl"
#include "Common/Shadows.glsl"

layout(set = 0, binding = 0) uniform ViewData
{
    mat4 view;
    mat4 viewProj;
    vec4 eyePos;
} viewData;

// HR::TerrainUniform.
struct TerrainLayer
{
    float tiling;
    float metallic;
    float roughness;
    uint  flags;
};

layout(set = 1, binding = 0) uniform TerrainData
{
    TerrainLayer layers[TERRAIN_LAYER_COUNT];
} terrainData;

layout(set = 1, binding = 1) uniform sampler2D splatMap;
layout(set = 1, binding = 2) uniform sampler2D baseColorMap0;
layout(set = 1, binding = 3) uniform sampler2D baseColorMap1;
layout(set = 1, binding = 4) uniform sampler2D baseColorMap2;
layout(set = 1, binding = 5) uniform sampler2D baseColorMap3;
layout(set = 1, binding = 6) uniform sampler2D normalMap0;
layout(set = 1, binding = 7) uniform sampler2D normalMap1;
layout(set = 1, binding = 8) uniform sampler2D normalMap2;
layout(set = 1, binding = 9) uniform sampler2D normalMap3;

// HR::TERRAIN_LAYER_NORMAL_FLAG.
const uint NORMAL_MAP_FLAG = 1u << 1;

layout(set = 2, binding = 0) uniform SceneLights
{
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} sceneLights;

layout(location = 0) out vec4 outColor;

vec3 SampleBaseColor(int layer, vec2 uv)
{
    if (layer == 0) return texture(baseColorMap0, uv).rgb;
    if (layer == 1) return texture(baseColorMap1, uv).rgb;
    if (layer == 2) return texture(baseColorMap2, uv).rgb;
    return texture(baseColorMap3, uv).rgb;
}

// The layer's tangent-space normal, +Z without a normal map.
vec3 SampleNormal(int layer, vec2 uv)
{
    if ((terrainData.layers[layer].flags & NORMAL_MAP_FLAG) == 0u)
        return vec3(0.0f, 0.0f, 1.0f);
    vec4 texel;
    if (layer == 0) texel = texture(normalMap0, uv);
    else if (layer == 1) texel = texture(normalMap1, uv);
    else if (layer == 2) texel = texture(normalMap2, uv);
    else texel = texture(normalMap3, uv);
    return texel.xyz * 2.0f - 1.0f;
}

void main()
{
    // The weights, normalized; a texel painted with nothing shows layer 0.
    vec4        weights = texture(splatMap, fragTexCoord);
    const float total   = weights.r + weights.g + weights.b + weights.a;
    weights = total > 1e-4f ? weights / total : vec4(1.0f, 0.0f, 0.0f, 0.0f);

    vec3  baseColor = vec3(0.0f);
    vec3  tangentNormal = vec3(0.0f);
    float metallic  = 0.0f;
    float roughness = 0.0f;
    for (int layer = 0; layer < TERRAIN_LAYER_COUNT; ++layer)
    {
        const float weight = weights[layer];
        if (weight <= 0.0f)
            continue;
        const vec2 uv = fragTexCoord * terrainData.layers[layer].tiling;
        baseColor     += weight * SampleBaseColor(layer, uv);
        tangentNormal += weight * SampleNormal(layer, uv);
        metallic      += weight * terrainData.layers[layer].metallic;
        roughness     += weight * terrainData.layers[layer].roughness;
    }

    const vec3 geometricNormal = normalize(inNormal.xyz);
    const vec3 tangent         = normalize(inTangent.xyz - geometricNormal * dot(geometricNormal, inTangent.xyz));
    const vec3 bitangent       = cross(geometricNormal, tangent) * (inTangent.w < 0.0f ? -1.0f : 1.0f);

    PbrSurface surface;
    surface.position  = inWorldPosition.xyz;
    surface.normal    = normalize(mat3(tangent, bitangent, geometricNormal) * tangentNormal);
    surface.view      = normalize(viewData.eyePos.xyz - inWorldPosition.xyz);
    surface.baseColor = baseColor;
    surface.metallic  = clamp(metallic, 0.0f, 1.0f);
    surface.roughness = clamp(roughness, 0.0f, 1.0f);

    vec3 radiance = vec3(0.0f);
    for (int i = 0; i < sceneLights.lightCount; ++i)
    {
        vec3 light = ShadeLight(sceneLights.lights[i], surface);
        if (i == shadowData.lightIndex)
            light *= SunShadow(surface.position, geometricNormal, normalize(sceneLights.lights[i].direction));
        radiance += light;
    }
    radiance += AmbientLight(surface, 1.0f);
    outColor = vec4(radiance, 1.0f);
}
