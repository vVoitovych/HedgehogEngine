#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "Logger/api/Logger.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        // The rotation argument: a Quat, or Euler degrees as a Vector3 (as Transform.eulerAngles
        // stores them).
        HM::Vector3 ReadRotation(const sol::object& rotation)
        {
            if (rotation.is<HM::Quaternion>())
                return rotation.as<HM::Quaternion>().Normalize().ToEuler();
            if (rotation.is<HM::Vector3>())
                return rotation.as<HM::Vector3>();
            throw std::runtime_error("Prefab.instantiate: rotation must be a Quat or a Vector3 of Euler degrees");
        }
    }

    void RegisterPrefab(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        // Spawning changes the play world only: outside Play (a script's top level also runs when
        // the editor describes it) the call does nothing, and the first one says why.
        auto warned = std::make_shared<bool>(false);

        sol::table prefab = lua.create_named_table("Prefab");
        prefab.set_function(
            "instantiate",
            [&context, warned](const std::string& path, sol::optional<HM::Vector3> position, sol::object rotation,
                               sol::optional<ScriptEntity> parent) -> ScriptEntity
            {
                ECS::ECS& ecs = context.GetECS();
                // Checked first, so a wrong argument is an error in Edit mode too.
                const std::string virtualPath = "assets://" + ToAssetPath(path, "A prefab", "Prefabs");
                if (parent)
                    RequireValid(ecs, *parent);
                std::optional<HM::Vector3> euler;
                if (rotation.valid() && rotation.get_type() != sol::type::lua_nil)
                    euler = ReadRotation(rotation);

                if (context.GetPlayState() == HedgehogEngine::PlayState::Edit)
                {
                    if (!*warned)
                        LOGWARNING("[Script] Prefabs spawn only in Play mode; Prefab.instantiate did nothing.");
                    *warned = true;
                    return ScriptEntity{};
                }

                // The PrefabManager's cache serves every call; a prefab that does not load is
                // logged there and gives an invalid handle. The instance's scripts start at the
                // next sync, as a spawned entity's do; the hook's own iteration is a snapshot.
                const ECS::Entity instance = context.GetPrefabs().Instantiate(
                    virtualPath, parent ? std::optional<ECS::Entity>(parent->Id) : std::nullopt);
                if (instance == ECS::INVALID_ENTITY)
                    return ScriptEntity{};
                if ((position || euler) && ecs.HasComponent<HedgehogEngine::TransformComponent>(instance))
                {
                    auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(instance);
                    if (position)
                        transform.Position = *position;
                    if (euler)
                        transform.Rotation = *euler;
                    context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ instance });
                }
                return MakeScriptEntity(ecs, instance);
            });
    }
}
