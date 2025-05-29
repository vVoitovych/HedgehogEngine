#pragma once

#include <memory>
#include <string>

namespace Wrappers
{
    class RenderPass;
    class Pipeline;
    class FrameBuffer;
}

namespace Context
{
    class Context;
}

namespace Renderer
{
    class ResourceManager;

	class ShadowmapPass
	{
    public:
        ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager);
        ~ShadowmapPass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void UpdateResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        std::unique_ptr<Wrappers::RenderPass> m_RenderPass;
        std::unique_ptr<Wrappers::FrameBuffer> m_FrameBuffer;
        std::unique_ptr<Wrappers::Pipeline> m_Pipeline;
             
	};

}


