#pragma once

#include "ECS/Entity.h"

class lua_State;

namespace Scene
{
	class ScriptComponent
	{
	public:
		bool m_Enable = true;
	
	private:
		bool m_HasOnEnable = false;
		bool m_HasOnDisable = false;
		bool m_HasOnUpdate = false;

	public:
		std::string m_ScriptPath;

		friend class ScriptSystem;

	private:
		lua_State* m_LuaState = nullptr;
		
	};

}




