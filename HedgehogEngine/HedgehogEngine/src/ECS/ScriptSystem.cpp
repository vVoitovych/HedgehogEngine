// Lua file loading (luaL_dofile) is intentionally left using Lua's own file API.
// It is treated as an out-of-scope third-party reader, symmetric with the yaml-cpp/stb exclusion.
// Migrating to luaL_loadbuffer + FileSystem::ReadTextFile is future work if needed.
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LuaHelpers.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "Logger/api/Logger.hpp"
#include "DialogueWindows/api/ScriptDialogue.hpp"

#include "HedgehogMath/api/Vector.hpp"

extern "C"
{
#include "ThirdParty/Lua/lua/lua.h"
#include "ThirdParty/Lua/lua/lualib.h"
#include "ThirdParty/Lua/lua/lauxlib.h"
}

#include <filesystem>
#include <functional>
#include <string_view>

namespace HedgehogEngine
{
namespace
{
    const std::string s_BaseActorScript = "Scripts/Base/ActorScript.lua";

    std::string GetFileNameWithoutExtension(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    void RegisterLuaBindings(lua_State* L, TransformComponent* transform,
                             ECS::Entity entity, EventBus& bus)
    {
        lua_pushlightuserdata(L, transform);
        lua_setglobal(L, "__transform_ptr");

        lua_pushinteger(L, static_cast<lua_Integer>(entity));
        lua_setglobal(L, "__entity_id");

        // raw void* required by the Lua C API; valid for the lifetime of EngineContext
        lua_pushlightuserdata(L, &bus);
        lua_setglobal(L, "__event_bus");

        lua_register(L, "GetPosition", [](lua_State* L) -> int
            {
                lua_getglobal(L, "__transform_ptr");
                auto* t = static_cast<TransformComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushnumber(L, t->m_Position.x()); lua_setfield(L, -2, "x");
                lua_pushnumber(L, t->m_Position.y()); lua_setfield(L, -2, "y");
                lua_pushnumber(L, t->m_Position.z()); lua_setfield(L, -2, "z");
                return 1;
            }
        );

        lua_register(L, "SetPosition", [](lua_State* L) -> int
            {
                lua_getglobal(L, "__transform_ptr");
                auto* t = static_cast<TransformComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, 1, "x"); t->m_Position.x() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_getfield(L, 1, "y"); t->m_Position.y() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_getfield(L, 1, "z"); t->m_Position.z() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);

                lua_getglobal(L, "__event_bus");
                auto* bus = static_cast<EventBus*>(lua_touserdata(L, -1)); lua_pop(L, 1);
                lua_getglobal(L, "__entity_id");
                const auto eid = static_cast<ECS::Entity>(lua_tointeger(L, -1)); lua_pop(L, 1);
                bus->Publish(TransformChangedEvent{ eid });
                return 0;
            }
        );

        lua_register(L, "GetRotation", [](lua_State* L) -> int
            {
                lua_getglobal(L, "__transform_ptr");
                auto* t = static_cast<TransformComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushnumber(L, t->m_Rotation.x()); lua_setfield(L, -2, "x");
                lua_pushnumber(L, t->m_Rotation.y()); lua_setfield(L, -2, "y");
                lua_pushnumber(L, t->m_Rotation.z()); lua_setfield(L, -2, "z");
                return 1;
            }
        );

        lua_register(L, "SetRotation", [](lua_State* L) -> int
            {
                lua_getglobal(L, "__transform_ptr");
                auto* t = static_cast<TransformComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, 1, "x"); t->m_Rotation.x() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_getfield(L, 1, "y"); t->m_Rotation.y() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_getfield(L, 1, "z"); t->m_Rotation.z() = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);

