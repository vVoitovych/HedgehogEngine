// A material's set (ResourceRegistry's MaterialUniform, std140, then its maps from binding 1), at
// the set MATERIAL_SET names: the forward pass's set 1, the cutoff depth shaders' 1 or 2. A slot
// without a map samples its neutral default. Only the uniform and the base colour map are declared
// here; a shader reading the other maps declares them after.
#ifndef MATERIAL_SET
#error "Define MATERIAL_SET before including Common/Material.glsl"
#endif

layout(set = MATERIAL_SET, binding = 0) uniform MaterialData
{
    vec4  baseColorFactor;
    vec4  emissiveFactor;
    float metallic;
    float roughness;
    float normalScale;
    float occlusionStrength;
    float transparency;
    uint  textureFlags;
    float alphaCutoff;
    uint  alphaMode; // HedgehogEngine::MaterialAlphaMode
} materialData;

layout(set = MATERIAL_SET, binding = 1) uniform sampler2D baseColorMap;

const uint ALPHA_MODE_OPAQUE      = 0u;
const uint ALPHA_MODE_CUTOFF      = 1u;
const uint ALPHA_MODE_TRANSPARENT = 2u;

// The base colour at uv: its map times the factor, with the alpha times the material's transparency.
vec4 MaterialBaseColor(vec2 uv)
{
    vec4 color = texture(baseColorMap, uv) * materialData.baseColorFactor;
    color.a *= materialData.transparency;
    return color;
}

// Whether a fragment of this alpha is cut out: a Cutoff material's below its cutoff.
bool IsCutOut(float alpha)
{
    return materialData.alphaMode == ALPHA_MODE_CUTOFF && alpha < materialData.alphaCutoff;
}
