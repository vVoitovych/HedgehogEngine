#pragma once

#include <memory>

namespace Context
{
    class Context;
}

namespace Renderer
{
    class Renderer;
}

namespace HedgehogClient
{
    class EditorGui;

    class HedgehogClient
    {
    public:
        HedgehogClient();
        ~HedgehogClient();

        void Run();

    private:
        void InitVulkan();
        void MainLoop();
        void Cleanup();

        float GetFrameTime();

    private:
        std::unique_ptr<Context::Context>   mContext;
        std::unique_ptr<Renderer::Renderer> mRenderer;
        std::unique_ptr<EditorGui>          mEditorGui;
    };
}
