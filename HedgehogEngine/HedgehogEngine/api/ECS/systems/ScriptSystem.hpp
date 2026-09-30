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

    class ScriptSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, float dt, EventBus& bus);

        HEDGEHOG_ENGINE_API void ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs);
        // physicalPath: absolute file-system path chosen by the caller.
        // The caller is responsible for opening any file dialog.
        HEDGEHOG_ENGINE_API void ChangeScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                              const FS::FileSystemManager& fileSystem,
                                              const std::string& physicalPath);
        HEDGEHOG_ENGINE_API void InitScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                            const FS::FileSystemManager& fileSystem);

    private:
        void CallOnEnable(ECS::ECS& ecs, EventBus& bus);
        void CallUpdate(ECS::ECS& ecs, float dt, EventBus& bus);
        void CallOnDisable(ECS::ECS& ecs, EventBus& bus);
    };
}
