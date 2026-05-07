#include "Application.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogGui/EditorGuiNode.hpp"

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
        m_Context  = std::make_unique<HedgehogEngine::HedgehogEngine>();
        m_Renderer = std::make_unique<Renderer::Renderer>(*m_Context);

        auto& window = m_Context->GetWindowContext().GetWindow();
        m_Renderer->AppendNode("Gui",
            std::make_unique<HedgehogGui::EditorGuiNode>(window, *m_Renderer, *m_Context));

        m_Context->GetEngineContext().LoadDefaultScene();

        LOGINFO("Editor initialized");
    }

    void EditorApplication::MainLoop()
    {
        while (!m_Context->GetWindowContext().ShouldClose())
        {
            const float dt = GetFrameTime();
            m_Context->GetWindowContext().HandleInput();
            m_Context->UpdateContext(dt, m_Renderer->GetAspectRatio());

            m_Renderer->OnBeginFrame();
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
