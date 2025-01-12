#pragma once

#include <memory>
#include <vector>

namespace Wrappers
{
	class Image;
}

namespace Context
{
	class Context;
}

namespace Renderer
{
	class ResourceManager
	{
	public:
		ResourceManager(const Context::Context& context);
		~ResourceManager();

		void Cleanup(const Context::Context& context);

		void ResizeResources(const Context::Context& context);

		const Wrappers::Image& GetColorBuffer() const;
		const Wrappers::Image& GetDepthBuffer() const;

	private:
		void CreateDepthBuffer(const Context::Context& context);
		void CreateColorBuffer(const Context::Context& context);

	private:
		std::unique_ptr<Wrappers::Image> mDepthBuffer;
		std::unique_ptr<Wrappers::Image> mColorBuffer;
	};
}