                lua_getglobal(L, "__event_bus");
                auto* bus = static_cast<EventBus*>(lua_touserdata(L, -1)); lua_pop(L, 1);
                lua_getglobal(L, "__entity_id");
                const auto eid = static_cast<ECS::Entity>(lua_tointeger(L, -1)); lua_pop(L, 1);
                bus->Publish(TransformChangedEvent{ eid });
                return 0;
            }
        );
    }
}

    void ScriptSystem::Update(ECS::ECS& ecs, float dt, EventBus& bus)
    {
        CallOnEnable(ecs, bus);
        CallUpdate(ecs, dt, bus);
        CallOnDisable(ecs, bus);
    }

    void ScriptSystem::ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs)
    {
        auto& component = ecs.GetComponent<ScriptComponent>(entity);
        if (component.m_LuaState != nullptr)
        {
            lua_close(component.m_LuaState);
            component.m_LuaState    = nullptr;
            component.m_InstanceRef = 0;
        }
    }

    void ScriptSystem::ChangeScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                     const FS::FileSystemManager& fileSystem)
    {
        std::string scriptPath = DialogueWindows::ScriptChooseDialogue();
        if (scriptPath.empty())
            return;

        const auto virtualPath = fileSystem.ToVirtualPath(scriptPath);
        if (!virtualPath)
        {
            LOGERROR("ScriptSystem::ChangeScript: path is not under any registered mount (path: ", scriptPath, ")");
            return;
        }

        auto& component = ecs.GetComponent<ScriptComponent>(entity);
        if (component.m_LuaState != nullptr)
        {
            lua_close(component.m_LuaState);
            component.m_LuaState = nullptr;
        }

        // Strip "assets://" prefix; m_ScriptPath stores the relative path.
        constexpr std::string_view ASSETS_PREFIX = "assets://";
        component.m_ScriptPath = virtualPath->substr(ASSETS_PREFIX.size());
        component.m_LuaState   = luaL_newstate();
        luaL_openlibs(component.m_LuaState);

        auto& transform = ecs.GetComponent<TransformComponent>(entity);
        RegisterLuaBindings(component.m_LuaState, &transform, entity, bus);

        const auto basePhysPath = fileSystem.ResolvePhysical("assets://" + s_BaseActorScript);
        if (!basePhysPath)
        {
            LOGERROR("ScriptSystem::ChangeScript: cannot resolve base actor script path.");
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        if (luaL_dofile(component.m_LuaState, basePhysPath->string().c_str()) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        if (luaL_dofile(component.m_LuaState, scriptPath.c_str()) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        const std::string className = GetFileNameWithoutExtension(scriptPath);
        lua_getglobal(component.m_LuaState, className.c_str());
        lua_getfield(component.m_LuaState, -1, "new");
        lua_pushvalue(component.m_LuaState, -2);
        if (lua_pcall(component.m_LuaState, 1, 1, 0) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }
        component.m_InstanceRef = luaL_ref(component.m_LuaState, LUA_REGISTRYINDEX);

        component.m_Params.clear();
        component.m_Params = ParseParameters(component.m_LuaState);

        if (component.m_Enable)
            CallMethod(component.m_LuaState, component.m_InstanceRef, "OnEnable");
    }

    void ScriptSystem::InitScript(ECS::Entity entity, ECS::ECS& ecs, EventBus& bus,
                                   const FS::FileSystemManager& fileSystem)
    {
        auto& component = ecs.GetComponent<ScriptComponent>(entity);

        if (component.m_LuaState != nullptr)
        {
            lua_close(component.m_LuaState);
            component.m_LuaState = nullptr;
        }
        if (component.m_Enable)
            component.m_NewEnable = true;

        component.m_LuaState = luaL_newstate();
        luaL_openlibs(component.m_LuaState);

        auto& transform = ecs.GetComponent<TransformComponent>(entity);
        RegisterLuaBindings(component.m_LuaState, &transform, entity, bus);

        const auto basePhysPath = fileSystem.ResolvePhysical("assets://" + s_BaseActorScript);
        if (!basePhysPath)
        {
            LOGERROR("ScriptSystem::InitScript: cannot resolve base actor script path.");
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        if (luaL_dofile(component.m_LuaState, basePhysPath->string().c_str()) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        const auto scriptPhysPath = fileSystem.ResolvePhysical("assets://" + component.m_ScriptPath);
        if (!scriptPhysPath)
        {
            LOGERROR("ScriptSystem::InitScript: cannot resolve script path '", component.m_ScriptPath, "'.");
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        if (luaL_dofile(component.m_LuaState, scriptPhysPath->string().c_str()) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }

        const std::string className = GetFileNameWithoutExtension(scriptPhysPath->string());
        lua_getglobal(component.m_LuaState, className.c_str());
        lua_getfield(component.m_LuaState, -1, "new");
        lua_pushvalue(component.m_LuaState, -2);
        if (lua_pcall(component.m_LuaState, 1, 1, 0) != LUA_OK)
        {
            LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
            lua_pop(component.m_LuaState, 1);
            lua_close(component.m_LuaState);
            component.m_LuaState   = nullptr;
            component.m_ScriptPath = "";
            return;
        }
        component.m_InstanceRef = luaL_ref(component.m_LuaState, LUA_REGISTRYINDEX);

        for (const auto& param : component.m_Params)
        {
            switch (param.second.type)
            {
            case ParamType::Boolean:
                SetGlobalBool(component.m_LuaState, param.first, std::get<bool>(param.second.value));
                break;
            case ParamType::Number:
                SetGlobalNumber(component.m_LuaState, param.first, std::get<float>(param.second.value));
                break;
            default:
                break;
            }
        }
    }

    void ScriptSystem::CallOnEnable(ECS::ECS& ecs, EventBus& bus)
    {
        for (auto const& entity : m_Entities)
        {
            auto& component = ecs.GetComponent<ScriptComponent>(entity);
            if (component.m_NewEnable.has_value() && component.m_NewEnable.value())
            {
                component.m_Enable = true;
                if (component.m_LuaState == nullptr)
                {
                    component.m_NewEnable.reset();
                    continue;
                }
                component.m_NewEnable.reset();
                auto& transform = ecs.GetComponent<TransformComponent>(entity);
                RegisterLuaBindings(component.m_LuaState, &transform, entity, bus);
                CallMethod(component.m_LuaState, component.m_InstanceRef, "OnEnable");
            }
        }
    }

    void ScriptSystem::CallOnDisable(ECS::ECS& ecs, EventBus& bus)
    {
        for (auto const& entity : m_Entities)
        {
            auto& component = ecs.GetComponent<ScriptComponent>(entity);
            if (component.m_NewEnable.has_value() && !component.m_NewEnable.value())
            {
                component.m_Enable = false;
                if (component.m_LuaState == nullptr)
                {
                    component.m_NewEnable.reset();
                    continue;
                }
                component.m_NewEnable.reset();
                auto& transform = ecs.GetComponent<TransformComponent>(entity);
                RegisterLuaBindings(component.m_LuaState, &transform, entity, bus);
                CallMethod(component.m_LuaState, component.m_InstanceRef, "OnDisable");
            }
        }
    }

    void ScriptSystem::CallUpdate(ECS::ECS& ecs, float dt, EventBus& bus)
    {
        for (auto const& entity : m_Entities)
        {
            auto& component = ecs.GetComponent<ScriptComponent>(entity);

            for (auto& param : component.m_Params)
            {
                if (param.second.dirty)
                {
                    param.second.dirty = false;
                    switch (param.second.type)
                    {
                    case ParamType::Boolean:
                        SetGlobalBool(component.m_LuaState, param.first,
                            std::get<bool>(param.second.value));
                        break;
                    case ParamType::Number:
                        SetGlobalNumber(component.m_LuaState, param.first,
                            std::get<float>(param.second.value));
                        break;
                    default:
                        break;
                    }
                }
            }

            if (component.m_Enable && component.m_LuaState != nullptr)
            {
                auto& transform = ecs.GetComponent<TransformComponent>(entity);
                RegisterLuaBindings(component.m_LuaState, &transform, entity, bus);

                CallMethod(component.m_LuaState, component.m_InstanceRef, "OnUpdate", 1,
                    [&]() { lua_pushnumber(component.m_LuaState, dt); }
                );
            }
        }
    }
}
