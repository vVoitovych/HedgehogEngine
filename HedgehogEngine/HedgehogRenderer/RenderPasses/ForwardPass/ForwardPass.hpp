#pragma once

#include "HedgehogCommon/Common/RendererSettings.hpp"
#include "HedgehogContext/Containers/LightContainer/Light.hpp"
#include "HedgehogMath/Matrix.hpp"

#include <vector>
#include <memory>
#include <string>

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

	class ForwardPass
	{
    public:
        ForwardPass(const Context::Context& context, const ResourceManager& resourceManager);
        ~ForwardPass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        struct ForwardPassFrameUniform
        {
            alignas(16) HM::Matrix4x4 view;
            alignas(16) HM::Matrix4x4 viewProj;
            alignas(16) HM::Vector3 eyePosition;
            alignas(16) Context::Light lights[MAX_LIGHTS_COUNT];
            size_t lightCount;
        };

    private:
        std::unique_ptr<Wrappers::RenderPass> m_RenderPass;
        std::unique_ptr<Wrappers::FrameBuffer> m_FrameBuffer;
        std::unique_ptr<Wrappers::Pipeline> m_Pipeline;
        
        std::unique_ptr<Wrappers::DescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<Wrappers::DescriptorAllocator> m_FrameAllocator;

        std::vector<Wrappers::UBO<ForwardPassFrameUniform>> m_FrameUniforms;
        std::vector<Wrappers::DescriptorSet> m_FrameSets;

	};

}


