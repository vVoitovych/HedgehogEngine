#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RHI { class IRHIDevice; }

namespace Renderer
{
    class IRenderNode;
    class ResourceManager;
    struct PreRenderContext;
    struct RenderContext;

    class RenderQueue
    {
    public:
        RenderQueue(RHI::IRHIDevice& device, ResourceManager& resourceManager);
        ~RenderQueue();

        RenderQueue(const RenderQueue&)            = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;
        RenderQueue(RenderQueue&&)                 = delete;
        RenderQueue& operator=(RenderQueue&&)      = delete;

        void Cleanup(RHI::IRHIDevice& device);

        void  OnBeginFrame();
        void  OnDiscardFrame();
        void* QueryNodeExport(const std::string& nodeName, const std::string& key) const;

        // Append an externally created node to the end of the queue.
        void AppendNode(const std::string& name, std::unique_ptr<IRenderNode> node);

        void PreRender(const PreRenderContext& ctx);
        void Render(RenderContext& ctx);

    private:
        std::vector<std::string>                  m_NodeNames;
        std::vector<std::unique_ptr<IRenderNode>> m_Nodes;
    };

} // namespace Renderer
