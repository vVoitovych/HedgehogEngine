#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        // Every game object named name, in hierarchy order (depth first from the scene root),
        // the root itself excluded.
        std::vector<ECS::Entity> FindByName(ECS::ECS& ecs, const std::string& name, bool firstOnly)
        {
            std::vector<ECS::Entity> found;
            std::vector<ECS::Entity> pending{ ecs.GetRoot() };
            while (!pending.empty())
            {
                const ECS::Entity entity = pending.back();
                pending.pop_back();
                if (!ecs.IsAlive(entity) || !ecs.HasComponent<ECS::HierarchyComponent>(entity))
                    continue;
                const ECS::HierarchyComponent& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(entity);
                if (entity != ecs.GetRoot() && hierarchy.Name == name)
                {
                    found.push_back(entity);
                    if (firstOnly)
                        break;
                }
                // Reversed, so the first child is visited first.
                pending.insert(pending.end(), hierarchy.Children.rbegin(), hierarchy.Children.rend());
            }
            return found;
        }

        // Queues handle for destruction once every script in the current hook has run. A stale
        // handle, or one already queued, is a warning and nothing more.
        void QueueDestroy(ECS::ECS& ecs, std::vector<ScriptEntity>& pending, const ScriptEntity& handle)
        {
            if (!IsValid(ecs, handle))
            {
                LOGWARNING("[Script] destroy: " + Describe(handle) + " no longer exists; nothing to destroy.");
                return;
            }
            const bool queued = std::any_of(pending.begin(), pending.end(), [&](const ScriptEntity& other)
            {
                return other.Id == handle.Id && other.Generation == handle.Generation;
            });
            if (queued)
            {
                LOGWARNING("[Script] destroy: " + Describe(handle) + " is already being destroyed.");
                return;
            }
            pending.push_back(handle);
        }
    }

    void RegisterScene(sol::state& lua, HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys)
    {
        ECS::ECS&                     ecs    = context.GetECS();
        HedgehogEngine::SceneManager& scenes = context.GetSceneManager();

        sol::table scene = lua.create_named_table("Scene");

        // The first game object with that name, or an invalid handle when there is none.
        scene.set_function("find", [&ecs](const std::string& name)
        {
            const std::vector<ECS::Entity> found = FindByName(ecs, name, true);
            return found.empty() ? ScriptEntity{} : MakeScriptEntity(ecs, found.front());
        });
        scene.set_function("findAll", [&ecs](const std::string& name)
        {
            std::vector<ScriptEntity> handles;
            for (const ECS::Entity entity : FindByName(ecs, name, false))
                handles.push_back(MakeScriptEntity(ecs, entity));
            return sol::as_table(std::move(handles));
        });

        // A new game object, through SceneManager as the editor makes one: Transform and Hierarchy,
        // a unique name unless one is given, under parent or the scene root.
        scene.set_function("spawn", [&ecs, &scenes](sol::optional<std::string> name, sol::optional<ScriptEntity> parent)
        {
            if (parent)
                RequireValid(ecs, *parent);
            const ECS::Entity entity = scenes.CreateGameObject(parent ? std::optional<ECS::Entity>(parent->Id) : std::nullopt);
            if (name)
                ecs.GetComponent<ECS::HierarchyComponent>(entity).Name = *name;
            return MakeScriptEntity(ecs, entity);
        });

        auto destroy = [&ecs, &pendingDestroys](const ScriptEntity& handle) { QueueDestroy(ecs, pendingDestroys, handle); };
        scene.set_function("destroy", destroy);
        sol::usertype<ScriptEntity> entity = lua["Entity"];
        entity["destroy"]                  = destroy;
    }

    void FlushDestroys(HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys)
    {
        ECS::ECS&                     ecs    = context.GetECS();
        HedgehogEngine::SceneManager& scenes = context.GetSceneManager();

        // Destroying one may destroy or recycle another queued one, so each is checked again.
        for (const ScriptEntity& handle : std::exchange(pendingDestroys, {}))
        {
            if (!IsValid(ecs, handle) || handle.Id == ecs.GetRoot())
                continue;
            if (ecs.HasComponent<ECS::HierarchyComponent>(handle.Id))
                scenes.DeleteGameObject(handle.Id); // children move up to the grandparent
            else
                ecs.DestroyEntity(handle.Id);
        }
    }
}
