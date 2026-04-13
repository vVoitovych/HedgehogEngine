#pragma once

#include "ECS/System.h"
#include "ECS/ECS.h"

namespace Scene
{
	class ScriptComponent;

	class ScriptSystem : public ECS::System
	{
	public:
		void Update(ECS::ECS& ecs, float dt);

		void ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs);
		void ChangeScript(ECS::Entity entity, ECS::ECS& ecs);
		void InitScript(ECS::Entity entity, ECS::ECS& ecs);

	private:
		void CallOnEnable(ECS::ECS& ecs);
		void CallUpdate(ECS::ECS& ecs, float dt);
		void CallOnDisable(ECS::ECS& ecs);

	};
}


