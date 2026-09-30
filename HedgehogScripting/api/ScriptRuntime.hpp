#pragma once

#include "HedgehogScripting/api/ScriptClassCache.hpp"
#include "HedgehogScripting/api/ScriptInstance.hpp"
#include "HedgehogScripting/api/ScriptVM.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Simulation/ISimulationSystem.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EventBus;
}

namespace HedgehogScripting
{
    // Entities with a ScriptComponent and a TransformComponent; kept up to date by the ECS.
    class ScriptedEntities : public ECS::System
    {
    };

    // Runs every ScriptComponent's script in one sandboxed VM, as a gameplay system
    // of the Simulation. Add it with Simulation::AddSystem.
    //
    // Lifecycle of each entity's script:
    //   - OnPlayStart makes an instance of every enabled script; a script enabled or
    //     added later is made on the next Update.
    //   - On its first Update an instance gets OnStart, then OnEnable.
    //   - Enable/NewEnable toggles give OnDisable and OnEnable.
    //   - Update(dt) gives every enabled instance OnUpdate(dt).
    //   - OnDestroy runs when the ScriptComponent is removed or its entity destroyed,
    //     and for every instance on OnPlayStop, before the scene snapshot comes back.
    // Nothing runs in Edit mode or while paused, because the Simulation calls none of
    // this then. A faulted instance is skipped; the others carry on.
    //
    // Each instance's self.entity is an Entity handle to its own entity, with
    // self.entity.transform for position, rotation and scale (setters publish
    // TransformChangedEvent, so TransformSystem picks them up the same frame). The
    // component's Params are plain globals. The legacy globals GetPosition/SetPosition/
    // GetRotation/SetRotation still work for one epic, with a deprecation warning the
    // first time each script file uses one.
    //
    // The runtime owns the ECS's ScriptComponent removal callback while it lives,
    // replacing any other; it clears it on destruction.
    class ScriptRuntime : public HedgehogEngine::ISimulationSystem
    {
    public:
        ScriptRuntime(ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus, const FS::FileSystemManager& fileSystem);
        ~ScriptRuntime() override;

        ScriptRuntime(const ScriptRuntime&)            = delete;
        ScriptRuntime& operator=(const ScriptRuntime&) = delete;

        void OnPlayStart() override;
        void OnPlayStop() override;
        void Update(float deltaTime) override;

        // The parameters a script declares (its top-level number and boolean
        // globals), for the inspector in Edit mode, when no instance exists.
        [[nodiscard]] std::unordered_map<std::string, HedgehogEngine::ScriptParam> DescribeScript(
            const std::string& scriptPath);

        // The instance running for entity, if any (only while playing).
        [[nodiscard]] ScriptInstance* FindInstance(ECS::Entity entity);
        [[nodiscard]] size_t          GetInstanceCount() const;

        ScriptVM& GetVM();

        // "Scripts\\Player.lua" or "assets://Scripts/Player.lua" -> "assets://Scripts/Player.lua".
        [[nodiscard]] static std::string ToVirtualPath(const std::string& scriptPath);

    private:
        struct RunningScript
        {
            ScriptInstance Instance;
            bool           Started = false;
            bool           Enabled = false;
        };

        void Instantiate(ECS::Entity entity);
        void ApplyParams(ScriptInstance& instance, HedgehogEngine::ScriptComponent& component, bool onlyDirty);
        void Destroy(ECS::Entity entity);
        std::string EntityName(ECS::Entity entity) const;

        ECS::ECS&                         m_ECS;
        HedgehogEngine::EventBus&         m_EventBus;
        ScriptVM                          m_VM;
        ScriptClassCache                  m_Classes;
        std::shared_ptr<ScriptedEntities> m_Entities;

        std::unordered_map<ECS::Entity, RunningScript> m_Running;
        // "<script>|<function>" for each legacy function whose deprecation was logged.
        std::shared_ptr<std::unordered_set<std::string>> m_DeprecationWarned;
    };
}
