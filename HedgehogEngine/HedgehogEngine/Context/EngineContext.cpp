#include "EngineContext.hpp"
#include "WindowContext.hpp"

#include "HedgehogEngine/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogEngine/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogEngine/Containers/LightContainer/LightContainer.hpp"
#include "HedgehogEngine/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogEngine/Containers/MaterialContainer/MaterialData.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Camera.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/HierarchyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "HedgehogEngine/api/ECS/GameObjectHelpers.hpp"
#include "HedgehogEngine/api/ECS/SceneSerializer.hpp"

#include "HedgehogEngine/src/Frame/FrameDataBuilder.hpp"

#include <filesystem>

namespace HedgehogEngine
{
    EngineContext::EngineContext()
        : m_RootEntity(0)
        , m_SceneName("Default")
    {
        m_Camera = std::make_unique<Camera>();
        InitECS();

        m_MeshContainer = std::make_unique<MeshContainer>();
        m_MeshContainer->Update(*m_MeshSystem);

        m_TextureContainer  = std::make_unique<TextureContainer>();
        m_LightContainer    = std::make_unique<LightContainer>();
        m_LightContainer->UpdateLights(m_ECS, *m_LightSystem);

        m_MaterialContainer = std::make_unique<MaterialContainer>();

        m_Settings = std::make_unique<HedgehogSettings::Settings>();
    }

    EngineContext::~EngineContext()
    {
    }

