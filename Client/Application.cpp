#include "Application.hpp"
#include "EditorGui.hpp"

#include "HedgehogEngine/HedgehogContext/Context/Context.hpp"
#include "HedgehogEngine/HedgehogContext/Context/WindowContext.hpp"

#include "HedgehogEngine/HedgehogRenderer/Renderer/Renderer.hpp"

#include "Logger/api/Logger.hpp"

#include <chrono>

namespace HedgehogClient
{
    HedgehogClient::HedgehogClient()
    {
    }

    HedgehogClient::~HedgehogClient()
    {
    }

    void HedgehogClient::Run()
    {
        InitVulkan();
        MainLoop();
    }

    void HedgehogClient::InitVulkan()
    {
        m_Context   = std::make_unique<Context::Context>();
        m_Renderer  = std::make_unique<Renderer::Renderer>(*m_Context);
        m_EditorGui = std::make_unique<EditorGui>();
        LOGINFO("Vulkan initialized");
    }

    void HedgehogClient::MainLoop()
    {
        while (!m_Context->GetWindowContext().ShouldClose())
        {
            const float dt = GetFrameTime();
            m_Context->GetWindowContext().HandleInput();
            m_Context->UpdateContext(dt, m_Renderer->GetAspectRatio());

            // GUI preparation: NewFrame + editor panels + draw data generation
            m_Renderer->BeginGui();
            m_EditorGui->Draw(*m_Context);

            m_Renderer->DrawFrame(*m_Context);
        }

        Cleanup();
    }

    void HedgehogClient::Cleanup()
    {
        m_Renderer->Cleanup(*m_Context);
        m_Context->Cleanup();
    }

    float HedgehogClient::GetFrameTime()
    {
        static auto prevTime = std::chrono::high_resolution_clock::now();

        const auto  currentTime = std::chrono::high_resolution_clock::now();
        const float deltaTime   = std::chrono::duration<float, std::chrono::seconds::period>(
            currentTime - prevTime).count();
        prevTime = currentTime;
        return deltaTime;
    }
}
