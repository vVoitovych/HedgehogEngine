#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

namespace HedgehogEngine
{
    class ScriptComponent;

    // Legacy per-entity Lua scripts. Scripts load in any mode, but OnEnable, OnUpdate and
    // OnDisable run only from the OnUpdate play-mode event, so only while the engine plays.
    class ScriptSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API explicit ScriptSystem(EventBus& bus);

        HEDGEHOG_ENGINE_API void OnUpdate(ECS::ECS& ecs, float deltaTime) override;

        // Closes the script's lua_State, if it has one. The engine calls it from the
        // ScriptComponent removal callback, so a state never outlives its component.
        HEDGEHOG_ENGINE_API static void ReleaseScript(ScriptComponent& component);

        HEDGEHOG_ENGINE_API void ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs);
        // physicalPath: absolute file-system path chosen by the caller.
        // The caller is responsible for opening any file dialog.
        HEDGEHOG_ENGINE_API void ChangeScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                              const FS::FileSystemManager& fileSystem,
                                              const std::string& physicalPath);
        HEDGEHOG_ENGINE_API void InitScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                            const FS::FileSystemManager& fileSystem);

    private:
        EventBus& m_EventBus;

        void CallOnEnable(ECS::ECS& ecs, EventBus& bus);
        void CallUpdate(ECS::ECS& ecs, float dt, EventBus& bus);
        void CallOnDisable(ECS::ECS& ecs, EventBus& bus);
    };
}
