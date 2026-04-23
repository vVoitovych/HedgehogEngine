#pragma once

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
        LightContainer();
        void UpdateLights(const Scene::Scene& scene);
        size_t GetLightCount() const;
        const std::vector<FD::LightData>& GetLights() const;

    private:
        std::vector<FD::LightData> m_Lights;
        size_t                     m_LightCount = 0;
    };
}
