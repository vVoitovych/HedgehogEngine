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

namespace Editor
{
    class EditorGui;

    class EditorApplication
    {
    public:
        EditorApplication();
        ~EditorApplication();

        EditorApplication(const EditorApplication&)            = delete;
        EditorApplication& operator=(const EditorApplication&) = delete;
        EditorApplication(EditorApplication&&)                 = delete;
        EditorApplication& operator=(EditorApplication&&)      = delete;

        void Run();

    private:
        void  Init();
        void  MainLoop();
        void  Cleanup();
        float GetFrameTime();

    private:
        std::unique_ptr<Context::Context>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<EditorGui>          m_EditorGui;
    };
}
