#include "HedgehogEditor.hpp"
#include "ThirdParty/ImGui/imgui/imgui.h"

#include <chrono>

namespace HedgehogEditor
{
	HedgehogEditor::HedgehogEditor()
	{
		InitImGUI();
	}

	HedgehogEditor::~HedgehogEditor()
	{
	}

	void HedgehogEditor::Run()
	{
		MainLoop();
		Cleanup();
	}

	void HedgehogEditor::MainLoop()
	{
		while (!m_ShouldClose)
		{
			RenderEditorUI();
			DrawEditorPanels();
		}
	}

	void HedgehogEditor::Cleanup()
	{
	}

	void HedgehogEditor::InitImGUI()
	{

		
	}

	void HedgehogEditor::RenderEditorUI()
	{
	}

	void HedgehogEditor::DrawEditorPanels()
	{
	}

	float HedgehogEditor::GetFrameTime()
	{
		static auto prevTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		prevTime = currentTime;
		return deltaTime;
	}

}


