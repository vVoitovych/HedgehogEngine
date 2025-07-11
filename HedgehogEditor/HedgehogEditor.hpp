#pragma once

namespace HedgehogEditor
{
	class HedgehogEditor
	{
	public:
		HedgehogEditor();
		~HedgehogEditor();

		void Run();

	private:
		void MainLoop();
		void Cleanup();

		void InitImGUI();
		void RenderEditorUI();
		void DrawEditorPanels();

		float GetFrameTime();

	private:
		bool m_ShouldClose = false;
	};


}



