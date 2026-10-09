#ifndef PBR
#define PBR

// glTF's metallic-roughness model, lit by the scene's punctual lights: a Lambert diffuse lobe and
// a Cook-Torrance specular lobe (GGX distribution, height-correlated Smith visibility, Schlick
// Fresnel), as Filament and the glTF sample viewer define them; and by the scene's environment,
// with the split-sum approximation (AmbientLight).
//
// Light units follow the "pi convention": a light's colour times its intensity is the radiance a
// white Lambert surface facing it shows (at 1 m for a point or spot light), so scenes lit before
// PBR keep roughly their brightness.

#include "Common/LightData.glsl"

const float PI = 3.14159265359f;

// Below this roughness the GGX lobe is narrower than a half-float can resolve.
const float MIN_ROUGHNESS = 0.045f;
// Point and spot lights are treated as no closer than this (in m) for the inverse square.
const float MIN_LIGHT_DISTANCE = 0.1f;
// A spot light's cone fades in over the outermost fifth of its half angle's cosine range.
const float SPOT_FADE = 0.2f;

// A shaded point: what the material and the geometry say about it, in world space.
struct PbrSurface
{
    vec3  position;
    vec3  normal;    // unit, facing the viewer's side
    vec3  view;      // unit, from the surface towards the eye
    vec3  baseColor; // linear
    float metallic;
    float roughness; // perceptual, squared for GGX
};

