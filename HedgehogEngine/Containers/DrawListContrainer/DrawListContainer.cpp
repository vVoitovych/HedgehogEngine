#include "DrawListContainer.hpp"
#include "Containers/MaterialContainer/MaterialContainer.hpp"

#include "Containers/MaterialContainer/MaterialData.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

#include "Logger/Logger.hpp"

#include <algorithm>

namespace Context
{
	void DrawListContainer::Update(const Scene::Scene& scene, const MaterialContainer& materialContainer)
	{
		mOpaqueList.clear();
		mCutoffList.clear();
		mTransparentList.clear();

		for (auto entity : scene.GetRenderableEntities())
		{
			auto& renderComponent = scene.GetRenderComponent(entity);
			if (renderComponent.mIsVisible && renderComponent.mMaterialIndex.has_value())
			{
				auto& mesh = scene.GetMeshComponent(entity);
				auto& transform = scene.GetTransformComponent(entity);

				if (!mesh.mMeshIndex.has_value())
				{
					LOGERROR("Entity", entity, " do not have mesh component, but has render component");
					continue;
				}
				auto materialIndex = renderComponent.mMaterialIndex.value();
				auto& material = materialContainer.GetMaterialDataByIndex(materialIndex);

				DrawObject object;
				object.meshIndex = mesh.mMeshIndex.value();
				object.objMatrix = transform.mObjMatrix;

				switch (material.type)
				{
				case MaterialType::Opaque:
					AddObjectToOpaqueList(object, materialIndex);
					break;
				case MaterialType::Cutoff:
					AddObjectToCutoffList(object, materialIndex);
					break;
				case MaterialType::Transparent:
					AddObjectToTransparentList(object, materialIndex);
					break;
				default:
					LOGERROR("Unsupported material type");
					break;
				}
			}
		}
	}

	const DrawList& DrawListContainer::GetOpaqueList() const
	{
		return mOpaqueList;
	}

	const DrawList& DrawListContainer::GetCutoffList() const
	{
		return mCutoffList;
	}

	const DrawList& DrawListContainer::GetTransparentList() const
	{
		return mTransparentList;
	}

	void DrawListContainer::AddObjectToOpaqueList(DrawObject object, size_t materialIndex)
	{
		auto& drawList = mOpaqueList;
		AddObjectToList(drawList, object, materialIndex);
	}

	void DrawListContainer::AddObjectToCutoffList(DrawObject object, size_t materialIndex)
	{
		auto& drawList = mCutoffList;
		AddObjectToList(drawList, object, materialIndex);
	}

	void DrawListContainer::AddObjectToTransparentList(DrawObject object, size_t materialIndex)
	{
		auto& drawList = mTransparentList;
		AddObjectToList(drawList, object, materialIndex);
	}

	void DrawListContainer::AddObjectToList(DrawList& drawList, DrawObject object, size_t materialIndex)
	{
		auto it = std::find_if(drawList.begin(), drawList.end(), [materialIndex](DrawNode  node) { return node.materialIndex == materialIndex; });
		if (it == drawList.end())
		{
			DrawNode node;
			node.materialIndex = materialIndex;
			node.objects.push_back(object);
			drawList.push_back(node);
		}
		else
		{
			it->objects.push_back(object);
		}
	}

}


