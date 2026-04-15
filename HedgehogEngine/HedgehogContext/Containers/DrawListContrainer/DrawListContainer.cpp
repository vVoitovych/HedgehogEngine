#include "DrawListContainer.hpp"

#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

#include "Logger/api/Logger.hpp"

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
            if (renderComponent.m_IsVisible && renderComponent.m_MaterialIndex.has_value())
            {
                auto& mesh      = scene.GetMeshComponent(entity);
                auto& transform = scene.GetTransformComponent(entity);

                if (!mesh.m_MeshIndex.has_value())
                {
                    LOGERROR("Entity ", entity, " does not have a mesh component, but has a render component.");
                    continue;
                }

                auto  materialIndex = renderComponent.m_MaterialIndex.value();
                auto& material      = materialContainer.GetMaterialDataByIndex(materialIndex);

                DrawObject object;
                object.meshIndex = mesh.m_MeshIndex.value();
                object.objMatrix = transform.m_ObjMatrix;

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
                    LOGERROR("Unsupported material type.");
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

    void DrawListContainer::AddObjectToOpaqueList(DrawObject object, uint64_t materialIndex)
    {
        AddObjectToList(mOpaqueList, object, materialIndex);
    }

    void DrawListContainer::AddObjectToCutoffList(DrawObject object, uint64_t materialIndex)
    {
        AddObjectToList(mCutoffList, object, materialIndex);
    }

    void DrawListContainer::AddObjectToTransparentList(DrawObject object, uint64_t materialIndex)
    {
        AddObjectToList(mTransparentList, object, materialIndex);
    }

    void DrawListContainer::AddObjectToList(DrawList& drawList, DrawObject object, uint64_t materialIndex)
    {
        auto it = std::find_if(drawList.begin(), drawList.end(),
            [materialIndex](const DrawNode& node) { return node.materialIndex == materialIndex; });

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
