#include "ScriptSystem.hpp"

#include "Scene/SceneComponents/ScriptComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Logger/Logger.hpp"

#include "HedgehogMath/Vector.hpp"

extern "C" 
{
#include "ThirdParty/Lua/lua/lua.h"
#include "ThirdParty/Lua/lua/lualib.h"
#include "ThirdParty/Lua/lua/lauxlib.h"
}


namespace Scene
{
	void RegisterLuaBindings(lua_State* L, TransformComponent* transform) 
	{
		lua_pushlightuserdata(L, transform);
		lua_setglobal(L, "__transform_ptr");

		lua_register(L, "GetPosition", [](lua_State* L) -> int 
			{
				lua_getglobal(L, "__transform_ptr");
				auto* transform = static_cast<TransformComponent*>(lua_touserdata(L, -1));
				lua_pop(L, 1);
				lua_newtable(L);
				lua_pushnumber(L, transform->mPosition.x()); lua_setfield(L, -2, "x");
				lua_pushnumber(L, transform->mPosition.y()); lua_setfield(L, -2, "y");
				lua_pushnumber(L, transform->mPosition.z()); lua_setfield(L, -2, "z");
				return 1;
			}
		);

		lua_register(L, "SetPosition", [](lua_State* L) -> int 
			{
				lua_getglobal(L, "__transform_ptr");
				auto* transform = static_cast<TransformComponent*>(lua_touserdata(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, 1, "x"); transform->mPosition.x() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, 1, "y"); transform->mPosition.y() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, 1, "z"); transform->mPosition.z() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				return 0;
			}
		);

		lua_register(L, "GetRotation", [](lua_State* L) -> int 
			{
				lua_getglobal(L, "__transform_ptr");
				auto* transform = static_cast<TransformComponent*>(lua_touserdata(L, -1));
				lua_pop(L, 1);
				lua_newtable(L);
				lua_pushnumber(L, transform->mRotation.x()); lua_setfield(L, -2, "x");
				lua_pushnumber(L, transform->mRotation.y()); lua_setfield(L, -2, "y");
				lua_pushnumber(L, transform->mRotation.z()); lua_setfield(L, -2, "z");
				return 1;
			}
		);

		lua_register(L, "SetRotation", [](lua_State* L) -> int 
			{
				lua_getglobal(L, "__transform_ptr");
				auto* transform = static_cast<TransformComponent*>(lua_touserdata(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, 1, "x"); transform->mRotation.x() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, 1, "y"); transform->mRotation.y() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, 1, "z"); transform->mRotation.z() = (float)lua_tonumber(L, -1); lua_pop(L, 1);
				return 0;
			}
		);
	}


	bool CheckFunctionExists(lua_State* L, const char* func)
	{
		lua_getglobal(L, func);
		bool exists = lua_isfunction(L, -1);
		lua_pop(L, 1);
		return exists;
	}

	void CallFunction(lua_State* L, const char* func)
	{
		lua_getglobal(L, func);
		if (lua_isfunction(L, -1)) 
		{
			if (lua_pcall(L, 0, 0, 0) != LUA_OK) 
			{
				LOGERROR("[Lua Call Error] ", lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}
		else 
		{
			lua_pop(L, 1);
		}
	}


	void ScriptSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& transform = coordinator.GetComponent<TransformComponent>(entity);

			auto& position = transform.mPosition;
			auto& rotation = transform.mRotation;

		}
	}

	void ScriptSystem::ChangeEnable(ScriptComponent& component, bool val)
	{
		if (val)
		{
			if (component.m_HasOnEnable)
			{
				CallFunction(component.m_LuaState, "OnEnable");
			}
		}
		else
		{
			if (component.m_HasOnDisable)
			{
				CallFunction(component.m_LuaState, "OnDisable");
			}
		}
	}

	void ScriptSystem::ClearScriptComponent(ECS::Entity entity, ECS::Coordinator& coordinator)
	{
		auto& component = coordinator.GetComponent<ScriptComponent>(entity);
		if (component.m_LuaState != nullptr)
		{
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
		}
	}

	void ScriptSystem::ChangeScript(ECS::Entity entity, std::string scriptPath, ECS::Coordinator& coordinator)
	{
		auto& component = coordinator.GetComponent<ScriptComponent>(entity);
		auto& transform = coordinator.GetComponent<TransformComponent>(entity);
		if (component.m_LuaState != nullptr)
		{
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
		}

		component.m_ScriptPath = scriptPath;
		component.m_LuaState = luaL_newstate();
		luaL_openlibs(component.m_LuaState);
		RegisterLuaBindings(component.m_LuaState, &transform);

		if (luaL_dofile(component.m_LuaState, scriptPath.c_str()) != LUA_OK)
		{
			LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
			lua_pop(component.m_LuaState, 1);
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
			component.m_ScriptPath = "";
			return;
		}

		// Cache function presence
		component.m_HasOnEnable = CheckFunctionExists(component.m_LuaState, "OnEnable");
		component.m_HasOnDisable = CheckFunctionExists(component.m_LuaState, "OnDisable");
		component.m_HasOnUpdate = CheckFunctionExists(component.m_LuaState, "OnUpdate");

		if (component.m_Enable && component.m_HasOnEnable)
		{
			CallFunction(component.m_LuaState, "OnEnable");
		}
	}




}


