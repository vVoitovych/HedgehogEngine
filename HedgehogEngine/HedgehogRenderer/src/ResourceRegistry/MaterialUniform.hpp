#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include <cstdint>

namespace HR
{
    // A material's texture slots, in the order of the forward pipelines' set 1 bindings after the
    // uniform (binding 1 + slot): glTF's metallic-roughness maps.
    enum class MaterialTextureBinding : uint32_t
    {
        BaseColor         = 0, // sRGB
        Normal            = 1, // linear
        MetallicRoughness = 2, // linear: G roughness, B metallic
        Occlusion         = 3, // linear: R
        Emissive          = 4, // sRGB
        Count             = 5,
    };

    inline constexpr uint32_t MATERIAL_TEXTURE_BINDING_COUNT = static_cast<uint32_t>(MaterialTextureBinding::Count);

    // Set 1, binding 0 of the forward pipelines: the material's factors, std140. A bit of
    // TextureFlags is set for each slot whose map the material names (bit = MaterialTextureBinding);
    // a slot without one is bound to its neutral default, so the shader may sample every slot.
    struct MaterialUniform
    {
        float    BaseColorFactor[4];    // offset 0, linear RGBA
        float    EmissiveFactor[4];     // offset 16, linear RGB; w unused
        float    Metallic;              // offset 32
        float    Roughness;             // offset 36
        float    NormalScale;           // offset 40
        float    OcclusionStrength;     // offset 44
        float    Transparency;          // offset 48
        uint32_t TextureFlags;          // offset 52
        float    AlphaCutoff;           // offset 56
        uint32_t AlphaMode;             // offset 60, HedgehogEngine::MaterialAlphaMode
    };
    static_assert(sizeof(MaterialUniform) == 64, "MaterialUniform must match Base.frag's std140 block");

    [[nodiscard]] MaterialUniform MakeMaterialUniform(const HedgehogEngine::MaterialView& material);

    // The map path of a slot (empty for none).
    [[nodiscard]] const std::string& GetMaterialTexturePath(const HedgehogEngine::MaterialView& material,
                                                            MaterialTextureBinding           slot);

    // Whether a slot's map holds colour (sRGB) rather than data (linear).
    [[nodiscard]] constexpr bool IsColorTexture(MaterialTextureBinding slot)
    {
        return slot == MaterialTextureBinding::BaseColor || slot == MaterialTextureBinding::Emissive;
    }
}
