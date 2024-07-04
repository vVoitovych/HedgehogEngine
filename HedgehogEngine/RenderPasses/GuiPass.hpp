#pragma once

#include "ECS/Entity.h"

#include <memory>
#include <string>

namespace Wrappers
{
	class RenderPass;
	class DescriptorAllocator;
	class FrameBuffer;

}

namespace Renderer
{
	class RenderContext;
	class ResourceManager;

	class GuiPass
	{
	public:
		GuiPass(const RenderContext& context, const ResourceManager& resourceManager);
		~GuiPass();

		void Render(RenderContext& context, const ResourceManager& resourceManager);
		void Cleanup(const RenderContext& context);

		void ResizeResources(const RenderContext& context, const ResourceManager& resourceManager);
	public:
		static bool IsCursorPositionInGUI();

	private:
		void DrawGui(RenderContext& context);
		void DrawInspector(RenderContext& context);
		void DrawTitle(RenderContext& context);
		void DrawTransform(RenderContext& context);
		void DrawMesh(RenderContext& context);
		void DrawRender(RenderContext& context);
		void DrawLight(RenderContext& context);

		void DrawScene(RenderContext& context);
		void DrawHierarchyNode(RenderContext& context, ECS::Entity entity, int& index);
		void ShowAppMainMenuBar(RenderContext& context);

		void UploadFonts();

	private:
		std::unique_ptr<Wrappers::RenderPass> mRenderPass;
		std::unique_ptr<Wrappers::FrameBuffer> mFrameBuffer;
		std::unique_ptr<Wrappers::DescriptorAllocator> mDescriptorAllocator;

	};
}


