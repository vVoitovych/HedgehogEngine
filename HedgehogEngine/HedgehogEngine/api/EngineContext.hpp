#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <memory>

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

    class EngineContext
    {
    public:
        HEDGEHOG_ENGINE_API EngineContext();
        HEDGEHOG_ENGINE_API ~EngineContext();

        HEDGEHOG_ENGINE_API void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt,
                                                bool tickGameLogic);

        HEDGEHOG_ENGINE_API ResourceCatalog&       GetResourceCatalog();
        HEDGEHOG_ENGINE_API const ResourceCatalog& GetResourceCatalog() const;

        HEDGEHOG_ENGINE_API SceneManager&       GetSceneManager();
        HEDGEHOG_ENGINE_API const SceneManager& GetSceneManager() const;

        HEDGEHOG_ENGINE_API const FrameData&         GetFrameData()         const;

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

        FrameData m_FrameData;

        std::unique_ptr<HedgehogSettings::Settings>                    m_Settings;
        std::unique_ptr<EcsSerialization::ComponentSerializerRegistry> m_ComponentRegistry;

        // Constructed after ECS/systems/component-registry are ready (it creates the scene root
        // and needs live system references) — see EngineContext.cpp for the ordering.
        std::unique_ptr<SceneManager> m_SceneManager;
    };
}
