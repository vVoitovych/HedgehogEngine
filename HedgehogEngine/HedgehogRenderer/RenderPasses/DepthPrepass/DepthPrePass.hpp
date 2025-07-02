#pragma once

#include <HedgehogMath/Matrix.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Wrappers
{
    class RenderPass;
    class Pipeline;
    class FrameBuffer;
    class DescriptorSetLayout;
    class DescriptorAllocator;
    class DescriptorSet;

    template<typename T>
    class UBO;
}

namespace Context
{
    class Context;
}

namespace Renderer
{
    class ResourceManager;

	class DepthPrePass
	{
    public:
        DepthPrePass(const Context::Context& context, const ResourceManager& resourceManager);
        ~DepthPrePass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        struct DepthPrepassFrameUniform
        {
            alignas(16) HM::Matrix4x4 viewProj;
        };

    private:
        std::unique_ptr<Wrappers::RenderPass> m_RenderPass;
        std::unique_ptr<Wrappers::FrameBuffer> m_FrameBuffer;
        std::unique_ptr<Wrappers::Pipeline> m_Pipeline;
            
        std::unique_ptr<Wrappers::DescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<Wrappers::DescriptorAllocator> m_FrameAllocator;

        std::vector<Wrappers::UBO<DepthPrepassFrameUniform>> m_FrameUniforms;
        std::vector<Wrappers::DescriptorSet> m_FrameSets;

	};

}


