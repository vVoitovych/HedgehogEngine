#pragma once

#include "HedgehogScripting/api/ScriptClassCache.hpp"
#include "HedgehogScripting/api/ScriptInstance.hpp"
#include "HedgehogScripting/api/ScriptVM.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

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
    // The ECS system that runs every ScriptComponent's script, in one sandboxed VM. Its
    // entities are those with a ScriptComponent and a TransformComponent. Register it in the
    // engine's ECS with ScriptSystem::Register; the Simulation then calls its play-mode hooks.
    //
    // Lifecycle of each entity's script:
    //   - OnPlayStart makes an instance of every enabled script; a script enabled or
    //     added later is made on the next Update.
    //   - On its first Update an instance gets OnStart, then OnEnable.
    //   - Enable/NewEnable toggles give OnDisable and OnEnable.
    //   - OnUpdate(dt) gives every enabled instance OnUpdate(dt).
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
    // The system owns the ECS's ScriptComponent removal callback while it lives,
    // replacing any other; it clears it on destruction. The ECS owns the system.
    class ScriptSystem : public ECS::System
    {
    public:
        // Registers a ScriptSystem in ecs and sets its signature, picking up entities that
        // already have a script. The ECS keeps it; the returned pointer is for the
        // application's editor-side calls (DescribeScript).
        static std::shared_ptr<ScriptSystem> Register(ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                                                      const FS::FileSystemManager& fileSystem);

        // Use Register; public only so the ECS can construct it.
        ScriptSystem(ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus, const FS::FileSystemManager& fileSystem);
        ~ScriptSystem() override;

        ScriptSystem(const ScriptSystem&)            = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;

        void OnPlayStart(ECS::ECS& ecs) override;
        void OnPlayStop(ECS::ECS& ecs) override;
        void OnUpdate(ECS::ECS& ecs, float deltaTime) override;

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

        std::unordered_map<ECS::Entity, RunningScript> m_Running;
        // "<script>|<function>" for each legacy function whose deprecation was logged.
        std::shared_ptr<std::unordered_set<std::string>> m_DeprecationWarned;
    };
}
