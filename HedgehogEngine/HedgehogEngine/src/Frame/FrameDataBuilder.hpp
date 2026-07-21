#pragma once

#include "HedgehogCommon/api/Frame/FrameData.hpp"
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
    class CameraSystem;
    class Camera;

    class FrameDataBuilder
    {
    public:
        FrameData Build(
            const ECS::ECS&                              ecs,
            const LightSystem&                           lightSystem,
            const RenderSystem&                          renderSystem,
            const CameraSystem&                          cameraSystem,
            const Camera&                                camera,
            float                                        gameAspectRatio,
            float                                        deltaTime,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup) const;

    private:
        void BuildDrawList(
            const ECS::ECS&                              ecs,
            const RenderSystem&                          renderSystem,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup,
            DrawList&                                    outDrawList) const;

        // Resolves the primary CameraComponent (if any) into a view/projection for the game view.
        std::optional<CameraData> BuildGameCamera(
            const ECS::ECS&     ecs,
            const CameraSystem& cameraSystem,
            float               aspectRatio) const;
    };
}
