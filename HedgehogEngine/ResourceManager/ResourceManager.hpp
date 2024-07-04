#pragma once

#include <memory>
#include <vector>

namespace Wrappers
{
	class FrameBuffer;
	class Image;
}

namespace Renderer
{
	class RenderContext;

	class ResourceManager
	{
	public:
		ResourceManager(const RenderContext& context);
		~ResourceManager();

		void Cleanup(const RenderContext& context);

		void ResizeResources(const RenderContext& context);

		const Wrappers::Image& GetColorBuffer() const;
		const Wrappers::Image& GetDepthBuffer() const;

	private:
		void CreateDepthBuffer(const RenderContext& context);
		void CreateColorBuffer(const RenderContext& context);

	private:
		std::unique_ptr<Wrappers::Image> mDepthBuffer;
		std::unique_ptr<Wrappers::Image> mColorBuffer;
	};
}



