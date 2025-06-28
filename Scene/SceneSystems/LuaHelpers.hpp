#pragma once

extern "C"
{
#include "ThirdParty/Lua/lua/lua.h"
#include "ThirdParty/Lua/lua/lualib.h"
#include "ThirdParty/Lua/lua/lauxlib.h"
}

#include <functional>
#include <string>
#include <unordered_map>

namespace Scene
{
	struct ScriptParam;

	bool CallMethod(lua_State* L, int instanceRef, const std::string& methodName, int numArgs = 0, std::function<void()> pushArgs = nullptr);

	std::unordered_map<std::string, ScriptParam> ParseParameters(lua_State* L);

	float GetGlobalNumber(lua_State* L, const std::string& name);
	void SetGlobalNumber(lua_State* L, const std::string& name, float val);

	bool GetGlobalBool(lua_State* L, const std::string& name);
	void SetGlobalBool(lua_State* L, const std::string& name, bool val);

}


