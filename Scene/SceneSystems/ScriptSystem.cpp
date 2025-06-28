#include "ScriptSystem.hpp"
#include "LuaHelpers.hpp"

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

		component.m_Params.clear();
		component.m_Params = ParseParameters(component.m_LuaState);

		if (component.m_Enable)
		{
			CallMethod(component.m_LuaState, component.m_InstanceRef, "OnEnable");
		}
	}

	void ScriptSystem::InitScript(ECS::Entity entity, ECS::Coordinator& coordinator)
	{
		auto& component = coordinator.GetComponent<ScriptComponent>(entity);

		if (component.m_LuaState != nullptr)
		{
			lua_close(component.m_LuaState);
			component.m_LuaState = nullptr;
		}
		if (component.m_Enable)
		{
			component.m_NewEnable = true;
		}
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

		std::string scriptPath = ContentLoader::GetAssetsDirectory() + component.m_ScriptPath;
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

	void ScriptSystem::CallOnEnable(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& component = coordinator.GetComponent<ScriptComponent>(entity);
			if (component.m_NewEnable.has_value() && component.m_NewEnable.value())
			{
				component.m_Enable = true;
				component.m_NewEnable.reset();
				CallMethod(component.m_LuaState, component.m_InstanceRef, "OnEnable");
			}
		}
	}

	void ScriptSystem::CallOnDisable(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& component = coordinator.GetComponent<ScriptComponent>(entity);
			if (component.m_NewEnable.has_value() && !component.m_NewEnable.value())
			{
				component.m_Enable = false;
				component.m_NewEnable.reset();
				CallMethod(component.m_LuaState, component.m_InstanceRef, "OnDisable");
			}
		}
	}

	void ScriptSystem::CallUpdate(ECS::Coordinator& coordinator, float dt)
	{
		for (auto const& entity : entities)
		{
			auto& component = coordinator.GetComponent<ScriptComponent>(entity);

			for (auto& param : component.m_Params)
			{
				if (param.second.dirty)
				{
					bool bVal;
					float nVal;

					param.second.dirty = false;
					switch (param.second.type)
					{
					case ParamType::Boolean:
						bVal = std::get<bool>(param.second.value);
						SetGlobalBool(component.m_LuaState, param.first, bVal);
						break;
					case ParamType::Number:
						nVal = std::get<float>(param.second.value);
						SetGlobalNumber(component.m_LuaState, param.first, nVal);
						break;
					default:
						break;
					}
				}
			}

			if (component.m_Enable && component.m_LuaState != nullptr)
			{
				auto& transform = coordinator.GetComponent<TransformComponent>(entity);

				RegisterLuaBindings(component.m_LuaState, &transform);
				
				CallMethod(component.m_LuaState, component.m_InstanceRef, "OnUpdate", 1,
					[&]() 
					{
						lua_pushnumber(component.m_LuaState, dt);
					}
				);

			}
		}
	}




}


