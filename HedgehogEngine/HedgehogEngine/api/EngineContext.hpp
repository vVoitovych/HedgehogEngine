#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <memory>
#include <optional>

namespace HedgehogSettings
{
    class Settings;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
}

namespace HedgehogEngine
{
    class WindowContext;
    class Camera;

    class TransformSystem;
    class HierarchySystem;
    class MeshSystem;
    class LightSystem;
    class RenderSystem;
    class ScriptSystem;
    class CameraSystem;

    // Edit: gameplay does not run. Playing: every frame runs the fixed steps and the update.
    // Paused: nothing runs, and the scene waits to be resumed or stopped.
    enum class PlayState
    {
        Edit,
        Playing,
        Paused
    };

    class EngineContext
    {
    public:
        HEDGEHOG_ENGINE_API EngineContext();
        HEDGEHOG_ENGINE_API ~EngineContext();

        HEDGEHOG_ENGINE_API void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt);

        // Play mode. EngineContext owns the state, the scene snapshot and the clock; the ECS
        // only forwards the events to its systems. Each call returns false and does nothing
        // in the wrong state.
        //   Play   (from Edit):            snapshot the scene, reset the clock, OnPlayStart.
        //   Pause  (from Playing):         OnPlayPause.
        //   Resume (from Paused):          OnPlayResume.
        //   Stop   (from Playing/Paused):  OnPlayStop in reverse order, then restore the snapshot.
        HEDGEHOG_ENGINE_API bool Play();
        HEDGEHOG_ENGINE_API bool Pause();
        HEDGEHOG_ENGINE_API bool Resume();
        HEDGEHOG_ENGINE_API bool Stop();

        [[nodiscard]] HEDGEHOG_ENGINE_API PlayState GetPlayState() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API FixedStepClock&       GetFixedStepClock();
        [[nodiscard]] HEDGEHOG_ENGINE_API const FixedStepClock& GetFixedStepClock() const;

        // One frame of gameplay, only while Playing: the clock's fixed steps of OnFixedUpdate,
        // then one OnUpdate with the scaled frame time. UpdateContext calls it; it is public
        // so tests can drive frames without a window.
        HEDGEHOG_ENGINE_API void UpdatePlayMode(float dt);

        HEDGEHOG_ENGINE_API ResourceCatalog&       GetResourceCatalog();
        HEDGEHOG_ENGINE_API const ResourceCatalog& GetResourceCatalog() const;

        HEDGEHOG_ENGINE_API SceneManager&       GetSceneManager();
        HEDGEHOG_ENGINE_API const SceneManager& GetSceneManager() const;

        HEDGEHOG_ENGINE_API HedgehogSettings::Settings&       GetSettings();
        HEDGEHOG_ENGINE_API const HedgehogSettings::Settings& GetSettings() const;

        HEDGEHOG_ENGINE_API const Camera& GetCamera() const;

        HEDGEHOG_ENGINE_API EventBus&            GetEventBus();

        HEDGEHOG_ENGINE_API ECS::ECS&           GetECS();
        HEDGEHOG_ENGINE_API TransformSystem*    GetTransformSystem() const;
        HEDGEHOG_ENGINE_API HierarchySystem*    GetHierarchySystem() const;
        HEDGEHOG_ENGINE_API MeshSystem*         GetMeshSystem()      const;
        HEDGEHOG_ENGINE_API LightSystem*        GetLightSystem()     const;
        HEDGEHOG_ENGINE_API RenderSystem*       GetRenderSystem()    const;
        HEDGEHOG_ENGINE_API ScriptSystem*       GetScriptSystem()    const;
        HEDGEHOG_ENGINE_API CameraSystem*       GetCameraSystem()    const;

        HEDGEHOG_ENGINE_API const FS::FileSystemManager& GetFileSystem() const;

    private:
        void InitECS();
        void InitFileSystem();
        void RegisterComponents();
        void UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt);

    private:
        FS::FileSystemManager m_FileSystem;

        EventBus m_EventBus;

        std::unique_ptr<Camera> m_Camera;

        ECS::ECS m_ECS;

        std::shared_ptr<TransformSystem>  m_TransformSystem;
        std::shared_ptr<HierarchySystem>  m_HierarchySystem;
        std::shared_ptr<MeshSystem>       m_MeshSystem;
        std::shared_ptr<LightSystem>      m_LightSystem;
        std::shared_ptr<RenderSystem>     m_RenderSystem;
        std::shared_ptr<ScriptSystem>     m_ScriptSystem;
        std::shared_ptr<CameraSystem>     m_CameraSystem;

        ResourceCatalog m_ResourceCatalog;

        std::unique_ptr<HedgehogSettings::Settings>                    m_Settings;
        std::unique_ptr<EcsSerialization::ComponentSerializerRegistry> m_ComponentRegistry;

        // Constructed after ECS/systems/component-registry are ready (it creates the scene root
        // and needs live system references) — see EngineContext.cpp for the ordering.
        std::unique_ptr<SceneManager> m_SceneManager;

        PlayState                    m_PlayState = PlayState::Edit;
        FixedStepClock               m_Clock;
        std::optional<SceneSnapshot> m_PlaySnapshot;
    };
}
