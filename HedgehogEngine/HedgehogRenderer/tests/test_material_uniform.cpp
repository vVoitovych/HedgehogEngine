#include "doctest/doctest/doctest.h"

#include "../src/ResourceRegistry/MaterialUniform.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include <cstddef>
#include <string>

using HR::MaterialTextureBinding;
using HR::MaterialUniform;

namespace
{
    // A material's values, kept alive for the MaterialView that refers into them.
    struct MaterialValues
    {
        std::string BaseColor;
        std::string MetallicRoughness;
        std::string Normal;
        std::string Occlusion;
        std::string Emissive;

        HedgehogEngine::MaterialView View(float transparency = 1.0f) const
        {
            return { transparency,
                     BaseColor,
                     false,
                     HM::Vector4(0.5f, 0.25f, 1.0f, 0.75f),
                     0.8f,
                     0.3f,
                     MetallicRoughness,
                     Normal,
                     0.6f,
                     Occlusion,
                     0.4f,
                     Emissive,
                     HM::Vector3(2.0f, 1.0f, 0.5f) };
        }
    };
}

TEST_CASE("Material uniform - its std140 offsets match Base.frag's block")
{
    CHECK(offsetof(MaterialUniform, BaseColorFactor) == 0);
    CHECK(offsetof(MaterialUniform, EmissiveFactor) == 16);
    CHECK(offsetof(MaterialUniform, Metallic) == 32);
    CHECK(offsetof(MaterialUniform, Roughness) == 36);
    CHECK(offsetof(MaterialUniform, NormalScale) == 40);
    CHECK(offsetof(MaterialUniform, OcclusionStrength) == 44);
    CHECK(offsetof(MaterialUniform, Transparency) == 48);
    CHECK(offsetof(MaterialUniform, TextureFlags) == 52);
    CHECK(sizeof(MaterialUniform) == 64);
}

TEST_CASE("Material uniform - the factors are packed and a flag is set per named map")
{
    MaterialValues values;
    values.BaseColor = "Textures/brick.png";
    values.Normal    = "Textures/brick_n.png";
    values.Emissive  = "Textures/brick_e.png";

    const MaterialUniform uniform = HR::MakeMaterialUniform(values.View(0.25f));
    CHECK(uniform.BaseColorFactor[0] == 0.5f);
    CHECK(uniform.BaseColorFactor[1] == 0.25f);
    CHECK(uniform.BaseColorFactor[2] == 1.0f);
    CHECK(uniform.BaseColorFactor[3] == 0.75f);
    CHECK(uniform.EmissiveFactor[0] == 2.0f);
    CHECK(uniform.EmissiveFactor[1] == 1.0f);
    CHECK(uniform.EmissiveFactor[2] == 0.5f);
    CHECK(uniform.EmissiveFactor[3] == 0.0f);
    CHECK(uniform.Metallic == 0.8f);
    CHECK(uniform.Roughness == 0.3f);
    CHECK(uniform.NormalScale == 0.6f);
    CHECK(uniform.OcclusionStrength == 0.4f);
    CHECK(uniform.Transparency == 0.25f);
    CHECK(uniform.TextureFlags == ((1u << 0) | (1u << 1) | (1u << 4)));
}

TEST_CASE("Material uniform - a material with no maps sets no flag")
{
    const MaterialValues values;
    CHECK(HR::MakeMaterialUniform(values.View()).TextureFlags == 0);
}

TEST_CASE("Material uniform - each slot names its map, and only colour maps are sRGB")
{
    MaterialValues values;
    values.BaseColor         = "b.png";
    values.Normal            = "n.png";
    values.MetallicRoughness = "mr.png";
    values.Occlusion         = "ao.png";
    values.Emissive          = "e.png";
    const HedgehogEngine::MaterialView view = values.View();
    CHECK(HR::GetMaterialTexturePath(view, MaterialTextureBinding::BaseColor) == "b.png");
    CHECK(HR::GetMaterialTexturePath(view, MaterialTextureBinding::Normal) == "n.png");
    CHECK(HR::GetMaterialTexturePath(view, MaterialTextureBinding::MetallicRoughness) == "mr.png");
    CHECK(HR::GetMaterialTexturePath(view, MaterialTextureBinding::Occlusion) == "ao.png");
    CHECK(HR::GetMaterialTexturePath(view, MaterialTextureBinding::Emissive) == "e.png");

    CHECK(HR::IsColorTexture(MaterialTextureBinding::BaseColor));
    CHECK(HR::IsColorTexture(MaterialTextureBinding::Emissive));
    CHECK_FALSE(HR::IsColorTexture(MaterialTextureBinding::Normal));
    CHECK_FALSE(HR::IsColorTexture(MaterialTextureBinding::MetallicRoughness));
    CHECK_FALSE(HR::IsColorTexture(MaterialTextureBinding::Occlusion));
    CHECK(HR::MATERIAL_TEXTURE_BINDING_COUNT == HedgehogEngine::MAX_TEXTURES_PER_MATERIAL);
}
