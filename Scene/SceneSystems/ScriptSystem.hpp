#pragma once

#include "ECS/System.h"
#include "ECS/Coordinator.h"

namespace Scene
{
	class ScriptComponent;

	class ScriptSystem : public ECS::System 
	{
	public:
		void Update(ECS::Coordinator& coordinator, float dt);

		void ClearScriptComponent(ECS::Entity entity, ECS::Coordinator& coordinator);
		void ChangeScript(ECS::Entity entity, ECS::Coordinator& coordinator);

	private:
		void CallOnEnable(ECS::Coordinator& coordinator);
		void CallUpdate(ECS::Coordinator& coordinator, float dt);
		void CallOnDisable(ECS::Coordinator& coordinator);

	};
}


