#pragma once

#include <memory>
#include <string>

namespace HedgehogEngine
{
    class HedgehogEngine;
}

namespace Renderer
{
    class RHIContext;
    class ThreadContext;
    class ResourceManager;
    class ShaderManager;
    class PipelineManager;
    class RenderNodeManager;
    class RenderGraph;

    enum class RenderMode
    {
        Editor,
        Game,
    };

    class Renderer
    {
    public:
        explicit Renderer(HedgehogEngine::HedgehogEngine& context);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup(HedgehogEngine::HedgehogEngine& context);

        // Tear down the current graph and rebuild it from the rgq preset for the given mode.
        void SetMode(RenderMode mode);

        void  BeginGui();
        void  DrawFrame(HedgehogEngine::HedgehogEngine& context);
        float GetAspectRatio() const;
        void* GetSceneViewTextureId() const;
        void  SetSceneViewSize(uint32_t width, uint32_t height);

    private:
        void BuildGraph(const std::string& presetPath);

        std::unique_ptr<RHIContext>        m_RHIContext;
        std::unique_ptr<ThreadContext>     m_ThreadContext;
        std::unique_ptr<ResourceManager>   m_ResourceManager;
        std::unique_ptr<ShaderManager>     m_ShaderManager;
        std::unique_ptr<PipelineManager>   m_PipelineManager;
        std::unique_ptr<RenderNodeManager> m_NodeManager;
        std::unique_ptr<RenderGraph>       m_RenderGraph;

        RenderMode m_Mode        = RenderMode::Editor;
        uint32_t   m_DesiredSceneW = 0;
        uint32_t   m_DesiredSceneH = 0;
    };
}
