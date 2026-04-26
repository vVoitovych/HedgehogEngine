#pragma once

#include <memory>

namespace Context
{
    class Context;
}

namespace Renderer
{
    class RHIContext;
    class ThreadContext;
    class ResourceManager;
    class RenderQueue;

    class Renderer
    {
    public:
        explicit Renderer(Context::Context& context);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup(Context::Context& context);

        void  BeginGui();
        void  DrawFrame(Context::Context& context);
        float GetAspectRatio() const;
        void* GetSceneViewTextureId() const;
        void  SetSceneViewSize(uint32_t width, uint32_t height);

    private:
        std::unique_ptr<RHIContext>      m_RHIContext;
        std::unique_ptr<ThreadContext>   m_ThreadContext;
        std::unique_ptr<ResourceManager> m_ResourceManager;
        std::unique_ptr<RenderQueue>     m_RenderQueue;

        uint32_t m_DesiredSceneW = 0;
        uint32_t m_DesiredSceneH = 0;
    };
}
