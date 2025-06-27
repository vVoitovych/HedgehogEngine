#include "ScriptSystem.hpp"

#include "Scene/SceneComponents/ScriptComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Logger/Logger.hpp"

#include "DialogueWindows/ScriptDialogue/ScriptDialogue.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#include "HedgehogMath/Vector.hpp"

extern "C" 
{
#include "ThirdParty/Lua/lua/lua.h"
#include "ThirdParty/Lua/lua/lualib.h"
#include "ThirdParty/Lua/lua/lauxlib.h"
}

#include <filesystem>
#include <functional>

namespace Scene
{
	static std::string baseActorScript = "Scripts/Base/ActorScript.lua";

	std::string GetFileNameWithoutExtension(const std::string& path)
	{
		std::filesystem::path filePath = path;
		std::string result = filePath.stem().string();
		return result;
	}

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

	bool CallMethod(lua_State* L, int instanceRef, const std::string& methodName, int numArgs = 0, std::function<void()> pushArgs = nullptr) 
	{
		if (L == nullptr)
			return false;

		lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef); 

		lua_getfield(L, -1, methodName.c_str());      
		if (!lua_isfunction(L, -1)) 
		{
			lua_pop(L, 2); 
			return false;
		}

		lua_pushvalue(L, -2); 
		if (pushArgs) 
		{
			pushArgs();      
		}

		if (lua_pcall(L, 1 + numArgs, 0, 0) != LUA_OK) 
		{
			LOGERROR("[Lua Call Error] ", lua_tostring(L, -1));
			lua_pop(L, 1); 
		}

		lua_pop(L, 1); 
		return true;
	}

	void ScriptSystem::Update(ECS::Coordinator& coordinator, float dt)
	{
		CallOnEnable(coordinator);
		CallUpdate(coordinator, dt);
		CallOnDisable(coordinator);

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

	void ScriptSystem::ChangeScript(ECS::Entity entity, ECS::Coordinator& coordinator)
	{
		std::string scriptPath = DialogueWindows::ScriptChooseDialogue();
		if (scriptPath == "")
			return;

		auto& component = coordinator.GetComponent<ScriptComponent>(entity);
		if (component.m_LuaState != nullptr)
		{
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
		}

		component.m_ScriptPath = ContentLoader::GetAssetRelativetlyPath(scriptPath);
		component.m_LuaState = luaL_newstate();
		luaL_openlibs(component.m_LuaState);

		auto& transform = coordinator.GetComponent<TransformComponent>(entity);
		RegisterLuaBindings(component.m_LuaState, &transform);

		std::string basePath = ContentLoader::GetAssetsDirectory() + baseActorScript;

		if (luaL_dofile(component.m_LuaState, basePath.c_str()) != LUA_OK)
		{
			LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
			lua_pop(component.m_LuaState, 1);
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
			component.m_ScriptPath = "";
			return;
		}

		if (luaL_dofile(component.m_LuaState, scriptPath.c_str()) != LUA_OK)
		{
			LOGERROR("[Lua Error] ", lua_tostring(component.m_LuaState, -1));
			lua_pop(component.m_LuaState, 1);
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
			component.m_ScriptPath = "";
			return;
		}

		std::string className = GetFileNameWithoutExtension(scriptPath);
		lua_getglobal(component.m_LuaState, className.c_str()); 
		lua_getfield(component.m_LuaState, -1, "new");   
		lua_pushvalue(component.m_LuaState, -2);         
		lua_call(component.m_LuaState, 1, 1);            
		component.m_InstanceRef = luaL_ref(component.m_LuaState, LUA_REGISTRYINDEX); 


		if (component.m_Enable)
		{
			CallFunction(component.m_LuaState, "OnEnable");
		}
	}

	void ScriptSystem::CallOnEnable(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& script = coordinator.GetComponent<ScriptComponent>(entity);
			if (script.m_NewEnable.has_value() && script.m_NewEnable.value())
			{
				script.m_Enable = true;
				script.m_NewEnable.reset();
				CallMethod(script.m_LuaState, script.m_InstanceRef, "OnEnable");
			}
		}
	}

	void ScriptSystem::CallOnDisable(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& script = coordinator.GetComponent<ScriptComponent>(entity);
			if (script.m_NewEnable.has_value() && !script.m_NewEnable.value())
			{
				script.m_Enable = false;
				script.m_NewEnable.reset();
				CallMethod(script.m_LuaState, script.m_InstanceRef, "OnDisable");
			}
		}
	}

	void ScriptSystem::CallUpdate(ECS::Coordinator& coordinator, float dt)
	{
		for (auto const& entity : entities)
		{
			auto& script = coordinator.GetComponent<ScriptComponent>(entity);
			if (script.m_Enable && script.m_LuaState != nullptr)
			{
				auto& transform = coordinator.GetComponent<TransformComponent>(entity);

				RegisterLuaBindings(script.m_LuaState, &transform);
				
				CallMethod(script.m_LuaState, script.m_InstanceRef, "OnUpdate", 1, 
					[&]() 
					{
						lua_pushnumber(script.m_LuaState, dt);
					}
				);

			}
		}
	}




}


