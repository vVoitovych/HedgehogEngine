#include "EntityBindings.hpp"

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/ECS.hpp"

#include "Logger/api/Logger.hpp"

#include <utility>

namespace HedgehogScripting::Bindings
{
    using HedgehogEngine::TransformComponent;

    namespace
    {
        using Field = HM::Vector3 TransformComponent::*;

        // What each legacy function became.
        const char* ReplacementOf(const std::string& function)
        {
            if (function == "GetPosition" || function == "SetPosition")
                return "self.entity.transform.position";
            return "self.entity.transform.eulerAngles";
        }

        struct Legacy
        {
            ECS::ECS&                 Ecs;
            HedgehogEngine::EventBus& Bus;
            EntityHandle              Entity;
            std::string               ScriptPath;
            WarnedSet                 Warned;

            void WarnOnce(const std::string& function) const
            {
                if (!Warned->insert(ScriptPath + "|" + function).second)
                    return;
                LOGWARNING("[Script] " + ScriptPath + ": " + function + "() is deprecated; use " +
                           ReplacementOf(function) + ".");
            }

            // Like before the Entity API: nil, or nothing, once the entity or its transform is gone.
            TransformComponent* Find() const
            {
                if (!IsValid(Ecs, Entity) || !Ecs.HasComponent<TransformComponent>(Entity.Id))
                    return nullptr;
                return &Ecs.GetComponent<TransformComponent>(Entity.Id);
            }

            sol::object Get(Field field, const char* function, sol::this_state state) const
            {
                WarnOnce(function);
                const TransformComponent* transform = Find();
                if (transform == nullptr)
                    return sol::lua_nil;
                const HM::Vector3& value = transform->*field;
                sol::state_view    lua(state);
                return lua.create_table_with("x", value.x(), "y", value.y(), "z", value.z());
            }

            void Set(Field field, const char* function, const sol::table& value) const
            {
                WarnOnce(function);
                TransformComponent* transform = Find();
                if (transform == nullptr)
                    return;
                HM::Vector3& target = transform->*field;
                target.x()          = value.get_or("x", target.x());
                target.y()          = value.get_or("y", target.y());
                target.z()          = value.get_or("z", target.z());
                Bus.Publish(HedgehogEngine::TransformChangedEvent{ Entity.Id });
            }
        };
    }

    void AddLegacyFunctions(sol::table& environment, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                            EntityHandle entity, const std::string& scriptPath, WarnedSet warned)
    {
        const Legacy legacy{ ecs, eventBus, entity, scriptPath, std::move(warned) };

        environment.set_function("GetPosition", [legacy](sol::this_state s)
                                 { return legacy.Get(&TransformComponent::Position, "GetPosition", s); });
        environment.set_function("GetRotation", [legacy](sol::this_state s)
                                 { return legacy.Get(&TransformComponent::Rotation, "GetRotation", s); });
        environment.set_function("SetPosition", [legacy](const sol::table& v)
                                 { legacy.Set(&TransformComponent::Position, "SetPosition", v); });
        environment.set_function("SetRotation", [legacy](const sol::table& v)
                                 { legacy.Set(&TransformComponent::Rotation, "SetRotation", v); });
    }
}
