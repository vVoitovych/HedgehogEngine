#pragma once

#include "ECS/System.h"
#include "ECS/Coordinator.h"

namespace Scene
{
	class ScriptComponent;

	class ScriptSystem : public ECS::System 
	{
	public:
		void Update(ECS::Coordinator& coordinator);
		void ChangeEnable(ScriptComponent& component, bool val);

		void ClearScriptComponent(ECS::Entity entity, ECS::Coordinator& coordinator);
		void ChangeScript(ECS::Entity entity, std::string scriptPath, ECS::Coordinator& coordinator);

	};
}


