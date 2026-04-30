#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"

#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/api/Containers/LightContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"

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
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogEngine/api/ECS/GameObjectHelpers.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

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

        m_ECS.RegisterComponent<TransformComponent>();
        m_ECS.RegisterComponent<ECS::HierarchyComponent>();
        m_ECS.RegisterComponent<MeshComponent>();
        m_ECS.RegisterComponent<LightComponent>();
        m_ECS.RegisterComponent<RenderComponent>();
        m_ECS.RegisterComponent<ScriptComponent>();

        m_TransformSystem = m_ECS.RegisterSystem<TransformSystem>();
        m_HierarchySystem = m_ECS.RegisterSystem<HierarchySystem>();
        m_MeshSystem      = m_ECS.RegisterSystem<MeshSystem>();
        m_LightSystem     = m_ECS.RegisterSystem<LightSystem>();
        m_RenderSystem    = m_ECS.RegisterSystem<RenderSystem>();
        m_ScriptSystem    = m_ECS.RegisterSystem<ScriptSystem>();

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

        signature.set(m_ECS.GetComponentType<ScriptComponent>());
        m_ECS.SetSystemSignature<ScriptSystem>(signature);

        m_RootEntity = CreateSceneRoot(m_ECS, *m_HierarchySystem);

        RegisterComponents();
    }

    void EngineContext::RegisterComponents()
    {
        m_ComponentRegistry = std::make_unique<EcsSerialization::ComponentSerializerRegistry>();

        m_ComponentRegistry->RegisterVisitable<TransformComponent>("TransformComponent");
        m_ComponentRegistry->RegisterVisitable<MeshComponent>("MeshComponent");
        m_ComponentRegistry->RegisterVisitable<RenderComponent>("RenderComponent");

        // LightComponent: RegisterCustom to handle the "LightIntencity" backward-compat typo
        m_ComponentRegistry->RegisterCustom("LightComponent",
            [](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
            {
                EcsSerialization::ComponentSerializerRegistry::SerializeWithVisit<LightComponent>(out, ecs, e, "LightComponent");
            },
            [](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
            {
                EcsSerialization::ComponentSerializerRegistry::DeserializeWithVisit<LightComponent>(ecs, e, node);
                auto& comp = ecs.GetComponent<LightComponent>(e);
                if (!node["LightIntensity"] && node["LightIntencity"])
                    comp.m_Intensity = node["LightIntencity"].as<float>();
            },
            [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<LightComponent>(e); }
        );

        // ScriptComponent: RegisterCustom to handle m_Params and InitScript
        ScriptSystem* scriptSys = m_ScriptSystem.get();
        m_ComponentRegistry->RegisterCustom("ScriptComponent",
            [](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
            {
                ScriptComponent& script = ecs.GetComponent<ScriptComponent>(e);
                out << YAML::Key << "ScriptComponent" << YAML::BeginMap;
                EcsSerialization::YamlWriter w{out};
                script.Visit(w);
                if (!script.m_Params.empty())
                {
                    out << YAML::Key << "ScriptParams" << YAML::BeginMap;
                    for (const auto& [name, param] : script.m_Params)
                    {
                        out << YAML::Key << name << YAML::BeginMap;
                        out << YAML::Key << "ParamType" << YAML::Value << static_cast<size_t>(param.type);
                        switch (param.type)
                        {
                        case ParamType::Boolean:
                            out << YAML::Key << "ParamValue" << YAML::Value << std::get<bool>(param.value);
                            break;
                        case ParamType::Number:
                            out << YAML::Key << "ParamValue" << YAML::Value << std::get<float>(param.value);
                            break;
                        default: break;
                        }
                        out << YAML::EndMap;
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndMap;
            },
            [scriptSys](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
            {
                EcsSerialization::ComponentSerializerRegistry::DeserializeWithVisit<ScriptComponent>(ecs, e, node);
                ScriptComponent& script = ecs.GetComponent<ScriptComponent>(e);
                const YAML::Node params = node["ScriptParams"];
                if (params && params.IsMap())
                {
                    for (const auto& param : params)
                    {
                        std::string               paramName = param.first.as<std::string>();
                        const YAML::Node          data      = param.second;
                        ParamType                 type      = static_cast<ParamType>(data["ParamType"].as<size_t>());
                        std::variant<bool, float> value;
                        switch (type)
                        {
                        case ParamType::Boolean: value = data["ParamValue"].as<bool>();  break;
                        case ParamType::Number:  value = data["ParamValue"].as<float>(); break;
                        default: break;
                        }
                        script.m_Params[paramName] = { type, value, false };
                    }
                }
                scriptSys->InitScript(e, ecs);
            },
            [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<ScriptComponent>(e); }
        );
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

        auto materialTypeLookup = [this](uint64_t index) -> MaterialType
        {
            const auto& data = m_MaterialContainer->GetMaterialDataByIndex(index);
            return data.type;
        };

        FrameDataBuilder builder;
        m_FrameData = builder.Build(
            m_ECS, *m_LightSystem, *m_RenderSystem,
            *m_Camera, dt, materialTypeLookup);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const     { return *m_MeshContainer; }
    const TextureContainer& EngineContext::GetTextureContainer() const { return *m_TextureContainer; }
    TextureContainer& EngineContext::GetTextureContainer()             { return *m_TextureContainer; }
    const LightContainer& EngineContext::GetLightContainer() const     { return *m_LightContainer; }
    const MaterialContainer& EngineContext::GetMaterialContainer() const { return *m_MaterialContainer; }
    MaterialContainer& EngineContext::GetMaterialContainer()             { return *m_MaterialContainer; }

    const FrameData& EngineContext::GetFrameData() const { return m_FrameData; }

    HedgehogSettings::Settings& EngineContext::GetSettings()             { return *m_Settings; }
    const HedgehogSettings::Settings& EngineContext::GetSettings() const { return *m_Settings; }

    const Camera& EngineContext::GetCamera() const { return *m_Camera; }

    ECS::ECS&   EngineContext::GetECS()            { return m_ECS; }
    ECS::Entity EngineContext::GetRootEntity() const { return m_RootEntity; }
    std::string EngineContext::GetSceneName()  const { return m_SceneName; }

    TransformSystem*  EngineContext::GetTransformSystem()  const { return m_TransformSystem.get(); }
    HierarchySystem*  EngineContext::GetHierarchySystem()  const { return m_HierarchySystem.get(); }
    MeshSystem*       EngineContext::GetMeshSystem()       const { return m_MeshSystem.get(); }
    LightSystem*      EngineContext::GetLightSystem()      const { return m_LightSystem.get(); }
    RenderSystem*     EngineContext::GetRenderSystem()     const { return m_RenderSystem.get(); }
    ScriptSystem*     EngineContext::GetScriptSystem()     const { return m_ScriptSystem.get(); }

    void EngineContext::LoadScene(const std::string& filePath)
    {
        DeleteGameObjectAndChildren(m_ECS, m_RootEntity);
        EcsSerialization::EcsSerializer::Deserialize(*m_ComponentRegistry, m_ECS, m_RootEntity, m_SceneName, filePath);
        m_HierarchySystem->SetRoot(m_RootEntity);
        m_MeshSystem->Update(m_ECS);
        m_RenderSystem->UpdateSystem(m_ECS);
    }

    void EngineContext::SaveScene(const std::string& filePath)
    {
        const std::string sceneName = std::filesystem::path(filePath).stem().string();
        m_SceneName = sceneName;
        EcsSerialization::EcsSerializer::Serialize(*m_ComponentRegistry, m_ECS, m_RootEntity, m_SceneName, filePath);
    }

    void EngineContext::ResetScene()
    {
        DeleteGameObjectAndChildren(m_ECS, m_RootEntity);
        m_RootEntity = CreateSceneRoot(m_ECS, *m_HierarchySystem);
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

        if (inputState.m_KeyQ) posOffset.z() = -1.0f;
        if (inputState.m_KeyE) posOffset.z() =  1.0f;
        if (inputState.m_KeyW) posOffset.x() =  1.0f;
        if (inputState.m_KeyS) posOffset.x() = -1.0f;
        if (inputState.m_KeyD) posOffset.y() = -1.0f;
        if (inputState.m_KeyA) posOffset.y() =  1.0f;

        m_Camera->UpdateCamera(dt, aspectRatio, posOffset, dirOffset);
    }
}
