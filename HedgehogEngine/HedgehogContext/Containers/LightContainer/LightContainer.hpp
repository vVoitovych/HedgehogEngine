#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include "FrameData/FrameData.hpp"

#include <vector>

namespace Scene
{
    class Scene;
}

namespace Context
{
    class LightContainer
    {
    public:
        HEDGEHOG_CONTEXT_API LightContainer();
        HEDGEHOG_CONTEXT_API void UpdateLights(const Scene::Scene& scene);
        HEDGEHOG_CONTEXT_API size_t GetLightCount() const;
        HEDGEHOG_CONTEXT_API const std::vector<FD::LightData>& GetLights() const;

    private:
        std::vector<FD::LightData> m_Lights;
        size_t                     m_LightCount = 0;
    };
}
