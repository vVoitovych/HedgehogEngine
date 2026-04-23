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
        mContext   = std::make_unique<Context::Context>();
        mRenderer  = std::make_unique<Renderer::Renderer>(*mContext);
        mEditorGui = std::make_unique<EditorGui>();
        LOGINFO("Vulkan initialized");
    }

    void HedgehogClient::MainLoop()
    {
        while (!mContext->GetWindowContext().ShouldClose())
        {
            const float dt = GetFrameTime();
            mContext->GetWindowContext().HandleInput();
            mContext->UpdateContext(dt, mRenderer->GetAspectRatio());

            // GUI preparation: NewFrame + editor panels + draw data generation
            mRenderer->BeginGui();
            mEditorGui->Draw(*mContext);

            mRenderer->DrawFrame(*mContext);
        }

        Cleanup();
    }

    void HedgehogClient::Cleanup()
    {
        mRenderer->Cleanup(*mContext);
        mContext->Cleanup();
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
