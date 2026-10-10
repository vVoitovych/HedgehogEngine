#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <string>

namespace HedgehogEngine
{
    // How a material's alpha (the base colour map's alpha times baseColorFactor's times transparency)
    // is used: glTF's OPAQUE, MASK and BLEND.
    enum class MaterialType
    {
        Opaque,      // alpha ignored
        Cutoff,      // a fragment whose alpha is below alphaCutoff is discarded
        Transparent  // alpha-blended over what is behind it
    };

    // The texture maps of glTF's metallic-roughness model, in the order a material's slots are listed.
    enum class MaterialTextureSlot
    {
        BaseColor,
        Normal,
        MetallicRoughness, // glTF's channels: G roughness, B metallic
        Occlusion,         // R
        Emissive,
        Count
    };

    inline constexpr size_t MATERIAL_TEXTURE_SLOT_COUNT = static_cast<size_t>(MaterialTextureSlot::Count);

    // A .material file (MaterialSerializer). Every field has a default, which a key the file lacks
    // keeps: a plain dielectric with no map but its base colour.
    struct MaterialData
    {
        std::string path;

        MaterialType type = MaterialType::Opaque;
        std::string  baseColor; // the base colour map
        float        transparency = 1.0f;
        float        alphaCutoff  = 0.5f;  // Cutoff only, in [0, 1]
        bool         doubleSided  = false; // drawn without back-face culling

        HM::Vector4 baseColorFactor = HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // linear RGBA
        float       metallic        = 0.0f;
        float       roughness       = 0.5f;
        std::string metallicRoughnessMap;
        std::string normalMap;
        float       normalScale = 1.0f;
        std::string occlusionMap;
        float       occlusionStrength = 1.0f;
        std::string emissiveMap;
        HM::Vector3 emissiveFactor = HM::Vector3(0.0f, 0.0f, 0.0f); // linear RGB

        bool isDirty = false;
    };

    // The map of one slot; empty for none.
    inline std::string& GetMaterialTexture(MaterialData& material, MaterialTextureSlot slot)
    {
        switch (slot)
        {
        case MaterialTextureSlot::Normal:            return material.normalMap;
        case MaterialTextureSlot::MetallicRoughness: return material.metallicRoughnessMap;
        case MaterialTextureSlot::Occlusion:         return material.occlusionMap;
        case MaterialTextureSlot::Emissive:          return material.emissiveMap;
        default:                                     return material.baseColor;
        }
    }

    inline const std::string& GetMaterialTexture(const MaterialData& material, MaterialTextureSlot slot)
    {
        return GetMaterialTexture(const_cast<MaterialData&>(material), slot);
    }
}
