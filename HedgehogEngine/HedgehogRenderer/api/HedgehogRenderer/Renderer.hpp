#pragma once

#include <memory>

namespace HedgehogEngine
{
    class HedgehogEngine;
}

namespace Renderer
{
    class RHIContext;
    class ThreadContext;
    class ResourceManager;
    class RenderGraph;

    class Renderer
    {
    public:
        explicit Renderer(HedgehogEngine::HedgehogEngine& context);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup(HedgehogEngine::HedgehogEngine& context);

        void  BeginGui();
        void  DrawFrame(HedgehogEngine::HedgehogEngine& context);
        float GetAspectRatio() const;
        void* GetSceneViewTextureId() const;
        void  SetSceneViewSize(uint32_t width, uint32_t height);

    private:
        std::unique_ptr<RHIContext>      m_RHIContext;
        std::unique_ptr<ThreadContext>   m_ThreadContext;
        std::unique_ptr<ResourceManager> m_ResourceManager;
        std::unique_ptr<RenderGraph>     m_RenderGraph;

        uint32_t m_DesiredSceneW = 0;
        uint32_t m_DesiredSceneH = 0;
    };
}
