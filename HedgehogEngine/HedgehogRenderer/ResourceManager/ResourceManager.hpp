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

		void ResizeFrameBufferSizeDependenteResources(const Context::Context& context);
		void ResizeSettingsDependenteResources(const Context::Context& context);

		const Wrappers::Image& GetColorBuffer() const;
		const Wrappers::Image& GetDepthBuffer() const;

		const Wrappers::Image& GetShadowMap() const;
		const Wrappers::Image& GetShadowMask() const;

	private:
		void CreateDepthBuffer(const Context::Context& context);
		void CreateColorBuffer(const Context::Context& context);

		void CreateShadowMap(const Context::Context& context);
		void CreateShadowMask(const Context::Context& context);

	private:
		std::unique_ptr<Wrappers::Image> m_DepthBuffer;
		std::unique_ptr<Wrappers::Image> m_ColorBuffer;

		std::unique_ptr<Wrappers::Image> m_ShadowMap;
		std::unique_ptr<Wrappers::Image> m_ShadowMask;
	};
}



