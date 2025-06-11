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
		uint64_t meshIndex;
		HM::Matrix4x4 objMatrix;
	};

	struct DrawNode
	{
		uint64_t materialIndex;
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
		void AddObjectToOpaqueList(DrawObject object, uint64_t materialIndex);
		void AddObjectToCutoffList(DrawObject object, uint64_t materialIndex);
		void AddObjectToTransparentList(DrawObject object, uint64_t materialIndex);

		void AddObjectToList(DrawList& drawList, DrawObject object, uint64_t materialIndex);

	private:
		DrawList mOpaqueList;
		DrawList mCutoffList;
		DrawList mTransparentList;
	};
}

