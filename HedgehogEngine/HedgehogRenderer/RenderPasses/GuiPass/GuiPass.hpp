#pragma once

#include "ECS/Entity.h"

#include <memory>
#include <optional>
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

		static bool IsCursorPositionInGUI();
		void ShowMaterialWindow();
		void ShowCpuProfilingWindow();

	private:
		void DrawGui(Context::Context& context);

		void DrawInspector(Context::Context& context);
		void DrawTitle(Context::Context& context);
		void DrawTransform(Context::Context& context);
		void DrawMesh(Context::Context& context);
		void DrawRender(Context::Context& context);
		void DrawLight(Context::Context& context);
		void DrawScript(Context::Context& context);

		void DrawMainMenu(Context::Context& context);
		void DrawSceneInspector(Context::Context& context);
		void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index);

		void DrawSettingsWindow(Context::Context& context);
		void DrawCpuProfilingWindow(Context::Context& context);
		void UploadFonts();

	private:
		std::unique_ptr<Wrappers::RenderPass> m_RenderPass;
		std::unique_ptr<Wrappers::FrameBuffer> m_FrameBuffer;
		std::unique_ptr<Wrappers::DescriptorAllocator> m_DescriptorAllocator;

		std::optional<ECS::Entity> m_SelectedObject;

		bool m_MaterialWindowShow = false;
		bool m_CpuProfilingWindow = false;
	};
}


