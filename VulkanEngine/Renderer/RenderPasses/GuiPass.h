#pragma once

#include <memory>
#include <vector>

namespace Renderer
{
	class RenderContext;
	class RenderPass;
	class DescriptorPool;
	class FrameBuffer;

	class GuiPass
	{
	public:
		GuiPass(const std::unique_ptr<RenderContext>& context);
		~GuiPass();

		void Render(std::unique_ptr<RenderContext>& context);
		void Cleanup(const std::unique_ptr<RenderContext>& context);

		void ResizeResources(const std::unique_ptr<RenderContext>& context);
	public:
		static bool IsCursorPositionInGUI();

	private:
		void DrawGui(const std::unique_ptr<RenderContext>& context);
		void DrawTransformWindow(const std::unique_ptr<RenderContext>& context);

		void UploadFonts();

	private:
		std::unique_ptr<RenderPass> mRenderPass;
		std::vector<FrameBuffer> mFrameBuffers;
		std::unique_ptr<DescriptorPool> mDescriptorPool;
	};
}


