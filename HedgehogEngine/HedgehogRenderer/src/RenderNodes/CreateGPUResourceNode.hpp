#pragma once

#include "IRenderNode.hpp"
#include "IConfigurableNode.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include <vector>

namespace Renderer
{
    // A no-op render node whose sole purpose is to declare named GPU resources
    // (textures and buffers) in the render graph. Resources are created during
    // RenderGraph::Compile() and registered by name in the TextureRegistry so that
    // subsequent nodes can reference them via inputs/outputs slot declarations.
    class CreateGPUResourceNode : public IRenderNode, public IConfigurableNode
    {
    public:
        CreateGPUResourceNode()  = default;
        ~CreateGPUResourceNode() override = default;

        // Parses the "resources" block from the YAML node entry.
        void SetConfig(const YAML::Node& entry) override;

        // Registers all declared resource descs with the graph.
        void Setup(RenderGraph& graph) override;

        // No GPU work to submit — resources are created at Compile time.
        void Execute(RenderContext& /*ctx*/) override {}
        void Cleanup(RHI::IRHIDevice& /*device*/) override {}

    private:
        std::vector<GraphTextureDesc> m_TextureDescs;
        std::vector<GraphBufferDesc>  m_BufferDescs;
    };
}
