#pragma once

#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "MaterialDef.hpp"

#include <functional>
#include <cstdint>

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class LightSystem;
    class RenderSystem;
    class Camera;

    class FrameDataBuilder
    {
    public:
        FrameData Build(
            const ECS::ECS&                              ecs,
            const LightSystem&                           lightSystem,
            const RenderSystem&                          renderSystem,
            const Camera&                                camera,
            float                                        deltaTime,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup) const;

    private:
        void BuildDrawList(
            const ECS::ECS&                              ecs,
            const RenderSystem&                          renderSystem,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup,
            DrawList&                                    outDrawList) const;
    };
}
