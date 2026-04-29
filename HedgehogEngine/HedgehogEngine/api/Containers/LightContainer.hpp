#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include <vector>

namespace ECS
{
    class ECS;
}

namespace Scene
{
    class LightSystem;
}

namespace HedgehogEngine
{
    class LightContainer
    {
    public:
        HEDGEHOG_ENGINE_API LightContainer();
        HEDGEHOG_ENGINE_API void UpdateLights(const ECS::ECS& ecs, const Scene::LightSystem& lightSystem);
        HEDGEHOG_ENGINE_API size_t GetLightCount() const;
        HEDGEHOG_ENGINE_API const std::vector<FD::LightData>& GetLights() const;

    private:
        std::vector<FD::LightData> m_Lights;
        size_t                     m_LightCount = 0;
    };
}
