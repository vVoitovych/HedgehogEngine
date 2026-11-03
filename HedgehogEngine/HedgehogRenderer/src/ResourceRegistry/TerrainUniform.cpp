#include "TerrainUniform.hpp"

#include <algorithm>
#include <cmath>

namespace HR
{
    namespace
    {
        float Finite(float value, float fallback)
        {
            return std::isfinite(value) ? value : fallback;
        }
    }

    TerrainUniform MakeTerrainUniform(const HedgehogEngine::TerrainView& terrain)
    {
        TerrainUniform uniform{};
        for (uint32_t i = 0; i < HedgehogEngine::TERRAIN_LAYER_VIEW_COUNT; ++i)
        {
            const HedgehogEngine::TerrainLayerView& layer = terrain.layers[i];
            TerrainLayerUniform&                    out   = uniform.Layers[i];
            out.Tiling    = Finite(layer.tiling, 1.0f);
            out.Metallic  = std::clamp(Finite(layer.metallic, 0.0f), 0.0f, 1.0f);
            out.Roughness = std::clamp(Finite(layer.roughness, 1.0f), 0.0f, 1.0f);
            out.Flags     = (layer.baseColor.empty() ? 0u : TERRAIN_LAYER_BASE_COLOR_FLAG) |
                            (layer.normalMap.empty() ? 0u : TERRAIN_LAYER_NORMAL_FLAG);
        }
        return uniform;
    }

    std::vector<RHI::DescriptorBinding> MakeTerrainSetBindings()
    {
        std::vector<RHI::DescriptorBinding> bindings;
        bindings.push_back({ 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Fragment });
        for (uint32_t binding = TERRAIN_SPLAT_BINDING; binding < TERRAIN_BINDING_COUNT; ++binding)
            bindings.push_back({ binding, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment });
        return bindings;
    }
}
