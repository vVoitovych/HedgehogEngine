#pragma once

#include <vector>
#include <array>
#include <memory>
#include <string>

#include "HedgehogMath/Matrix.hpp"

namespace Wrappers
{
    class Device;
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

	class ShadowmapPass
	{
    public:
        ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager);
        ~ShadowmapPass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void UpdateData(const Context::Context& context);
        void UpdateResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        void UpdateFrameBuffer(const Context::Context& context, const ResourceManager& resourceManager);
        void CreateRenderPass(const Wrappers::Device& device, const ResourceManager& resourceManager);
        void CreateAllocator(const Wrappers::Device& device);
        void CreateLayout(const Wrappers::Device& device);
        void CreateUniforms(const Wrappers::Device& device);
        void CreateSets(const Wrappers::Device& device);
        void CreatePipeline(const Wrappers::Device& device);

        void UpdateShadowmapMatricies(const Context::Context& context);

    private:
        struct FrameUniform
        {
            alignas(16) HM::Matrix4x4 shadowMatrix;
        };

    private:
        std::array<HM::Matrix4x4, 4> m_ShadowmapMatricies;

        std::unique_ptr<Wrappers::RenderPass> m_RenderPass;
        std::unique_ptr<Wrappers::FrameBuffer> m_FrameBuffer;
        std::unique_ptr<Wrappers::Pipeline> m_Pipeline;
             
        std::unique_ptr<Wrappers::DescriptorSetLayout> m_ShadowmapLayout;
        std::unique_ptr<Wrappers::DescriptorAllocator> m_ShadowmapAllocator;

        std::vector<Wrappers::UBO<FrameUniform>> m_ShadowmapUniforms;
        std::vector<Wrappers::DescriptorSet> m_ShadowmapSets;
	};

}


