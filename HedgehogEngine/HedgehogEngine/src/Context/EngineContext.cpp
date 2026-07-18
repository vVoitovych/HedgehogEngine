#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/PathUtils.hpp"

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

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "HedgehogEngine/src/Frame/FrameDataBuilder.hpp"

#include "Logger/api/Logger.hpp"

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

        m_ResourceCatalog.Update(m_ECS, *m_LightSystem, *m_RenderSystem, *m_MeshSystem);

        m_Settings = std::make_unique<HedgehogSettings::Settings>();
    }

    EngineContext::~EngineContext()
    {
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

        m_TransformSystem = m_ECS.RegisterSystem<TransformSystem>();
        m_HierarchySystem = m_ECS.RegisterSystem<HierarchySystem>();
        m_MeshSystem      = m_ECS.RegisterSystem<MeshSystem>();
        m_LightSystem     = m_ECS.RegisterSystem<LightSystem>();
        m_RenderSystem    = m_ECS.RegisterSystem<RenderSystem>();
        m_ScriptSystem    = m_ECS.RegisterSystem<ScriptSystem>();

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

        signature.set(m_ECS.GetComponentType<ScriptComponent>());
        m_ECS.SetSystemSignature<ScriptSystem>(signature);

        RegisterComponents();
    }

    void EngineContext::RegisterComponents()
    {
        m_ComponentRegistry = std::make_unique<EcsSerialization::ComponentSerializerRegistry>();

        m_ComponentRegistry->RegisterReflected<TransformComponent>("TransformComponent");
        m_ComponentRegistry->RegisterReflected<MeshComponent>("MeshComponent");
        m_ComponentRegistry->RegisterReflected<RenderComponent>("RenderComponent");
        m_ComponentRegistry->RegisterReflected<LightComponent>("LightComponent");

        // ScriptComponent: RegisterCustom to handle m_Params and InitScript
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
            [scriptSystem = m_ScriptSystem, this](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
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
                scriptSystem->InitScript(e, ecs, m_EventBus, m_FileSystem);
            },
            [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<ScriptComponent>(e); }
        );
    }

    void EngineContext::UpdateContext(WindowContext& windowContext, float aspectRatio, float dt)
    {
        UpdateCamera(windowContext, aspectRatio, dt);

        // Update order is load-bearing: Script → Transform → Hierarchy → Light
        m_ScriptSystem->Update(m_ECS, dt, m_EventBus);
        m_TransformSystem->Update(m_ECS, m_EventBus);
        m_HierarchySystem->Update(m_ECS, m_EventBus);
        m_LightSystem->Update(m_ECS);

        m_ResourceCatalog.Update(m_ECS, *m_LightSystem, *m_RenderSystem, *m_MeshSystem);

        auto materialTypeLookup = [this](uint64_t index) -> MaterialType
        {
            const auto& data = m_ResourceCatalog.GetMaterialContainer().GetMaterialDataByIndex(index);
            return data.type;
        };

        FrameDataBuilder builder;
        m_FrameData = builder.Build(
            m_ECS, *m_LightSystem, *m_RenderSystem,
            *m_Camera, dt, materialTypeLookup);
    }

    ResourceCatalog& EngineContext::GetResourceCatalog()             { return m_ResourceCatalog; }
    const ResourceCatalog& EngineContext::GetResourceCatalog() const { return m_ResourceCatalog; }

    SceneManager& EngineContext::GetSceneManager()             { return *m_SceneManager; }
    const SceneManager& EngineContext::GetSceneManager() const { return *m_SceneManager; }

    const FrameData& EngineContext::GetFrameData() const { return m_FrameData; }

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
    ScriptSystem*     EngineContext::GetScriptSystem()     const { return m_ScriptSystem.get(); }

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
