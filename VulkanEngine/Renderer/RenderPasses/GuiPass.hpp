#pragma once

#include "ECS/Entity.h"

#include <memory>
#include <string>

namespace Renderer
{
	class RenderContext;
	class ResourceManager;
	class RenderPass;
	class DescriptorPool;
	class FrameBuffer;

	class GuiPass
	{
	public:
		GuiPass(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
		~GuiPass();

		void Render(std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
		void Cleanup(const std::unique_ptr<RenderContext>& context);

		void ResizeResources(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
	public:
		static bool IsCursorPositionInGUI();

	private:
		void DrawGui(const std::unique_ptr<RenderContext>& context);
		void DrawInspector(const std::unique_ptr<RenderContext>& context);
		void DrawScene(const std::unique_ptr<RenderContext>& context);
		void DrawHierarchyNode(const std::unique_ptr<RenderContext>& context, ECS::Entity entity, int& index);
		void ShowAppMainMenuBar(const std::unique_ptr<RenderContext>& context);

		void UploadFonts();

	private:
		std::unique_ptr<RenderPass> mRenderPass;
		std::unique_ptr<FrameBuffer> mFrameBuffer;
		std::unique_ptr<DescriptorPool> mDescriptorPool;

	};
}


