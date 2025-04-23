#include "MaterialEditor.hpp"

#include "HedgehogContext/Context/Context.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	bool MaterialEditor::s_MaterialWindowShow = false;

	void MaterialEditor::Draw(Context::Context& context)
	{
		if (s_MaterialWindowShow)
		{
			float sizeX = 300.0f;
			float sizeY = 300.0f;
			ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(450.0f, 150.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));

			ImGui::Begin("Material Editor", &s_MaterialWindowShow);
			ImGui::Text("Materials!");

			ImGui::End();
		}
	}

	void MaterialEditor::ShowMaterialWindow()
	{
		s_MaterialWindowShow = true;
	}

}



