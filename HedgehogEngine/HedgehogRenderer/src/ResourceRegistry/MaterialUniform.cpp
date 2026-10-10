#include "MaterialUniform.hpp"

namespace HR
{
    const std::string& GetMaterialTexturePath(const HedgehogEngine::MaterialView& material, MaterialTextureBinding slot)
    {
        switch (slot)
        {
        case MaterialTextureBinding::Normal:            return material.normalMap;
        case MaterialTextureBinding::MetallicRoughness: return material.metallicRoughnessMap;
        case MaterialTextureBinding::Occlusion:         return material.occlusionMap;
        case MaterialTextureBinding::Emissive:          return material.emissiveMap;
        default:                                        return material.baseColor;
        }
    }

    MaterialUniform MakeMaterialUniform(const HedgehogEngine::MaterialView& material)
    {
        MaterialUniform uniform{};
        for (size_t i = 0; i < 4; ++i)
            uniform.BaseColorFactor[i] = material.baseColorFactor[i];
        for (size_t i = 0; i < 3; ++i)
            uniform.EmissiveFactor[i] = material.emissiveFactor[i];
        uniform.Metallic          = material.metallic;
        uniform.Roughness         = material.roughness;
        uniform.NormalScale       = material.normalScale;
        uniform.OcclusionStrength = material.occlusionStrength;
        uniform.Transparency      = material.transparency;
        uniform.AlphaCutoff       = material.alphaCutoff;
        uniform.AlphaMode         = static_cast<uint32_t>(material.alphaMode);
        for (uint32_t slot = 0; slot < MATERIAL_TEXTURE_BINDING_COUNT; ++slot)
            if (!GetMaterialTexturePath(material, static_cast<MaterialTextureBinding>(slot)).empty())
                uniform.TextureFlags |= 1u << slot;
        return uniform;
    }
}
