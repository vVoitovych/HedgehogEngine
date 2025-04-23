#pragma once

#include <memory>
#include <string>

namespace Wrappers
{
	class RenderPass;
	class DescriptorAllocator;
	class FrameBuffer;
}

namespace Context
{
	class Context;
}

namespace Renderer
{
	class ResourceManager;

	class GuiPass
	{
	public:
		GuiPass(const Context::Context& context, const ResourceManager& resourceManager);
		~GuiPass();

		void Render(Context::Context& context, const ResourceManager& resourceManager);
		void Cleanup(const Context::Context& context);

		void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);
	public:
		static bool IsCursorPositionInGUI();

	private:
		void DrawGui(Context::Context& context);

		void UploadFonts();

	private:
		std::unique_ptr<Wrappers::RenderPass> mRenderPass;
		std::unique_ptr<Wrappers::FrameBuffer> mFrameBuffer;
		std::unique_ptr<Wrappers::DescriptorAllocator> mDescriptorAllocator;

	};
}


