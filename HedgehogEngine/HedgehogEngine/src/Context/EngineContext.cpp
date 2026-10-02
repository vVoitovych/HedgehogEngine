#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Camera.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Scene/ScriptComponentSerializer.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>

namespace HedgehogEngine
{
    EngineContext::EngineContext()
        : m_ResourceCatalog(m_FileSystem)
    {
        InitFileSystem();

        m_Camera = std::make_unique<Camera>();
        InitECS();

        // SceneManager creates the scene root on construction, so it must come after InitECS
        // (systems + component registry ready) and must be the only scene-root creator —
        // InitECS used to call CreateSceneRoot() itself; that call was removed to avoid a double root.
        m_SceneManager = std::make_unique<SceneManager>(
            m_ECS, m_EventBus, m_FileSystem, *m_ComponentRegistry,
            *m_TransformSystem, *m_MeshSystem, *m_RenderSystem);

        m_ResourceCatalog.Update(*m_RenderSystem, *m_MeshSystem);

        m_Settings = std::make_unique<HedgehogSettings::Settings>();
    }

    EngineContext::~EngineContext()
    {
        // Systems hear that play ends, but the scene is going away, so it is not restored.
        if (m_PlayState != PlayState::Edit)
            m_ECS.NotifyPlayStop();
    }

    void EngineContext::InitFileSystem()
    {
        auto fileSystem = std::make_unique<FS::FileSystem>();
        const std::filesystem::path root = FS::GetEngineRootDirectory();

        const bool okEngine  = fileSystem->RegisterPath("engine://",  root);
        const bool okAssets  = fileSystem->RegisterPath("assets://",  root / "Assets");
        const bool okShaders = fileSystem->RegisterPath("shaders://",
            root / "HedgehogEngine" / "HedgehogRenderer" / "assets" / "Shaders");

        if (!okEngine || !okAssets || !okShaders)
            LOGERROR("EngineContext::InitFileSystem: one or more mount points failed to register — file I/O will be broken.");
        assert(okEngine  && "engine:// mount failed");
        assert(okAssets  && "assets:// mount failed");
        assert(okShaders && "shaders:// mount failed");

        m_FileSystem.Register(std::move(fileSystem));
    }

