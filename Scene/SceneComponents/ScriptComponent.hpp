#pragma once

#include "ECS/Entity.h"

#include <optional>

struct lua_State;

namespace Scene
{
	class ScriptComponent
	{
	public:
		bool m_Enable = true;
	
		std::optional<bool> m_NewEnable;
		std::string m_ScriptPath;

		friend class ScriptSystem;

	private:
		lua_State* m_LuaState = nullptr;
		int m_InstanceRef;
	};

}




