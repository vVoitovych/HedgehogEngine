#include "LuaHelpers.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"

#include "Logger/Logger.hpp"

namespace Scene
{

	bool CallMethod(lua_State* L, int instanceRef, const std::string& methodName, int numArgs , std::function<void()> pushArgs)
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

	std::unordered_map<std::string, ScriptParam> ParseParameters(lua_State* L)
	{
		std::unordered_map<std::string, ScriptParam> result;
		lua_pushglobaltable(L);

		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			const char* name = lua_tostring(L, -2);
			int type = lua_type(L, -1);

			if (type == LUA_TNUMBER || type == LUA_TBOOLEAN || type == LUA_TSTRING)
			{
				float bVal;
				bool nVal;

				switch (type) {
				case LUA_TNUMBER:
					bVal = (float)lua_tonumber(L, -1);
					result[name] = { ParamType::Number, bVal, false };
					break;
				case LUA_TBOOLEAN:
					nVal = lua_toboolean(L, -1);
					result[name] = { ParamType::Boolean, nVal, false };
					break;
				}

			}

			lua_pop(L, 1);
		}

		lua_pop(L, 1);

		return result;
	}

	float GetGlobalNumber(lua_State* L, const std::string& name)
	{
		lua_getglobal(L, name.c_str());
		float val = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);
		return val;
	}

	void SetGlobalNumber(lua_State* L, const std::string& name, float val)
	{
		lua_pushnumber(L, val);
		lua_setglobal(L, name.c_str());
	}

	bool GetGlobalBool(lua_State* L, const std::string& name)
	{
		lua_getglobal(L, name.c_str());
		bool val = (bool)lua_toboolean(L, -1);
		lua_pop(L, 1);
		return val;
	}

	void SetGlobalBool(lua_State* L, const std::string& name, bool val)
	{
		lua_pushboolean(L, val);
		lua_setglobal(L, name.c_str());
	}

}


