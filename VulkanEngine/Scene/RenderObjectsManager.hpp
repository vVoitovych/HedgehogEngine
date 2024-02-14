#pragma once

#include "ECS/Entity.h"

#include <vector>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 

#include <glm/glm.hpp>

namespace Scene
{
	struct RenderableObject
	{
		glm::mat4 objMatrix;
		size_t meshIndex;
		bool isVisible;
	};

	class RenderObjectsManager
	{
	public:
		RenderableObject& AddEntity(ECS::Entity entity);
		void RemoveEntity(ECS::Entity entity);
		RenderableObject& GetEntityData(ECS::Entity entity);
		std::vector<RenderableObject>& GetRenderableObjects();
		const std::vector<RenderableObject>& GetRenderableObjects() const;
		ECS::Entity GetEntityByIndex(size_t index);
	private:
		std::vector<RenderableObject> mRenderableObjects;
		std::unordered_map<ECS::Entity, size_t> mEntityToIndexMap;
		std::unordered_map<size_t, ECS::Entity> mIndexToEntityMap;

	};
}



