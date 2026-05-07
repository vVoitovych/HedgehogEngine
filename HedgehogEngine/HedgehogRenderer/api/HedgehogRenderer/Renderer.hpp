#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace HedgehogEngine
{
    class HedgehogEngine;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Renderer
{
    class IRenderNode;
    class RHIContext;
    class ThreadContext;
    class ResourceManager;
    class RenderQueue;

    class Renderer
    {
    public:
        explicit Renderer(HedgehogEngine::HedgehogEngine& context);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup(HedgehogEngine::HedgehogEngine& context);

        void  OnBeginFrame();
        void  DrawFrame(HedgehogEngine::HedgehogEngine& context);
        float GetAspectRatio() const;

        // Append an externally created node to the end of the render queue.
        // Call before the first DrawFrame.
        void AppendNode(const std::string& name, std::unique_ptr<IRenderNode> node);

        // Access underlying device and named GPU textures (for external nodes).
        RHI::IRHIDevice&         GetDevice();
        const RHI::IRHITexture&  GetTexture(const std::string& name) const;

        // Query an opaque resource exported by a named node. Returns nullptr if not found.
        void* QueryNodeExport(const std::string& nodeName, const std::string& key) const;

        // Request a render-target resize; applied at end of the current frame.
        void  SetRenderTargetSize(const std::string& resourceName, uint32_t width, uint32_t height);

    private:
        std::unique_ptr<RHIContext>      m_RHIContext;
        std::unique_ptr<ThreadContext>   m_ThreadContext;
        std::unique_ptr<ResourceManager> m_ResourceManager;
        std::unique_ptr<RenderQueue>     m_RenderQueue;

        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_PendingResizes;
    };
}