    void EngineContext::InitECS()
    {
        m_ECS.Init();

        m_ECS.RegisterComponent<TransformComponent>();
        m_ECS.RegisterComponent<ECS::HierarchyComponent>();
        m_ECS.RegisterComponent<MeshComponent>();
        m_ECS.RegisterComponent<LightComponent>();
        m_ECS.RegisterComponent<RenderComponent>();
        m_ECS.RegisterComponent<ScriptComponent>();
        m_ECS.RegisterComponent<CameraComponent>();
        m_ECS.RegisterComponent<AnimatorComponent>();
        m_ECS.RegisterComponent<UiCanvasComponent>();
        m_ECS.RegisterComponent<UiRectComponent>();
        m_ECS.RegisterComponent<UiImageComponent>();
        m_ECS.RegisterComponent<UiTextComponent>();
        m_ECS.RegisterComponent<UiButtonComponent>();

        m_TransformSystem = m_ECS.RegisterSystem<TransformSystem>();
        m_HierarchySystem = m_ECS.RegisterSystem<HierarchySystem>();
        m_MeshSystem      = m_ECS.RegisterSystem<MeshSystem>();
        m_LightSystem     = m_ECS.RegisterSystem<LightSystem>();
        m_RenderSystem    = m_ECS.RegisterSystem<RenderSystem>();
        m_CameraSystem    = m_ECS.RegisterSystem<CameraSystem>();
        m_AnimationSystem = m_ECS.RegisterSystem<AnimationSystem>();
        m_UiSystem        = m_ECS.RegisterSystem<UiSystem>();

        m_TransformSystem->Init(m_EventBus);
        m_HierarchySystem->Init(m_EventBus);
        m_LightSystem->Init(m_EventBus);

        ECS::Signature signature;

        signature.set(m_ECS.GetComponentType<TransformComponent>());
        m_ECS.SetSystemSignature<TransformSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<ECS::HierarchyComponent>());
        m_ECS.SetSystemSignature<HierarchySystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<MeshComponent>());
        m_ECS.SetSystemSignature<MeshSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<LightComponent>());
        m_ECS.SetSystemSignature<LightSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<RenderComponent>());
        m_ECS.SetSystemSignature<RenderSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<CameraComponent>());
        m_ECS.SetSystemSignature<CameraSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<AnimatorComponent>());
        signature.set(m_ECS.GetComponentType<MeshComponent>());
        m_ECS.SetSystemSignature<AnimationSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<UiCanvasComponent>());
        m_ECS.SetSystemSignature<UiSystem>(signature);

        RegisterComponents();
    }

    void EngineContext::RegisterComponents()
    {
        m_ComponentRegistry = std::make_unique<EcsSerialization::ComponentSerializerRegistry>();

        m_ComponentRegistry->RegisterReflected<TransformComponent>("TransformComponent");
        m_ComponentRegistry->RegisterReflected<MeshComponent>("MeshComponent");
        m_ComponentRegistry->RegisterReflected<RenderComponent>("RenderComponent");
        m_ComponentRegistry->RegisterReflected<LightComponent>("LightComponent");
        m_ComponentRegistry->RegisterReflected<CameraComponent>("CameraComponent");
        m_ComponentRegistry->RegisterReflected<AnimatorComponent>("AnimatorComponent");
        m_ComponentRegistry->RegisterReflected<UiCanvasComponent>("UiCanvasComponent");
        m_ComponentRegistry->RegisterReflected<UiRectComponent>("UiRectComponent");
        m_ComponentRegistry->RegisterReflected<UiImageComponent>("UiImageComponent");
        m_ComponentRegistry->RegisterReflected<UiTextComponent>("UiTextComponent");
        m_ComponentRegistry->RegisterReflected<UiButtonComponent>("UiButtonComponent");

        // Scripts run in the application's script system; loading a scene only reads the data.
        RegisterScriptComponentSerializer(*m_ComponentRegistry);
    }

    void EngineContext::UpdateContext(WindowContext& windowContext, float aspectRatio, float dt)
    {
        UpdateCamera(windowContext, aspectRatio, dt);

        // Update order is load-bearing: gameplay (Play only) → Animation → Transform → Hierarchy
        // → Light. Animation runs after every script hook of the frame (see AnimationSystem), and
        // it, Transform, Hierarchy and Light run in every mode, so edits show in Edit mode too.
        UpdatePlayMode(dt);
        UpdateAnimation(dt);
        m_TransformSystem->Update(m_ECS, m_EventBus);
        m_HierarchySystem->Update(m_ECS, m_EventBus);
        m_LightSystem->Update(m_ECS);

        m_ResourceCatalog.Update(*m_RenderSystem, *m_MeshSystem);
    }

    bool EngineContext::Play()
    {
        if (m_PlayState != PlayState::Edit)
            return false;

        m_PlaySnapshot = m_SceneManager->CaptureSnapshot();
        ResetFixedStepClock(m_Clock);
        m_PlayState = PlayState::Playing;
        m_ECS.NotifyPlayStart();
        return true;
    }

    bool EngineContext::Pause()
    {
        if (m_PlayState != PlayState::Playing)
            return false;

        m_PlayState = PlayState::Paused;
        m_ECS.NotifyPlayPause();
        return true;
    }

    bool EngineContext::Resume()
    {
        if (m_PlayState != PlayState::Paused)
            return false;

        m_PlayState = PlayState::Playing;
        m_ECS.NotifyPlayResume();
        return true;
    }

    bool EngineContext::Stop()
    {
        if (m_PlayState == PlayState::Edit)
            return false;

        m_PlayState = PlayState::Edit;
        m_ECS.NotifyPlayStop();
        if (m_PlaySnapshot && !m_SceneManager->RestoreSnapshot(*m_PlaySnapshot))
            LOGERROR("EngineContext::Stop: the scene could not be restored to its state before Play.");
        m_PlaySnapshot.reset();
        return true;
    }

    PlayState EngineContext::GetPlayState() const { return m_PlayState; }

    FixedStepClock&       EngineContext::GetFixedStepClock()       { return m_Clock; }
    const FixedStepClock& EngineContext::GetFixedStepClock() const { return m_Clock; }

    void EngineContext::UpdatePlayMode(float dt)
    {
        if (m_PlayState != PlayState::Playing)
            return;

        const uint32_t steps = AdvanceFixedStepClock(m_Clock, dt);
        for (uint32_t step = 0; step < steps; ++step)
            m_ECS.RunFixedUpdate(m_Clock.FixedDeltaTime);
        // Scaled like the fixed steps; a negative scale counts as 0, as the clock treats it.
        m_ECS.RunUpdate(std::max(dt * m_Clock.TimeScale, 0.0f));
    }

    void EngineContext::UpdateAnimation(float dt)
    {
        // The same scaled time OnUpdate got; nothing advances while paused.
        const float scaled = m_PlayState == PlayState::Playing ? std::max(dt * m_Clock.TimeScale, 0.0f) : 0.0f;
        m_AnimationSystem->Update(m_ECS, m_ResourceCatalog.GetMeshContainer(), m_EventBus, m_PlayState != PlayState::Edit,
                                  scaled);
    }

    ResourceCatalog& EngineContext::GetResourceCatalog()             { return m_ResourceCatalog; }
    const ResourceCatalog& EngineContext::GetResourceCatalog() const { return m_ResourceCatalog; }

    SceneManager& EngineContext::GetSceneManager()             { return *m_SceneManager; }
    const SceneManager& EngineContext::GetSceneManager() const { return *m_SceneManager; }

    const FS::FileSystemManager& EngineContext::GetFileSystem() const { return m_FileSystem; }

    EventBus& EngineContext::GetEventBus() { return m_EventBus; }

    HedgehogSettings::Settings& EngineContext::GetSettings()             { return *m_Settings; }
    const HedgehogSettings::Settings& EngineContext::GetSettings() const { return *m_Settings; }

    const Camera& EngineContext::GetCamera() const { return *m_Camera; }

    ECS::ECS& EngineContext::GetECS() { return m_ECS; }

    TransformSystem*  EngineContext::GetTransformSystem()  const { return m_TransformSystem.get(); }
    HierarchySystem*  EngineContext::GetHierarchySystem()  const { return m_HierarchySystem.get(); }
    MeshSystem*       EngineContext::GetMeshSystem()       const { return m_MeshSystem.get(); }
    LightSystem*      EngineContext::GetLightSystem()      const { return m_LightSystem.get(); }
    RenderSystem*     EngineContext::GetRenderSystem()     const { return m_RenderSystem.get(); }
    CameraSystem*     EngineContext::GetCameraSystem()     const { return m_CameraSystem.get(); }
    AnimationSystem*  EngineContext::GetAnimationSystem()  const { return m_AnimationSystem.get(); }
    UiSystem*         EngineContext::GetUiSystem()         const { return m_UiSystem.get(); }

    void EngineContext::UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt)
    {
        const auto& inputState = windowContext.GetWindow().GetInputState();

        HM::Vector3 posOffset(0.0f, 0.0f, 0.0f);
        HM::Vector2 dirOffset(inputState.MouseDelta.x(), inputState.MouseDelta.y());

        if (inputState.KeyQ) posOffset.z() = -1.0f;
        if (inputState.KeyE) posOffset.z() =  1.0f;
        if (inputState.KeyW) posOffset.x() =  1.0f;
        if (inputState.KeyS) posOffset.x() = -1.0f;
        if (inputState.KeyD) posOffset.y() = -1.0f;
        if (inputState.KeyA) posOffset.y() =  1.0f;

        m_Camera->UpdateCamera(dt, aspectRatio, posOffset, dirOffset);
    }
}