// The GGX (Trowbridge-Reitz) normal distribution, alpha = roughness^2.
float DistributionGgx(float NdotH, float alpha)
{
    const float a2 = alpha * alpha;
    const float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// The height-correlated Smith visibility term, G / (4 NdotL NdotV).
float VisibilitySmithGgxCorrelated(float NdotV, float NdotL, float alpha)
{
    const float a2    = alpha * alpha;
    const float ggxV  = NdotL * sqrt(NdotV * NdotV * (1.0f - a2) + a2);
    const float ggxL  = NdotV * sqrt(NdotL * NdotL * (1.0f - a2) + a2);
    return 0.5f / max(ggxV + ggxL, 1e-5f);
}

vec3 FresnelSchlick(float VdotH, vec3 f0)
{
    const float f = pow(1.0f - VdotH, 5.0f);
    return f0 + (vec3(1.0f) - f0) * f;
}

// The reflectance at normal incidence: 4 % for a dielectric, the base colour for a metal.
vec3 SpecularColor(PbrSurface surface)
{
    return mix(vec3(0.04f), surface.baseColor, surface.metallic);
}

// The BRDF times the cosine of the incoming light, for unit vector toLight: what one unit of
// incoming radiance from that direction reflects towards the viewer. A metal has no diffuse lobe.
vec3 EvaluateBrdf(PbrSurface surface, vec3 toLight)
{
    const float NdotL = dot(surface.normal, toLight);
    if (NdotL <= 0.0f)
        return vec3(0.0f);

    const vec3  halfway = normalize(surface.view + toLight);
    const float NdotV   = max(dot(surface.normal, surface.view), 1e-4f);
    const float NdotH   = clamp(dot(surface.normal, halfway), 0.0f, 1.0f);
    const float VdotH   = clamp(dot(surface.view, halfway), 0.0f, 1.0f);
    const float rough   = clamp(surface.roughness, MIN_ROUGHNESS, 1.0f);
    const float alpha   = rough * rough;

    const vec3 specular = DistributionGgx(NdotH, alpha) * VisibilitySmithGgxCorrelated(NdotV, NdotL, alpha)
                        * FresnelSchlick(VdotH, SpecularColor(surface));
    const vec3 diffuse  = surface.baseColor * (1.0f - surface.metallic) / PI;
    return (diffuse + specular) * NdotL;
}

// 1 well inside radius, falling smoothly to 0 at it (a radius of 0 never fades), so a light's
// reach ends where its Radius says.
float RangeWindow(float distanceSquared, float radius)
{
    if (radius <= 0.0f)
        return 1.0f;
    const float ratio  = distanceSquared / (radius * radius);
    const float window = clamp(1.0f - ratio * ratio, 0.0f, 1.0f);
    return window * window;
}

// The radiance light sends towards position, and the unit direction towards it. A directional
// light's direction points towards the light; a spot light shines along its direction.
vec3 IncomingRadiance(Light light, vec3 position, out vec3 toLight)
{
    const vec3 radiance = light.color * light.data.y * PI;
    if (light.data.x < 0.5f) // directional
    {
        toLight = normalize(light.direction);
        return radiance;
    }

    const vec3  offset          = light.position - position;
    const float distanceSquared = max(dot(offset, offset), MIN_LIGHT_DISTANCE * MIN_LIGHT_DISTANCE);
    toLight = offset * inversesqrt(max(dot(offset, offset), 1e-8f));
    float attenuation = RangeWindow(distanceSquared, light.data.z) / distanceSquared;

    if (light.data.x >= 1.5f) // spot: data.w is the cosine of its half angle
    {
        const float cosOuter = light.data.w;
        const float cosInner = mix(cosOuter, 1.0f, SPOT_FADE);
        attenuation *= smoothstep(cosOuter, cosInner, dot(-toLight, normalize(light.direction)));
    }
    return radiance * attenuation;
}

// The light the surface reflects towards the viewer from one light.
vec3 ShadeLight(Light light, PbrSurface surface)
{
    vec3       toLight;
    const vec3 radiance = IncomingRadiance(light, surface.position, toLight);
    return EvaluateBrdf(surface, toLight) * radiance;
}

// ---- Image-based lighting: the scene's environment as ambient light ----

// Renderer::EnvironmentUniform (std140): ContentLoader's bake of the environment map.
layout(set = 3, binding = 2) uniform EnvironmentData
{
    vec4  sh[9];       // SH9 irradiance over pi, times the intensity (rgb)
    float intensity;   // the prefiltered radiance's scale; 0 without an environment
    float rotationCos; // the environment's turn about +Z
    float rotationSin;
    float maxMip;      // the radiance cube's last mip, roughness 1
} environmentData;

// The radiance cube prefiltered for GGX, mip m for roughness m / maxMip, faces in world space.
layout(set = 3, binding = 3) uniform samplerCube radianceCube;
// The split-sum table: N.V along x, roughness along y; the scale and bias applied to F0.
layout(set = 3, binding = 4) uniform sampler2D brdfLut;

// A world direction in the environment's own frame: turned back by its rotation about +Z.
vec3 EnvironmentDirection(vec3 direction)
{
    const float c = environmentData.rotationCos;
    const float s = environmentData.rotationSin;
    return vec3(c * direction.x + s * direction.y, -s * direction.x + c * direction.y, direction.z);
}

// The radiance a white Lambert surface facing unit normal n reflects (ContentLoader's
// EvaluateShIrradiance: the same basis, the same coefficient order).
vec3 EnvironmentIrradiance(vec3 n)
{
    const vec3 d = EnvironmentDirection(n);
    return environmentData.sh[0].rgb * 0.282095f
         + environmentData.sh[1].rgb * (0.488603f * d.y)
         + environmentData.sh[2].rgb * (0.488603f * d.z)
         + environmentData.sh[3].rgb * (0.488603f * d.x)
         + environmentData.sh[4].rgb * (1.092548f * d.x * d.y)
         + environmentData.sh[5].rgb * (1.092548f * d.y * d.z)
         + environmentData.sh[6].rgb * (0.315392f * (3.0f * d.z * d.z - 1.0f))
         + environmentData.sh[7].rgb * (1.092548f * d.x * d.z)
         + environmentData.sh[8].rgb * (0.546274f * (d.x * d.x - d.y * d.y));
}

// The environment's light the surface reflects towards the viewer, times occlusion (which glTF
// applies to ambient light only): the SH irradiance through the diffuse lobe, and the prefiltered
// radiance along the reflection, at the surface's roughness, through the split-sum table.
vec3 AmbientLight(PbrSurface surface, float occlusion)
{
    const float NdotV       = max(dot(surface.normal, surface.view), 1e-4f);
    const float rough       = clamp(surface.roughness, MIN_ROUGHNESS, 1.0f);
    const vec2  scaleBias   = texture(brdfLut, vec2(NdotV, rough)).rg;
    const vec3  reflected   = reflect(-surface.view, surface.normal);
    const vec3  prefiltered = textureLod(radianceCube, EnvironmentDirection(reflected),
                                         rough * environmentData.maxMip).rgb * environmentData.intensity;

    const vec3 specular = prefiltered * (SpecularColor(surface) * scaleBias.x + scaleBias.y);
    const vec3 diffuse  = surface.baseColor * (1.0f - surface.metallic) * max(EnvironmentIrradiance(surface.normal), vec3(0.0f));
    return (diffuse + specular) * occlusion;
}

#endif
