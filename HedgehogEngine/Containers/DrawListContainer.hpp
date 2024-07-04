#pragma once

#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

namespace Scene
{
	class Scene;
}

namespace Context
{
	class MaterialContainer;

	struct DrawObject
	{
		size_t meshIndex;
		glm::mat4 objMatrix;
	};

	struct DrawNode
	{
		size_t materialIndex;
		std::vector<DrawObject> objects;
	};

	using DrawList = std::vector<DrawNode>;

	class DrawListContainer
	{
	public:
		void Update(const Scene::Scene& scene, const MaterialContainer& materialContainer);

		const DrawList& GetOpaqueList() const;
		const DrawList& GetCutoffList() const;
		const DrawList& GetTransparentList() const;
	private:
		void AddObjectToOpaqueList(DrawObject object, size_t materialIndex);
		void AddObjectToCutoffList(DrawObject object, size_t materialIndex);
		void AddObjectToTransparentList(DrawObject object, size_t materialIndex);

		void AddObjectToList(DrawList& drawList, DrawObject object, size_t materialIndex);

	private:
		DrawList mOpaqueList;
		DrawList mCutoffList;
		DrawList mTransparentList;
	};
}

