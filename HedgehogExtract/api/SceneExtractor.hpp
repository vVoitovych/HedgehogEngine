#pragma once

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class RenderSystem;
    class LightSystem;
    class CameraSystem;
}

namespace HX
{
    struct RenderScene;

    // The one type that reads the ECS read-only and writes a RenderScene (RENDERING.md
    // section 3.1). After extraction, nothing downstream touches the ECS again.
    //
    // Deliberately single-threaded and stateless — safe to construct per call.
    class SceneExtractor
    {
    public:
        // Does not call outScene.Clear() itself: callers reuse one RenderScene across frames
        // and decide when to clear it (see RenderScene::Clear).
        void Extract(
            const ECS::ECS&                     ecs,
            const HedgehogEngine::RenderSystem&  renderSystem,
            const HedgehogEngine::LightSystem&   lightSystem,
            const HedgehogEngine::CameraSystem&  cameraSystem,
            RenderScene&                         outScene) const;

    private:
        void ExtractInstances(const ECS::ECS& ecs, const HedgehogEngine::RenderSystem& renderSystem,
                               RenderScene& outScene) const;
        void ExtractLights(const ECS::ECS& ecs, const HedgehogEngine::LightSystem& lightSystem,
                            RenderScene& outScene) const;
        void ExtractCameras(const ECS::ECS& ecs, const HedgehogEngine::CameraSystem& cameraSystem,
                             RenderScene& outScene) const;
    };
}
