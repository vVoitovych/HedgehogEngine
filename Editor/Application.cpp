#include "Application.hpp"
#include "EditorGui.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <vector>

namespace Editor
{
    EditorApplication::EditorApplication()  = default;
    EditorApplication::~EditorApplication() = default;

    void EditorApplication::Run(uint32_t maxFrames)
    {
        Init();
        MainLoop(maxFrames);
    }

    void EditorApplication::Init()
    {
        m_Context   = std::make_unique<HedgehogEngine::HedgehogEngine>();
        m_Renderer  = std::make_unique<Renderer::Renderer>(*m_Context);
        m_EditorGui = std::make_unique<EditorGui>(*m_Context);

        // WantCaptureMouse is true even over the scene image (it's an ImGui window); exempt it.
        m_Context->GetWindowContext().GetWindow().SetGuiCallback([this]()
        {
            return ImGui::GetIO().WantCaptureMouse && !m_EditorGui->IsSceneViewHovered();
        });

        LOGINFO("Editor initialized");
    }

    void EditorApplication::RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames)
    {
        Init();
        loadBenchmarkScene();

        LOGINFO("Benchmark: warming up for ", warmupFrames, " frame(s)...");
        auto& windowContext = m_Context->GetWindowContext();
        for (uint32_t i = 0; i < warmupFrames && !windowContext.ShouldClose(); ++i)
            stepFrame();

        LOGINFO("Benchmark: measuring ", measureFrames, " frame(s)...");
        m_Renderer->BeginFrameStatsCapture();

        std::vector<double> frameTimesMs;
        frameTimesMs.reserve(measureFrames);
        for (uint32_t i = 0; i < measureFrames && !windowContext.ShouldClose(); ++i)
            frameTimesMs.push_back(static_cast<double>(stepFrame()) * 1000.0);

        m_Renderer->EndFrameStatsCaptureAndLogReport();

        if (!frameTimesMs.empty())
        {
            std::vector<double> sorted = frameTimesMs;
            std::sort(sorted.begin(), sorted.end());

            const double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0)
                             / static_cast<double>(sorted.size());
            const size_t p95Index = std::min(sorted.size() - 1,
                static_cast<size_t>(static_cast<double>(sorted.size()) * 0.95));

            char line[192];
            std::snprintf(line, sizeof(line),
                "Benchmark  | %-20s | %8.3f | %8.3f | %8.3f | %8.3f | %7zu (avg %.1f FPS)",
                "Frame(wall)", avg, sorted.front(), sorted.back(), sorted[p95Index],
                sorted.size(), avg > 0.0 ? 1000.0 / avg : 0.0);
            LOGINFO(line);
        }

        Cleanup();
    }

    void EditorApplication::MainLoop(uint32_t maxFrames)
    {
        uint32_t frameIndex = 0;
        while (!m_Context->GetWindowContext().ShouldClose()
            && (maxFrames == 0 || frameIndex < maxFrames))
        {
            ++frameIndex;
            stepFrame();
        }

        Cleanup();
    }

    float EditorApplication::stepFrame()
    {
        const float dt = GetFrameTime();
        m_Context->GetWindowContext().HandleInput();
        m_Context->UpdateContext(dt, m_Renderer->GetAspectRatio());

        m_Renderer->BeginGui();
        m_EditorGui->Draw(*m_Context, m_Renderer->GetSceneViewTextureId());
        m_Renderer->SetSceneViewSize(m_EditorGui->GetSceneViewWidth(),
                                     m_EditorGui->GetSceneViewHeight());

        m_Renderer->DrawFrame(*m_Context);
        return dt;
    }

    void EditorApplication::loadBenchmarkScene()
    {
        constexpr const char* BENCHMARK_SCENE = "assets://Scenes/benchmark.yaml";

        auto&       engineContext = m_Context->GetEngineContext();
        const auto& fileSystem    = engineContext.GetFileSystem();

        const auto physicalPath = fileSystem.ResolvePhysical(BENCHMARK_SCENE);
        if (!physicalPath || !engineContext.LoadScene(physicalPath->string()))
        {
            LOGWARNING("Benchmark: failed to load '", BENCHMARK_SCENE,
                       "'; measuring whatever scene is currently open instead.");
        }
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
