#include "Application.hpp"
#include "EditorGui.hpp"

#include "HedgehogEngine/HedgehogContext/Context/Context.hpp"
#include "HedgehogEngine/HedgehogContext/Context/WindowContext.hpp"
#include "HedgehogEngine/HedgehogRenderer/Renderer/Renderer.hpp"

#include "Logger/api/Logger.hpp"

#include <chrono>

namespace Editor
{
    EditorApplication::EditorApplication()  = default;
    EditorApplication::~EditorApplication() = default;

    void EditorApplication::Run()
    {
        Init();
        MainLoop();
    }

    void EditorApplication::Init()
    {
        m_Context   = std::make_unique<Context::Context>();
        m_Renderer  = std::make_unique<Renderer::Renderer>(*m_Context);
        m_EditorGui = std::make_unique<EditorGui>();
        LOGINFO("Editor initialized");
    }

    void EditorApplication::MainLoop()
    {
        while (!m_Context->GetWindowContext().ShouldClose())
        {
            const float dt = GetFrameTime();
            m_Context->GetWindowContext().HandleInput();
            m_Context->UpdateContext(dt, m_Renderer->GetAspectRatio());

            m_Renderer->BeginGui();
            m_EditorGui->Draw(*m_Context, m_Renderer->GetSceneViewTextureId());
            m_Renderer->SetSceneViewSize(m_EditorGui->GetSceneViewWidth(),
                                         m_EditorGui->GetSceneViewHeight());

            m_Renderer->DrawFrame(*m_Context);
        }

        Cleanup();
    }

    void EditorApplication::Cleanup()
    {
        m_Renderer->Cleanup(*m_Context);
        m_Context->Cleanup();
    }

    float EditorApplication::GetFrameTime()
    {
        static auto s_PrevTime = std::chrono::high_resolution_clock::now();

        const auto  currentTime = std::chrono::high_resolution_clock::now();
        const float deltaTime   = std::chrono::duration<float, std::chrono::seconds::period>(
            currentTime - s_PrevTime).count();
        s_PrevTime = currentTime;
        return deltaTime;
    }
}
