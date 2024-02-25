#include "RenderObjectsManager.hpp"

#include <cassert>

namespace Scene
{
	RenderableObject& RenderObjectsManager::AddEntity(ECS::Entity entity)
	{
		assert(mEntityToIndexMap.find(entity) == mEntityToIndexMap.end() && "Try to add same entity twice.");

		size_t index = mRenderableObjects.size();
		mEntityToIndexMap[entity] = index;
		mIndexToEntityMap[index] = entity;
		mRenderableObjects.push_back(RenderableObject());
		return mRenderableObjects.back();
	}

	void RenderObjectsManager::RemoveEntity(ECS::Entity entity)
	{
		assert(mEntityToIndexMap.find(entity) != mEntityToIndexMap.end() && "Try to remove invalid entity.");

		size_t indexOfRemovedEntity = mEntityToIndexMap[entity];
		size_t indexOfLastElement = mRenderableObjects.size() - 1;
		mRenderableObjects[indexOfRemovedEntity] = mRenderableObjects[indexOfLastElement];

		const ECS::Entity entityOfLastElement = mIndexToEntityMap[indexOfLastElement];
		mEntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
		mIndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

		mEntityToIndexMap.erase(entity);
		mIndexToEntityMap.erase(indexOfLastElement);

		mRenderableObjects.pop_back();
	}

	RenderableObject& RenderObjectsManager::GetEntityData(ECS::Entity entity)
	{
		assert(mEntityToIndexMap.find(entity) != mEntityToIndexMap.end() && "Try to get invalid entity.");

		return mRenderableObjects[mEntityToIndexMap[entity]];
	}
	std::vector<RenderableObject>& RenderObjectsManager::GetRenderableObjects()
	{
		return mRenderableObjects;
	}
	const std::vector<RenderableObject>& RenderObjectsManager::GetRenderableObjects() const
	{
		return mRenderableObjects;
	}
	ECS::Entity RenderObjectsManager::GetEntityByIndex(size_t index)
	{
		assert(mIndexToEntityMap.find(index) != mIndexToEntityMap.end() && "Try to get invalid entity.");
		return mIndexToEntityMap[index];
	}
}



