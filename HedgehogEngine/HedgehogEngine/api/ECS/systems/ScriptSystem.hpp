#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Simulation/ISimulationSystem.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

namespace HedgehogEngine
{
    class ScriptComponent;

    // Legacy per-entity Lua scripts. Scripts load in any mode, but OnEnable,
    // OnUpdate and OnDisable run only while the simulation plays (Update).
    class ScriptSystem : public ECS::System, public ISimulationSystem
    {
    public:
        // Must be called before the system is ticked.
        HEDGEHOG_ENGINE_API void Init(ECS::ECS& ecs, EventBus& bus);

        HEDGEHOG_ENGINE_API void Update(float deltaTime) override;

        // Open lua_States across every ScriptSystem, for leak checks.
        HEDGEHOG_ENGINE_API static int GetOpenLuaStateCount();

        HEDGEHOG_ENGINE_API void ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs);
        // physicalPath: absolute file-system path chosen by the caller.
        // The caller is responsible for opening any file dialog.
        HEDGEHOG_ENGINE_API void ChangeScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                              const FS::FileSystemManager& fileSystem,
                                              const std::string& physicalPath);
        HEDGEHOG_ENGINE_API void InitScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                            const FS::FileSystemManager& fileSystem);

    private:
        ECS::ECS* m_ECS      = nullptr;
        EventBus* m_EventBus = nullptr;

        void CallOnEnable(ECS::ECS& ecs, EventBus& bus);
        void CallUpdate(ECS::ECS& ecs, float dt, EventBus& bus);
        void CallOnDisable(ECS::ECS& ecs, EventBus& bus);
    };
}
