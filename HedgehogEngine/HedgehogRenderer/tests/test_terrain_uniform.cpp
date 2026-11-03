#include "doctest/doctest/doctest.h"

#include "../src/ResourceRegistry/TerrainUniform.hpp"

#include <cstddef>
#include <limits>
#include <set>

using HR::TerrainLayerUniform;
using HR::TerrainUniform;

TEST_CASE("Terrain uniform - its std140 layout: four layers of tiling, metallic, roughness and flags")
{
    CHECK(sizeof(TerrainLayerUniform) == 16);
    CHECK(offsetof(TerrainLayerUniform, Tiling) == 0);
    CHECK(offsetof(TerrainLayerUniform, Metallic) == 4);
    CHECK(offsetof(TerrainLayerUniform, Roughness) == 8);
    CHECK(offsetof(TerrainLayerUniform, Flags) == 12);
    CHECK(offsetof(TerrainUniform, Layers) == 0);
    CHECK(sizeof(TerrainUniform) == 64);
}

TEST_CASE("Terrain uniform - each layer's values and a flag per map it names")
{
    HedgehogEngine::TerrainView terrain;
    terrain.layers[0] = { "Textures/Grass.png", "Textures/Grass_Normal.png", 16.0f, 0.0f, 0.9f };
    terrain.layers[1] = { "Textures/Rock.png", "", 4.0f, 0.25f, 0.6f };
    terrain.layers[2] = { "", "Textures/Sand_Normal.png", 8.0f, 0.0f, 1.0f };

    const TerrainUniform uniform = HR::MakeTerrainUniform(terrain);
    CHECK(uniform.Layers[0].Tiling == 16.0f);
    CHECK(uniform.Layers[0].Roughness == 0.9f);
    CHECK(uniform.Layers[0].Flags == (HR::TERRAIN_LAYER_BASE_COLOR_FLAG | HR::TERRAIN_LAYER_NORMAL_FLAG));
    CHECK(uniform.Layers[1].Metallic == 0.25f);
    CHECK(uniform.Layers[1].Flags == HR::TERRAIN_LAYER_BASE_COLOR_FLAG);
    CHECK(uniform.Layers[2].Flags == HR::TERRAIN_LAYER_NORMAL_FLAG);
    // An untouched layer: its defaults, no maps.
    CHECK(uniform.Layers[3].Tiling == 1.0f);
    CHECK(uniform.Layers[3].Metallic == 0.0f);
    CHECK(uniform.Layers[3].Roughness == 1.0f);
    CHECK(uniform.Layers[3].Flags == 0u);
}

TEST_CASE("Terrain uniform - non-finite values give the defaults and metallic and roughness are clamped")
{
    const float                 nan = std::numeric_limits<float>::quiet_NaN();
    HedgehogEngine::TerrainView terrain;
    terrain.layers[0] = { "", "", nan, nan, nan };
    terrain.layers[1] = { "", "", 2.0f, -1.0f, 5.0f };

    const TerrainUniform uniform = HR::MakeTerrainUniform(terrain);
    CHECK(uniform.Layers[0].Tiling == 1.0f);
    CHECK(uniform.Layers[0].Metallic == 0.0f);
    CHECK(uniform.Layers[0].Roughness == 1.0f);
    CHECK(uniform.Layers[1].Tiling == 2.0f);
    CHECK(uniform.Layers[1].Metallic == 0.0f);
    CHECK(uniform.Layers[1].Roughness == 1.0f);
}

TEST_CASE("Terrain uniform - the set's bindings: the uniform, the splat map and two maps per layer")
{
    const std::vector<RHI::DescriptorBinding> bindings = HR::MakeTerrainSetBindings();
    REQUIRE(bindings.size() == HR::TERRAIN_BINDING_COUNT);
    CHECK(HR::TERRAIN_BINDING_COUNT == 10);
    CHECK(bindings[0].Type == RHI::DescriptorType::UniformBuffer);
    std::set<uint32_t> numbers;
    for (const RHI::DescriptorBinding& binding : bindings)
    {
        numbers.insert(binding.Binding);
        CHECK(binding.Count == 1);
        CHECK(binding.Stages == RHI::ShaderStage::Fragment);
        if (binding.Binding != 0)
            CHECK(binding.Type == RHI::DescriptorType::CombinedImageSampler);
    }
    CHECK(numbers.size() == HR::TERRAIN_BINDING_COUNT);
    CHECK(*numbers.rbegin() == HR::TERRAIN_BINDING_COUNT - 1);
    CHECK(HR::TERRAIN_NORMAL_BINDING == HR::TERRAIN_BASE_COLOR_BINDING + 4);
}