    void EngineContext::InitECS()
    {
        m_ECS.Init();

        m_ECS.RegisterComponent<Scene::TransformComponent>();
        m_ECS.RegisterComponent<Scene::HierarchyComponent>();
        m_ECS.RegisterComponent<Scene::MeshComponent>();
        m_ECS.RegisterComponent<Scene::LightComponent>();
        m_ECS.RegisterComponent<Scene::RenderComponent>();
        m_ECS.RegisterComponent<Scene::ScriptComponent>();

        m_TransformSystem = m_ECS.RegisterSystem<Scene::TransformSystem>();
        m_HierarchySystem = m_ECS.RegisterSystem<Scene::HierarchySystem>();
        m_MeshSystem      = m_ECS.RegisterSystem<Scene::MeshSystem>();
        m_LightSystem     = m_ECS.RegisterSystem<Scene::LightSystem>();
        m_RenderSystem    = m_ECS.RegisterSystem<Scene::RenderSystem>();
        m_ScriptSystem    = m_ECS.RegisterSystem<Scene::ScriptSystem>();

        ECS::Signature signature;

        signature.set(m_ECS.GetComponentType<Scene::TransformComponent>());
        m_ECS.SetSystemSignature<Scene::TransformSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<Scene::HierarchyComponent>());
        m_ECS.SetSystemSignature<Scene::HierarchySystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<Scene::MeshComponent>());
        m_ECS.SetSystemSignature<Scene::MeshSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<Scene::LightComponent>());
        m_ECS.SetSystemSignature<Scene::LightSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<Scene::RenderComponent>());
        m_ECS.SetSystemSignature<Scene::RenderSystem>(signature);
        signature.reset();

        signature.set(m_ECS.GetComponentType<Scene::ScriptComponent>());
        m_ECS.SetSystemSignature<Scene::ScriptSystem>(signature);

        m_RootEntity = Components::CreateSceneRoot(m_ECS, *m_HierarchySystem);
    }

    void EngineContext::UpdateContext(WindowContext& windowContext, float aspectRatio, float dt)
    {
        UpdateCamera(windowContext, aspectRatio, dt);

        // Update order is load-bearing: Script → Transform → Hierarchy → Light
        m_ScriptSystem->Update(m_ECS, dt);
        m_TransformSystem->Update(m_ECS);
        m_HierarchySystem->Update(m_ECS);
        m_LightSystem->Update(m_ECS);

        m_LightContainer->UpdateLights(m_ECS, *m_LightSystem);
        m_MaterialContainer->Update(*m_RenderSystem);
        m_MeshContainer->Update(*m_MeshSystem);

        auto materialTypeLookup = [this](uint64_t index) -> FD::MaterialType
        {
            const auto& data = m_MaterialContainer->GetMaterialDataByIndex(index);
            return static_cast<FD::MaterialType>(static_cast<int>(data.type));
        };

        FD::FrameDataBuilder builder;
        m_FrameData = builder.Build(
            m_ECS, *m_LightSystem, *m_RenderSystem,
            *m_Camera, dt, materialTypeLookup);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return *m_MeshContainer;
    }

    const TextureContainer& EngineContext::GetTextureContainer() const
    {
        return *m_TextureContainer;
    }

    TextureContainer& EngineContext::GetTextureContainer()
    {
        return *m_TextureContainer;
    }

    const LightContainer& EngineContext::GetLightContainer() const
    {
        return *m_LightContainer;
    }

    const MaterialContainer& EngineContext::GetMaterialContainer() const
    {
        return *m_MaterialContainer;
    }

    MaterialContainer& EngineContext::GetMaterialContainer()
    {
        return *m_MaterialContainer;
    }

    const FD::FrameData& EngineContext::GetFrameData() const
    {
        return m_FrameData;
    }

    HedgehogSettings::Settings& EngineContext::GetSettings()
    {
        return *m_Settings;
    }

    const HedgehogSettings::Settings& EngineContext::GetSettings() const
    {
        return *m_Settings;
    }

    const Camera& EngineContext::GetCamera() const
    {
        return *m_Camera;
    }

    ECS::ECS& EngineContext::GetECS()
    {
        return m_ECS;
    }

    ECS::Entity EngineContext::GetRootEntity() const
    {
        return m_RootEntity;
    }

    std::string EngineContext::GetSceneName() const
    {
        return m_SceneName;
    }

    Scene::TransformSystem* EngineContext::GetTransformSystem() const
    {
        return m_TransformSystem.get();
    }

    Scene::HierarchySystem* EngineContext::GetHierarchySystem() const
    {
        return m_HierarchySystem.get();
    }

    Scene::MeshSystem* EngineContext::GetMeshSystem() const
    {
        return m_MeshSystem.get();
    }

    Scene::LightSystem* EngineContext::GetLightSystem() const
    {
        return m_LightSystem.get();
    }

    Scene::RenderSystem* EngineContext::GetRenderSystem() const
    {
        return m_RenderSystem.get();
    }

    Scene::ScriptSystem* EngineContext::GetScriptSystem() const
    {
        return m_ScriptSystem.get();
    }

    void EngineContext::LoadScene(const std::string& filePath)
    {
        Components::DeleteGameObjectAndChildren(m_ECS, m_RootEntity);
        Components::SceneSerializer::DeserializeScene(m_ECS, m_RootEntity, m_SceneName, filePath,
            *m_HierarchySystem, *m_ScriptSystem);
        m_MeshSystem->Update(m_ECS);
        m_RenderSystem->UpdateSystem(m_ECS);
    }

    void EngineContext::SaveScene(const std::string& filePath)
    {
        const std::string sceneName = std::filesystem::path(filePath).stem().string();
        m_SceneName = sceneName;
        Components::SceneSerializer::SerializeScene(m_ECS, m_RootEntity, m_SceneName, filePath);
    }

    void EngineContext::ResetScene()
    {
        Components::DeleteGameObjectAndChildren(m_ECS, m_RootEntity);
        m_RootEntity = Components::CreateSceneRoot(m_ECS, *m_HierarchySystem);
        m_SceneName  = "Default";
    }

    void EngineContext::SetSceneName(const std::string& name)
    {
        m_SceneName = name;
    }

    void EngineContext::UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt)
    {
        const auto& inputState = windowContext.GetWindow().GetInputState();

        HM::Vector3 posOffset(0.0f, 0.0f, 0.0f);
        HM::Vector2 dirOffset(inputState.m_MouseDelta.x(), inputState.m_MouseDelta.y());

        if (inputState.m_KeyQ)
            posOffset.z() = -1.0f;
        if (inputState.m_KeyE)
            posOffset.z() = 1.0f;
        if (inputState.m_KeyW)
            posOffset.x() = 1.0f;
        if (inputState.m_KeyS)
            posOffset.x() = -1.0f;
        if (inputState.m_KeyD)
            posOffset.y() = -1.0f;
        if (inputState.m_KeyA)
            posOffset.y() = 1.0f;

        m_Camera->UpdateCamera(dt, aspectRatio, posOffset, dirOffset);
    }
}
