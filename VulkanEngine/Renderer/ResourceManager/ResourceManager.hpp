#pragma once

#include <memory>
#include <vector>

namespace Renderer
{
	class RenderContext;
	class FrameBuffer;
	class Image;

	class ResourceManager
	{
	public:
		ResourceManager(const std::unique_ptr<RenderContext>& context);
		~ResourceManager();

		void Cleanup(const std::unique_ptr<RenderContext>& context);

		void ResizeResources(const std::unique_ptr<RenderContext>& context);

		const std::unique_ptr<Image>& GetColorBuffer() const;
		const std::unique_ptr<Image>& GetDepthBuffer() const;

	private:
		void CreateDepthBuffer(const std::unique_ptr<RenderContext>& context);
		void CreateColorBuffer(const std::unique_ptr<RenderContext>& context);

	private:
		std::unique_ptr<Image> mDepthBuffer;
		std::unique_ptr<Image> mColorBuffer;
	};
}



