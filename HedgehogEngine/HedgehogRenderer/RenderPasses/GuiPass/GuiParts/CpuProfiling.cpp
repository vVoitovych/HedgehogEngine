#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"
#include "HedgehogCommon/CpuProfiler/CpuProfiler.hpp"
#include "HedgehogCommon/CpuProfiler/CpuTimeStamp.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{ 
	void DrawTimeStamps(const std::vector<std::unique_ptr<Context::CpuTimeStampNode>>& nodes, int& index)
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;

		for (auto& node : nodes)
		{
			std::string stamp = node->GetName() + ": " + std::to_string(node->DurationMs());

			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, stamp.c_str());
			++index;
			if (node_open)
			{
				DrawTimeStamps(node->GetChildren(), index);
				ImGui::TreePop();
			}
		}
	}

	void GuiPass::DrawCpuProfilingWindow(Context::Context& context)
	{

		if (m_CpuProfilingWindow)
		{
			float sizeX = 600.0f;
			float sizeY = 400.0f;
			ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(750.0f, 150.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));

			ImGui::Begin("Cpu Profiling", &m_CpuProfilingWindow, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Main"))
				{

					if (ImGui::MenuItem("Quit")) { m_CpuProfilingWindow = false; }
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			auto& timeStamps = GET_TIME_STAMP();
			int index = 0;
			DrawTimeStamps(timeStamps, index);

			ImGui::End();
		}
	}

	void GuiPass::ShowCpuProfilingWindow()
	{
		m_CpuProfilingWindow = true;
	}

}