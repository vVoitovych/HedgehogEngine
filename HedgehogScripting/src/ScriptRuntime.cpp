#include "HedgehogScripting/api/ScriptRuntime.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace HedgehogScripting
{
    using HedgehogEngine::ScriptComponent;
    using HedgehogEngine::TransformComponent;

    namespace
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";

        // The Enable state a component asks for this frame, applied to the component.
        bool TakeDesiredEnable(ScriptComponent& component)
        {
            const bool desired = component.NewEnable.value_or(component.Enable);
            component.Enable   = desired;
            component.NewEnable.reset();
            return desired;
        }
    }

    ScriptRuntime::ScriptRuntime(ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                                 const FS::FileSystemManager& fileSystem)
        : m_ECS(ecs)
        , m_EventBus(eventBus)
        , m_VM(fileSystem)
        , m_Classes(m_VM)
    {
        m_Entities = m_ECS.HasSystem<ScriptedEntities>() ? m_ECS.GetSystem<ScriptedEntities>()
                                                         : m_ECS.RegisterSystem<ScriptedEntities>();
        ECS::Signature signature;
        signature.set(m_ECS.GetComponentType<ScriptComponent>());
        signature.set(m_ECS.GetComponentType<TransformComponent>());
        m_ECS.SetSystemSignature<ScriptedEntities>(signature); // picks up entities that already match

        m_ECS.SetComponentRemovedCallback<ScriptComponent>([this](ECS::Entity entity, ScriptComponent&)
        {
            if (m_Running.find(entity) != m_Running.end())
                Destroy(entity);
        });
    }

    ScriptRuntime::~ScriptRuntime()
    {
        m_ECS.SetComponentRemovedCallback<ScriptComponent>({});
    }

    void ScriptRuntime::OnPlayStart()
    {
        // No instances exist between Plays, so every script can be read afresh.
        m_Classes.Reload();
        const std::vector<ECS::Entity> entities = m_Entities->GetEntities();
        for (ECS::Entity entity : entities)
        {
            const ScriptComponent& component = m_ECS.GetComponent<ScriptComponent>(entity);
            if (component.NewEnable.value_or(component.Enable))
                Instantiate(entity);
        }
    }

    void ScriptRuntime::OnPlayStop()
    {
        std::vector<ECS::Entity> running;
        running.reserve(m_Running.size());
        for (const auto& [entity, script] : m_Running)
            running.push_back(entity);
        for (ECS::Entity entity : running)
            Destroy(entity);
    }

    void ScriptRuntime::Update(float deltaTime)
    {
        const std::vector<ECS::Entity> entities = m_Entities->GetEntities();

        // An entity that lost its transform while keeping its script leaves the set
        // without a removal callback.
        std::vector<ECS::Entity> departed;
        for (const auto& [entity, script] : m_Running)
        {
            if (std::find(entities.begin(), entities.end(), entity) == entities.end())
                departed.push_back(entity);
        }
        for (ECS::Entity entity : departed)
            Destroy(entity);

        for (ECS::Entity entity : entities)
        {
            if (!m_ECS.IsAlive(entity) || !m_ECS.HasComponent<ScriptComponent>(entity))
                continue;
            ScriptComponent& component = m_ECS.GetComponent<ScriptComponent>(entity);
            const bool       enable    = TakeDesiredEnable(component);

            auto it = m_Running.find(entity);
            if (it == m_Running.end())
            {
                if (!enable)
                    continue;
                Instantiate(entity);
                it = m_Running.find(entity);
                if (it == m_Running.end())
                    continue;
            }

            RunningScript& script = it->second;
            if (enable && !script.Enabled)
            {
                if (!script.Started)
                {
                    script.Started = true;
                    script.Instance.Call("OnStart");
                }
                script.Enabled = true;
                script.Instance.Call("OnEnable");
            }
            else if (!enable && script.Enabled)
            {
                script.Enabled = false;
                script.Instance.Call("OnDisable");
            }

            if (script.Enabled)
            {
                ApplyParams(script.Instance, component, true);
                script.Instance.Call("OnUpdate", deltaTime);
            }
        }
    }

    std::unordered_map<std::string, HedgehogEngine::ScriptParam> ScriptRuntime::DescribeScript(
        const std::string& scriptPath)
    {
        std::unordered_map<std::string, HedgehogEngine::ScriptParam> params;
        if (m_Running.empty())
            m_Classes.Reload(); // pick up edits to the file; never while instances depend on the cache
        const std::optional<sol::table> defaults = m_Classes.GetDefaults(ToVirtualPath(scriptPath));
        if (!defaults)
            return params;

        for (const auto& [key, value] : *defaults)
        {
            if (key.get_type() != sol::type::string)
                continue;
            const std::string name = key.as<std::string>();
            if (value.get_type() == sol::type::number)
                params[name] = { HedgehogEngine::ParamType::Number, value.as<float>(), false };
            else if (value.get_type() == sol::type::boolean)
                params[name] = { HedgehogEngine::ParamType::Boolean, value.as<bool>(), false };
        }
        return params;
    }

    ScriptInstance* ScriptRuntime::FindInstance(ECS::Entity entity)
    {
        const auto it = m_Running.find(entity);
        return it == m_Running.end() ? nullptr : &it->second.Instance;
    }

    size_t ScriptRuntime::GetInstanceCount() const
    {
        return m_Running.size();
    }

    ScriptVM& ScriptRuntime::GetVM()
    {
        return m_VM;
    }

    std::string ScriptRuntime::ToVirtualPath(const std::string& scriptPath)
    {
        std::string path = scriptPath;
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.rfind(ASSETS_PREFIX, 0) == 0)
            return path;
        return std::string(ASSETS_PREFIX) + path;
    }

    void ScriptRuntime::Instantiate(ECS::Entity entity)
    {
        ScriptComponent& component = m_ECS.GetComponent<ScriptComponent>(entity);
        if (component.ScriptPath.empty())
            return;

        std::optional<ScriptInstance> instance =
            m_Classes.CreateInstance(ToVirtualPath(component.ScriptPath), entity, EntityName(entity));
        if (!instance)
            return;

        AddLegacyBindings(*instance, entity);
        ApplyParams(*instance, component, false);
        m_Running.emplace(entity, RunningScript{ std::move(*instance) });
    }

    void ScriptRuntime::AddLegacyBindings(ScriptInstance& instance, ECS::Entity entity)
    {
        sol::table environment = instance.GetEnvironment();

        // A vector field of the entity's transform as a {x, y, z} table, or nil if it has none.
        auto get = [this, entity](HM::Vector3 TransformComponent::*field, sol::this_state state) -> sol::object
        {
            if (!m_ECS.IsAlive(entity) || !m_ECS.HasComponent<TransformComponent>(entity))
                return sol::lua_nil;
            const HM::Vector3& value = m_ECS.GetComponent<TransformComponent>(entity).*field;
            sol::state_view    lua(state);
            return lua.create_table_with("x", value.x(), "y", value.y(), "z", value.z());
        };
        auto set = [this, entity](HM::Vector3 TransformComponent::*field, const sol::table& value)
        {
            if (!m_ECS.IsAlive(entity) || !m_ECS.HasComponent<TransformComponent>(entity))
                return;
            HM::Vector3& target = m_ECS.GetComponent<TransformComponent>(entity).*field;
            target.x() = value.get_or("x", target.x());
            target.y() = value.get_or("y", target.y());
            target.z() = value.get_or("z", target.z());
            m_EventBus.Publish(HedgehogEngine::TransformChangedEvent{ entity });
        };

        environment.set_function("GetPosition", [get](sol::this_state s) { return get(&TransformComponent::Position, s); });
        environment.set_function("GetRotation", [get](sol::this_state s) { return get(&TransformComponent::Rotation, s); });
        environment.set_function("SetPosition", [set](const sol::table& v) { set(&TransformComponent::Position, v); });
        environment.set_function("SetRotation", [set](const sol::table& v) { set(&TransformComponent::Rotation, v); });
    }

    void ScriptRuntime::ApplyParams(ScriptInstance& instance, ScriptComponent& component, bool onlyDirty)
    {
        sol::table environment = instance.GetEnvironment();
        for (auto& [name, param] : component.Params)
        {
            if (onlyDirty && !param.dirty)
                continue;
            param.dirty = false;
            switch (param.type)
            {
            case HedgehogEngine::ParamType::Boolean:
                environment[name] = std::get<bool>(param.value);
                break;
            case HedgehogEngine::ParamType::Number:
                environment[name] = std::get<float>(param.value);
                break;
            default:
                break;
            }
        }
    }

    void ScriptRuntime::Destroy(ECS::Entity entity)
    {
        const auto it = m_Running.find(entity);
        if (it == m_Running.end())
            return;
        it->second.Instance.Call("OnDestroy");
        m_Running.erase(entity); // by key: OnDestroy may have changed the map
    }

    std::string ScriptRuntime::EntityName(ECS::Entity entity) const
    {
        if (m_ECS.HasComponent<ECS::HierarchyComponent>(entity))
            return m_ECS.GetComponent<ECS::HierarchyComponent>(entity).Name;
        return "Entity " + std::to_string(entity);
    }
}
