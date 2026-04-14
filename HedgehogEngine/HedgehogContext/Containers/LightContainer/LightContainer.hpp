#pragma once

#include "Light.hpp"

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
        const std::vector<Light>& GetLights() const;
    private:
        std::vector<Light> m_Lights;

        size_t m_CachedLightComponentCount;
        size_t m_LightCount;
    };

}

