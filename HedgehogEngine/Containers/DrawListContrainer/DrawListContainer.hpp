#pragma once

#include "HedgehogMath/Matrix.hpp"

#include <vector>

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
		HM::Matrix4x4 objMatrix;
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

