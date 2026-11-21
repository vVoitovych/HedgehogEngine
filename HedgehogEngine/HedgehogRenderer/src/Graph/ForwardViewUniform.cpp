#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include "HedgehogMath/api/Common.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
    ForwardViewUniform MakeForwardViewUniform(const GraphFrameData& frame, uint32_t viewportWidth, uint32_t viewportHeight)
    {
        ForwardViewUniform uniform;
        uniform.View            = frame.View;
        uniform.ViewProj        = frame.Proj * frame.View;
        uniform.EyePosition     = frame.EyePosition;
        uniform.InverseViewport = HM::Vector4(viewportWidth > 0 ? 1.0f / static_cast<float>(viewportWidth) : 0.0f,
                                              viewportHeight > 0 ? 1.0f / static_cast<float>(viewportHeight) : 0.0f,
                                              0.0f, 0.0f);
        return uniform;
    }

    SceneLightsUniform MakeSceneLightsUniform(std::span<const HX::RenderLight> lights)
    {
        SceneLightsUniform uniform;
        const size_t count = std::min(lights.size(), static_cast<size_t>(HedgehogEngine::MAX_LIGHTS_COUNT));
        for (size_t i = 0; i < count; ++i)
        {
            const HX::RenderLight& light = lights[i];
            GpuLight& gpu = uniform.Lights[i];
            gpu.Position  = light.Position;
            gpu.Direction = light.Direction;
            gpu.Color     = light.Color;
            gpu.Data      = HM::Vector4(static_cast<float>(light.Type), light.Intensity, light.Radius,
                                        std::cos(HM::ToRadians(light.ConeAngle)));
        }
        uniform.LightCount = static_cast<int32_t>(count);
        return uniform;
    }
}
