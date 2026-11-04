#include "TerrainBrush.hpp"

#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Editor
{
    float GetBrushWeight(float distance, float radius, float falloff)
    {
        if (!(radius > 0.0f) || distance >= radius)
            return 0.0f;
        const float inner = radius * (1.0f - std::clamp(falloff, 0.0f, 1.0f));
        if (distance <= inner)
            return 1.0f;
        const float t = (radius - distance) / (radius - inner); // 1 at the inner edge, 0 at the radius
        return t * t * (3.0f - 2.0f * t);
    }

    bool ApplyTerrainBrush(HedgehogEngine::TerrainData& data, const TerrainBrush& brush, float x, float y, float dt)
    {
        const uint32_t resolution = data.Resolution;
        if (resolution < 2 || data.Heights.size() != static_cast<size_t>(resolution) * resolution ||
            !(brush.Radius > 0.0f) || !(dt > 0.0f))
            return false;

        // The samples the brush can reach, and one more on every side for Smooth's neighbours.
        const float spacing = HedgehogEngine::GetTerrainSpacing(data);
        const auto  clampCell = [&](float coordinate)
        { return static_cast<int>(std::clamp(coordinate / spacing, 0.0f, static_cast<float>(resolution - 1))); };
        const int first[2] = { clampCell(x - brush.Radius), clampCell(y - brush.Radius) };
        const int last[2]  = { static_cast<int>(std::ceil(std::clamp((x + brush.Radius) / spacing, 0.0f, static_cast<float>(resolution - 1)))),
                               static_cast<int>(std::ceil(std::clamp((y + brush.Radius) / spacing, 0.0f, static_cast<float>(resolution - 1)))) };

        // Smooth reads the heights as they were before this frame.
        std::vector<float> before;
        if (brush.Tool == TerrainBrushTool::Smooth)
            before = data.Heights;
        const auto height = [&](int column, int row)
        {
            column = std::clamp(column, 0, static_cast<int>(resolution) - 1);
            row    = std::clamp(row, 0, static_cast<int>(resolution) - 1);
            return before[static_cast<size_t>(row) * resolution + column];
        };

        bool changed = false;
        for (int row = first[1]; row <= last[1]; ++row)
        {
            for (int column = first[0]; column <= last[0]; ++column)
            {
                const float dx     = column * spacing - x;
                const float dy     = row * spacing - y;
                const float weight = GetBrushWeight(std::sqrt(dx * dx + dy * dy), brush.Radius, brush.Falloff);
                if (weight <= 0.0f)
                    continue;

                float&      sample = data.Heights[static_cast<size_t>(row) * resolution + column];
                const float amount = brush.Strength * dt * weight;
                float       target = sample;
                switch (brush.Tool)
                {
                case TerrainBrushTool::Raise:   target = sample + amount; break;
                case TerrainBrushTool::Lower:   target = sample - amount; break;
                case TerrainBrushTool::Smooth:
                {
                    float sum = 0.0f;
                    for (int dr = -1; dr <= 1; ++dr)
                        for (int dc = -1; dc <= 1; ++dc)
                            sum += height(column + dc, row + dr);
                    target = sample + (sum / 9.0f - sample) * std::min(1.0f, amount);
                    break;
                }
                case TerrainBrushTool::Flatten:
                    target = sample + (brush.FlattenHeight - sample) * std::min(1.0f, amount);
                    break;
                }
                target = std::clamp(target, 0.0f, data.HeightScale);
                if (target != sample)
                {
                    sample  = target;
                    changed = true;
                }
            }
        }
        return changed;
    }

    std::vector<HM::Vector3> MakeBrushRing(const HedgehogEngine::TerrainData& data, float x, float y, float radius,
                                           uint32_t segments)
    {
        std::vector<HM::Vector3> ring;
        ring.reserve(segments);
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(segments);
            const float px    = x + radius * std::cos(angle);
            const float py    = y + radius * std::sin(angle);
            ring.push_back(HM::Vector3(px, py, HedgehogEngine::SampleTerrainHeight(data, px, py)));
        }
        return ring;
    }
}
