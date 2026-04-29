#pragma once

#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "MaterialDef.hpp"

#include <functional>
#include <cstdint>

namespace ECS
{
    class ECS;
}

namespace Scene
{
    class LightSystem;
    class RenderSystem;
}

namespace HedgehogEngine
{
    class Camera;
}

namespace FD
{
    class FrameDataBuilder
    {
    public:
        FrameData Build(
            const ECS::ECS&                              ecs,
            const Scene::LightSystem&                    lightSystem,
            const Scene::RenderSystem&                   renderSystem,
            const HedgehogEngine::Camera&                camera,
            float                                        deltaTime,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup) const;

    private:
        void BuildDrawList(
            const ECS::ECS&                              ecs,
            const Scene::RenderSystem&                   renderSystem,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup,
            DrawList&                                    outDrawList) const;
    };
}
