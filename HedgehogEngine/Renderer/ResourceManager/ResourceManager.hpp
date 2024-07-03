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
		ResourceManager(const RenderContext& context);
		~ResourceManager();

		void Cleanup(const RenderContext& context);

		void ResizeResources(const RenderContext& context);

		const Image& GetColorBuffer() const;
		const Image& GetDepthBuffer() const;

	private:
		void CreateDepthBuffer(const RenderContext& context);
		void CreateColorBuffer(const RenderContext& context);

	private:
		std::unique_ptr<Image> mDepthBuffer;
		std::unique_ptr<Image> mColorBuffer;
	};
}



