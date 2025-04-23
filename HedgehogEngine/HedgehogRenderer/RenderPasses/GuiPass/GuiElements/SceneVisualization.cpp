#include "SceneVisualization.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"

#include "ECS/Entity.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index)
	{
		auto& scene = context.GetEngineContext().GetScene();
		auto& component = scene.GetHierarchyComponent(entity);
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
		bool isSelected = false;
		if (scene.IsGameObjectSelected() && entity == scene.GetSelectedGameObject())
		{
			isSelected = true;
		}
		if (isSelected)
			nodeFlags |= ImGuiTreeNodeFlags_Selected;

		if (component.mChildren.size() > 0)
		{
			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				scene.SelectGameObject(entity);
			}
			++index;
			if (node_open)
			{
				for (auto entity : component.mChildren)
				{
					DrawHierarchyNode(context, entity, index);
				}
				ImGui::TreePop();
			}
		}
		else
		{
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				scene.SelectGameObject(entity);
			}
			++index;
		}

	}

	void SceneVisualization::Draw(Context::Context& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(sizeX, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		auto& scene = context.GetEngineContext().GetScene();
		ImGui::Begin(scene.GetSceneName().c_str(), NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

		{
			int index = 0;

			ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);

			if (ImGui::Button("Create object", sz))
			{
				scene.CreateGameObject();
			}
			if (ImGui::Button("Delete object", sz))
			{
				scene.DeleteGameObject();
			}

			DrawHierarchyNode(context, scene.GetRoot(), index);

		}
		ImGui::End();
	}
}

