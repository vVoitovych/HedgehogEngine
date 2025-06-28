#pragma once

#include "ECS/Entity.h"

#include <optional>
#include <unordered_map>
#include <string>
#include <variant>

struct lua_State;

namespace Scene
{
	enum class ParamType
	{
		Boolean,
		Number
	};

	struct ScriptParam
	{
		ParamType type;
		std::variant<bool, float> value;
		bool dirty = false;
	};

	class ScriptComponent
	{
	public:
		bool m_Enable = true;	
		std::optional<bool> m_NewEnable;
		std::string m_ScriptPath;
		std::unordered_map<std::string, ScriptParam> m_Params;

		friend class ScriptSystem;

	private:
		lua_State* m_LuaState = nullptr;
		int m_InstanceRef;

	};

}




