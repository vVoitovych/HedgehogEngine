#include "MainMenu.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"

#include "Scene/Scene.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	void MainMenu::Draw(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New")) { scene.ResetScene(); }
				if (ImGui::MenuItem("Rename")) { scene.RenameScene(); }
				if (ImGui::MenuItem("Open")) { scene.Load(); }
				if (ImGui::MenuItem("Save")) { scene.Save(); }

				ImGui::Separator();
				if (ImGui::MenuItem("Quit", "Alt+F4")) {}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Create game object")) { scene.CreateGameObject(); }

				ImGui::Separator();
				if (ImGui::BeginMenu("Add component"))
				{
					if (ImGui::MenuItem("Mesh component")) { scene.TryToAddMeshComponent(); }
					if (ImGui::MenuItem("Render component")) { scene.TryToAddRenderComponent(); }
					if (ImGui::MenuItem("Light component")) { scene.TryToAddLightComponent(); }
					if (ImGui::MenuItem("Script component")) {}
					ImGui::EndMenu();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Create material")) { context.GetEngineContext().GetMaterialContainer().CreateNewMaterial(); }
				ImGui::EndMenu();
			}


			ImGui::EndMainMenuBar();
		}
	}
}

